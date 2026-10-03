#pragma once
#include <JuceHeader.h>
#include "NeuralEngine.h"

struct VoiceParameters
{
    float x = 0.35f, y = 0.2f, detune = 7, sub = 0.2f, noise = 0;
    float attack = 0.008f, decay = 0.25f, sustain = 0.7f, release = 0.3f;
    float cutoff = 6000, resonance = 0.2f, filterEnv = 0.3f, width = 0.5f, custom = 0;
};
class ForgeSynthAudioProcessor final : public juce::AudioProcessor
{
public:
    ForgeSynthAudioProcessor();
    ~ForgeSynthAudioProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> outputPeak {0}, tempo {120};
    std::atomic<int> playingVoices {0}, currentStep {-1};
    std::atomic<bool> hostSync {false};
    void panic() noexcept { panicRequested.store(true); }
    void loadFactoryPreset(int index);
    static juce::StringArray factoryPresetNames();
    juce::Result savePreset(const juce::File&);
    juce::Result loadPreset(const juce::File&);
    juce::String getPresetName() const;
    bool hasImportedWavetable() const noexcept { return hasCustomWavetable.load(); }
    void randomiseTimbre();
    juce::Result loadWavetable(const juce::File&);
    WavetableBank::Table previewTable() const;
    const WavetableBank& getWavetableBank() const noexcept { return bank; }
    float value(const char* id) const;
    void setParameterValue(const juce::String& id, float value);
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    bool restoreState(const juce::ValueTree&);
    WavetableBank bank;
    CustomWavetable customWavetable;
    std::array<float, WavetableBank::size> customSource {};
    std::atomic<bool> hasCustomWavetable {false};
    VoiceParameters voiceParameters;
    juce::Synthesiser synth;
    juce::Reverb reverb;
    juce::AudioBuffer<float> delayBuffer;
    int delayWrite = 0;
    double rate = 44100, freePpq = 0;
    bool wasPlaying = false;
    std::atomic<bool> panicRequested {false};
    juce::SmoothedValue<float> master, drive, delayMix, delaySamples, gateGain, delayFeedback;
    std::array<std::atomic<float>*, 16> stepValues {};
    struct Raw
    {
        std::atomic<float> *x, *y, *detune, *sub, *noise, *attack, *decay, *sustain, *release;
        std::atomic<float> *cutoff, *resonance, *filterEnv, *width, *gain, *drive, *custom;
        std::atomic<float> *delay, *feedback, *delayRate, *reverb, *gate, *gateRate, *swing, *bpm;
    } raw {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ForgeSynthAudioProcessor)
};
