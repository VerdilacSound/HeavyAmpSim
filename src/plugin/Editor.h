#pragma once
#include "Processor.h"

class HeavyEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit HeavyEditor(HeavyProcessor&);
    ~HeavyEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Row final : juce::Component {
        Row(HeavyProcessor&, const ControlSpec&);
        void resized() override;
        juce::Label label;
        juce::Slider slider;
        juce::ComboBox choice;
        juce::ToggleButton toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    };
    struct Panel final : juce::Component {
        void resized() override;
        std::vector<std::unique_ptr<Row>> rows;
    };
    void timerCallback() override;
    HeavyProcessor& processor;
    juce::LookAndFeel_V4 look;
    juce::TabbedComponent tabs {juce::TabbedButtonBar::TabsAtTop};
    std::vector<std::unique_ptr<Panel>> panels;
    juce::Label meter;
};
