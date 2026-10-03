"""Optional local wavetable API. The VST never makes network calls on its audio thread."""
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Annotated, Literal
import io
import json
import logging
import os
import wave

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import Response
import numpy as np
from pydantic import BaseModel, ConfigDict, Field

TABLE_SIZE = 2048
LOGGER = logging.getLogger("forge.ai")
ROOT = Path(__file__).resolve().parents[2]


class Settings(BaseModel):
    model_config = ConfigDict(extra="forbid")
    backend: Literal["deterministic", "onnx"] = "deterministic"
    model_path: str = ""
    device: Literal["cpu", "cuda"] = "cpu"
    threads: int = Field(default=1, ge=1, le=16)

    @classmethod
    def from_environment(cls):
        path_text = os.getenv("FORGE_AI_CONFIG")
        data = {}
        base = ROOT
        if path_text:
            path = Path(path_text).expanduser().resolve()
            base = path.parent
            data = json.loads(path.read_text(encoding="utf-8"))
            if not isinstance(data, dict):
                raise ValueError("FORGE_AI_CONFIG must contain a JSON object")
        mapping = {"FORGE_AI_BACKEND": "backend", "ONNX_MODEL_PATH": "model_path",
                   "ONNX_DEVICE": "device", "FORGE_AI_THREADS": "threads"}
        for env, field in mapping.items():
            if env in os.environ:
                data[field] = os.environ[env]
        settings = cls.model_validate(data)
        if settings.model_path:
            model = Path(settings.model_path).expanduser()
            settings.model_path = str((base / model).resolve())
        return settings


Coordinate = Annotated[float, Field(ge=0, le=1, allow_inf_nan=False, strict=True)]


class WavetableRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")
    latent: tuple[Coordinate, Coordinate]


class WavetableResponse(BaseModel):
    backend: str
    size: int
    wavetable: list[float]


def normalise_table(values):
    table = np.asarray(values, dtype=np.float32)
    if table.shape != (TABLE_SIZE,) or not np.isfinite(table).all():
        raise ValueError("Model must produce 2048 finite samples")
    # Float64 accumulation prevents overflow while validating hostile model output.
    table = table.astype(np.float64)
    table -= table.mean()
    peak = np.abs(table).max()
    if peak < 1e-8:
        raise ValueError("Model produced silence or a constant waveform")
    return (table / max(1.0, peak)).astype(np.float32)


class ForgeSynthAI:
    def __init__(self, settings: Settings | None = None):
        self.settings = settings or Settings.from_environment()
        self.session = None
        # Precompute periodic Fourier corners once, with endpoint excluded.
        phase = np.arange(TABLE_SIZE, dtype=np.float64) * (2 * np.pi / TABLE_SIZE)
        harmonics = np.arange(1, 257, dtype=np.float64)[:, None]
        sinus = np.sin(harmonics * phase)
        odd = harmonics[:, 0] % 2 == 1
        sine = np.sin(phase)
        saw = (sinus / harmonics).sum(axis=0) * 0.58
        square = (sinus[odd] / harmonics[odd]).sum(axis=0) * 1.08
        sign = np.where(harmonics[odd] % 4 == 1, 1, -1)
        triangle = (sinus[odd] * sign / harmonics[odd] ** 2).sum(axis=0) * 0.81
        self.corners = np.stack((sine, saw, triangle, square)).astype(np.float32)
        if self.settings.backend == "onnx":
            if not self.settings.model_path or not Path(self.settings.model_path).is_file():
                raise ValueError("ONNX_MODEL_PATH must point to an existing model")
            import onnxruntime as ort  # Optional dependency, never needed for local synthesis.
            provider = "CUDAExecutionProvider" if self.settings.device == "cuda" else "CPUExecutionProvider"
            if provider not in ort.get_available_providers():
                raise ValueError(f"Requested provider is unavailable: {provider}")
            options = ort.SessionOptions()
            options.intra_op_num_threads = self.settings.threads
            options.inter_op_num_threads = 1
            self.session = ort.InferenceSession(self.settings.model_path, sess_options=options, providers=[provider])
            if provider not in self.session.get_providers():
                raise ValueError(f"Requested provider failed to initialise: {provider}")
            inputs, outputs = self.session.get_inputs(), self.session.get_outputs()
            if (len(inputs) != 1 or len(outputs) != 1 or inputs[0].name != "latent_input"
                    or inputs[0].type != "tensor(float)" or len(inputs[0].shape) != 2 or inputs[0].shape[1] != 2
                    or inputs[0].shape[0] not in (1, None, "batch")
                    or outputs[0].name != "wavetable_output" or outputs[0].type != "tensor(float)"
                    or len(outputs[0].shape) != 2 or outputs[0].shape[1] != TABLE_SIZE):
                raise ValueError("Expected float32 latent_input [1,2] -> wavetable_output [1,2048]")
            self.generate_wavetable((0.5, 0.5))  # Fail startup if the configured model is unusable.
        elif self.settings.device != "cpu":
            raise ValueError("The deterministic backend supports CPU only")

    def generate_wavetable(self, latent_vector):
        latent = np.asarray(latent_vector, dtype=np.float32)
        if latent.shape != (2,) or not np.isfinite(latent).all() or np.any(latent < 0) or np.any(latent > 1):
            raise ValueError("latent must contain exactly two finite values in [0,1]")
        if self.session is not None:
            output = self.session.run(["wavetable_output"], {"latent_input": latent[None, :]})[0]
            if output.shape != (1, TABLE_SIZE):
                raise ValueError("Unexpected model output shape")
            return normalise_table(output[0])
        x, y = latent
        bottom = self.corners[0] * (1 - x) + self.corners[1] * x
        top = self.corners[2] * (1 - x) + self.corners[3] * x
        return normalise_table(bottom * (1 - y) + top * y)


def create_app(settings: Settings | None = None):
    @asynccontextmanager
    async def lifespan(application):
        application.state.engine = ForgeSynthAI(settings)
        LOGGER.info("FORGE backend ready: %s", application.state.engine.settings.backend)
        yield
        application.state.engine = None

    application = FastAPI(title="FORGE Wavetable API", version="0.2.0", lifespan=lifespan)

    @application.get("/health")
    def health(request: Request):
        engine = request.app.state.engine
        return {"status": "ok", "backend": engine.settings.backend, "device": engine.settings.device, "table_size": TABLE_SIZE}

    def generate(payload, request):
        try:
            return request.app.state.engine.generate_wavetable(payload.latent)
        except Exception as error:
            LOGGER.exception("Wavetable generation failed")
            raise HTTPException(503, "Wavetable generation failed; check the local server log") from error

    @application.post("/wavetable", response_model=WavetableResponse)
    def wavetable(payload: WavetableRequest, request: Request):
        table = generate(payload, request)
        return {"backend": request.app.state.engine.settings.backend, "size": TABLE_SIZE, "wavetable": table.tolist()}

    @application.post("/wavetable.wav")
    def wavetable_wav(payload: WavetableRequest, request: Request):
        table = generate(payload, request)
        data = io.BytesIO()
        with wave.open(data, "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(48000)
            output.writeframes(np.round(table * 32767).astype("<i2").tobytes())
        return Response(data.getvalue(), media_type="audio/wav", headers={"Content-Disposition": 'attachment; filename="forge-wavetable.wav"'})

    return application


app = create_app()
