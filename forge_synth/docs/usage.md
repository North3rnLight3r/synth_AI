# ForgeSynth Usage Guide

This documentation explains how to build, run, and operate the neural‑driven wavetable synthesizer.  It assumes a **Linux** host with network‑ready tools **CMake<10** and **Ninja**.  For macOS or Windows contact the maintainers for platform‑specific wrappers.

---

## 1. Prerequisites

| Item | Version | Notes |
|------|---------|-------|
| `gcc`/`clang` | >= 10 | Required for building the C++ core. |
| `CMake` | >= 3.10 | Installs via `sudo apt install cmake`. |
| `Ninja` | >= 1.10 | Optional, but used by the provided `CMakeLists.txt`. |
| `python3` | >= 3.8 | Used to run the optional FastAPI server.
| `pip` | <= 24 | To install the Python dependencies listed in `requirements.txt`. |
| `JUCE` SDK | 8.x or greater | Must be placed inside the repository, e.g. `forge_synth/juce-8.0.0`. |

> **The JUCE engine must reside in the same repository.**  The CMake configuration in `plugin_source/CMakeLists.txt` looks for `JUCE_ROOT` in a relative location.  If you place the SDK somewhere else, update the `JUCE_ROOT` definition accordingly.

---

## 2. Building the Plugin

```bash
# 1. Install prerequisites (Ubuntu example)
# sudo apt install clang cmake ninja-build

# 2. (Optional) Pull the latest JUCE SDK
# wget https://github.com/juce-framework/JUCE/archive/refs/tags/v8.0.0.tar.gz -O juce-8.0.0.tar.gz
# tar xf juce-8.0.0.tar.gz
# mv JUCE-8.0.0 juce-8.0.0

# 3. Build
mkdir -p plug_source/build && cd plug_source/build
cmake .. -GNinja -DJUCE_ROOT=../juce-8.0.0
ninja
```

The commands above produce:

* `ForgeSynth_Standalone` – a cross‑platform executable.
* `ForgeSynth_VST3/ForgeSynth.vst3` – a VST3 bundle for DAWs.
* `ForgeSynth_AU/ForgeSynth.bundle` – an AU bundle (macOS only).

---

## 3. Running the Standalone

```bash
./ForgeSynth_Standalone
```

You will see a window with sliders for:

* **Latent X/Y** – moves the latent vector
* **Attack / Decay / Sustain / Release** – ADSR envelope
* **Filter Cutoff / Resonance** – low‑pass filter
* **Master Gain** – overall volume

Drag the sliders and play a key from an attached MIDI controller to hear the wavetable oscillator in action.

---

## 4. Loading as a Plugin

Drop the `ForgeSynth.vst3` bundle into your DAW’s plugin folder (e.g. `/usr/share/vst3/`), then load it like any other plugin.  The same GUI will appear inside the DAW.

---

## 5. Python AI Routing (Optional)

The synthesizer ships with a skeleton AI router that can be used to generate wavetable artefacts on‑the‑fly.  The router is a **FastAPI** service that loads a pre‑exported ONNX model.

```bash
# Activate the venv and install dependencies
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Start the router
uvicorn ai_engine.ai_routing:app --reload
```

The router listens on `localhost:8000` and offers a single endpoint:

```
POST /wavetable
Content‑Type: application/json

{
  "latent": [0.3, 0.7]
}
```

It returns a JSON payload with the generated wavetable samples (32 by default).  The Flask application currently uses a deterministic fallback in place of a real neural network.

---

## 6. Configuring the AI Service

Place the following file in the repo root: `config/ai.env`.  It supplies the run‑time configuration for the Python service.

```
ONNX_MODEL_PATH=ai_engine/model.onnx
ONNX_DEVICE=cpu
ONNX_INFERENCE_RATE_HZ=30
CLOUD_API_KEY=xxxxxxxxxxxx
CLOUD_API_URL=https://api.provider.com/v1/generate
AI_LOG_LEVEL=INFO
```

The Python script reads these values via `os.getenv`.  Adjust them to point at your own ONNX model or cloud endpoint.

---

## 7. Extending the Synth

The `NeuralEngine` class in `plugin_source/Source/NeuralEngine.h` can be swapped for an actual ONNX‑runtime inference wrapper.  The template in `ai_engine/ai_routing.py` shows how to load an exported Pytorch model using `onnxruntime`.  Replace the existing fallback with a real `NeuralEngine` implementation that performs inference on the latent vector.

---

## 8. Known Limitations

* The current SSE engine does **not** perform real neural inference – it simply morphs a sine wave.  To enable RTX‑level wavetable generation you must supply a proper ONNX model.
* The FastAPI router is a *stubs*framework; no GPU acceleration or batch inference is configured.
* On Windows, the build system requires the MinGW toolchain or Visual Studio.  The provided `CMakeLists.txt` is tuned for Linux/Unix.

---

## 9. Troubleshooting

* **`cmake` fails with “FAILED TO FIND JUCEConfig.cmake”** – place the JUCE SDK under `forge_synth/juce-8.0.0` or set `JUCE_ROOT` to its location.
* **`ninja build` stops with errors** – run `ninja -v` to see the detailed command.  Missing headers are usually a sign of a broken JUCE path.
* **On Windows, missing `Microsoft Visual Studio`** – install the Community edition or use the portable `vswhere` tooling.

---

## 10. Further Reading

* `docs/architecture.md` – overall system design
* `docs/ai_api.md` – API contract for the AI router
* `docs/ai_config_schema.md` – JSON schema for the AI settings

---

Happy hacking! 🚀
