# Optional AI backend configuration

The VST has no service or API-key requirement; it synthesizes locally. This schema configures only the separate optional Python service. The checked-in example `config/ai_config.json` is:

```json
{
  "backend": "deterministic",
  "model_path": "../forge_synth/models/forge_reference.onnx",
  "device": "cpu",
  "threads": 1
}
```

The JSON root must be an object and unknown keys are rejected. `backend` accepts `deterministic` or `onnx`; `model_path` can be absolute or relative to the JSON file; `device` accepts `cpu` or `cuda`; and `threads` is 1–16 (default 1). The deterministic backend is local CPU. CUDA is accepted only when ONNX Runtime reports an available CUDA provider. There is no trained model distributed with this project.

| Environment variable | JSON key | Default | Meaning |
|---|---|---|---|
| `FORGE_AI_CONFIG` | — | No JSON file | Path to the optional JSON file. |
| `FORGE_AI_BACKEND` | `backend` | `deterministic` | Select `deterministic` or `onnx`. |
| `ONNX_MODEL_PATH` | `model_path` | Empty | Local, compatible ONNX model. |
| `ONNX_DEVICE` | `device` | `cpu` | ONNX Execution Provider. |
| `FORGE_AI_THREADS` | `threads` | `1` | ONNX inference thread limit. |

Environment variables override JSON. Host and port are passed to the Uvicorn command and are not VST settings. The backend does not implicitly read `.env` files. Use a local ignored config file for machine-specific settings; never commit API keys. The backend has no authentication or cloud API routes. Keep it bound to `127.0.0.1` unless you independently provide and review network security.

An ONNX decoder must accept float32 `latent_input[1,2]` with each latent in `[0,1]`, and emit float32 `wavetable_output[1,2048]`. It must return finite, nonconstant samples. The HTTP API validates, recentres and normalises output. The optional exporter writes an **untrained** reference graph to check the interface; that graph is not a learned or production-ready sound model.
