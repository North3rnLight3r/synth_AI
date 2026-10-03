#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr double tau = juce::MathConstants<double>::twoPi;
class ForgeSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class ForgeVoice final : public juce::SynthesiserVoice
{
public:
    ForgeVoice(const WavetableBank& b, const VoiceParameters& p)
        : bank(b), params(p), rng(++voiceSeed * 2654435761u) {}
    bool canPlaySound(juce::SynthesiserSound* sound) override { return dynamic_cast<ForgeSound*>(sound) != nullptr; }
    void prepare(double sr, int blockSize, const CustomWavetable& custom)
    {
        sampleRate = sr;
        customWavetable = &custom;
        envelope.setSampleRate(sr);
        filter.prepare({sr, static_cast<juce::uint32>(std::max(1, blockSize)), 2});
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        for (auto* s : {&x, &y, &cutoff, &width, &sub, &noise, &spread, &customMix}) s->reset(sr, 0.02);
        stopNote(0, false);
    }
    void startNote(int note, float vel, juce::SynthesiserSound*, int wheel) override
    {
        stolenTail = last;
        stealFade = 1.0f;
        velocity = vel;
        frequency = juce::MidiMessage::getMidiNoteInHertz(note);
        phaseA = phaseB = subPhase = 0;
        pitchWheelMoved(wheel);
        envelope.reset();
        envelope.setParameters({params.attack, params.decay, params.sustain, params.release});
        envelope.noteOn();
        filter.reset();
        x.setCurrentAndTargetValue(params.x); y.setCurrentAndTargetValue(params.y);
        cutoff.setCurrentAndTargetValue(params.cutoff); width.setCurrentAndTargetValue(params.width);
        sub.setCurrentAndTargetValue(params.sub); noise.setCurrentAndTargetValue(params.noise);
        spread.setCurrentAndTargetValue(params.detune);
    }
    void stopNote(float, bool allowTail) override
    {
        if (allowTail) envelope.noteOff();
        else { envelope.reset(); clearCurrentNote(); last = {}; stolenTail = {}; }
    }
    void pitchWheelMoved(int wheel) override { bend = std::pow(2.0, (wheel - 8192) / 8192.0 * 2.0 / 12.0); }
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int start, int count) override
    {
        if (!isVoiceActive()) return;
        x.setTargetValue(params.x); y.setTargetValue(params.y); cutoff.setTargetValue(params.cutoff);
        width.setTargetValue(params.width); sub.setTargetValue(params.sub); noise.setTargetValue(params.noise);
        spread.setTargetValue(params.detune);
        filter.setResonance(0.707f + params.resonance * 3.0f);
        const int level = bank.levelFor(frequency * bend * std::pow(2.0, 35.0 / 1200.0), sampleRate);
        customMix.setTargetValue(params.custom);
        for (int i = 0; i < count; ++i)
        {
            const float env = envelope.getNextSample();
            if (!envelope.isActive()) { clearCurrentNote(); last = {}; break; }
            const float xx = x.getNextValue(), yy = y.getNextValue();
            const float detune = std::pow(2.0f, spread.getNextValue() / 1200.0f);
            const double increment = std::min(0.45, frequency * bend / sampleRate);
            const float mix = customMix.getNextValue();
            const float a = bank.sample(phaseA, level, xx, yy);
            const float b = bank.sample(phaseB, level, xx, yy);
            const float oscillatorA = a + (customWavetable->sample(phaseA, level) - a) * mix;
            const float oscillatorB = b + (customWavetable->sample(phaseB, level) - b) * mix;
            phaseA += increment * detune; phaseA -= std::floor(phaseA);
            phaseB += increment / detune; phaseB -= std::floor(phaseB);
            subPhase += increment * 0.5; subPhase -= std::floor(subPhase);
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const float n = (static_cast<float>(rng) / 2147483648.0f - 1.0f) * noise.getNextValue() * 0.35f;
            const float low = static_cast<float>(std::sin(subPhase * tau)) * sub.getNextValue();
            const float mid = (oscillatorA + oscillatorB) * 0.5f + low + n;
            const float side = (oscillatorA - oscillatorB) * width.getNextValue() * 0.5f;
            const float fc = cutoff.getNextValue();
            if ((controlCounter++ & 15) == 0)
                filter.setCutoffFrequency(juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.45),
                    fc * std::pow(2.0f, params.filterEnv * env * 4.0f)));
            stealFade = std::max(0.0f, stealFade - static_cast<float>(1.0 / (0.005 * sampleRate)));
            for (int ch = 0; ch < 2; ++ch)
                last[static_cast<size_t>(ch)] = filter.processSample(ch, mid + (ch == 0 ? side : -side))
                    * env * velocity * 0.18f + stolenTail[static_cast<size_t>(ch)] * stealFade;
            if (buffer.getNumChannels() == 1)
                buffer.addSample(0, start + i, (last[0] + last[1]) * 0.5f);
            else
                for (int ch = 0; ch < 2; ++ch) buffer.addSample(ch, start + i, last[static_cast<size_t>(ch)]);
        }
        filter.snapToZero();
    }
