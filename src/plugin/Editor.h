#pragma once
#include "Processor.h"
#include <map>

class HeavyEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit HeavyEditor(HeavyProcessor&);
    ~HeavyEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    class Theme final : public juce::LookAndFeel_V4 {
    public:
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
        void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                              juce::Slider::SliderStyle, juce::Slider&) override;
    };
    struct Control final : juce::Component {
        Control(HeavyProcessor&, const ControlSpec&, juce::Colour);
        void resized() override;
        juce::Label label;
        juce::Slider slider;
        juce::ComboBox choice;
        juce::ToggleButton toggle;
        bool isToggle = false, isChoice = false;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    };
    struct Meter final : juce::Component {
        void setLevel(float);
        void paint(juce::Graphics&) override;
        float level = 0.f;
    };
    void timerCallback() override;
    void place(const juce::String&, juce::Rectangle<int>);
    void card(juce::Graphics&, juce::Rectangle<int>, juce::String, juce::String, juce::Colour);
    HeavyProcessor& processor;
    Theme theme;
    std::map<juce::String, std::unique_ptr<Control>> controls;
    std::array<Meter, 4> laneMeters;
    Meter inputMeter, outputMeter;
    juce::Rectangle<int> driveArea, fuzzArea, delayArea, reverbArea;
    std::array<juce::Rectangle<int>, 4> ampAreas, mixAreas;
    std::array<int, 4> lastCabs {{-1, -1, -1, -1}};
    juce::Label inputReadout, outputReadout;
    juce::TextButton resetSolo {"CLEAR SOLOS"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeavyEditor)
};
