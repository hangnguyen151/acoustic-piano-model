#pragma once
#include <juce_dsp/juce_dsp.h>

/**
    DamperModel
    -----------
    Mô phỏng các âm thanh cơ học phụ (mechanical noises) đến từ bộ phận
    damper (nỉ hãm dây) của piano thật - đây là những chi tiết nhỏ nhưng
    quan trọng để âm thanh nghe "thật" như Pianoteq, thay vì chỉ có âm
    dây thuần túy:

    1) Damper lift noise: khi nhấn phím, damper nhấc khỏi dây tạo tiếng
       "sượt" nhẹ (nỉ ma sát với dây khi rời ra).
    2) Damper drop / key-off noise: khi nhả phím (và pedal không giữ),
       damper rơi trở lại tiếp xúc dây, tạo một tiếng "thụp" đục kèm
       nhiễu tần số trung-cao rất ngắn, đồng thời cắt nhanh biên độ dây
       (được xử lý bởi StringModel::setDamperLifted).
    3) Key-off "thud": tiếng gõ cơ học của phím khi trả về vị trí nghỉ,
       một xung biên độ thấp, tần số thấp, rất ngắn.

    Tất cả được tổng hợp từ nhiễu trắng lọc qua bộ lọc băng thông
    (bandpass) rồi nhân với bao hình (envelope) suy giảm theo hàm mũ.
*/
class DamperModel
{
public:
    void prepare (double sampleRate, double stringFrequencyHz);

    /** Kích hoạt tiếng nhấc damper (lúc bắt đầu nốt), tỉ lệ theo velocity. */
    void triggerLiftNoise (float velocity);

    /** Kích hoạt tiếng rơi damper + key-off thud (lúc nhả phím). */
    void triggerDropNoise (float releaseVelocity);

    /** Trả về sample nhiễu cơ học tiếp theo, cộng thẳng vào tín hiệu ra. */
    float getNextSample() noexcept;

    bool isActive() const noexcept { return liftEnvelope > 1.0e-4f || dropEnvelope > 1.0e-4f; }

private:
    double sampleRate = 44100.0;

    // Bộ lọc bandpass (dùng cấu trúc State Variable Filter của JUCE dsp)
    juce::dsp::StateVariableTPTFilter<float> liftNoiseFilter;
    juce::dsp::StateVariableTPTFilter<float> dropNoiseFilter;
    juce::dsp::StateVariableTPTFilter<float> thudFilter; // tiếng thụp tần số thấp

    juce::Random rng;

    float liftEnvelope = 0.0f, liftDecay = 0.999f, liftGain = 0.0f;
    float dropEnvelope = 0.0f, dropDecay = 0.999f, dropGain = 0.0f;
    float thudEnvelope = 0.0f, thudDecay = 0.999f, thudGain = 0.0f;
};