private:
    const WavetableBank& bank;
    const VoiceParameters& params;
    juce::ADSR envelope;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> x, y, cutoff, width, sub, noise, spread, customMix;
    const CustomWavetable* customWavetable = nullptr;
    double sampleRate = 44100, frequency = 440, bend = 1, phaseA = 0, phaseB = 0, subPhase = 0;
    float velocity = 1, stealFade = 0;
    std::array<float, 2> last {}, stolenTail {};
    inline static std::atomic<uint32_t> voiceSeed {0x12345678u};
    uint32_t rng = 0x12345678, controlCounter = 0;
};

struct Preset
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};
const Preset presets[] {
    {"Init / Clean Slate", {}},
    {"Bass / Sub Foundation", {{"x", 0.05f}, {"y", 0.1f}, {"sub", 0.65f}, {"detune", 0}, {"width", 0}, {"cutoff", 650}, {"attack", 0.004f}, {"decay", 0.35f}, {"sustain", 0.6f}, {"release", 0.12f}, {"drive", 0.18f}}},
    {"Bass / Rubber Circuit", {{"x", 0.8f}, {"y", 0.7f}, {"sub", 0.4f}, {"cutoff", 180}, {"resonance", 0.6f}, {"filterEnv", 0.85f}, {"decay", 0.22f}, {"sustain", 0.15f}, {"release", 0.1f}, {"drive", 0.3f}, {"detune", 2}}},
    {"Keys / Velvet Chords", {{"x", 0.2f}, {"y", 0.8f}, {"cutoff", 2200}, {"attack", 0.012f}, {"decay", 0.8f}, {"sustain", 0.35f}, {"release", 0.65f}, {"reverb", 0.24f}, {"width", 0.8f}, {"detune", 9}}},
    {"Pluck / Glass Arcade", {{"x", 0.65f}, {"y", 0.45f}, {"cutoff", 550}, {"filterEnv", 0.9f}, {"attack", 0.002f}, {"decay", 0.24f}, {"sustain", 0}, {"release", 0.16f}, {"delay", 0.25f}, {"reverb", 0.15f}, {"sub", 0}}},
    {"Lead / Neon Wire", {{"x", 0.9f}, {"y", 0.15f}, {"detune", 16}, {"width", 0.8f}, {"cutoff", 3800}, {"resonance", 0.3f}, {"delay", 0.2f}, {"reverb", 0.12f}, {"release", 0.22f}, {"sub", 0.1f}}},
    {"Pad / Slow Aurora", {{"x", 0.65f}, {"y", 0.35f}, {"detune", 22}, {"attack", 1.2f}, {"decay", 1.8f}, {"sustain", 0.8f}, {"release", 2.4f}, {"cutoff", 1800}, {"width", 1}, {"reverb", 0.45f}, {"delay", 0.15f}, {"sub", 0.1f}}},
    {"Rhythm / Midnight Pulse", {{"x", 0.85f}, {"y", 0.6f}, {"cutoff", 1900}, {"gate", 1}, {"swing", 0.18f}, {"delay", 0.22f}, {"reverb", 0.15f}, {"step3", 0}, {"step6", 0}, {"step7", 0}, {"step11", 0}, {"step14", 0}}},
    {"Rhythm / Broken Signal", {{"x", 0.7f}, {"y", 0.9f}, {"cutoff", 950}, {"drive", 0.22f}, {"gate", 1}, {"swing", 0.3f}, {"step1", 0}, {"step4", 0}, {"step5", 0}, {"step9", 0}, {"step10", 0}, {"step13", 0}, {"step15", 0}, {"delay", 0.3f}}},
    {"FX / Dust & Air", {{"x", 0.15f}, {"noise", 0.85f}, {"sub", 0}, {"attack", 0.1f}, {"decay", 0.5f}, {"sustain", 0.2f}, {"release", 1.2f}, {"cutoff", 5500}, {"reverb", 0.5f}, {"delay", 0.25f}}}
};
}

