#include "PluginProcessor.h"
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}
juce::File temporaryFile(const juce::String& extension)
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("forge-regression-" + juce::String(juce::Random::getSystemRandom().nextInt64()) + extension);
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ForgeSynthAudioProcessor processor;
    processor.prepareToPlay(48000, 256);
    processor.setParameterValue("attack", 0.001f);
    processor.setParameterValue("release", 0.02f);
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 32);
    processor.processBlock(audio, midi);
    if (audio.getMagnitude(0, 32) != 0 || audio.getMagnitude(32, 224) <= 0) return fail("sample-accurate MIDI note-on");
    if (processor.playingVoices.load() != 1 || processor.getTailLengthSeconds() < 0.02) return fail("voice reporting and host tail metadata");
    for (int i = 0; i < 5; ++i)
    {
        midi.clear();
        if (i == 0) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        processor.processBlock(audio, midi);
    }
    if (processor.playingVoices.load() != 0) return fail("note-off envelope termination");

    const auto tableFile = temporaryFile(".wav");
    std::unique_ptr<juce::OutputStream> stream = tableFile.createOutputStream();
    if (!stream) return fail("create import fixture");
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(48000)
        .withNumChannels(1).withBitsPerSample(16));
    if (!writer) return fail("write valid import fixture");
    std::array<float, WavetableBank::size> cycle {};
    for (int i = 0; i < WavetableBank::size; ++i)
        cycle[static_cast<size_t>(i)] = 0.2f * std::sin(juce::MathConstants<float>::twoPi * i / WavetableBank::size);
    const float* channel[] {cycle.data()};
    const bool written = writer->writeFromFloatArrays(channel, 1, WavetableBank::size);
    writer.reset();
    if (!written || !processor.loadWavetable(tableFile).wasOk() || !processor.hasImportedWavetable()) return fail("import valid single-cycle WAV");
    if (processor.parameters.getRawParameterValue("custom")->load() < 0.5f) return fail("enable imported wavetable");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
    processor.processBlock(audio, midi);
    if (audio.getMagnitude(0, audio.getNumSamples()) <= 0) return fail("play imported wavetable");
    processor.panic(); midi.clear(); processor.processBlock(audio, midi);

    const auto presetFile = temporaryFile(".forgepreset");
    processor.setParameterValue("x", 0.91f);
    if (!processor.savePreset(presetFile).wasOk()) return fail("save user preset");
    processor.setParameterValue("x", 0.1f);
    if (!processor.loadPreset(presetFile).wasOk() || processor.value("x") < 0.9f) return fail("restore preset parameters");
    if (!processor.hasImportedWavetable() || processor.value("custom") < 0.5f) return fail("restore embedded wavetable");

    const float original = processor.value("x");
    presetFile.replaceWithText("<not-a-forge-preset/>");
    if (processor.loadPreset(presetFile).wasOk() || processor.value("x") != original) return fail("reject malformed preset without changing patch");
    juce::MemoryBlock hostState;
    processor.getStateInformation(hostState);
    ForgeSynthAudioProcessor restored;
    restored.prepareToPlay(48000, 256);
    restored.setStateInformation(hostState.getData(), static_cast<int>(hostState.getSize()));
    if (!restored.hasImportedWavetable() || restored.value("x") < 0.9f)
        return fail("restore host state including its imported wavetable");
    presetFile.deleteFile(); tableFile.deleteFile();

    const auto state = processor.parameters.copyState();
    const auto parameterCount = static_cast<int>(processor.getParameters().size());
    if (state.getNumChildren() != parameterCount || ForgeSynthAudioProcessor::factoryPresetNames().size() < 8)
        return fail("host automation state and factory patch registration");
    std::cout << "MIDI timing, release, preset validation and host/custom-wavetable state recall passed.\n";
    return 0;
}
