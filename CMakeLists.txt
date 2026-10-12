cmake_minimum_required(VERSION 3.22)
project(DrummerViz VERSION 0.2.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# the whole drummer page is embedded into the plugin
juce_add_binary_data(DrummerVizData SOURCES Source/drummer.html)

juce_add_plugin(DrummerViz
    COMPANY_NAME "DIY"
    PLUGIN_MANUFACTURER_CODE Diyz
    PLUGIN_CODE Dvz2
    FORMATS VST3 Standalone
    PRODUCT_NAME "Drummer Viz"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Analyzer
    NEEDS_WEBVIEW2 TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(DrummerViz PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DrummerViz PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)

target_link_libraries(DrummerViz PRIVATE
    DrummerVizData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