juce::AudioProcessorValueTreeState::ParameterLayout ForgeSynthAudioProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&](const char* id, const char* name, float lo, float hi, float def, float skew = 1.0f, const char* unit = "")
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
            juce::NormalisableRange<float>{lo, hi, 0.0f, skew}, def,
            juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
        add("x", "Timbre X", 0, 1, 0.35f); add("y", "Timbre Y", 0, 1, 0.2f);
    add("detune", "Detune", 0, 35, 7, 1, "ct"); add("sub", "Sub", 0, 1, 0.2f); add("noise", "Noise", 0, 1, 0);
    add("attack", "Attack", 0.001f, 4, 0.008f, 0.3f, "s"); add("decay", "Decay", 0.01f, 4, 0.25f, 0.4f, "s");
    add("sustain", "Sustain", 0, 1, 0.7f); add("release", "Release", 0.01f, 6, 0.3f, 0.35f, "s");
    add("cutoff", "Cutoff", 20, 20000, 6000, 0.25f, "Hz"); add("resonance", "Resonance", 0, 1, 0.2f);
    add("filterEnv", "Filter Envelope", -1, 1, 0.3f); add("width", "Stereo Width", 0, 1, 0.5f);
    add("gain", "Output", -36, 6, -9, 1, "dB"); add("drive", "Drive", 0, 1, 0);
    add("delay", "Delay Mix", 0, 0.65f, 0); add("feedback", "Delay Feedback", 0, 0.8f, 0.35f);
    add("reverb", "Reverb Mix", 0, 0.65f, 0); add("swing", "Swing", 0, 0.6f, 0);
    add("bpm", "Internal Tempo", 40, 240, 120, 1, "BPM");
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"delayRate", 1}, "Delay Division", juce::StringArray{"1/16", "1/8", "1/8 D", "1/4", "1/2"}, 2));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"gateRate", 1}, "Gate Division", juce::StringArray{"1/8", "1/16", "1/32"}, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"gate", 1}, "Rhythm Gate", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"custom", 1}, "Use Imported Wavetable", false));
    for (int i = 0; i < 16; ++i)
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"step" + juce::String(i), 1}, "Gate Step " + juce::String(i + 1), (i % 2) == 0));
    return layout;
}

ForgeSynthAudioProcessor::ForgeSynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "FORGE_STATE", makeParameters())
{
    auto p = [&](const char* id) { return parameters.getRawParameterValue(id); };
    raw = {p("x"), p("y"), p("detune"), p("sub"), p("noise"), p("attack"), p("decay"), p("sustain"), p("release"),
        p("cutoff"), p("resonance"), p("filterEnv"), p("width"), p("gain"), p("drive"), p("custom"), p("delay"), p("feedback"),
        p("delayRate"), p("reverb"), p("gate"), p("gateRate"), p("swing"), p("bpm")};
    for (int i = 0; i < 16; ++i) stepValues[static_cast<size_t>(i)] = parameters.getRawParameterValue("step" + juce::String(i));
    for (int i = 0; i < 16; ++i) synth.addVoice(new ForgeVoice(bank, voiceParameters));
    synth.addSound(new ForgeSound());
    synth.setNoteStealingEnabled(true);
    synth.setMinimumRenderingSubdivisionSize(1, true);
    parameters.state.setProperty("schemaVersion", 1, nullptr);
    parameters.state.setProperty("presetName", "Init / Clean Slate", nullptr);
    parameters.state.setProperty("customWavetable", {}, nullptr);
}
ForgeSynthAudioProcessor::~ForgeSynthAudioProcessor() = default;

