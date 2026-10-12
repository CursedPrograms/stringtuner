cmake_minimum_required(VERSION 3.22)
project(stringtuner VERSION 0.1.0)

# Build the C++ runtime into the exe / VST3, so they run on PCs without the Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(STRINGTUNER_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Instruments.cpp
    Source/PitchDetector.cpp
    Source/Tuner.cpp)

set(STRINGTUNER_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1                      # ASIO in the standalone app
    JUCE_VST3_CAN_REPLACE_VST2=0
    STRINGTUNER_VERSION="${PROJECT_VERSION}")

juce_add_plugin(stringtuner
    COMPANY_NAME "Cursed Entertainment"
    PLUGIN_MANUFACTURER_CODE Crsd
    PLUGIN_CODE StTn
    FORMATS VST3 Standalone
    PRODUCT_NAME "stringtuner"
    DESCRIPTION "Tuner for every string instrument, from octobass to violino piccolo"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(stringtuner PRIVATE ${STRINGTUNER_SOURCES})
target_compile_features(stringtuner PRIVATE cxx_std_17)
target_compile_definitions(stringtuner PUBLIC ${STRINGTUNER_DEFINITIONS})
target_link_libraries(stringtuner
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline test: pitch accuracy from C0 to C8 at several sample rates, plus GUI snapshots.
juce_add_console_app(stringtunerCheck PRODUCT_NAME "stringtunerCheck")
target_sources(stringtunerCheck PRIVATE tools/check.cpp ${STRINGTUNER_SOURCES})
target_compile_features(stringtunerCheck PRIVATE cxx_std_17)
target_compile_definitions(stringtunerCheck PRIVATE ${STRINGTUNER_DEFINITIONS} JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(stringtunerCheck
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
