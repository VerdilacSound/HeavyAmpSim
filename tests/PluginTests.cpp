#include "plugin/Processor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        HeavyProcessor p;
        require(p.getParameters().size() == static_cast<int>(controlSpecs().size()), "All controls must be exposed to the host");
        int index = 0;
        for (const auto& spec : controlSpecs()) {
            auto* parameter = p.parameters.getParameter(spec.id);
            const float arbitrary = ++index % 2 == 0 ? .2f : .8f;
            parameter->setValueNotifyingHost(parameter->convertTo0to1(parameter->convertFrom0to1(arbitrary)));
        }
        auto* gain = p.parameters.getParameter("amp2.gain");
        gain->setValueNotifyingHost(.83f);
        juce::MemoryBlock saved; p.getStateInformation(saved);
        HeavyProcessor restored; restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        for (const auto& spec : controlSpecs()) {
            if (std::abs(restored.parameters.getParameter(spec.id)->getValue() - p.parameters.getParameter(spec.id)->getValue()) >= .00001f)
                std::cerr << "State mismatch: " << spec.id << '\n';
            require(std::abs(restored.parameters.getParameter(spec.id)->getValue() - p.parameters.getParameter(spec.id)->getValue()) < .00001f,
                    "Every control must survive state recall, including switches and choices");
        }
        require(std::abs(restored.parameters.getParameter("amp2.gain")->getValue() - .83f) < .00001f, "Host state must restore parameter values");
        for (const auto& spec : controlSpecs()) {
            auto* parameter = restored.parameters.getParameter(spec.id);
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
        }
        restored.prepareToPlay(48000, 256);
        require(restored.getLatencySamples() == 16, "Plugin must report oversampling latency");
        juce::AudioBuffer<float> buffer(2, 256); juce::MidiBuffer midi;
        double total = 0.;
        for (int block = 0; block < 50; ++block) {
            buffer.clear();
            for (int i = 0; i < 256; ++i) buffer.setSample(0, i, .2f * std::sin((block * 256 + i) * .03f));
            restored.processBlock(buffer, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) {
                require(std::isfinite(buffer.getSample(c, i)), "Host callback must produce finite audio");
                total += std::abs(buffer.getSample(c, i));
            }
        }
        require(total > 1., "Host callback must process actual guitar input");
        auto layout = restored.getBusesLayout();
        layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
        require(restored.setBusesLayout(layout), "Stereo input / stereo output layout must be supported");
        restored.prepareToPlay(96000, 511);
        for (int size : {0, 1, 17, 511}) {
            juce::AudioBuffer<float> variable(2, size); variable.clear();
            restored.processBlock(variable, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < size; ++i)
                require(std::isfinite(variable.getSample(c, i)), "Variable and zero blocks must be handled");
        }
        layout.inputBuses.set(0, juce::AudioChannelSet::mono());
        require(restored.setBusesLayout(layout), "Mono input must be restorable");
        restored.prepareToPlay(48000, 256); buffer.clear(); buffer.setSample(0, 0, 1);
        restored.processBlockBypassed(buffer, midi);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i)
            require(std::abs(buffer.getSample(c, i) - (i == 16 ? 1.f : 0.f)) < 1.e-7f, "Bypass must preserve signal and reported latency");
        std::cout << "PASS host parameter exposure, state recall, callback, latency and bypass\n";
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
