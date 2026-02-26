cmake_minimum_required(VERSION 3.22)

project(MyPlugin VERSION 0.0.1 LANGUAGES C CXX)

# Use C++20 for modern DSP features (e.g., std::numbers, spans)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/JUCE)

juce_add_binary_data(MyAssets 
SOURCES
"Source/ui/index.html"
"Source/ui/juce/index.js"
"Source/ui/juce/check_native_interop.js"
)

# --- Plugin Configuration ---
juce_add_plugin(${PROJECT_NAME}
COMPANY_NAME "KenMusic"
IS_SYNTH FALSE
NEEDS_MIDI_INPUT FALSE
NEEDS_MIDI_OUTPUT FALSE
FORMATS VST3 Standalone
NEEDS_WEBVIEW2 TRUE 
)

# Define source and header locations
target_sources(${PROJECT_NAME} PRIVATE
Source/src/PluginProcessor.cpp
Source/src/PluginEditor.cpp
Source/inc/PluginProcessor.h
Source/inc/PluginEditor.h
)

# Add the inc directory to search paths
target_include_directories(${PROJECT_NAME} PRIVATE Source/inc)

# Skip VST2 compatibility check to avoid compile errors
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
)

# Link essential JUCE modules
target_link_libraries(${PROJECT_NAME} PRIVATE
    MyAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_dsp
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)