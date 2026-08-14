#pragma once
#include "PluginProcessor.h"

/**
    AcousticPianoModelEditor
    --------------------------
    Giao diện đơn giản: các slider điều khiển trực tiếp tham số nhạc cụ
    thông qua AudioProcessorValueTreeState::SliderAttachment (tự động đồng
    bộ hai chiều GUI <-> automation của DAW, không cần code thủ công).
*/
class AcousticPianoModelEditor : public juce::AudioProcessorEditor
{
public:
    explicit AcousticPianoModelEditor (AcousticPianoModelProcessor&);
    ~AcousticPianoModelEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AcousticPianoModelProcessor& processorRef;

    juce::Slider hammerHardnessSlider, sympatheticSlider, reverbMixSlider,
                 reverbSizeSlider, outputGainSlider, unisonDetuneSlider;

    juce::Label hammerHardnessLabel, sympatheticLabel, reverbMixLabel,
                reverbSizeLabel, outputGainLabel, unisonDetuneLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> hammerHardnessAttachment, sympatheticAttachment,
                                 reverbMixAttachment, reverbSizeAttachment,
                                 outputGainAttachment, unisonDetuneAttachment;

    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcousticPianoModelEditor)
};
