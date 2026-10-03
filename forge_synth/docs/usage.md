# FORGE Synth user guide

## Build and load in a DAW

From the repository root, configure, build, and run the native processor tests:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 6
ctest --test-dir build --output-on-failure
```

On first configuration, CMake fetches pinned JUCE 8.0.15. To reuse a checkout, pass `-DJUCE_ROOT=/absolute/path/to/JUCE`. The VST3 is at `build/forge_synth/plugin_source/ForgeSynth_artefacts/Release/VST3/FORGE Synth.vst3`; install it in your DAW's VST3 folder and rescan. macOS builds AU at `build/forge_synth/plugin_source/ForgeSynth_artefacts/Release/AU/FORGE Synth.component`. Open standalone using:

```bash
open 'build/forge_synth/plugin_source/ForgeSynth_artefacts/Release/Standalone/FORGE Synth.app'
```

CMake builds VST3 and standalone on macOS, Windows and Linux, and adds AU on macOS. JUCE's per-platform system build dependencies still apply. See the [root README](../../README.md) for the verified macOS setup and Linux/Windows prerequisites.

## Play and make rhythms

Choose a factory patch, arm or monitor your DAW track, and play the instrument with MIDI or the on-screen keyboard. Enable **Rhythm Gate** and toggle steps to sequence held notes into a repeating 16-step pattern. DAW tempo and play position are detected automatically. Set internal BPM in standalone or a host without play-position data. Swing delays alternating gate steps. Disabled steps mask held notes but preserve note retriggers, sustain-pedal behavior and release tails.

Click or drag the timbre dot to morph the audible wavetable. Double-click a knob to reset it; enter values in its display to edit precisely. Explore randomises timbre, detune, cutoff, resonance and stereo width while retaining pattern, envelope and effects. Save or load `.forgepreset` files for your own patches. Loading a failed or invalid preset leaves the current patch untouched. The panic control silences active notes and clears effect buffers. Leave output headroom when playing chords; the peak meter shows output level and active voice count.

## Controls

| Control | Purpose |
|---|---|
| Timbre pad / X / Y | Morph the sine–saw and triangle–square corners. X/Y knobs provide keyboard-accessible control. |
| Detune / Sub / Noise / Width | Two detuned oscillators, a one-octave sub oscillator, noise, and stereo spread. Zero width is centred. |
| Attack / Decay / Sustain / Release | Velocity-responsive note envelope, with MIDI sustain support. |
| Cutoff / Resonance / Env Amount | Resonant low-pass cutoff and bipolar envelope modulation (±4 octaves). |
| Drive | Soft saturation before delay and reverb. |
| Delay / Feedback / Division | Tempo-synced stereo delay, feedback capped at 80%, and 1/16, 1/8, dotted 1/8, 1/4, or 1/2 note division. |
| Reverb / Output | Stereo room and master level in dB. |
| Load WT / Use Imported | Import a single-cycle mono/stereo WAV or AIFF file with exactly 2,048 frames. Files are centred, peak-normalised, and band-limited for high notes. The backend WAV export is ready to import. The cycle is saved with your preset and DAW project. |
| Rhythm Gate / division / steps | Enable the 4/4 16-step note gate; choose 1/8, 1/16 or 1/32 step length. |
| Swing / BPM | Adjust rhythmic swing and the free-running tempo fallback. Host BPM takes precedence when available. |
| Save / Load | Write or load a portable `.forgepreset` in XML format. |
| Explore / Panic | Discover a variation while keeping your groove, or stop voices and clear effect memory immediately. |

All synth and effect parameters are DAW automatable. Factory patches cover basses, keys, plucks, lead, pad, two step-gated rhythms, and a texture patch. Presets do not switch the master rhythm gate on automatically.

## Optional generator

The Python backend is independent of the plugin. Setup, backend routing, model/environment configuration, and the WAV download command are documented in the [backend guide](ai_api.md). The service binds to loopback when started as shown and does not add latency or network requirements to the VST.
