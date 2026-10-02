#include <JuceHeader.h>
#include "PluginProcessor.h"

class ForgeSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ForgeSynthAudioProcessorEditor (ForgeSynthAudioProcessor& p) : AudioProcessorEditor (&p), processor (p) {
        addAndMakeVisible (mainCanvas);
        mainCanvas.setComponentID("MainCanvas");
        
        // Setup a simple slider to simulate latent space manipulation
        addAndMakeVisible(latentSlider);
        latentSlider.setRange(0.0, 1.0);
        latentSlider.setValue(0.5);
        latentSlider.onValueChange = [this]() {
            // In a full build, this would update the latent vector in the processor
        };

        setSize (800, 600);
    }

    void paint (juce::Graphics& g) override {
        g.fillAll (juce::Colour(0x121212)); // Deep charcoal
        g.setColour (juce::Colours::cyan);
        g.setFont (30.0f);
        g.drawText ("FORGE SYNTH - NEURAL ENGINE", 0, 0, getWidth(), 50, juce::Justification::centred);
        
        g.setColour (juce::Colours::grey);
        g.setFont (15.0f);
        g.drawText ("Latent Space Exploration", 20, 60, 200, 20, juce::Justification::left);
    }

    void resized() override {
        mainCanvas.setBounds (20, 100, getWidth() - 40, getHeight() - 200);
        latentSlider.setBounds (20, 80, 300, 30);
    }

private:
    juce::Component mainCanvas;
    juce::Slider latentSlider;
    ForgeSynthAudioProcessor& processor;
};
