import io
import wave
import numpy as np
import pytest
from fastapi.testclient import TestClient
from forge_synth.ai_engine.ai_routing import ForgeSynthAI, Settings, create_app, normalise_table


@pytest.fixture
def client():
    with TestClient(create_app(Settings())) as value:
        yield value


def test_health_and_periodic_table(client):
    assert client.get("/health").json()["backend"] == "deterministic"
    result = client.post("/wavetable", json={"latent": [0, 0]}).json()
    table = np.array(result["wavetable"])
    assert result["size"] == len(table) == 2048
    assert np.isfinite(table).all() and np.max(np.abs(table)) <= 1
    assert abs(table.mean()) < 1e-7
    np.testing.assert_allclose(table, np.sin(np.arange(2048) * 2 * np.pi / 2048), atol=1e-6)
    other = client.post("/wavetable", json={"latent": [1, 1]}).json()["wavetable"]
    assert not np.allclose(table, other)


@pytest.mark.parametrize("payload", [{}, {"latent": []}, {"latent": [0]}, {"latent": [0, 0, 0]},
    {"latent": [-1, 0]}, {"latent": [0, 2]}, {"latent": ["bad", 0]}, {"latent": [True, 0]},
    {"latent": ["NaN", 0]}, {"latent": [0, 0], "extra": 1}])
def test_rejects_invalid_requests(client, payload):
    assert client.post("/wavetable", json=payload).status_code == 422


def test_wav_export(client):
    result = client.post("/wavetable.wav", json={"latent": [0.4, 0.7]})
    assert result.status_code == 200 and result.headers["content-type"] == "audio/wav"
    with wave.open(io.BytesIO(result.content)) as audio:
        assert (audio.getnchannels(), audio.getsampwidth(), audio.getnframes(), audio.getframerate()) == (1, 2, 2048, 48000)


def test_configuration_precedence_and_relative_paths(tmp_path, monkeypatch):
    path = tmp_path / "settings.json"
    path.write_text('{"backend":"onnx","model_path":"model.onnx","threads":2}')
    monkeypatch.setenv("FORGE_AI_CONFIG", str(path))
    monkeypatch.setenv("FORGE_AI_THREADS", "3")
    settings = Settings.from_environment()
    assert settings.threads == 3 and settings.model_path == str(tmp_path / "model.onnx")


@pytest.mark.parametrize("table", [np.zeros(2048), np.ones(2048), np.full(2048, np.nan), np.zeros(100), np.full(2048, np.inf)])
def test_invalid_model_output(table):
    with pytest.raises(ValueError):
        normalise_table(table)


def test_bad_model_fails_startup():
    with pytest.raises(ValueError, match="existing model"):
        with TestClient(create_app(Settings(backend="onnx", model_path="/missing/model.onnx"))):
            pass


def test_generation_error_is_not_a_fake_success(client):
    client.app.state.engine.generate_wavetable = lambda _: (_ for _ in ()).throw(ValueError("private model path"))
    result = client.post("/wavetable", json={"latent": [0.5, 0.5]})
    assert result.status_code == 503 and "private model path" not in result.text


def test_onnx_reference_matches_deterministic(tmp_path):
    pytest.importorskip("onnx")
    pytest.importorskip("onnxruntime")
    from forge_synth.ai_engine.model_exporter import export_to_onnx
    path = export_to_onnx(tmp_path / "reference.onnx")
    onnx_engine = ForgeSynthAI(Settings(backend="onnx", model_path=str(path)))
    deterministic = ForgeSynthAI(Settings())
    for latent in [(0, 0), (1, 1), (0.2, 0.8), (1, 0), (0, 1)]:
        np.testing.assert_allclose(onnx_engine.generate_wavetable(latent), deterministic.generate_wavetable(latent), atol=2e-6)
