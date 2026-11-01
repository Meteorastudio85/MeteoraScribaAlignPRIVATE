cmake_minimum_required(VERSION 3.22)
project(ScribaAlign VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# Nella DAW appare come "Scriba Align (Meteora Plugins)", come Scriba
juce_add_plugin(ScribaAlign
    COMPANY_NAME               "Meteora Plugins"
    COMPANY_EMAIL              "meteora_plugins@aol.com"
    PLUGIN_MANUFACTURER_CODE   Mtra          # usa lo stesso codice di Scriba
    PLUGIN_CODE                Scal
    FORMATS                    VST3 AU
    PRODUCT_NAME               "Scriba Align"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            Fx Tools
    AU_MAIN_TYPE               kAudioUnitType_Effect)

juce_add_binary_data(ScribaAlignAssets SOURCES Resources/logo.png)

target_sources(ScribaAlign PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AlignRegistry.cpp
    Source/PhaseAnalyzer.cpp)

target_compile_definitions(ScribaAlign PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ScribaAlign
    PRIVATE
        ScribaAlignAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
