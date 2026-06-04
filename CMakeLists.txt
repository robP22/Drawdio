cmake_minimum_required(VERSION 3.24)
project(Drawdio VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add Assets directory for textures and skins
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
if(EXISTS "${ASSETS_DIR}")
    message(STATUS "Assets directory found: ${ASSETS_DIR}")
    file(GLOB_RECURSE ASSET_FILES "${ASSETS_DIR}/*.png")
    foreach(file IN LISTS ASSET_FILES)
        message(STATUS "  Asset: ${file}")
    endforeach()
endif()

# Create binary data for assets
juce_add_binary_data(DrawdioAssets SOURCES ${ASSET_FILES})

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Drawdio
    PRODUCT_NAME              "Drawdio"
    PLUGIN_MANUFACTURER       "robP"
    PLUGIN_MANUFACTURER_CODE  DrDd
    PLUGIN_CODE               Draw
    PLUGIN_DESCRIPTION        "VST effects plugin that routes audio and changes knob parameters of 6 unique guitar pedals based on the image you draw on the rendered canvas."
    PLUGIN_IS_A_SYNTH         FALSE
    PLUGIN_WANTS_MIDI_INPUT   FALSE
    PLUGIN_NEEDS_MIDI_OUTPUT  FALSE

    FORMATS VST3 AU Standalone

    PLUGIN_CHANNEL_CONFIGS "0 1" "1 1" "2 2"

    DEFINITIONS
        JUCE_WEB_BROWSER=0
)

juce_generate_juce_header(Drawdio)

target_sources(Drawdio PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UnifiedPedalProcessor.cpp
    Source/CompilerThread.cpp
    Source/CanvasMessageQueue.cpp
    Source/PixelCanvasComponent.cpp
    Source/PenDebouncer.cpp
    Source/CompilerEngine.cpp
    Source/PedalComponent.cpp
    Source/PedalSkinManager.cpp
    Source/PedalboardGrid.cpp
    Source/DspState.cpp
    Source/HeadlessDspTest.cpp
    Source/StateSerializer.cpp
    Source/DspGraph.cpp
    Source/Effects/GranularBaseEffect.cpp
    Source/Effects/DistortionEffects.cpp
    Source/Effects/FilterEffects.cpp
    Source/Effects/DelayEffects.cpp
    Source/Effects/ReverbEffects.cpp
    Source/Effects/PitchEffects.cpp
    Source/Effects/MiscEffects.cpp
    Source/RenderUtils.cpp
    Source/ResourceManager.cpp
    Source/ThemeManager.cpp
    Source/PedalDefinition.cpp
    Source/CanvasRoutingManager.cpp
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
)

target_compile_definitions(Drawdio PRIVATE
    JucePlugin_EditorWidth=1400
    JucePlugin_EditorHeight=800
)
