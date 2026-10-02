# AI API

This project ships a minimal FastAPI server that exposes the pretrained ONNX model used by the VST plug‑in.  The server accepts a JSON payload describing a 2‑D latent vector and returns a 2048‑sample wavetable.

## End‑points

| Method | Path | Body | Response | Notes |
|--------|------|------|----------|-------|
| POST | `/wavetable` | `{"latent":[x,y]}` | `{ "wavetable": [f0, f1, …, f2047] }` | The server internally loads the ONNX model (see `ai_config.json`) and performs a single inference pass.

## Running the server

```bash
source venv/bin/activate
export $(cut -d= -f1 config/ai.env)  # loads env variables
uvicorn ai_engine.ai_routing:app --port 8000
```

The server will log each request to `stderr`.

## Example using cURL

```bash
curl -X POST http://localhost:8000/wavetable \
  -H "Content-Type: application/json" \
  -d '{"latent":[0.42, 0.73]}'
```