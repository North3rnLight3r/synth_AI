cmake_minimum_required(VERSION 3.15)
project(ForgeSynth VERSION 1.0.0)

# JUCE integration
find_package(JUCE CONFIG REQUIRED)

juce_add_plugin(ForgeSynth
    PRODUCT_NAME "ForgeSynth"
    PLUGIN_MANUFACTURER "FORGE"
    IS_SYNTH TRUE
    PLUGIN_CODE "FSyn"
    FORMATS VST3 AU Standalone
)

target_sources(ForgeSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/NeuralEngine.h
)

target_link_libraries(ForgeSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_opengl
)
