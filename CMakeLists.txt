cmake_minimum_required(VERSION 3.22)

# Đặt phiên bản macOS tối thiểu TRƯỚC dòng project() để có hiệu lực đầy đủ.
# 10.15 (Catalina, phát hành 2019) đảm bảo tương thích với hầu hết máy Mac
# đang dùng hiện nay, kể cả các máy đời cũ hơn chưa nâng cấp lên phiên bản
# macOS mới nhất. Build ra bản "universal" chạy được cả trên chip Intel
# (x86_64) và Apple Silicon (arm64).
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures")
endif()

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
    COPY_PLUGIN_AFTER_BUILD TRUE
    # macOS yêu cầu khai báo lý do xin quyền truy cập Bluetooth (dùng để dò
    # tìm bàn phím MIDI không dây) và Microphone; thiếu dòng này app sẽ bị
    # hệ điều hành tự đóng ngay khi khởi động (TCC crash).
    PLIST_TO_MERGE "<key>NSBluetoothAlwaysUsageDescription</key><string>Ứng dụng dùng Bluetooth để dò tìm bàn phím MIDI không dây (nếu có).</string><key>NSBluetoothPeripheralUsageDescription</key><string>Ứng dụng dùng Bluetooth để dò tìm bàn phím MIDI không dây (nếu có).</string><key>NSMicrophoneUsageDescription</key><string>Ứng dụng có thể xử lý tín hiệu audio đầu vào nếu người dùng bật.</string>")

juce_generate_juce_header(AcousticPianoModel)

target_sources(AcousticPianoModel PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    PianoVoice.cpp
    PianoVoice.h
    StringModel.cpp
    StringModel.h
    HammerModel.cpp
    HammerModel.h
    DamperModel.cpp
    DamperModel.h
    PedalResonanceEngine.cpp
    PedalResonanceEngine.h
    ConvolutionReverbUnit.cpp
    ConvolutionReverbUnit.h)

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
