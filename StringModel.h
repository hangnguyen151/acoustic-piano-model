#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

/**
    StringModel
    -----------
    Mô phỏng dây đàn piano bằng phương pháp "Digital Waveguide Synthesis"
    (Julius O. Smith), là nghiệm số của phương trình sóng 1 chiều có tổn hao
    và độ cứng (stiff, damped wave equation):

        y_tt = c^2 * y_xx  -  2*b1*y_t  +  b3*y_xxt  -  kappa^2 * y_xxxx

    Thay vì giải PDE trực tiếp bằng sai phân hữu hạn (tốn CPU), ta dùng
    nghiệm D'Alembert: y(x,t) = y+(t - x/c) + y-(t + x/c), tức tổng của
    một sóng truyền phải (rightGoing) và một sóng truyền trái (leftGoing),
    mỗi sóng được lưu trong một delay-line vòng (circular buffer).

    - Độ dài delay-line quyết định tần số cơ bản (f0 = SR / (2*N)).
    - Tổn hao năng lượng (ma sát không khí, hấp thụ tại ngựa đàn) được mô
      phỏng bằng một bộ lọc thông thấp một cực (loss filter) đặt trong
      vòng lặp phản hồi.
    - Độ cứng dây thép (stiffness) gây ra "inharmonicity" (các họa âm
      không đúng bội số nguyên) được mô phỏng bằng chuỗi bộ lọc allpass
      bậc nhất (dispersion filter), điều khiển bởi hệ số B trong công thức
      Fletcher: f_n = n*f0*sqrt(1 + B*n^2).
    - Đầu vào kích thích (hammer excitation) được "commuted" (giao hoán)
      và bơm vào cả hai delay-line tại vị trí tương ứng với điểm búa gõ.
*/
class StringModel
{
public:
    StringModel() = default;

    /** Chuẩn bị dây đàn cho một cao độ MIDI cụ thể. */
    void prepare (double sampleRate, double midiNoteFrequencyHz, float inharmonicityCoeffB);

    /** Búa gõ vào dây: velocity 0..1, hardness 0..1 (0 = búa mềm, 1 = búa rất cứng),
        strikePosition 0..1 (vị trí gõ dọc theo dây, ~1/8 chiều dài với đàn thật). */
    void hammerStrike (float velocity, float hardness, float strikePosition);

    /** Nhả phím: nếu pedal không giữ, damper sẽ rơi xuống dây và tắt âm dần. */
    void keyReleased (float releaseVelocity);

    /** Bàn đạp sustain (damper pedal) được nhấn/nhả cho riêng dây này
        (được điều khiển tập trung từ PedalResonanceEngine). */
    void setDamperLifted (bool lifted);

    /** Bơm 1 sample lực búa (đến từ HammerModel) vào dây, tách thành hai
        sóng lan truyền ngược chiều nhau với biên độ bằng nhau (nguyên lý
        chồng chập - superposition - của phương trình sóng tuyến tính). */
    void injectExcitation (float forceSample) noexcept;

    /** Xử lý 1 sample. sympatheticInput là năng lượng bơm vào từ các dây khác
        (cộng hưởng qua bàn cộng hưởng khi pedal giữ). Trả về sample đầu ra tại
        điểm "pickup" (tương đương vị trí ngựa đàn / bridge pickup). */
    float processSample (float sympatheticInput) noexcept;

    bool isActive() const noexcept   { return active; }
    float getFrequencyHz() const     { return frequencyHz; }

    /** Dùng bởi PedalResonanceEngine để quyết định có cộng dồn dây này vào
        bus cộng hưởng chung hay không (chỉ các dây có damper đang nhấc). */
    bool canResonateSympathetically() const noexcept { return damperLifted; }

private:
    void updateLossFilterCoefficient();
    void updateDispersionFilters (float B);
    float readDelayLine (const std::vector<float>& line, int writePos, float delaySamples) const;

    double sampleRate = 44100.0;
    double frequencyHz = 440.0;

    // --- Waveguide delay lines (traveling-wave decomposition of y(x,t)) ---
    std::vector<float> rightGoing;   // sóng truyền từ búa -> ngựa đàn (bridge)
    std::vector<float> leftGoing;    // sóng truyền từ búa -> nut/agraffe
    int lineLength = 100;
    int writeR = 0, writeL = 0;
    float fracDelay = 0.0f;          // phần thập phân của độ trễ, để chỉnh cao độ chính xác
    float allpassFracState = 0.0f;   // trạng thái bộ nội suy allpass phân số

    // --- Loss (energy dissipation term of the wave equation) ---
    float lossCoeff = 0.998f;        // hệ số lọc thông thấp 1 cực mô phỏng ma sát
    float lossFilterStateR = 0.0f, lossFilterStateL = 0.0f;
    float baseLossCoeff = 0.998f;

    // --- Dispersion filters (stiffness term -> inharmonicity) ---
    static constexpr int numDispersionStages = 6;
    struct AllpassStage { float a = 0.0f; float x1 = 0.0f, y1 = 0.0f; };
    std::array<AllpassStage, numDispersionStages> dispersionChain;

    // --- Warmth filter (giảm sắc cạnh số hóa, tăng cảm giác ấm) ---
    float warmthFilterState = 0.0f;
    float warmthCoeff = 0.6f;

    // --- Termination reflection coefficients ---
    float bridgeReflection = -0.996f; // ngựa đàn gần như cứng nhưng có rò rỉ năng lượng ra soundboard
    float nutReflection    = -0.999f; // đầu agraffe gần như phản xạ hoàn toàn

    // --- Excitation shaping ---
    int strikePosSamples = 8;
    float strikePositionRatio = 0.12f;

    // --- Envelope / damper state ---
    bool active = false;
    bool damperLifted = false;       // true khi phím đang giữ hoặc sustain pedal đang giữ
    float dampingTarget = 0.998f;    // hệ số lossCoeff mục tiêu (thay đổi mượt khi damper rơi)
    float dampingSmoothing = 0.0f;   // hệ số làm mượt chuyển tiếp khi nhả phím

    float outputLevel = 0.0f;        // theo dõi biên độ để biết khi nào tắt voice
};