void ForgeSynthAudioProcessor::prepareToPlay(double sr, int blockSize)
{
    rate = sr;
    synth.setCurrentPlaybackSampleRate(sr);
    for (int i = 0; i < synth.getNumVoices(); ++i) static_cast<ForgeVoice*>(synth.getVoice(i))->prepare(sr, blockSize, customWavetable);
    delayBuffer.setSize(2, static_cast<int>(sr * 4.0) + 2);
    delayBuffer.clear(); delayWrite = 0; freePpq = 0; wasPlaying = false;
    reverb.setSampleRate(sr); reverb.reset();
    for (auto* s : {&master, &drive, &delayMix, &delayFeedback}) s->reset(sr, 0.02);
    delaySamples.reset(sr, 0.15); gateGain.reset(sr, 0.003);
    master.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(raw.gain->load()));
    drive.setCurrentAndTargetValue(raw.drive->load()); delayMix.setCurrentAndTargetValue(raw.delay->load());
    delayFeedback.setCurrentAndTargetValue(raw.feedback->load());
    delaySamples.setCurrentAndTargetValue(static_cast<float>(sr * 0.375)); gateGain.setCurrentAndTargetValue(1);
    keyboard.reset(); outputPeak.store(0); playingVoices.store(0); currentStep.store(-1);
}
void ForgeSynthAudioProcessor::releaseResources()
{
    synth.allNotesOff(0, false); reverb.reset(); delayBuffer.clear(); outputPeak.store(0);
}
bool ForgeSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& buses) const
{
    return buses.getMainInputChannelSet().isDisabled()
        && (buses.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || buses.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}
double ForgeSynthAudioProcessor::getTailLengthSeconds() const
{
    const double release = raw.release->load();
    constexpr double delayDivisions[] {0.25, 0.5, 0.75, 1.0, 2.0};
    const auto tempoNow = juce::jlimit(20.0, 400.0, static_cast<double>(tempo.load()));
    const double delayDuration = 60.0 / tempoNow * delayDivisions[juce::jlimit(0, 4, static_cast<int>(raw.delayRate->load()))];
    const double feedback = juce::jlimit(0.0, 0.8, static_cast<double>(raw.feedback->load()));
    const double repeats = raw.delay->load() > 0.0001f && feedback > 0.0001
        ? juce::jlimit(1.0, 40.0, std::ceil(std::log(0.001) / std::log(feedback))) : 1.0;
    const double room = raw.reverb->load() > 0.0001f ? 4.0 : 0.0;
    return std::min(124.0, release + delayDuration * repeats + room);
}
void ForgeSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int count = buffer.getNumSamples(), channels = buffer.getNumChannels();
    if (count == 0 || channels == 0 || delayBuffer.getNumSamples() == 0) { midi.clear(); return; }
    keyboard.processNextMidiBuffer(midi, 0, count, true);
    double bpm = raw.bpm->load(), ppq = freePpq;
    bool playing = false, synced = false;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
        {
            playing = position->getIsPlaying();
            if (auto hostBpm = position->getBpm(); hostBpm && std::isfinite(*hostBpm) && *hostBpm > 0) bpm = juce::jlimit(20.0, 400.0, *hostBpm);
            if (auto hostPpq = position->getPpqPosition(); playing && hostPpq && std::isfinite(*hostPpq)) { ppq = *hostPpq; synced = true; }
        }
    tempo.store(static_cast<float>(bpm)); hostSync.store(synced);
    if (wasPlaying && !playing) synth.allNotesOff(0, true);
    wasPlaying = playing;
    if (panicRequested.exchange(false))
    {
        synth.allNotesOff(0, false); keyboard.reset(); midi.clear();
        delayBuffer.clear(); reverb.reset(); gateGain.setCurrentAndTargetValue(0);
    }
    voiceParameters = {raw.x->load(), raw.y->load(), raw.detune->load(), raw.sub->load(), raw.noise->load(),
        raw.attack->load(), raw.decay->load(), raw.sustain->load(), raw.release->load(),
        raw.cutoff->load(), raw.resonance->load(), raw.filterEnv->load(), raw.width->load(), raw.custom->load()};
    customWavetable.consume();
    synth.renderNextBlock(buffer, midi, 0, count);
    midi.clear(); // Instrument consumes MIDI; it does not advertise MIDI output.
    int voices = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i) if (synth.getVoice(i)->isVoiceActive()) ++voices;
    playingVoices.store(voices);
    master.setTargetValue(juce::Decibels::decibelsToGain(raw.gain->load()));
    drive.setTargetValue(raw.drive->load()); delayMix.setTargetValue(raw.delay->load()); delayFeedback.setTargetValue(raw.feedback->load());
    constexpr float divisions[] {0.25f, 0.5f, 0.75f, 1.0f, 2.0f};
    const int delaySize = delayBuffer.getNumSamples();
    delaySamples.setTargetValue(juce::jlimit(1.0f, static_cast<float>(delaySize - 2),
        static_cast<float>(rate * 60.0 / bpm) * divisions[juce::jlimit(0, 4, static_cast<int>(raw.delayRate->load()))]));
    const bool gate = raw.gate->load() > 0.5f;
    const double stepsPerBeat = std::pow(2.0, 1 + juce::jlimit(0, 2, static_cast<int>(raw.gateRate->load())));
    const double swing = raw.swing->load();
    const double ppqIncrement = bpm / (60.0 * rate);
    std::array<bool, 16> steps {};
    for (size_t i = 0; i < steps.size(); ++i) steps[i] = stepValues[i]->load() > 0.5f;
    for (int i = 0; i < count; ++i)
    {
        const double stepPosition = (ppq + i * ppqIncrement) * stepsPerBeat;
        // Swing lengthens the first step of each pair, preserving the pair duration.
        const double pair = std::floor(stepPosition * 0.5);
        const double withinPair = stepPosition - pair * 2;
        const auto absoluteStep = static_cast<int>(std::fmod(pair, 8.0)) * 2 + (withinPair >= 1 + swing ? 1 : 0);
        const int step = (absoluteStep % 16 + 16) % 16;
        gateGain.setTargetValue(!gate || steps[static_cast<size_t>(step)] ? 1.0f : 0.0f);
        const float gateLevel = gateGain.getNextValue(), amount = drive.getNextValue();
        const float pregain = 1.0f + amount * 7.0f, compensation = 1.0f / std::sqrt(pregain);
        float readPosition = static_cast<float>(delayWrite) - delaySamples.getNextValue();
        if (readPosition < 0) readPosition += static_cast<float>(delaySize);
        const int first = static_cast<int>(readPosition), second = (first + 1) % delaySize;
        const float fraction = readPosition - static_cast<float>(first);
        const float wet = delayMix.getNextValue(), feedback = delayFeedback.getNextValue();
        for (int ch = 0; ch < channels; ++ch)
        {
            float dry = buffer.getSample(ch, i) * gateLevel;
            dry = dry * (1.0f - amount) + std::tanh(dry * pregain) * compensation * amount;
            auto* data = delayBuffer.getWritePointer(ch);
            const float delayed = data[first] + (data[second] - data[first]) * fraction;
            data[delayWrite] = dry + delayed * feedback;
            buffer.setSample(ch, i, dry + delayed * wet);
        }
        delayWrite = (delayWrite + 1) % delaySize;
        if (i == count - 1) currentStep.store(gate ? step : -1);
    }
    freePpq = std::fmod(ppq + count * ppqIncrement, 16384.0);
    juce::Reverb::Parameters space;
    space.roomSize = 0.72f; space.damping = 0.55f; space.wetLevel = raw.reverb->load() * 0.5f;
    space.dryLevel = 1.0f; space.width = 1.0f; space.freezeMode = 0;
    reverb.setParameters(space);
    if (channels == 1) reverb.processMono(buffer.getWritePointer(0), count);
    else reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), count);
    float peak = 0;
    for (int i = 0; i < count; ++i)
    {
        const float gain = master.getNextValue();
        for (int ch = 0; ch < channels; ++ch)
        {
            const float input = buffer.getSample(ch, i) * gain;
            // Last-resort output ceiling; gain staging should keep normal playing below it.
            const float output = std::isfinite(input) ? juce::jlimit(-0.98f, 0.98f, input) : 0.0f;
            buffer.setSample(ch, i, output); peak = std::max(peak, std::abs(output));
        }
    }
    outputPeak.store(std::max(peak, outputPeak.load() * std::exp(-static_cast<float>(count / rate) * 6)));
}

