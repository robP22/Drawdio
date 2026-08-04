#include "PresetFileController.h"

void PresetFileController::savePreset(std::function<juce::MemoryBlock()> createState)
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.drawdio");
    chooser->launchAsync(2, [chooser, createState = std::move(createState)](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result == juce::File{}) return;
        juce::File file = result;
        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension(".drawdio");
        juce::MemoryBlock state = createState();
        juce::FileOutputStream stream(file);
        if (stream.openedOk())
            stream.write(state.getData(), state.getSize());
    });
}

void PresetFileController::loadPreset(std::function<bool(const void*, int)> applyState,
                                      std::function<void()> onLoaded)
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.drawdio");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [chooser, applyState = std::move(applyState), onLoaded = std::move(onLoaded)](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result == juce::File{}) return;
        juce::MemoryBlock data;
        if (result.loadFileAsData(data))
        {
            if (applyState(data.getData(), static_cast<int>(data.getSize())))
                onLoaded();
        }
    });
}
