# AI Configuration Schema

The *ai_config.json* file controls how the Python AI routing server loads and runs the neural inference engine.  The JSON must contain the following keys:

| Key | Type | Description | Example |
|-----|------|-------------|---------|
| `model_path` | string | Filesystem path to the ONNX model file. | "/home/.../FORGE_Synth/ai_engine/model.onnx" |
| `device` | string | Device to run inference on: ``cpu`` or ``cuda`` (if a GPU is available). | "cpu" |
| `inference_rate_hz` | integer | Minimum Hz at which the server should attempt to run inference.  Lower than this is rate‑controlled. | 30 |
| `log_level` | string | Logging verbosity: ``debug``, ``info``, ``warning``, ``error``. | "info" |

The schema is intentionally permissive for legacy projects.  Validation is performed in `ai_engine/ai_router.py` using `pydantic`, so providing an extra key simply results in a validation error.
