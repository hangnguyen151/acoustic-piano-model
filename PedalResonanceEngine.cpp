#include "PedalResonanceEngine.h"

void PedalResonanceEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };

    // Tần số các mode cộng hưởng chính của bàn cộng hưởng (soundboard modes),
    // xấp xỉ theo dữ liệu đo đạc thực tế trên đàn grand piano cỡ lớn.
    static constexpr std::array<float, numModes> modeFreqs { 85.0f, 165.0f, 330.0f, 660.0f };

    for (int i = 0; i < numModes; ++i)
    {
        modeFilters[(size_t) i].prepare (spec);
        modeFilters[(size_t) i].setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        modeFilters[(size_t) i].setCutoffFrequency (modeFreqs[(size_t) i]);
        modeFilters[(size_t) i].setResonance (0.85f); // Q cao -> cộng hưởng rõ, "ù" đặc trưng
    }

    accumulatedThisSample = 0.0f;
    feedbackValue = 0.0f;
    soundboardWash = 0.0f;
}

void PedalResonanceEngine::beginSample() noexcept
{
    accumulatedThisSample = 0.0f;
}

void PedalResonanceEngine::accumulate (float stringOutput, bool damperLifted) noexcept
{
    // Chỉ những dây có damper đang nhấc (đang được đánh, hoặc được nhấc do
    // sustain pedal giữ) mới góp năng lượng vào cộng hưởng chung - đúng bản
    // chất vật lý: dây bị damper chặn thì gần như không dao động/không phát ra.
    if (damperLifted)
        accumulatedThisSample += stringOutput;
}

void PedalResonanceEngine::finishSample() noexcept
{
    if (! sustainDown)
    {
        // Không giữ pedal: vẫn cho phép cộng hưởng nhẹ giữa các dây đang vang
        // (piano thật cũng có "ghép chéo" nhẹ qua ngựa đàn ngay cả khi không
        // đạp pedal), nhưng hệ số nhỏ hơn nhiều.
        feedbackValue = accumulatedThisSample * 0.02f * userAmount;
        soundboardWash *= 0.995f;
        return;
    }

    // Đưa tổng năng lượng qua mạng lọc mode cộng hưởng của soundboard.
    float wash = 0.0f;
    for (int i = 0; i < numModes; ++i)
        wash += modeFilters[(size_t) i].processSample (0, accumulatedThisSample) * modeGains[(size_t) i];

    soundboardWash = wash * 0.25f * userAmount;

    // Hệ số hồi tiếp (feedback gain) được giữ nhỏ (<< 1) để đảm bảo ổn định
    // (không tự kích/hú); giá trị phụ thuộc số dây đang góp mặt được chuẩn
    // hoá gần đúng bằng hệ số cố định nhỏ, nhân thêm hệ số người dùng.
    feedbackValue = wash * 0.06f * userAmount;
}
