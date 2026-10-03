cmake_minimum_required(VERSION 3.22)
project(ForgeSynth VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_ROOT "" CACHE PATH "Optional path to an existing JUCE checkout")
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS version")
endif()
option(FORGE_FETCH_JUCE "Download pinned JUCE when JUCE_ROOT is unset" ON)
if(JUCE_ROOT)
    add_subdirectory("${JUCE_ROOT}" "${CMAKE_BINARY_DIR}/juce")
elseif(FORGE_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 91ad83ae34a81e0833b1a2b0866f54846370ae53) # 8.0.15
    FetchContent_MakeAvailable(JUCE)
else()
    find_package(JUCE 8 CONFIG REQUIRED)
endif()
set(FORGE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FORGE_FORMATS AU)
endif()
juce_add_plugin(ForgeSynth
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "FORGE"
    BUNDLE_ID "audio.forge.synth"
    PLUGIN_MANUFACTURER_CODE Frge
    PLUGIN_CODE Fsyn
    PRODUCT_NAME "FORGE Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS ${FORGE_FORMATS})
juce_generate_juce_header(ForgeSynth)
target_sources(ForgeSynth PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(ForgeSynth PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=0)
target_link_libraries(ForgeSynth PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
include(CTest)
if(BUILD_TESTING)
    juce_add_console_app(ForgeSynthTests PRODUCT_NAME "FORGE Synth Tests")
    juce_generate_juce_header(ForgeSynthTests)
    target_sources(ForgeSynthTests PRIVATE ../../tests/PluginTests.cpp)
    target_include_directories(ForgeSynthTests PRIVATE Source)
    target_link_libraries(ForgeSynthTests PRIVATE ForgeSynth juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME plugin_regression COMMAND ForgeSynthTests)
endif()
