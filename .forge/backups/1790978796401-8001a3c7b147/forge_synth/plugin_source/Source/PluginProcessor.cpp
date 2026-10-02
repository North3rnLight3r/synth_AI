#include <JuceHeader.h>
#include "NeuralEngine.h"

class ForgeSynthAudioProcessor : public juce::AudioProcessor
{
public:
    ForgeSynthAudioProcessor() : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) 
    {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        neuralEngine.initialize("models/forge_synth_v1.onnx");
        oscillator.prepare(sampleRate);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        for (const auto* message : midiMessages) {
            auto* m = dynamic_cast<const juce::MidiMessage&>(*message);
            if (m.isNoteOn()) {
                auto latentVec = mapMidiToLatent(m.getNoteNumber());
                auto table = neuralEngine.infer(latentVec);
                oscillator.setWavetable(table);
            }
        }
        oscillator.process(buffer);
    }

    std::vector<float> mapMidiToLatent(int note) {
        return { (float)note / 127.0f, 0.5f }; // Simple mapping for scaffold
    }

private:
    NeuralEngine neuralEngine;
    WavetableOscillator oscillator;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForgeSynthAudioProcessor)
};
