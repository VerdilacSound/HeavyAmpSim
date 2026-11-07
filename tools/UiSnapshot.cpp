#include "plugin/Processor.h"
#include <iostream>
#include <stdexcept>

template <typename Type>
Type* findControl(juce::Component& component, const juce::String& id) {
    if (auto* control = dynamic_cast<Type*>(&component))
        if (control->getTooltip().startsWith(id + " /") || control->getTooltip() == id) return control;
    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (auto* result = findControl<Type>(*component.getChildComponent(i), id)) return result;
    return nullptr;
}

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::cerr << "Usage: heavy_ui_snapshot output.png [width height]\n"; return 1; }
    HeavyProcessor processor; processor.prepareToPlay(48000., 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* gain = findControl<juce::Slider>(*editor, "amp0.gain");
    auto* pan = findControl<juce::Slider>(*editor, "amp0.pan");
    auto* solo = findControl<juce::ToggleButton>(*editor, "amp0.solo");
    check(gain && pan && solo, "Actual editor controls must be attached and discoverable");
    const double original = gain->getValue();
    gain->setValue(.72, juce::sendNotificationSync);
    check(std::abs(processor.parameters.getRawParameterValue("amp0.gain")->load() - .72f) < 1.e-5f,
          "GUI gain must reach the host parameter");
    processor.parameters.getParameter("amp0.gain")->setValueNotifyingHost(.37f);
    check(std::abs(gain->getValue() - .37) < 1.e-5, "Host automation must reach GUI gain");
    check(gain->getTextFromValue(.5) == "5.0", "Knob must display musical 0..10 units");
    check(std::abs(pan->getValueFromText("L 50") + .5) < 1.e-8 &&
          std::abs(pan->getValueFromText("R 50") - .5) < 1.e-8, "Typed pan values must preserve direction");
    solo->setToggleState(true, juce::sendNotificationSync);
    check(processor.parameters.getRawParameterValue("amp0.solo")->load() > .5f,
          "Solo control must reach the audio parameter");
    gain->setValue(original, juce::sendNotificationSync); solo->setToggleState(false, juce::sendNotificationSync);
    std::cout << "PASS real GUI controls, host automation, numeric entry and solo attachment\n";
    if (argc == 4) editor->setSize(juce::String(argv[2]).getIntValue(), juce::String(argv[3]).getIntValue());
    auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    output.deleteFile(); juce::FileOutputStream stream(output);
    if (!stream.openedOk() || !juce::PNGImageFormat().writeImageToStream(image, stream)) return 2;
    std::cout << "Rendered actual JUCE editor: " << output.getFullPathName() << '\n';
}
