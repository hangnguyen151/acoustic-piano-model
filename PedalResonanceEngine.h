#pragma once
#include <juce_dsp/juce_dsp.h>
#include "StringModel.h"
#include <vector>
#include <array>

/**
    PedalResonanceEngine
    ---------------------
    Mô phỏng hiệu ứng "sustain pedal resonance" (cộng hưởng bàn đạp) của
    piano thật: khi bàn đạp sustain (damper pedal, CC64) được giữ, TẤT CẢ
    damper của toàn bộ 88 dây được nhấc khỏi dây cùng lúc - không chỉ dây
    của các nốt đang được đánh. Điều này cho phép các dây không được đánh
    trực tiếp vẫn dao động cộng hưởng (sympathetic vibration) theo năng
    lượng lan truyền qua ngựa đàn (bridge) và bàn cộng hưởng (soundboard).

    Kiến trúc:
    - Mỗi StringModel khi damper được nhấc (damperLifted == true) sẽ đóng
      góp sample đầu ra của nó vào "resonance bus" chung của engine này.
    - Resonance bus được đưa qua một mạng lọc cộng hưởng nhỏ (một chuỗi
      bộ lọc bandpass mô phỏng các mode dao động chính của bàn cộng hưởng
      gỗ - soundboard modal resonances) để tạo màu âm đặc trưng của
      "pedal resonance" (hơi ù, dày, có các đỉnh cộng hưởng tần thấp).
    - Kết quả feedback được chia lại (queryResonanceInput) cho từng
      StringModel ở sample kế tiếp, tạo vòng lặp năng lượng liên-dây
      giống cơ chế vật lý thật, nhưng với hệ số ghép nối (coupling gain)
      đủ nhỏ để hệ thống ổn định (không tự dao động/hú).
*/
class PedalResonanceEngine
{
public:
    void prepare (double sampleRate);

    /** Bàn đạp sustain toàn cục - ảnh hưởng tới mọi dây đã đăng ký. */
    void setSustainPedalDown (bool isDown) { sustainDown = isDown; }
    bool isSustainDown() const noexcept    { return sustainDown; }

    /** Hệ số điều khiển từ người dùng (tham số "Sympathetic Resonance" trên GUI),
        0 = tắt hẳn cộng hưởng liên dây, 1 = cường độ mặc định đầy đủ. */
    void setResonanceAmount (float amount01) { userAmount = juce::jlimit (0.0f, 1.5f, amount01); }

    /** Gọi mỗi khối audio (hoặc mỗi sample) BƯỚC 1: thu năng lượng mà mỗi
        dây vừa tạo ra ở sample trước, để tính resonance bus cho sample này. */
    void beginSample() noexcept;

    /** Mỗi StringModel gọi hàm này sau khi tính processSample() của chính nó
        để đóng góp năng lượng vào bus cộng hưởng chung. */
    void accumulate (float stringOutput, bool damperLifted) noexcept;

    /** Sau khi mọi dây đã accumulate() cho sample hiện tại, gọi hàm này để
        chạy bộ lọc soundboard-mode và chuẩn bị giá trị feedback cho sample kế. */
    void finishSample() noexcept;

    /** StringModel gọi hàm này để lấy tín hiệu cộng hưởng cần bơm vào chính nó. */
    float getResonanceFeedback() const noexcept { return feedbackValue; }

    /** Tín hiệu "wash" (màu âm bàn cộng hưởng) có thể được cộng thêm trực
        tiếp vào output bus tổng, tạo hiệu ứng "sàn âm thanh" đặc trưng khi
        đạp pedal (giống chế độ "resonance body" của Pianoteq). */
    float getSoundboardWashSample() const noexcept { return soundboardWash; }

private:
    double sampleRate = 44100.0;
    bool sustainDown = false;
    float userAmount = 0.7f;

    float accumulatedThisSample = 0.0f;
    float feedbackValue = 0.0f;
    float soundboardWash = 0.0f;

    // Chuỗi bộ lọc bandpass mô phỏng các mode cộng hưởng chính của soundboard
    // (tần số tham khảo gần đúng với các mode thấp của bàn cộng hưởng piano
    // grand cỡ lớn).
    static constexpr int numModes = 4;
    std::array<juce::dsp::StateVariableTPTFilter<float>, numModes> modeFilters;
    std::array<float, numModes> modeGains { 1.0f, 0.7f, 0.5f, 0.35f };
};