float ForgeSynthAudioProcessor::value(const char* id) const { return parameters.getRawParameterValue(id)->load(); }
juce::Result ForgeSynthAudioProcessor::loadWavetable(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) return juce::Result::fail("Choose an uncompressed WAV or AIFF file.");
    if (reader->lengthInSamples != WavetableBank::size || reader->numChannels < 1 || reader->numChannels > 2)
        return juce::Result::fail("A wavetable must be exactly 2,048 frames with one or two channels.");
    juce::AudioBuffer<float> audio(static_cast<int>(reader->numChannels), WavetableBank::size);
    if (!reader->read(&audio, 0, WavetableBank::size, 0, true, true))
        return juce::Result::fail("The audio file could not be decoded.");
    double mean = 0;
    for (int i = 0; i < WavetableBank::size; ++i)
    {
        const double mono = reader->numChannels == 2 ? (audio.getSample(0, i) + audio.getSample(1, i)) * 0.5 : audio.getSample(0, i);
        mean += mono / WavetableBank::size;
    }
    double peak = 0;
    for (int i = 0; i < WavetableBank::size; ++i)
    {
        const double mono = reader->numChannels == 2 ? (audio.getSample(0, i) + audio.getSample(1, i)) * 0.5 : audio.getSample(0, i);
        if (!std::isfinite(mono)) return juce::Result::fail("The wavetable contains invalid audio samples.");
        peak = std::max(peak, std::abs(mono - mean));
    }
    if (peak < 1.0e-6) return juce::Result::fail("The wavetable is silent. Choose a non-silent single cycle.");
    for (int i = 0; i < WavetableBank::size; ++i)
    {
        const double mono = reader->numChannels == 2 ? (audio.getSample(0, i) + audio.getSample(1, i)) * 0.5 : audio.getSample(0, i);
        customSource[static_cast<size_t>(i)] = static_cast<float>(juce::jlimit(-1.0, 1.0, (mono - mean) / peak));
    }
    // The XML preset carries a portable base64 Float32 source cycle; the mip
    // tables are rebuilt here off the audio thread when a preset is restored.
    juce::MemoryOutputStream encoded;
    for (const float sample : customSource) encoded.writeFloat(sample);
    parameters.state.setProperty("customWavetable", encoded.getMemoryBlock().toBase64Encoding(), nullptr);
    customWavetable.publish(customSource);
    hasCustomWavetable.store(true);
    setParameterValue("custom", 1.0f);
    parameters.state.setProperty("presetName", "Custom / Imported Wavetable", nullptr);
    return juce::Result::ok();
}
WavetableBank::Table ForgeSynthAudioProcessor::previewTable() const
{
    WavetableBank::Table result {};
    const float x = raw.x->load(), y = raw.y->load(), mix = raw.custom->load();
    for (int i = 0; i < WavetableBank::size; ++i)
    {
        const double phase = static_cast<double>(i) / WavetableBank::size;
        const float internal = bank.sample(phase, WavetableBank::levels - 1, x, y);
        result[static_cast<size_t>(i)] = internal + (customWavetable.sample(phase, WavetableBank::levels - 1) - internal) * mix;
    }
    result[WavetableBank::size] = result[0];
    return result;
}
void ForgeSynthAudioProcessor::setParameterValue(const juce::String& id, float val)
{
    if (auto* param = parameters.getParameter(id))
    {
        param->beginChangeGesture(); param->setValueNotifyingHost(param->convertTo0to1(val)); param->endChangeGesture();
    }
}
juce::StringArray ForgeSynthAudioProcessor::factoryPresetNames()
{
    juce::StringArray names;
    for (const auto& preset : presets) names.add(preset.name);
    return names;
}
void ForgeSynthAudioProcessor::loadFactoryPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(presets))) return;
    panic();
    for (auto* param : getParameters())
    {
        param->beginChangeGesture(); param->setValueNotifyingHost(param->getDefaultValue()); param->endChangeGesture();
    }
    for (const auto& item : presets[index].values) setParameterValue(item.first, item.second);
    parameters.state.setProperty("presetName", presets[index].name, nullptr);
}
void ForgeSynthAudioProcessor::randomiseTimbre()
{
    auto& random = juce::Random::getSystemRandom();
    for (const char* id : {"x", "y", "width"}) setParameterValue(id, random.nextFloat());
    setParameterValue("detune", random.nextFloat() * 25);
    setParameterValue("cutoff", 250 * std::pow(32.0f, random.nextFloat()));
    setParameterValue("resonance", random.nextFloat() * 0.6f);
    parameters.state.setProperty("presetName", "Custom / Explored", nullptr);
}
juce::String ForgeSynthAudioProcessor::getPresetName() const { return parameters.state.getProperty("presetName", "Custom").toString(); }
void ForgeSynthAudioProcessor::getStateInformation(juce::MemoryBlock& data)
{
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, data);
}
bool ForgeSynthAudioProcessor::restoreState(const juce::ValueTree& state)
{
    if (!state.hasType("FORGE_STATE") || static_cast<int>(state.getProperty("schemaVersion", 0)) != 1
        || state.getNumChildren() != getParameters().size()) return false;
    juce::StringArray seen;
    for (const auto& child : state)
    {
        const auto id = child.getProperty("id").toString();
        auto* parameter = parameters.getParameter(id);
        const auto stored = child.getProperty("value").toString();
        if (!child.hasType("PARAM") || !parameter || seen.contains(id)) return false;
        char* end = nullptr;
        const float v = std::strtof(stored.toRawUTF8(), &end);
        if (end == stored.toRawUTF8() || end == nullptr || *end != '\0') return false;
        const auto& range = parameter->getNormalisableRange();
        if (!std::isfinite(v) || v < range.start || v > range.end) return false;
        seen.add(id);
    }
    std::array<float, WavetableBank::size> restoredTable {};
    const auto base64 = state.getProperty("customWavetable").toString();
    if (base64.isNotEmpty())
    {
        juce::MemoryBlock decoded;
        if (!decoded.fromBase64Encoding(base64) || decoded.getSize() != sizeof(float) * WavetableBank::size) return false;
        juce::MemoryInputStream input(decoded, false);
        float peak = 0;
        double mean = 0;
        for (auto& sample : restoredTable)
        {
            sample = input.readFloat();
            if (!std::isfinite(sample) || std::abs(sample) > 1.001f) return false;
            peak = std::max(peak, std::abs(sample)); mean += sample / WavetableBank::size;
        }
        if (peak < 0.5f || std::abs(mean) > 0.02) return false;
    }
    else
    {
        for (const auto& child : state)
            if (child.getProperty("id").toString() == "custom" && child.getProperty("value").toString().getFloatValue() > 0.5f)
                return false;
    }
    parameters.replaceState(state.createCopy());
    customSource = restoredTable;
    hasCustomWavetable.store(base64.isNotEmpty());
    if (hasCustomWavetable.load()) customWavetable.publish(customSource);
    panic();
    return true;
}
void ForgeSynthAudioProcessor::setStateInformation(const void* data, int size)
{
    if (size <= 0 || size > 1024 * 1024) return;
    if (auto xml = getXmlFromBinary(data, size)) restoreState(juce::ValueTree::fromXml(*xml));
}
juce::Result ForgeSynthAudioProcessor::savePreset(const juce::File& file)
{
    auto xml = parameters.copyState().createXml();
    if (!xml || !file.replaceWithText(xml->toString())) return juce::Result::fail("Could not write preset. Choose a writable folder.");
    return juce::Result::ok();
}
juce::Result ForgeSynthAudioProcessor::loadPreset(const juce::File& file)
{
    if (!file.existsAsFile() || file.getSize() > 1024 * 1024) return juce::Result::fail("Preset is missing or exceeds 1 MB.");
    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !restoreState(juce::ValueTree::fromXml(*xml))) return juce::Result::fail("Invalid or incompatible FORGE preset. Current sound was kept.");
    return juce::Result::ok();
}
juce::AudioProcessorEditor* ForgeSynthAudioProcessor::createEditor() { return new ForgeSynthAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ForgeSynthAudioProcessor(); }
