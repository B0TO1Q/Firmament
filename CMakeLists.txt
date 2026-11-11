cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple silicon + Intel)")

project(Firmament VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded automatically. To use a local copy instead:
#   cmake -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE ...
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(FIRMAMENT_FORMATS VST3 AU Standalone)
else()
    set(FIRMAMENT_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Firmament
    COMPANY_NAME "DeFab Labs"
    BUNDLE_ID com.defablabs.firmament
    PLUGIN_MANUFACTURER_CODE Dfab
    PLUGIN_CODE Frmt
    FORMATS ${FIRMAMENT_FORMATS}
    PRODUCT_NAME "Firmament"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Reverb
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(Firmament PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Firmament PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Firmament
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Engine-only test (no JUCE): cmake --build build --target engine_test
add_executable(engine_test EXCLUDE_FROM_ALL Tests/engine_test.cpp)
