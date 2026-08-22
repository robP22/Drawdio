cmake_minimum_required(VERSION 3.24)
project(Drawdio VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add Assets directory for textures
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
if(EXISTS "${ASSETS_DIR}")
    message(STATUS "Assets directory found: ${ASSETS_DIR}")
    file(GLOB_RECURSE ASSET_FILES "${ASSETS_DIR}/*.png")
    foreach(file IN LISTS ASSET_FILES)
        message(STATUS "  Asset: ${file}")
    endforeach()
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Create binary data for assets (after JUCE is loaded)
juce_add_binary_data(DrawdioAssets SOURCES ${ASSET_FILES})

juce_add_plugin(Drawdio
    PRODUCT_NAME              "Drawdio"
    PLUGIN_NAME               "Drawdio"
    VERSION                   "0.2.0"
    BUILD_VERSION             "0.2.0"
    COMPANY_NAME              "robP"
    COMPANY_WEBSITE           "https://github.com/robP22/Drawdio"
    COMPANY_EMAIL             "drawdio@proton.me"
    BUNDLE_ID                 "com.robp.drawdio"
    PLUGIN_MANUFACTURER_CODE  DrDd
    PLUGIN_CODE               Draw
    DESCRIPTION               "Visual audio effects pedalboard driven by canvas drawings."
    PLUGIN_IS_A_SYNTH         FALSE
    PLUGIN_WANTS_MIDI_INPUT   FALSE
    PLUGIN_NEEDS_MIDI_OUTPUT  FALSE

    FORMATS VST3 AU Standalone

    PLUGIN_CHANNEL_CONFIGS "0 1" "1 1" "2 2"

    DEFINITIONS
        JUCE_WEB_BROWSER=0
)

# Link the binary data target so its include directories are available
target_link_libraries(Drawdio PRIVATE DrawdioAssets)

juce_generate_juce_header(Drawdio)

target_sources(Drawdio PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UnifiedPedalProcessor.cpp
    Source/PedalboardGrid.cpp
    Source/RenderUtils.cpp
    Source/Core/CanvasAnalysis.cpp
    Source/Dsp/ReverbNetwork.cpp
    Source/Dsp/DspEffectFactory.cpp
    Source/Compile/CompilerThread.cpp
    Source/Compile/CanvasMessageQueue.cpp
    Source/Compile/PenDebouncer.cpp
    Source/Compile/CompilerEngine.cpp
    Source/UI/Canvas/PixelCanvasComponent.cpp
    Source/UI/Canvas/ColorPalette.cpp
    Source/UI/Canvas/ArcButton.cpp
    Source/UI/Pedalboard/PedalComponent.cpp
    Source/UI/Pedalboard/CablePathBuilder.cpp
    Source/UI/Pedalboard/CableRenderer.cpp
    Source/UI/Pedalboard/ManualRoutingController.cpp
    Source/UI/EditorSyncController.cpp
    Source/UI/Controls/MixerStrip.cpp
    Source/UI/Controls/BottomControlBar.cpp
    Source/UI/Controls/AutomationDisplay.cpp
    Source/UI/Theme/ThemeManager.cpp
    Source/Resources/ResourceManager.cpp
    Source/State/ParameterCache.cpp
    Source/State/PedalState.cpp
    Source/State/ReleaseQueue.cpp
    Source/State/ManualConnectionModel.cpp
    Source/State/CanvasRoutingManager.cpp
    Source/State/EffectConfigRegistry.cpp
    Source/State/PedalDefinition.cpp
    Source/State/StateSerializer.cpp
    Source/State/ConfigManager.cpp
    Source/State/AutomationEnvelope.cpp
    Source/State/AutomationCompiler.cpp
    Source/State/AutomationPlayer.cpp
    Source/Effects/GranularBaseEffect.cpp
    Source/Effects/DistortionEffects.cpp
    Source/Effects/FilterEffects.cpp
    Source/Effects/DelayEffects.cpp
    Source/Effects/ReverbEffects.cpp
    Source/Effects/PitchEffects.cpp
    Source/Effects/MiscEffects.cpp
    Source/Effects/ReverseBufferEffect.cpp
    Source/Effects/GrainScrubberEffect.cpp
    Source/Effects/SpectralFilterEffect.cpp
    Source/Effects/ConvolutionSpaceEffect.cpp
    Source/Effects/ResamplerEffect.cpp
    Source/Effects/ModulationEffects.cpp
    Source/Effects/ReTimeEffect.cpp
)

target_include_directories(Drawdio PRIVATE Source)

target_link_libraries(Drawdio PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)

target_compile_definitions(Drawdio PRIVATE
    JucePlugin_EditorWidth=1400
    JucePlugin_EditorHeight=900
)
