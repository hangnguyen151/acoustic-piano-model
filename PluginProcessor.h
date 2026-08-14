#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PianoVoice.h"
#include "PedalResonanceEngine.h"
#include "ConvolutionReverbUnit.h"

/**
    AcousticPianoModelProcessor
    ----------------------------
    AudioProcessor chính của plugin. Quản lý:
      - Một pool PianoVoice (tối đa maxVoices), cấp phát theo MIDI note-on
        (voice-stealing đơn giản: cướp voice cũ nhất khi hết voice rảnh).
      - PedalResonanceEngine dùng chung cho mọi voice (cộng hưởng sustain).
      - ConvolutionReverbUnit ở cuối chuỗi tín hiệu (master bus).
      - AudioProcessorValueTreeState chứa toàn bộ tham số nhạc cụ có thể
        tự động hóa (automatable) từ DAW: hammer hardness, damping,
        sympathetic resonance amount, reverb mix/size, output gain.

    Vòng lặp xử lý audio tuân thủ đúng thứ tự bắt buộc để cộng hưởng bàn
    đạp hoạt động chính xác (xem PedalResonanceEngine):
        for mỗi sample trong block:
            pedalEngine.beginSample()
            for mỗi voice: voice.processOneSample()  // tự accumulate()
            pedalEngine.finishSample()
        rồi đưa toàn bộ block qua ConvolutionReverbUnit.
*/
class AcousticPianoModelProcessor : public juce::AudioProcessor
{
public:
    AcousticPianoModelProcessor();
    ~AcousticPianoModelProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Acoustic Piano Model"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void handleMidiEvent (const juce::MidiMessage& msg);
    PianoVoice* findFreeVoice (int midiNoteNumber);

    static constexpr int maxVoices = 24;

    PedalResonanceEngine pedalEngine;
    std::vector<std::unique_ptr<PianoVoice>> voices;
    ConvolutionReverbUnit reverb;

    double currentSampleRate = 44100.0;
    juce::AudioBuffer<float> voiceMixBuffer;

    // Cached parameter pointers (đọc mỗi block để tránh tra cứu theo tên liên tục)
    std::atomic<float>* hammerHardnessParam = nullptr;
    std::atomic<float>* sympatheticAmountParam = nullptr;
    std::atomic<float>* reverbMixParam = nullptr;
    std::atomic<float>* reverbSizeParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* unisonDetuneParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcousticPianoModelProcessor)
};
