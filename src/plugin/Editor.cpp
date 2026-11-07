#include "Editor.h"

namespace {
const juce::Colour background(0xff111317), panel(0xff1a1d23), border(0xff303640);
const juce::Colour text(0xffedf0f5), muted(0xff8b95a5), gold(0xffe4b879);
const std::array<juce::Colour, 4> colours {juce::Colour(0xffd7ad71), juce::Colour(0xff8aba9d),
                                        juce::Colour(0xffa7a5df), juce::Colour(0xff79b7d3)};
const juce::StringArray names {"MODEL T", "GT120", "SLO100", "V4"};
const juce::StringArray descriptors {"Sunn / first-generation target", "MatAmp / GT120 target",
                                    "Soldano / SLO100 target", "Ampeg / early-70s target"};
void label(juce::Graphics& g, juce::String value, juce::Rectangle<int> area,
           float size = 11.f, juce::Colour colour = muted, bool bold = false) {
    g.setColour(colour); g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value, area, juce::Justification::centredLeft);
}
}
void HeavyEditor::Theme::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                         float value, float start, float end, juce::Slider& s) {
    auto r = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                    static_cast<float>(w), static_cast<float>(h)).reduced(5.f);
    const float radius = juce::jmin(r.getWidth(), r.getHeight()) * .5f;
    const auto centre = r.getCentre();
    auto dial = juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre);
    juce::Path track; track.addCentredArc(centre.x, centre.y, radius, radius, 0.f, start, end, true);
    g.setColour(juce::Colour(0xff343b47)); g.strokePath(track, juce::PathStrokeType(2.5f));
    juce::Path arc; arc.addCentredArc(centre.x, centre.y, radius, radius, 0.f, start, start + value * (end - start), true);
    g.setColour(s.findColour(juce::Slider::rotarySliderFillColourId)); g.strokePath(arc, juce::PathStrokeType(2.5f));
    dial.reduce(6.f, 6.f);
    g.setColour(juce::Colours::black.withAlpha(.35f)); g.fillEllipse(dial.translated(0, 2));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff444b57), dial.getTopLeft(),
                                          juce::Colour(0xff242a33), dial.getBottomRight(), false));
    g.fillEllipse(dial); g.setColour(juce::Colour(0xff535d6c)); g.drawEllipse(dial, .7f);
    const float angle = start + value * (end - start);
    const juce::Point<float> a(centre.x + std::sin(angle) * radius * .32f, centre.y - std::cos(angle) * radius * .32f);
    const juce::Point<float> b(centre.x + std::sin(angle) * radius * .66f, centre.y - std::cos(angle) * radius * .66f);
    g.setColour(s.findColour(juce::Slider::rotarySliderFillColourId)); g.drawLine({a, b}, 2.4f);
}
void HeavyEditor::Theme::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool) {
    auto r = b.getLocalBounds().toFloat().reduced(1.f);
    const auto accent = b.findColour(juce::ToggleButton::tickColourId);
    g.setColour(b.getToggleState() ? accent.withAlpha(.2f) : juce::Colour(0xff242a33)); g.fillRoundedRectangle(r, 5.f);
    g.setColour(b.getToggleState() ? accent : border.brighter(over ? .3f : 0.f)); g.drawRoundedRectangle(r, 5.f, 1.f);
    g.setColour(b.getToggleState() ? accent : muted); g.setFont(juce::FontOptions(10.f, juce::Font::bold));
    g.drawText(b.getButtonText(), r, juce::Justification::centred);
}
void HeavyEditor::Theme::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider& s) {
    const float cy = static_cast<float>(y) + static_cast<float>(h) * .5f;
    g.setColour(juce::Colour(0xff303743)); g.fillRoundedRectangle(static_cast<float>(x), cy - 2, static_cast<float>(w), 4, 2);
    g.setColour(s.findColour(juce::Slider::trackColourId));
    g.fillRoundedRectangle(static_cast<float>(x), cy - 2, juce::jmax(0.f, pos - static_cast<float>(x)), 4, 2);
    g.setColour(text); g.fillRoundedRectangle(pos - 3, cy - 8, 6, 16, 2);
}
HeavyEditor::Control::Control(HeavyProcessor& p, const ControlSpec& s, juce::Colour accent)
    : isToggle(s.toggle), isChoice(!s.choices.isEmpty()) {
    label.setText(s.label.toUpperCase(), juce::dontSendNotification);
    label.setFont(juce::FontOptions(9.5f, juce::Font::bold)); label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, muted);
    if (isToggle) {
        addAndMakeVisible(toggle); toggle.setButtonText(s.id.endsWith(".solo") ? "SOLO" : "ON");
        toggle.setColour(juce::ToggleButton::tickColourId, accent); toggle.setTooltip(s.id);
        ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, s.id, toggle);
    } else if (isChoice) {
        addAndMakeVisible(label); addAndMakeVisible(choice); choice.addItemList(s.choices, 1); choice.setTooltip(s.id);
        ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, s.id, choice);
    } else {
        addAndMakeVisible(label); addAndMakeVisible(slider);
        const bool fader = s.id.endsWith(".level");
        slider.setSliderStyle(fader ? juce::Slider::LinearHorizontal : juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        slider.setTextBoxStyle(fader ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow, false, 58, 18);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent); slider.setColour(juce::Slider::trackColourId, accent);
        slider.setColour(juce::Slider::textBoxTextColourId, text);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setDoubleClickReturnValue(true, s.initial); slider.setNumDecimalPlacesToDisplay(1);
        slider.setTooltip(s.id + " / double-click to reset");
        sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, s.id, slider);
        if (s.id.endsWith(".pan")) {
            slider.textFromValueFunction = [](double v) { return std::abs(v) < .005 ? juce::String("C") : juce::String(v < 0 ? "L " : "R ") + juce::String(std::abs(v) * 100., 0); };
            slider.valueFromTextFunction = [](const juce::String& t) {
                if (t.containsIgnoreCase("C")) return 0.;
                const double value = t.retainCharacters("0123456789.-").getDoubleValue() * .01;
                return t.containsIgnoreCase("L") ? -std::abs(value) : value;
            };
        } else if (fader) {
            slider.textFromValueFunction = [](double v) { return v < .00001 ? juce::String("-inf") : juce::String(juce::Decibels::gainToDecibels(v), 1); };
            slider.valueFromTextFunction = [](const juce::String& t) { return t.containsIgnoreCase("inf") ? 0. : juce::Decibels::decibelsToGain(t.getDoubleValue()); };
        } else if (s.id == "input" || s.id == "output") {
            slider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
            slider.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
        } else if (s.id == "delayMs") {
            slider.textFromValueFunction = [](double v) { return juce::String(v, 0) + " ms"; };
            slider.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
        } else if (s.id == "feedback" || s.id.endsWith("Blend")) {
            slider.textFromValueFunction = [](double v) { return juce::String(v * 100., 0) + " %"; };
            slider.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() * .01; };
        } else if (s.id == "reverbDecay") {
            slider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " s"; };
            slider.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
        } else if (std::abs(s.minimum) < 1.e-6f && std::abs(s.maximum - 1.f) < 1.e-6f) {
            slider.textFromValueFunction = [](double v) { return juce::String(v * 10., 1); };
            slider.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() * .1; };
        }
        slider.updateText();
    }
}
void HeavyEditor::Control::resized() {
    auto r = getLocalBounds();
    if (isToggle) { toggle.setBounds(r); return; }
    label.setBounds(r.removeFromTop(16));
    if (isChoice) choice.setBounds(r.reduced(2, 2)); else slider.setBounds(r);
}
void HeavyEditor::Meter::setLevel(float v) { level = std::max(v, level * .92f); repaint(); }
void HeavyEditor::Meter::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0d1014)); g.fillRoundedRectangle(r, 3.f);
    const float norm = juce::jlimit(0.f, 1.f, (juce::Decibels::gainToDecibels(level, -60.f) + 60.f) / 60.f);
    auto fill = r.reduced(2.f);
    if (r.getHeight() > r.getWidth()) { const float h = fill.getHeight() * norm; fill = fill.removeFromBottom(h); }
    else fill.setWidth(fill.getWidth() * norm);
    g.setColour(level >= .98f ? juce::Colour(0xffef7a78) : juce::Colour(0xff8dc9a7)); g.fillRoundedRectangle(fill, 1.f);
}
HeavyEditor::HeavyEditor(HeavyProcessor& p) : AudioProcessorEditor(p), processor(p) {
    theme.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff11151b));
    theme.setColour(juce::ComboBox::textColourId, text); theme.setColour(juce::ComboBox::outlineColourId, border);
    theme.setColour(juce::PopupMenu::backgroundColourId, panel); theme.setColour(juce::PopupMenu::textColourId, text);
    theme.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff252b34));
    theme.setColour(juce::TextButton::textColourOffId, muted); setLookAndFeel(&theme);
    for (const auto& spec : controlSpecs()) {
        auto accent = gold;
        if (spec.id.startsWith("amp")) accent = colours[static_cast<std::size_t>(spec.id.substring(3, 4).getIntValue())];
        else if (spec.id.startsWith("drive")) accent = juce::Colour(0xff8aba9d);
        else if (spec.id.startsWith("fuzz") || spec.id == "sustain") accent = juce::Colour(0xffd8a086);
        auto c = std::make_unique<Control>(p, spec, accent); addAndMakeVisible(*c); controls.emplace(spec.id, std::move(c));
    }
    addAndMakeVisible(inputMeter); addAndMakeVisible(outputMeter);
    for (auto& m : laneMeters) addAndMakeVisible(m);
    for (auto* l : {&inputReadout, &outputReadout}) {
        addAndMakeVisible(l); l->setFont(juce::FontOptions(10.f)); l->setColour(juce::Label::textColourId, muted);
    }
    addAndMakeVisible(resetSolo);
    resetSolo.onClick = [this] {
        for (int i = 0; i < 4; ++i) {
            auto* param = processor.parameters.getParameter("amp" + juce::String(i) + ".solo");
            param->beginChangeGesture(); param->setValueNotifyingHost(0.f); param->endChangeGesture();
        }
    };
    setResizable(true, true); setResizeLimits(1180, 900, 1800, 1200); setSize(1400, 940);
    timerCallback(); startTimerHz(30);
}
HeavyEditor::~HeavyEditor() { stopTimer(); setLookAndFeel(nullptr); }
void HeavyEditor::place(const juce::String& id, juce::Rectangle<int> area) {
    const auto found = controls.find(id); if (found != controls.end()) found->second->setBounds(area);
}
void HeavyEditor::card(juce::Graphics& g, juce::Rectangle<int> r, juce::String title,
                        juce::String subtitle, juce::Colour accent) {
    const auto f = r.toFloat();
    g.setColour(juce::Colours::black.withAlpha(.22f)); g.fillRoundedRectangle(f.translated(0.f, 3.f), 10.f);
    g.setColour(panel); g.fillRoundedRectangle(f, 10.f); g.setColour(border); g.drawRoundedRectangle(f, 10.f, .8f);
    g.setColour(accent); g.fillRoundedRectangle(static_cast<float>(r.getX() + 16), static_cast<float>(r.getY() + 18), 3.f, 23.f, 1.5f);
    label(g, title, {r.getX() + 28, r.getY() + 13, r.getWidth() - 90, 20}, 14.f, text, true);
    label(g, subtitle, {r.getX() + 28, r.getY() + 33, r.getWidth() - 42, 17}, 10.f);
}
void HeavyEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff242a32), 0.f, 0.f, background, static_cast<float>(getWidth()), 160.f, false));
    g.fillRect(0, 0, getWidth(), 92);
    label(g, "HEAVYAMP", {26, 17, 300, 38}, 31.f, text, true);
    label(g, "FOUR VOICES. ONE MASSIVE RIG.", {28, 55, 320, 19}, 10.5f, gold, true);
    label(g, "4x OVERSAMPLING", {getWidth() - 680, 27, 170, 20}, 10.f, muted, true);
    label(g, juce::String(processor.getLatencySamples()) + " SAMPLES / STEREO OUT", {getWidth() - 680, 48, 190, 18}, 10.f);
    card(g, driveArea, "DRIVE", "Shared input / Centaur target", juce::Colour(0xff8aba9d));
    card(g, fuzzArea, "FUZZ", "Shared input / Muff targets", juce::Colour(0xffd8a086));
    card(g, delayArea, "STEREO ECHO", "Parallel wet return", juce::Colour(0xff8ab7c7));
    card(g, reverbArea, "STEREO SPACE", "Independent parallel return", juce::Colour(0xffaaa5ce));
    label(g, "SIGNAL FLOW", {26, 320, 105, 22}, 9.5f, muted, true);
    label(g, "INPUT   >   DRIVE   >   FUZZ   >   4 x AMP / CAB   >   STEREO MIX   >   PARALLEL ECHO + SPACE   >   OUTPUT", {132, 320, getWidth() - 165, 22}, 10.f, juce::Colour(0xffabb4c2));
    for (std::size_t i = 0; i < ampAreas.size(); ++i) {
        const auto r = ampAreas[i]; card(g, r, names[static_cast<int>(i)], descriptors[static_cast<int>(i)], colours[i]);
        label(g, "AMPLIFIER", {r.getX() + 16, r.getY() + 64, 120, 18}, 9.f, muted, true);
        g.setColour(border); g.drawHorizontalLine(r.getY() + 187, static_cast<float>(r.getX() + 16), static_cast<float>(r.getRight() - 16));
        const int cabY = r.getBottom() - 116;
        const int selection = static_cast<int>(processor.parameters.getRawParameterValue("amp" + juce::String(static_cast<int>(i)) + ".cabinet")->load());
        const int rows = selection == 4 ? 4 : (selection == 5 ? 1 : 2);
        auto cabinet = juce::Rectangle<float>(static_cast<float>(r.getX() + 18), static_cast<float>(cabY + 51), 58.f, 49.f);
        g.setColour(juce::Colour(0xff101318)); g.fillRoundedRectangle(cabinet, 3.f);
        g.setColour(border); g.drawRoundedRectangle(cabinet, 3.f, 1.f);
        for (int row = 0; row < rows; ++row) for (int col = 0; col < 2; ++col) {
            const float diameter = rows == 4 ? 9.f : 16.f;
            const float x = cabinet.getX() + 9.f + static_cast<float>(col) * 23.f;
            const float y = cabinet.getY() + (49.f - static_cast<float>(rows) * diameter) * .5f + static_cast<float>(row) * diameter;
            g.setColour(colours[i].withAlpha(.25f)); g.drawEllipse(x, y, diameter - 2.f, diameter - 2.f, 1.f);
        }
        label(g, "SYNTHETIC CAB", {r.getX() + 88, cabY + 54, r.getWidth() - 102, 18}, 9.f, muted, true);
        label(g, "IR engine pending", {r.getX() + 88, cabY + 72, r.getWidth() - 102, 17}, 10.f);
        const auto mix = mixAreas[i]; g.setColour(panel); g.fillRoundedRectangle(mix.toFloat(), 8.f);
        g.setColour(colours[i]); g.fillRect(mix.getX(), mix.getY() + 15, 2, mix.getHeight() - 30);
        label(g, juce::String(static_cast<int>(i) + 1) + "  " + names[static_cast<int>(i)], {mix.getX() + 16, mix.getY() + 9, 160, 22}, 11.f, text, true);
    }
    label(g, "STEREO MIXER", {26, mixAreas[0].getY() - 24, 200, 18}, 9.5f, muted, true);
    label(g, "ENGINEERING BUILD / CIRCUIT REBUILD IN PROGRESS", {26, getHeight() - 36, 610, 23}, 9.5f, muted);
    label(g, "Double-click to reset   /   Click a value to type", {getWidth() - 355, getHeight() - 36, 335, 23}, 9.5f);
}
void HeavyEditor::resized() {
    const int w = getWidth(), gap = 12;
    place("input", {w - 448, 7, 72, 80}); place("output", {w - 234, 7, 72, 80});
    inputMeter.setBounds(w - 365, 36, 103, 10); outputMeter.setBounds(w - 151, 36, 103, 10);
    inputReadout.setBounds(w - 368, 48, 112, 20); outputReadout.setBounds(w - 154, 48, 112, 20);
    const int available = w - 48, pedalW = (available * 43 / 100 - gap) / 2;
    driveArea = {24, 104, pedalW, 196}; fuzzArea = {driveArea.getRight() + gap, 104, pedalW, 196};
    const int fxX = fuzzArea.getRight() + gap;
    const int echoW = (w - 24 - fxX - gap) / 2;
    delayArea = {fxX, 104, echoW, 196}; reverbArea = {fxX + echoW + gap, 104, echoW, 196};
    auto dials = [this](juce::Rectangle<int> area, const juce::StringArray& ids, int top, int height) {
        const int width = (area.getWidth() - 24) / ids.size();
        for (int i = 0; i < ids.size(); ++i) place(ids[i], {area.getX() + 12 + i * width, top, width, height});
    };
    place("driveOn", {driveArea.getRight() - 52, 119, 36, 24}); place("fuzzOn", {fuzzArea.getRight() - 52, 119, 36, 24});
    dials(driveArea, {"drive", "driveTone", "driveLevel"}, 179, 104);
    place("fuzzType", {fuzzArea.getX() + 14, 157, fuzzArea.getWidth() - 28, 39});
    dials(fuzzArea, {"sustain", "fuzzTone", "fuzzLevel"}, 201, 88);
    place("delayMode", {delayArea.getX() + 14, 157, delayArea.getWidth() - 28, 39});
    dials(delayArea, {"delayMs", "feedback", "delayBlend"}, 201, 88);
    dials(reverbArea, {"reverbSize", "reverbDecay", "reverbDamping", "reverbBlend"}, 179, 104);
    const int cardW = (available - 3 * gap) / 4, mixerY = getHeight() - 182;
    for (int i = 0; i < 4; ++i) {
        const int x = 24 + i * (cardW + gap); const auto prefix = "amp" + juce::String(i) + ".";
        auto& area = ampAreas[static_cast<std::size_t>(i)]; area = {x, 354, cardW, mixerY - 392};
        place(prefix + "enabled", {area.getRight() - 51, area.getY() + 16, 35, 24});
        dials(area, {prefix + "gain", prefix + "bass", prefix + "mid", prefix + "treble", prefix + "presence"}, area.getY() + 82, 100);
        place(prefix + "cabinet", {x + 14, area.getBottom() - 116, cardW - 28, 44});
        mixAreas[static_cast<std::size_t>(i)] = {x, mixerY, cardW, 126};
        place(prefix + "solo", {x + cardW - 69, mixerY + 12, 51, 23});
        place(prefix + "level", {x + 12, mixerY + 46, cardW - 106, 57});
        place(prefix + "pan", {x + cardW - 86, mixerY + 41, 66, 74});
        laneMeters[static_cast<std::size_t>(i)].setBounds(x + cardW - 14, mixerY + 47, 7, 63);
    }
    resetSolo.setBounds(w - 142, mixerY - 27, 112, 21);
}
void HeavyEditor::timerCallback() {
    const float input = processor.inputPeak.load(std::memory_order_relaxed), output = processor.outputPeak.load(std::memory_order_relaxed);
    inputMeter.setLevel(input); outputMeter.setLevel(output);
    inputReadout.setText("IN " + juce::String(juce::Decibels::gainToDecibels(input, -90.f), 1) + " dBFS", juce::dontSendNotification);
    outputReadout.setText("OUT " + juce::String(juce::Decibels::gainToDecibels(output, -90.f), 1) + " dBFS", juce::dontSendNotification);
    for (std::size_t i = 0; i < laneMeters.size(); ++i) {
        laneMeters[i].setLevel(processor.lanePeaks[i].load(std::memory_order_relaxed));
        const int cab = static_cast<int>(processor.parameters.getRawParameterValue("amp" + juce::String(static_cast<int>(i)) + ".cabinet")->load());
        if (cab != lastCabs[i]) { lastCabs[i] = cab; repaint(ampAreas[i]); }
    }
}
