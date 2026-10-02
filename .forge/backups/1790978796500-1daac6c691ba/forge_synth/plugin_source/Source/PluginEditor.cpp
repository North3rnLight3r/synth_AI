#include <JuceHeader.h>
#include "PluginProcessor.h"

class ForgeSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ForgeSynthAudioProcessorEditor (ForgeSynthAudioProcessor& p) : AudioProcessorEditor (&p), processor (p) {
        addAndMakeVisible (mainCanvas);
        mainCanvas.setComponentID("MainCanvas");
        setSize (800, 600);
    }

    void paint (juce::Graphics& g) override {
        g.fillAll (juce::Colours::darkgrey);
        g.setColour (juce::Colours::cyan);
        g.drawText ("FORGE SYNTH - NEURAL ENGINE", getLocalBounds(), juce::Justification::centred);
    }

    void resized() override {
        mainCanvas.setBounds (20, 20, getWidth() - 40, getHeight() - 100);
    }

private:
    juce::Component mainCanvas;
    ForgeSynthAudioProcessor& processor;
};
