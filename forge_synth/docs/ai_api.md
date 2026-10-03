# Local FORGE wavetable API

The Python API is a separate local tool for wavetable generation. The VST does not call it or depend on a network connection. Generate one cycle and import it in FORGE using **Load WT**; enable **Use Imported** and save your host session or preset to retain the table.

## Start the backend

Python 3.10+ is recommended. From the repository root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python -m uvicorn forge_synth.ai_engine.ai_routing:app --host 127.0.0.1 --port 8000
```

The service initializes the CPU deterministic generator at startup. Check `http://127.0.0.1:8000/health` or browse the API schema at `http://127.0.0.1:8000/docs`. The example binds only to localhost. Do not expose an unauthenticated development service to the public internet. Uvicorn logs errors locally; the response omits private runtime details.

## Routes and responses

`POST /wavetable` accepts JSON such as `{"latent":[0.42,0.73]}` and returns a backend label, table size (2,048), and 2,048 finite floating-point samples. Each of the two finite coordinates must be in `[0,1]`. Invalid input returns HTTP 422.

`POST /wavetable.wav` accepts the same JSON and returns a one-cycle, 48 kHz, mono 16-bit PCM WAV containing exactly 2,048 frames. Save and import it in the plugin:

```bash
curl --fail --silent --show-error \
  http://127.0.0.1:8000/wavetable.wav \
  -H 'Content-Type: application/json' \
  --data '{"latent":[0.42,0.73]}' \
  --output /tmp/forge-cycle.wav
```

The plug-in checks file size, channel count and signal quality, then centres, normalises, band-limits and stores the cycle in the patch and host project state. Import accepts an uncompressed mono or stereo 2,048-frame WAV or AIFF.

## Environment and model routing

The out-of-the-box deterministic CPU generator works without a trained model. It shares the VST's sine–saw / triangle–square timbre coordinates and returns distinct, normalised tables. It is sound generation, **not learned or neural synthesis**.

To add optional ONNX Runtime packages and export an interface reference:

```bash
python -m pip install -r requirements-onnx.txt
python -m forge_synth.ai_engine.model_exporter --output forge_synth/models/forge_reference.onnx
```

The generated reference checks inference plumbing. It is untrained and is not a production sound model. Supply a decoder trained for `float32 latent_input[1,2]` → `float32 wavetable_output[1,2048]`. The service validates this contract at startup and checks that the selected provider is available.

```bash
export FORGE_AI_BACKEND=onnx
export ONNX_MODEL_PATH=/absolute/path/to/trained-decoder.onnx
export ONNX_DEVICE=cpu
export FORGE_AI_THREADS=1
python -m uvicorn forge_synth.ai_engine.ai_routing:app --host 127.0.0.1 --port 8000
```

Set `ONNX_DEVICE=cuda` only when the installed runtime lists `CUDAExecutionProvider`. Missing, incompatible, or unusable ONNX models fail at startup. Failed requests return 503 and are recorded in the local process logs. See [the configuration schema](../../docs/ai_config_schema.md) for all settings and precedence rules.

`requirements.txt` installs FastAPI, Uvicorn, NumPy and environment handling. `requirements-onnx.txt` adds ONNX and ONNX Runtime. `requirements-dev.txt` adds test tools. Run `.venv/bin/python -m pytest tests/test_ai.py -q` from the project root for API, request validation, WAV output, environment precedence, and ONNX reference smoke tests.

The application deliberately does not source environment files, accept secrets, or invoke a cloud endpoint. For local setup, use shell exports or a process manager; the ignored `config/ai.local.json` path may be selected with `FORGE_AI_CONFIG`. If independently operating a remote deployment, add and review TLS, authentication, rate limits, process security and secret handling first. The example setup does not create those protections.
