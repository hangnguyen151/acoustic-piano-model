cmake_minimum_required(VERSION 3.22)

project(AcousticPianoModel VERSION 1.0.0)

# ---------------------------------------------------------------------------
# This project expects the JUCE framework to be available either as a
# sibling folder ("../JUCE") or installed and discoverable via find_package.
# Get JUCE from https://github.com/juce-framework/JUCE (tag 7.x or 8.x).
# ---------------------------------------------------------------------------
if (NOT TARGET juce::juce_audio_utils)
    if (DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
        add_subdirectory(${JUCE_PATH} JUCE)
    elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)
    elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE)
    else()
        find_package(JUCE CONFIG REQUIRED)
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

juce_add_plugin(AcousticPianoModel
    COMPANY_NAME "YourCompany"
    PLUGIN_MANUFACTURER_CODE Ymcy
    PLUGIN_CODE Apmd
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Acoustic Piano Model"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(AcousticPianoModel)

target_sources(AcousticPianoModel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PianoVoice.cpp
    Source/PianoVoice.h
    Source/DSP/StringModel.cpp
    Source/DSP/StringModel.h
    Source/DSP/HammerModel.cpp
    Source/DSP/HammerModel.h
    Source/DSP/DamperModel.cpp
    Source/DSP/DamperModel.h
    Source/DSP/PedalResonanceEngine.cpp
    Source/DSP/PedalResonanceEngine.h
    Source/DSP/ConvolutionReverbUnit.cpp
    Source/DSP/ConvolutionReverbUnit.h)

target_compile_definitions(AcousticPianoModel PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AcousticPianoModel PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
