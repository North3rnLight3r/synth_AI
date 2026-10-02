# Building and Running FORGE Synth

## Prerequisites

- **CMake ≥ 3.18** with **Ninja** generator.  The project ships its own *cached* generator, so `cmake .. -GNinja` works on most Linux distributions.
- **JUCE SDK** (v8+) unpacked in the repository.  In this example the SDK lives in `juce-8.0.0` at the repository root.  Adjust the `JUCE_ROOT` variable in `CMakeLists.txt` if you use another location.
- **Python 3.8+** with `pip` and `virtualenv`.  The Python portion of the project (AI routing API and model export) is optional.

## Build the C++ Plugin

```bash
# From root of the repository
cd forge_synth/plugin_source
mkdir -p build && cd build
cmake .. -GNinja -DJUCE_ROOT=../juce-8.0.0  # point to the local JUCE SDK
ninja   # produces Standalone, VST3 and (on macOS) AU
```

You should now have:

- `ForgeSynth_Standalone`: a native GUI host you can launch from the command line.
- `ForgeSynth_VST3/ForgeSynth.vst3`: a fully‑functional VST3 bundle ready to drop into a DAW.
- `ForgeSynth_AU/ForgeSynth.aox` (on macOS): an Audio Unit home.

## Run the Standalone

```bash
./ForgeSynth_Standalone
```

Your screen will show a window with sliders for **latent X**, **latent Y**, ADSR parameters, a filter, and a master gain.  Moving the sliders instantly changes the wavetable that a poly‑voice oscillator uses.

## Using the Plugin in a DAW
1. Open the DAW and set it to scan for new plugins.
2. Drop `ForgeSynth.vst3` into the plugin directory (Linux: `~/.vst3/` or the DAW‑specific folder).
3. Insert the plugin on a track and play MIDI notes; each note triggers a voice that is synthesized based on the latent vector mapped from the note number.

---

## Python AI Server (Optional)

The Python side provides an endpoint that can feed real neural network wavetable data into a running instance of the plugin. See the descriptions in `docs/ai_api.md`.
