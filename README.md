# FORGE Synth

FORGE Synth is a **neural‑driven wavetable synthesizer** written in C++ with JUCE and Python.  The project is split into two main parts:

1.  The **C++ core** – an AudioProcessor that accepts MIDI, reads a latent vector, runs a neural network (ONNX) or a deterministic fallback, and outputs a wavetable‑oscillated stream.
2.  The **Python API** – a tiny FastAPI server that can expose the model and an AI toolchain for generating wave tables on demand.

## Features

| Feature | Status |
|---------|--------|
| VST3 / AU / Standalone | Buildable (once JUCE is present)
| MIDI Poly‑voice | ✅ A simple wavetable oscillator
| Latent‑space UI | ✅ Two sliders (X/Y)
| ADSR envelope | ✅ In‑voice
| Filter | ✅ Low‑pass Q
| Master gain | ✅ Linear
| Python inference engine | **Skeleton** – ready to plug a Torch model
| ONNX export script | ✅ `model_exporter.py`

## Build Flow

```bash
# From the root of the repository
cd forge_synth/plugin_source
mkdir -p build && cd build
cmake .. -GNinja -DJUCE_ROOT=../juce-8.0.0   # point JUCE into the repo
ninja                                         # produces Standalone, VST3, AU
```

> **Note** – JUCE must be located inside the project tree (`/home/.../Projects/FORGE_Synth/juce-8.0.0`).  The build script queries `JUCE_ROOT` provided above.

## Running the Plugin

After build, launch the stand‑alone executable:

```bash
./ForgeSynth_Standalone
```

Inside the GUI you can tweak:

* **Latent X/Y** – moves the latent vector in 2‑D space
* **ADSR** – Attack / Decay / Sustain / Release
* **Filter** – Cut‑off & Resonance
* **Master Gain** – overall output level

For DAW usage, drop `ForgeSynth.vst3` into your DAW’s plugin folder.  The plugin will respond to MIDI Note On/Off.

## AI Export / Routing

The small Python script `ai_engine/model_exporter.py` shows how you could export a Pytorch model to ONNX:

```bash
python ai_engine/model_exporter.py
```

The following folder contains a sample config file (`config/ai_config.json`) and environment variables (`config/ai.env`).

## Documentation

* `docs/architecture.md` – high‑level architecture
* `docs/usage.md` – build & run instructions
* `docs/ai_api.md` – FastAPI endpoint description
* `docs/ai_config_schema.md` – JSON schema for the config file

Happy hacking! 🚀
# synth_AI
