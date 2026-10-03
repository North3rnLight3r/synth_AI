#include "PluginEditor.h"

namespace
{
const juce::Colour background {0xff101318}, panel {0xff191e26}, border {0xff2c3441}, ink {0xffe9edf4}, muted {0xff8f9cac}, accent {0xff72ebc5};
juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
}
ForgeLookAndFeel::ForgeLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, ink);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::rotarySliderFillColourId, accent);
    setColour(juce::Slider::thumbColourId, accent);
    setColour(juce::Slider::trackColourId, accent);
    setColour(juce::ComboBox::backgroundColourId, background);
    setColour(juce::ComboBox::textColourId, ink);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, border);
    setColour(juce::TextButton::buttonColourId, border);
    setColour(juce::TextButton::buttonOnColourId, accent);
    setColour(juce::TextButton::textColourOffId, ink);
    setColour(juce::TextButton::textColourOnId, background);
    setColour(juce::ToggleButton::textColourId, ink);
    setColour(juce::ToggleButton::tickColourId, accent);
    setColour(juce::Label::textColourId, muted);
    setColour(juce::TooltipWindow::backgroundColourId, ink);
    setColour(juce::TooltipWindow::textColourId, background);
}
void ForgeLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(5);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    juce::Path track, arc;
    track.addCentredArc(centre.x, centre.y, radius - 2, radius - 2, 0, start, end, true);
    arc.addCentredArc(centre.x, centre.y, radius - 2, radius - 2, 0, start, start + pos * (end - start), true);
    g.setColour(border); g.strokePath(track, juce::PathStrokeType(3.0f));
    g.setColour(accent); g.strokePath(arc, juce::PathStrokeType(3.0f));
    g.setColour(juce::Colour(0xff252d38)); g.fillEllipse(centre.x - radius + 7, centre.y - radius + 7, (radius - 7) * 2, (radius - 7) * 2);
    const float angle = start + pos * (end - start);
    g.setColour(ink);
    g.drawLine(centre.x + std::sin(angle) * (radius * 0.36f), centre.y - std::cos(angle) * (radius * 0.36f),
        centre.x + std::sin(angle) * (radius - 11), centre.y - std::cos(angle) * (radius - 11), 2.0f);
}
ParameterKnob::ParameterKnob(ForgeSynthAudioProcessor& p, const char* id, const char* title, const char* tip)
{
    label.setText(title, juce::dontSendNotification); label.setFont(font(12)); label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label); addAndMakeVisible(slider);
    slider.setName(title); slider.setTooltip(tip);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 19);
    slider.setScrollWheelEnabled(false);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, id, slider);
    auto* param = p.parameters.getParameter(id);
    slider.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    const juce::String unit = param->getLabel();
    slider.textFromValueFunction = [unit](double v)
    {
        if (unit == "Hz") return v >= 1000 ? juce::String(v / 1000, 2) + " kHz" : juce::String(v, 0) + " Hz";
        if (unit == "s") return v < 1 ? juce::String(v * 1000, 0) + " ms" : juce::String(v, 2) + " s";
        if (unit.isNotEmpty()) return juce::String(v, 1) + " " + unit;
        return juce::String(v * 100, 0) + "%";
    };
    slider.valueFromTextFunction = [unit](const juce::String& text)
    {
        const auto v = text.getDoubleValue();
        if (unit == "Hz") return text.containsIgnoreCase("k") ? v * 1000 : v;
        if (unit == "s") return text.containsIgnoreCase("ms") ? v / 1000 : v;
        return unit.isEmpty() ? v / 100 : v;
    };
}
void ParameterKnob::resized()
{
    auto bounds = getLocalBounds(); label.setBounds(bounds.removeFromTop(18)); slider.setBounds(bounds);
}
TimbrePad::TimbrePad(ForgeSynthAudioProcessor& p) : processor(p)
{
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    setTitle("Timbre space"); setDescription("Drag to morph the waveform. Timbre X and Y knobs provide keyboard-accessible controls.");
}
TimbrePad::~TimbrePad()
{
    if (dragging) { processor.parameters.getParameter("x")->endChangeGesture(); processor.parameters.getParameter("y")->endChangeGesture(); }
}
void TimbrePad::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(1);
    g.setColour(background); g.fillRoundedRectangle(area, 8);
    g.setColour(border);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine(static_cast<int>(area.getWidth() * i / 4), 0, area.getHeight());
        g.drawHorizontalLine(static_cast<int>(area.getHeight() * i / 4), 0, area.getWidth());
    }
    const float x = processor.value("x"), y = processor.value("y");
    // Lightweight waveform preview of the same four morph corners.
    juce::Path wave;
    for (int i = 0; i < getWidth(); ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(getWidth());
        const float sine = std::sin(t * juce::MathConstants<float>::twoPi);
        const float saw = 1.0f - 2.0f * t;
        const float triangle = 2.0f / juce::MathConstants<float>::pi * std::asin(sine);
        const float square = sine >= 0 ? 0.8f : -0.8f;
        const float v = (sine + x * (saw - sine)) * (1 - y) + (triangle + x * (square - triangle)) * y;
        const float py = area.getCentreY() - v * area.getHeight() * 0.25f;
        if (i == 0) wave.startNewSubPath(0, py); else wave.lineTo(static_cast<float>(i), py);
    }
    g.setColour(accent.withAlpha(0.23f)); g.strokePath(wave, juce::PathStrokeType(2));
    const auto point = juce::Point<float>(8 + x * (getWidth() - 16), 8 + (1 - y) * (getHeight() - 16));
    g.setColour(accent.withAlpha(0.13f)); g.fillEllipse(point.x - 16, point.y - 16, 32, 32);
    g.setColour(accent); g.fillEllipse(point.x - 5, point.y - 5, 10, 10);
    g.setFont(font(10)); g.setColour(muted);
    g.drawText("TRIANGLE", 8, 5, 90, 16, juce::Justification::left);
    g.drawText("SQUARE", getWidth() - 85, 5, 77, 16, juce::Justification::right);
    g.drawText("SINE", 8, getHeight() - 23, 80, 16, juce::Justification::left);
    g.drawText("SAW", getWidth() - 80, getHeight() - 23, 72, 16, juce::Justification::right);
}
void TimbrePad::mouseDown(const juce::MouseEvent& event)
{
    dragging = true;
    processor.parameters.getParameter("x")->beginChangeGesture(); processor.parameters.getParameter("y")->beginChangeGesture(); mouseDrag(event);
}
void TimbrePad::mouseDrag(const juce::MouseEvent& event)
{
    if (!dragging) return;
    processor.parameters.getParameter("x")->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, (event.position.x - 8) / (getWidth() - 16)));
    processor.parameters.getParameter("y")->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, 1 - (event.position.y - 8) / (getHeight() - 16)));
}
void TimbrePad::mouseUp(const juce::MouseEvent&)
{
    if (dragging) { dragging = false; processor.parameters.getParameter("x")->endChangeGesture(); processor.parameters.getParameter("y")->endChangeGesture(); }
}

