#include "Editor.h"

HeavyEditor::Row::Row(HeavyProcessor& p, const ControlSpec& s) {
    label.setText(s.label, juce::dontSendNotification); label.setColour(juce::Label::textColourId, juce::Colour(0xffd8d1bd));
    addAndMakeVisible(label);
    if (s.toggle) {
        addAndMakeVisible(toggle); toggle.setButtonText("Enabled");
        ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, s.id, toggle);
    } else if (!s.choices.isEmpty()) {
        choice.addItemList(s.choices, 1); addAndMakeVisible(choice);
        ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, s.id, choice);
    } else {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 24);
        slider.setDoubleClickReturnValue(true, s.initial);
        slider.setNumDecimalPlacesToDisplay(2); addAndMakeVisible(slider);
        sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, s.id, slider);
    }
}
void HeavyEditor::Row::resized() {
    auto r = getLocalBounds().reduced(8, 2); label.setBounds(r.removeFromLeft(150));
    slider.setBounds(r); choice.setBounds(r.reduced(0, 3)); toggle.setBounds(r);
}
void HeavyEditor::Panel::resized() {
    auto r = getLocalBounds().reduced(18, 12);
    for (auto& row : rows) row->setBounds(r.removeFromTop(36));
}
HeavyEditor::HeavyEditor(HeavyProcessor& p) : AudioProcessorEditor(p), processor(p) {
    look.setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff171b18));
    look.setColour(juce::Slider::thumbColourId, juce::Colour(0xffd4b56a));
    look.setColour(juce::Slider::trackColourId, juce::Colour(0xff667447));
    look.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff303a30));
    look.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff252d24));
    look.setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colour(0xffe7ddc5));
    setLookAndFeel(&look);
    addAndMakeVisible(tabs); addAndMakeVisible(meter);
    meter.setJustificationType(juce::Justification::centredRight);
    meter.setColour(juce::Label::textColourId, juce::Colour(0xffd4b56a));
    const juce::StringArray titles {"Pedalboard", "T / prototype", "GT / prototype", "SLO / prototype", "V4 / prototype", "Stereo returns", "I/O"};
    for (int i = 0; i < titles.size(); ++i) {
        auto panel = std::make_unique<Panel>();
        for (const auto& spec : controlSpecs()) {
            bool belongs = false;
            if (i == 0) belongs = spec.id == "driveOn" || spec.id == "drive" || spec.id == "driveTone" || spec.id == "driveLevel"
                    || spec.id == "fuzzOn" || spec.id == "fuzzType" || spec.id == "sustain" || spec.id == "fuzzTone" || spec.id == "fuzzLevel";
            else if (i <= 4) belongs = spec.id.startsWith("amp" + juce::String(i - 1) + ".");
            else if (i == 5) belongs = spec.id.startsWith("delay") || spec.id == "feedback" || spec.id.startsWith("reverb");
            else belongs = spec.id == "input" || spec.id == "output";
            if (belongs) {
                auto row = std::make_unique<Row>(p, spec); panel->addAndMakeVisible(*row);
                panel->rows.push_back(std::move(row));
            }
        }
        tabs.addTab(titles[i], juce::Colour(0xff252d24), panel.get(), false);
        panels.push_back(std::move(panel));
    }
    setSize(960, 620); startTimerHz(15);
}
HeavyEditor::~HeavyEditor() { stopTimer(); tabs.clearTabs(); setLookAndFeel(nullptr); }
void HeavyEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff171b18));
    g.setColour(juce::Colour(0xffd4b56a)); g.setFont(juce::FontOptions(30.f, juce::Font::bold));
    g.drawText("HEAVY AMP SIM", 24, 16, 480, 40, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffa9b09c)); g.setFont(juce::FontOptions(14.f));
    g.drawText("DRIVE > FUZZ > FOUR AMP / CAB LANES > STEREO MIX > PARALLEL RETURNS", 26, 61, 900, 24, juce::Justification::centredLeft);
    g.drawText("0.1 / development prototype - provisional voicings and synthetic cabinets", 24, getHeight() - 38, 900, 24, juce::Justification::centredLeft);
}
void HeavyEditor::resized() {
    tabs.setBounds(18, 102, getWidth() - 36, getHeight() - 155);
    meter.setBounds(getWidth() - 260, 22, 230, 30);
}
void HeavyEditor::timerCallback() {
    const auto peak = processor.outputPeak.load(std::memory_order_relaxed);
    meter.setText("OUT  " + juce::String(juce::Decibels::gainToDecibels(peak, -100.f), 1) + " dBFS", juce::dontSendNotification);
}
