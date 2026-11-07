#include "plugin/Processor.h"
#include <iostream>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::cerr << "Usage: heavy_ui_snapshot output.png [width height]\n"; return 1; }
    HeavyProcessor processor; processor.prepareToPlay(48000., 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (argc == 4) editor->setSize(juce::String(argv[2]).getIntValue(), juce::String(argv[3]).getIntValue());
    auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    output.deleteFile(); juce::FileOutputStream stream(output);
    if (!stream.openedOk() || !juce::PNGImageFormat().writeImageToStream(image, stream)) return 2;
    std::cout << "Rendered actual JUCE editor: " << output.getFullPathName() << '\n';
}
