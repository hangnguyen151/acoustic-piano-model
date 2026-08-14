#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "StringModel.h"
#include "HammerModel.h"
#include "DamperModel.h"
#include "PedalResonanceEngine.h"
#include <array>

/**
    PianoVoice
    ----------
    Một giọng đàn = một dây đàn vật lý (hoặc nhiều dây unison) + búa
    (HammerModel) + damper (DamperModel).

    LƯU Ý KIẾN TRÚC: lớp này KHÔNG kế thừa juce::SynthesiserVoice, vì
    juce::Synthesiser xử lý trọn 1 khối audio cho từng voice một
    (voice A render hết block rồi mới tới voice B). Điều đó không phù hợp
    với PedalResonanceEngine, vốn cần thu năng lượng từ TẤT CẢ voice đang
    kêu tại CÙNG một sample-time để tính cộng hưởng bàn đạp chính xác.

    Vì vậy PluginProcessor sẽ tự quản lý một mảng PianoVoice và gọi
    processOneSample() cho từng voice, đúng thứ tự: với mỗi sample ->
    beginSample() -> mọi voice xử lý (tự accumulate vào engine) ->
    finishSample() -> lấy feedback cho sample kế tiếp.
*/
class PianoVoice
{
public:
    explicit PianoVoice (PedalResonanceEngine& sharedPedalEngine);

    void prepare (double sampleRate, float inharmonicityCoeffB);

    void startNote (int midiNoteNumber, float velocity);
    void stopNote (float releaseVelocity);

    /** Xử lý đúng 1 sample. Phải được gọi sau pedalEngine.beginSample() của
        block hiện tại và trước pedalEngine.finishSample(). Tự động gọi
        pedalEngine.accumulate() bên trong. Trả về sample âm thanh mono. */
    float processOneSample() noexcept;

    bool isVoiceActive() const noexcept;
    int getCurrentMidiNote() const noexcept { return currentMidiNote; }
    bool isNoteHeldDown() const noexcept    { return noteIsOn; }

    void setHammerHardness (float h01)      { hammerHardness = h01; }
    void setUnisonDetuneCents (float cents) { unisonDetuneCents = cents; }

private:
    static constexpr int maxUnisonStrings = 3;

    std::array<StringModel, maxUnisonStrings> strings;
    int numUnisonStrings = 2;

    HammerModel hammer;
    DamperModel damper;
    PedalResonanceEngine& pedalEngine;

    double sampleRate = 44100.0;
    float hammerHardness = 0.5f;
    float unisonDetuneCents = 3.5f;
    float noteVelocity = 0.0f;
    int currentMidiNote = -1;
    bool noteIsOn = false;
};
