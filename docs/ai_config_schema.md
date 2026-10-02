# AI Configuration Schema

The **FORGE_Synth** plugin can optionally load an ONNX neural model for wavetable generation. The configuration file `ai_config.json` located in the working directory specifies the location of the model and optional runtime parameters.

```json
{
  "model_path": "ai_engine/model.onnx",
  "device": "cpu",          
  "inference_rate_hz": 30
}
```

* `model_path` – path to the exported ONNX file.  It may be relative to the workflow directory or an absolute path.
* `device` – either `cpu` or `cuda`.  The Python routing module will attempt to use the specified backend.
* `inference_rate_hz` – frequency at which the inference engine recomputes a wavetable; a higher value trades latency for computational cost.  Default is `30` Hz.
```

The file is JSON‑encoded and parsed by `ai_routing.py` (or the future C++ wrapper) during startup.
