#pragma once
#include "PluginProcessor.h"

class ForgeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ForgeLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};
class ParameterKnob final : public juce::Component
{
public:
    ParameterKnob(ForgeSynthAudioProcessor&, const char* id, const char* title, const char* tip);
    void resized() override;
private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
class TimbrePad final : public juce::Component
{
public:
    explicit TimbrePad(ForgeSynthAudioProcessor&);
    ~TimbrePad() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    ForgeSynthAudioProcessor& processor;
    bool dragging = false;
};
class ForgeSynthAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ForgeSynthAudioProcessorEditor(ForgeSynthAudioProcessor&);
    ~ForgeSynthAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void choosePresetFile(bool save);
    void chooseWavetable();
    ForgeSynthAudioProcessor& processor;
    ForgeLookAndFeel look;
    juce::Component surface;
    TimbrePad timbrePad;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips {this, 650};
    std::vector<std::unique_ptr<ParameterKnob>> knobs;
    juce::ComboBox presets, gateRate, delayRate;
    juce::TextButton previous {"<"}, next {">"}, importTable {"Load WT"}, save {"Save"}, load {"Load"}, explore {"Explore"}, panic {"PANIC"};
    juce::ToggleButton gate {"RHYTHM GATE"}, useCustom {"USE IMPORTED"};
    juce::Slider bpm, swing;
    juce::Label status, tempoLabel, patchLabel;
    std::array<juce::TextButton, 16> steps;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gateRateAttachment, delayRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment, swingAttachment;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int activeStep = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ForgeSynthAudioProcessorEditor)
};