ForgeSynthAudioProcessorEditor::ForgeSynthAudioProcessorEditor(ForgeSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), timbrePad(p), keyboard(p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&look); addAndMakeVisible(surface);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&timbrePad, &keyboard, &presets, &previous, &next, &importTable, &save, &load, &explore, &panic, &gate,
        &gateRate, &delayRate, &bpm, &swing, &status, &tempoLabel, &patchLabel, &useCustom}) surface.addAndMakeVisible(c);
    auto knob = [&](const char* id, const char* title, const char* tip)
    { knobs.push_back(std::make_unique<ParameterKnob>(p, id, title, tip)); surface.addAndMakeVisible(knobs.back().get()); };
    knob("x", "TIMBRE X", "Morph from soft to harmonically rich. Double-click knobs to reset; type values below.");
    knob("y", "TIMBRE Y", "Morph from sine/saw to triangle/square.");
    knob("attack", "ATTACK", "Time for a new note to reach full level."); knob("decay", "DECAY", "Time to fall to the sustain level.");
    knob("sustain", "SUSTAIN", "Level while a key is held."); knob("release", "RELEASE", "Fade time after releasing a key or sustain pedal.");
    knob("cutoff", "CUTOFF", "Low-pass filter cutoff frequency."); knob("resonance", "RESONANCE", "Emphasise the filter cutoff.");
    knob("filterEnv", "ENV AMOUNT", "Bipolar filter modulation, up to four octaves, using the amplitude envelope."); knob("drive", "DRIVE", "Soft saturation before delay and reverb.");
    knob("detune", "DETUNE", "Spread two oscillators by up to +/-35 cents."); knob("sub", "SUB", "Sine oscillator one octave below the played note.");
    knob("noise", "NOISE", "Add air and texture."); knob("width", "WIDTH", "Stereo oscillator separation. Set to zero for a centred bass.");
    knob("delay", "DELAY", "Tempo-synced delay mix. Uses DAW tempo when available."); knob("feedback", "FEEDBACK", "Delay repeats, capped at 80%.");
    knob("reverb", "REVERB", "Stereo room ambience."); knob("gain", "OUTPUT", "Master gain in dB. Leave headroom when playing dense chords.");
    presets.addItemList(ForgeSynthAudioProcessor::factoryPresetNames(), 1);
    presets.setText(processor.getPresetName(), juce::dontSendNotification);
    presets.setTooltip("Factory sounds. Loading replaces all sound parameters and the rhythm pattern.");
    presets.onChange = [this] { if (presets.getSelectedId() > 0) processor.loadFactoryPreset(presets.getSelectedId() - 1); };
    previous.onClick = [this] { presets.setSelectedId((std::max(1, presets.getSelectedId()) + presets.getNumItems() - 2) % presets.getNumItems() + 1); };
    next.onClick = [this] { presets.setSelectedId(std::max(0, presets.getSelectedId()) % presets.getNumItems() + 1); };
    save.onClick = [this] { choosePresetFile(true); }; load.onClick = [this] { choosePresetFile(false); };
    importTable.onClick = [this] { chooseWavetable(); };
    explore.onClick = [this] { processor.randomiseTimbre(); status.setText("New timbre. Envelope, rhythm and effects preserved.", juce::dontSendNotification); };
    explore.setTooltip("Randomise timbre, detune, filter and stereo width.");
    panic.onClick = [this] { processor.panic(); }; panic.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffb28d));
    panic.setTooltip("Immediately silence voices, held screen keys, delay and reverb.");
    gateRate.addItemList({"1/8", "1/16", "1/32"}, 1); delayRate.addItemList({"1/16", "1/8", "1/8 D", "1/4", "1/2"}, 1);
    gateRate.setTooltip("Rhythm step duration. Sixteen 1/16 steps make one 4/4 bar."); delayRate.setTooltip("Delay duration. D means dotted.");
    gateRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "gateRate", gateRate);
    delayRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "delayRate", delayRate);
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "gate", gate));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "custom", useCustom));
    useCustom.setTooltip("Blend the imported single-cycle table with the current timbre. Load a WAV or AIFF wavetable first.");
    useCustom.setEnabled(false);
    for (int i = 0; i < 16; ++i)
    {
        auto& step = steps[static_cast<size_t>(i)]; surface.addAndMakeVisible(step);
        step.setButtonText(juce::String(i + 1).paddedLeft('0', 2)); step.setClickingTogglesState(true);
        step.setTooltip("Step " + juce::String(i + 1) + ": enable to let held notes through. Gate runs before the effects.");
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "step" + juce::String(i), step));
    }
    for (auto* s : {&bpm, &swing}) { s->setSliderStyle(juce::Slider::LinearHorizontal); s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 22); s->setScrollWheelEnabled(false); }
    bpm.setTextValueSuffix(" BPM"); bpm.setNumDecimalPlacesToDisplay(0); bpm.setTooltip("Internal tempo for standalone or hosts without tempo. DAW tempo takes priority.");
    swing.setTextValueSuffix(""); swing.setTooltip("Swing delays every second gate step, keeping each pair the same length.");
    bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "bpm", bpm);
    swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "swing", swing);
    swing.textFromValueFunction = [](double v) { return juce::String(v * 100, 0) + "%"; };
    swing.valueFromTextFunction = [](const juce::String& s) { return s.getDoubleValue() / 100; };
    keyboard.setAvailableRange(24, 96); keyboard.setLowestVisibleKey(48); keyboard.setKeyWidth(24); keyboard.setVelocity(0.8f, true);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffcdd4dd));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, background);
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha(0.8f));
    keyboard.setWantsKeyboardFocus(true);
    for (auto* b : {&previous, &next, &importTable, &save, &load, &explore, &panic}) b->setWantsKeyboardFocus(false);
    status.setFont(font(11)); tempoLabel.setFont(font(11, true)); tempoLabel.setColour(juce::Label::textColourId, accent);
    patchLabel.setFont(font(11));
    status.setText("Ready. Play MIDI or click the keyboard. Double-click a knob to reset.", juce::dontSendNotification);
    setResizable(true, true); setResizeLimits(896, 624, 1680, 1170); getConstrainer()->setFixedAspectRatio(1120.0 / 780.0);
    setSize(1120, 780); startTimerHz(24);
}
ForgeSynthAudioProcessorEditor::~ForgeSynthAudioProcessorEditor() { stopTimer(); setLookAndFeel(nullptr); }
void ForgeSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background); g.addTransform(juce::AffineTransform::scale(getWidth() / 1120.0f));
    g.setFont(font(34, true)); g.setColour(ink); g.drawText("FORGE", 24, 15, 220, 40, juce::Justification::left);
    g.setFont(font(11)); g.setColour(accent); g.drawText("WAVETABLE SYNTHESIZER  /  0.2", 26, 58, 280, 18, juce::Justification::left);
    g.setColour(border); g.drawHorizontalLine(88, 24, 1096);
    auto card = [&](int x, int y, int w, int h, const char* title, const char* number)
    {
        g.setColour(panel); g.fillRoundedRectangle(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h), 10);
        g.setColour(border); g.drawRoundedRectangle(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h), 10, 1);
        g.setColour(accent); g.setFont(font(11, true)); g.drawText(number, x + 16, y + 13, 24, 18, juce::Justification::left);
        g.setColour(ink); g.drawText(title, x + 44, y + 13, w - 65, 18, juce::Justification::left);
    };
    card(24, 104, 392, 300, "TIMBRE SPACE", "01");
    card(432, 104, 664, 142, "AMPLITUDE", "02");
    card(432, 262, 664, 142, "TONE & COLOUR", "03");
    card(24, 420, 392, 146, "OSCILLATORS", "04");
    card(432, 420, 664, 146, "SPACE & OUTPUT", "05");
    card(24, 582, 1072, 98, "", "06");
    g.setColour(muted); g.setFont(font(11));
    g.drawText("Drag to discover. Shape it with the knobs.", 40, 370, 300, 20, juce::Justification::left);
    g.drawText("SWING", 400, 596, 58, 18, juce::Justification::left);
    g.drawText("MIDI IN  /  16 VOICES  /  PITCH BEND +/-2", 26, 686, 490, 16, juce::Justification::left);
    g.drawText("OUTPUT", 892, 705, 100, 18, juce::Justification::left);
    const float level = processor.outputPeak.load();
    g.setColour(border); g.fillRoundedRectangle(892, 731, 184, 8, 4);
    g.setColour(level >= 0.95f ? juce::Colour(0xffffb28d) : accent);
    g.fillRoundedRectangle(892, 731, 184 * juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(level, -60.0f) + 60) / 60), 8, 4);
    g.setColour(muted); g.setFont(font(11));
    g.drawText(juce::String(processor.playingVoices.load()) + " voices    " + (level < 0.001f ? "-inf" : juce::String(juce::Decibels::gainToDecibels(level), 1)) + " dBFS", 892, 746, 190, 18, juce::Justification::left);
}
void ForgeSynthAudioProcessorEditor::resized()
{
    surface.setBounds(0, 0, 1120, 780); surface.setTransform(juce::AffineTransform::scale(getWidth() / 1120.0f));
    previous.setBounds(320, 24, 30, 32); presets.setBounds(358, 24, 330, 32); next.setBounds(696, 24, 30, 32);
    importTable.setBounds(744, 24, 72, 32); save.setBounds(824, 24, 55, 32); load.setBounds(887, 24, 53, 32);
    explore.setBounds(948, 24, 65, 32); panic.setBounds(1021, 24, 75, 32);
    patchLabel.setBounds(354, 60, 720, 20);
    timbrePad.setBounds(40, 149, 242, 211);
    useCustom.setBounds(299, 357, 105, 28);
    knobs[0]->setBounds(300, 149, 100, 98); knobs[1]->setBounds(300, 256, 100, 98);
    for (int i = 0; i < 4; ++i)
    {
        knobs[static_cast<size_t>(2 + i)]->setBounds(450 + i * 162, 140, 140, 97);
        knobs[static_cast<size_t>(6 + i)]->setBounds(450 + i * 162, 298, 140, 97);
        knobs[static_cast<size_t>(10 + i)]->setBounds(34 + i * 95, 457, 91, 101);
        knobs[static_cast<size_t>(14 + i)]->setBounds(450 + i * 162, 457, 140, 101);
    }
    delayRate.setBounds(992, 430, 87, 24);
    gate.setBounds(68, 590, 156, 30); gateRate.setBounds(258, 592, 104, 26);
    swing.setBounds(456, 593, 150, 26); bpm.setBounds(638, 593, 195, 26); tempoLabel.setBounds(854, 593, 222, 26);
    for (int i = 0; i < 16; ++i) steps[static_cast<size_t>(i)].setBounds(40 + i * 65, 634, 58, 29);
    keyboard.setBounds(24, 709, 840, 53); status.setBounds(24, 763, 1060, 16);
}
void ForgeSynthAudioProcessorEditor::timerCallback()
{
    timbrePad.repaint(); repaint();
    const auto name = processor.getPresetName();
    useCustom.setEnabled(processor.hasImportedWavetable());
    patchLabel.setText(name + "   |   LOCAL ENGINE", juce::dontSendNotification);
    if (presets.getText() != name) presets.setText(name, juce::dontSendNotification);
    tempoLabel.setText((processor.hostSync.load() ? "HOST  /  " : "INTERNAL  /  ") + juce::String(processor.tempo.load(), 1) + " BPM", juce::dontSendNotification);
    const int newStep = processor.currentStep.load();
    if (newStep != activeStep)
    {
        for (int i = 0; i < 16; ++i)
        {
            steps[static_cast<size_t>(i)].setColour(juce::TextButton::buttonColourId, i == newStep ? juce::Colour(0xff667486) : border);
            steps[static_cast<size_t>(i)].setColour(juce::TextButton::buttonOnColourId, i == newStep ? juce::Colours::white : accent);
        }
        activeStep = newStep;
    }
}
void ForgeSynthAudioProcessorEditor::chooseWavetable()
{
    fileChooser = std::make_unique<juce::FileChooser>("Import one-cycle WAV or AIFF (2,048 frames)",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wav;*.aif;*.aiff");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<ForgeSynthAudioProcessorEditor>(this)](const juce::FileChooser& chooser)
        {
            if (!safe) return;
            const auto file = chooser.getResult();
            if (file == juce::File{}) return;
            const auto result = safe->processor.loadWavetable(file);
            safe->status.setText(result.wasOk() ? "Imported and enabled " + file.getFileName() : result.getErrorMessage(), juce::dontSendNotification);
        });
}
void ForgeSynthAudioProcessorEditor::choosePresetFile(bool shouldSave)
{
    const auto directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    fileChooser = std::make_unique<juce::FileChooser>(shouldSave ? "Save FORGE preset" : "Load FORGE preset", directory.getChildFile("Untitled.forgepreset"), "*.forgepreset");
    const int flags = shouldSave ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting
                                 : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [safe = juce::Component::SafePointer<ForgeSynthAudioProcessorEditor>(this), shouldSave](const juce::FileChooser& chooser)
    {
        if (!safe) return;
        auto file = chooser.getResult(); if (file == juce::File{}) return;
        if (shouldSave) file = file.withFileExtension("forgepreset");
        const auto result = shouldSave ? safe->processor.savePreset(file) : safe->processor.loadPreset(file);
        safe->status.setText(result.wasOk() ? (shouldSave ? "Saved " : "Loaded ") + file.getFileName() : result.getErrorMessage(), juce::dontSendNotification);
    });
}
