# ForgeSynth Architecture

## High‑level Overview

The project is split into three independent layers that cooperate to produce a real‑time wavetable synthesizer that can be extended with neural‑network inference.

1. **C++ Core (JUCE + ONNX Runtime)**
   * `AudioProcessor` – Receives MIDI, maps note number to a 2‑D latent vector, and performs DSP.
   * `NeuralEngine` – Thin C++ wrapper around ONNX Runtime that loads a model and returns a wavetable.
   * `WavetableOscillator` – Generates audio from a 2048‑sample wavetable.
   * `ADSR` and `Low‑pass Filter` – Applied per‑voice.
   * **Build** – CMake + Ninja produces a Stand‑alone GUI, a VST3 bundle, and (on macOS) an AU bundle.

2. **Python API Layer**
   * `ai_routing.py` – A minimal FastAPI server exposing a `/wavetable` endpoint.
   * `model_exporter.py` – Example code that exports a PyTorch VAE‑style model to ONNX.
   * `config/ai.env` and `config/ai_config.json` – Runtime configuration and secrets.

3. **JUCE UI**
   * `PluginEditor.cpp` – Two sliders for latent X/Y plus controls for ADSR, filter and master gain.
   * Iteratively, the UI feeds the latent vector into the processor.

## Data Flow Diagram
```
MIDI Note On  →  mapMidiToLatent  →  NeuralEngine.infer  →  WavetableOscillator
                                ↑                                    ↓
                               lat‑vec (2‑D)                    wavetable (2048 samples)
```

## Building the Project
```
# 1. Clone repo (with JUCE submodule).
# 2. Ensure JUCE is checked out under <repo>/juce-8.0.0.
# 3. Inside forge_synth/plugin_source:
mkdir -p build && cd build
cmake .. -GNinja -DJUCE_ROOT=../../juce-8.0.0
ninja
```
After build you will find:
```
ForgeSynth_Standalone
ForgeSynth_VST3/ForgeSynth.vst3
ForgeSynth_AU/... (macOS only)
```

## Runtime
Launch the standalone binary for debugging or drop `ForgeSynth.vst3` into a DAW for real‑time use.  The UI exposes controls for latent space exploration, ADSR envelope, low‑pass filter and master gain.

## AI Extension
The FastAPI server in `ai_engine/ai_routing.py` can be run to expose a REST interface that returns a wavetable for a given latent vector.  The C++ `NeuralEngine` can load the ONNX model produced by `model_exporter.py`.
