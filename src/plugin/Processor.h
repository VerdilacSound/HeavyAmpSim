#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/Engine.h"

struct ControlSpec {
    juce::String id, label;
    float minimum = 0.f, maximum = 1.f, initial = .5f;
    juce::StringArray choices;
    bool toggle = false;
};
const std::vector<ControlSpec>& controlSpecs();

class HeavyProcessor final : public juce::AudioProcessor {
public:
    HeavyProcessor();
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "HeavyAmpSim"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 140.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Doom foundation"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> outputPeak {0.f};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    heavy::Parameters readParameters() const noexcept;
    std::vector<std::atomic<float>*> values;
    heavy::Engine engine;
    std::array<std::array<float, heavy::latencySamples>, 2> bypassHistory {};
    std::size_t bypassPosition = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeavyProcessor)
};
