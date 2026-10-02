# ForgeSynth Architecture

1. **C++ Core**
   * **AudioProcessor** – handles MIDI, receives a latent vector, runs the neural network (ONNX) or deterministic fallback, and outputs a wavetable‑oscillated stream.
   * **NeuralEngine** – C++ wrapper around ONNX Runtime that loads the model file.
   * **WavetableOscillator** – uses the wavetable buffer to generate a continuous waveform.
   * **Envelope / Filter** – ADSR envelope and a low‑pass filter (IIR) applied to the oscillator.

2. **Python API**
   * **FastAPI** – lightweight server exposing endpoints for generating wave tables from a latent vector.
   * **Model Exporter** – script that trains / exports a PyTorch model to ONNX.

3. **JUCE UI**
   * **PluginEditor** – slider UI for Latent X/Y, ADSR, filter, master gain.
   * **PluginProcessor** – ties UI to DSP logic.

### Data Flow
```
MIDI Note → mapMidiToLatent → NeuralEngine → WavetableOscillator → ADSR + Filter → Output
```
