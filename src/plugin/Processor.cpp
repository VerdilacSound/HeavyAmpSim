#include "Processor.h"
#include "Editor.h"

const std::vector<ControlSpec>& controlSpecs() {
    static const std::vector<ControlSpec> specs = [] {
        std::vector<ControlSpec> s;
        auto f = [&s](juce::String id, juce::String name, float lo, float hi, float v) {
            s.push_back({id, name, lo, hi, v, {}, false});
        };
        auto b = [&s](juce::String id, juce::String name, bool v) {
            s.push_back({id, name, 0, 1, v ? 1.f : 0.f, {}, true});
        };
        auto c = [&s](juce::String id, juce::String name, juce::StringArray choices, int v) {
            s.push_back({id, name, 0, static_cast<float>(choices.size() - 1), static_cast<float>(v), choices, false});
        };
        f("input", "Input dB", -24, 24, 0); f("output", "Output dB", -60, 6, -6);
        b("driveOn", "Drive on", false);
        f("drive", "Drive", 0, 1, .35f); f("driveTone", "Tone", 0, 1, .55f); f("driveLevel", "Level", 0, 1, .6f);
        b("fuzzOn", "Fuzz on", false);
        c("fuzzType", "Fuzz voice", {"Triangle inspired", "Ram inspired", "Russian inspired", "Op-amp inspired"}, 0);
        f("sustain", "Sustain", 0, 1, .6f); f("fuzzTone", "Tone", 0, 1, .45f); f("fuzzLevel", "Level", 0, 1, .5f);
        for (int i = 0; i < 4; ++i) {
            const auto prefix = "amp" + juce::String(i) + ".";
            b(prefix + "enabled", "Lane on", true);
            f(prefix + "gain", "Gain", 0, 1, .5f); f(prefix + "bass", "Bass", 0, 1, .5f);
            f(prefix + "mid", "Mid", 0, 1, .5f); f(prefix + "treble", "Treble", 0, 1, .5f);
            f(prefix + "presence", "Presence", 0, 1, .4f);
            c(prefix + "cabinet", "Cabinet", {"4x12 / dark", "4x12 / British", "4x12 / low-tuned", "4x12 / modern", "8x10 / bass", "2x15 / bass"}, i);
            f(prefix + "level", "Level", 0, 1, i == 0 ? .7f : 0.f);
            f(prefix + "pan", "Pan", -1, 1, 0);
        }
        c("delayMode", "Delay mode", {"Single-head tape", "Four-head drum", "Studio tape"}, 0);
        f("delayMs", "Time ms", 40, 1500, 380); f("feedback", "Feedback", 0, .88f, .35f);
        f("delayBlend", "Delay return", 0, 1, 0);
        f("reverbSize", "Size", 0, 1, .65f); f("reverbDecay", "Decay s", .2f, 12, 2.5f);
        f("reverbDamping", "Damping", 0, 1, .55f); f("reverbBlend", "Reverb return", 0, 1, 0);
        return s;
    }();
    return specs;
}
juce::AudioProcessorValueTreeState::ParameterLayout HeavyProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : controlSpecs()) {
        const juce::ParameterID id(s.id, 1);
        juce::String name = s.label;
        if (s.id.startsWith("amp")) {
            const juce::StringArray lanes {"T", "GT", "SLO", "V4"};
            name = lanes[s.id.substring(3, 4).getIntValue()] + " / " + name;
        } else if (s.id.startsWith("drive")) name = "Drive / " + name;
        else if (s.id.startsWith("fuzz") || s.id == "sustain") name = "Fuzz / " + name;
        else if (s.id.startsWith("delay") || s.id == "feedback") name = "Delay / " + name;
        else if (s.id.startsWith("reverb")) name = "Reverb / " + name;
        if (s.toggle) layout.add(std::make_unique<juce::AudioParameterBool>(id, name, s.initial > .5f));
        else if (!s.choices.isEmpty()) layout.add(std::make_unique<juce::AudioParameterChoice>(id, name, s.choices, static_cast<int>(s.initial)));
        else layout.add(std::make_unique<juce::AudioParameterFloat>(id, name,
            juce::NormalisableRange<float>(s.minimum, s.maximum), s.initial));
    }
    return layout;
}
HeavyProcessor::HeavyProcessor()
    : AudioProcessor(BusesProperties().withInput("Guitar", juce::AudioChannelSet::mono(), true)
                                     .withOutput("Stereo", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "HeavyAmpSimState", makeLayout()) {
    for (const auto& s : controlSpecs()) values.push_back(parameters.getRawParameterValue(s.id));
}
bool HeavyProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    return (layout.getMainInputChannelSet() == juce::AudioChannelSet::mono()
         || layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo())
         && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
heavy::Parameters HeavyProcessor::readParameters() const noexcept {
    heavy::Parameters p;
    std::size_t i = 0;
    auto next = [&]() { return values[i++]->load(std::memory_order_relaxed); };
    p.inputDb = next(); p.outputDb = next();
    p.pedals.driveOn = next() > .5f;
    p.pedals.drive = next(); p.pedals.driveTone = next(); p.pedals.driveLevel = next();
    p.pedals.fuzzOn = next() > .5f; p.pedals.fuzzType = static_cast<int>(next());
    p.pedals.sustain = next(); p.pedals.fuzzTone = next(); p.pedals.fuzzLevel = next();
    for (auto& a : p.lanes) {
        a.enabled = next() > .5f; a.gain = next(); a.bass = next(); a.mid = next();
        a.treble = next(); a.presence = next(); a.cabinet = static_cast<int>(next());
        a.level = next(); a.pan = next();
    }
    p.delayMode = static_cast<heavy::DelayMode>(static_cast<int>(next()));
    p.delayMs = next(); p.feedback = next(); p.delayBlend = next();
    p.reverbSize = next(); p.reverbDecay = next(); p.reverbDamping = next(); p.reverbBlend = next();
    return p;
}
void HeavyProcessor::prepareToPlay(double rate, int) {
    engine.setParameters(readParameters()); engine.prepare(rate);
    bypassHistory = {}; bypassPosition = 0;
    setLatencySamples(heavy::latencySamples);
}
void HeavyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    engine.setParameters(readParameters());
    auto* left = buffer.getWritePointer(0); auto* right = buffer.getWritePointer(1);
    float peak = 0.f;
    const bool stereoInput = getTotalNumInputChannels() == 2;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float inL = left[i], inR = stereoInput ? right[i] : inL;
        bypassHistory[0][bypassPosition] = inL; bypassHistory[1][bypassPosition] = inR;
        if (++bypassPosition == heavy::latencySamples) bypassPosition = 0;
        const auto y = engine.tick(.5f * (inL + inR));
        left[i] = y.left; right[i] = y.right;
        peak = std::max(peak, std::max(std::abs(y.left), std::abs(y.right)));
    }
    outputPeak.store(peak, std::memory_order_relaxed);
}
void HeavyProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const bool stereoInput = getTotalNumInputChannels() == 2;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float l = buffer.getSample(0, i), r = stereoInput ? buffer.getSample(1, i) : l;
        buffer.setSample(0, i, bypassHistory[0][bypassPosition]);
        buffer.setSample(1, i, bypassHistory[1][bypassPosition]);
        bypassHistory[0][bypassPosition] = l; bypassHistory[1][bypassPosition] = r;
        if (++bypassPosition == heavy::latencySamples) bypassPosition = 0;
        // Advance state so resuming doesn't revive a frozen tail.
        engine.tick(0.f);
    }
}
juce::AudioProcessorEditor* HeavyProcessor::createEditor() { return new HeavyEditor(*this); }
void HeavyProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = parameters.copyState(); state.setProperty("schemaVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void HeavyProcessor::setStateInformation(const void* data, int bytes) {
    if (auto xml = getXmlFromBinary(data, bytes))
        if (xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HeavyProcessor(); }
