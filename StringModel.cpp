#include "StringModel.h"

void StringModel::prepare (double newSampleRate, double midiNoteFrequencyHz, float inharmonicityCoeffB)
{
    sampleRate = newSampleRate;
    frequencyHz = midiNoteFrequencyHz;

    // Chiều dài delay-line (đơn vị: sample) sao cho vòng lặp full round-trip
    // (right + left) tương ứng đúng 1 chu kỳ của f0: N_total = SR / f0.
    // Mỗi delay-line (1 chiều) dài N_total / 2.
    double totalDelaySamples = sampleRate / frequencyHz;
    double oneWayDelay = totalDelaySamples * 0.5;

    lineLength = juce::jmax (4, (int) std::floor (oneWayDelay));
    fracDelay  = (float) (oneWayDelay - (double) lineLength);

    rightGoing.assign ((size_t) lineLength + 4, 0.0f);
    leftGoing.assign  ((size_t) lineLength + 4, 0.0f);
    writeR = writeL = 0;
    allpassFracState = 0.0f;

    // Tổn hao phụ thuộc tần số: dây trầm tổn hao chậm hơn (sustain dài),
    // dây cao tổn hao nhanh hơn (giống piano thật).
    baseLossCoeff = (float) juce::jmap (frequencyHz, 27.5, 4186.0, 0.99985, 0.9950);
    lossCoeff = baseLossCoeff;
    dampingTarget = baseLossCoeff;

    updateDispersionFilters (inharmonicityCoeffB);

    strikePositionRatio = 0.12f; // vị trí gõ búa mặc định (~1/8 chiều dài dây)
    active = false;
    damperLifted = false;
}

void StringModel::updateDispersionFilters (float B)
{
    // Mỗi tầng allpass bậc nhất trễ pha phi tuyến theo tần số, tổng hợp lại
    // xấp xỉ hiệu ứng "stiffness dispersion" của dây thép: các bội âm cao
    // lệch dần so với bội số nguyên của f0 theo hệ số B (Fletcher-Rossing).
    float a = juce::jlimit (-0.5f, 0.5f, B * 8.0f - 0.05f);
    for (auto& stage : dispersionChain)
    {
        stage.a = a;
        stage.x1 = stage.y1 = 0.0f;
    }
}

void StringModel::hammerStrike (float velocity, float hardness, float strikePosition)
{
    active = true;
    damperLifted = true;
    lossCoeff = baseLossCoeff;
    strikePositionRatio = juce::jlimit (0.02f, 0.5f, strikePosition);

    // Vị trí gõ búa dọc theo dây quyết định các họa âm bị triệt tiêu
    // (comb filtering tại vị trí đó) - búa gõ gần đầu dây -> âm sáng hơn.
    strikePosSamples = juce::jmax (1, (int) std::round (strikePositionRatio * (float) lineLength));

    // Xóa năng lượng cũ còn sót lại (nếu dây vừa được đánh lại trước khi tắt hẳn)
    // được giữ nguyên một phần để mô phỏng re-strike tự nhiên hơn là reset cứng.
    for (auto& v : rightGoing) v *= 0.15f;
    for (auto& v : leftGoing)  v *= 0.15f;
}

void StringModel::injectExcitation (float forceSample) noexcept
{
    // Đơn giản hóa hợp lý: bơm trực tiếp tại con trỏ ghi hiện tại thay vì mô
    // phỏng đầy đủ vị trí lan truyền vật lý của điểm gõ dọc theo dây; điều
    // này vẫn giữ đúng bản chất "commuted synthesis" (chồng chập 2 sóng nửa
    // biên độ ngược chiều) mà không cần buffer trễ vị trí riêng.
    rightGoing[(size_t) writeR] += forceSample * 0.5f;
    leftGoing[(size_t) writeL]  += forceSample * 0.5f;
}

void StringModel::keyReleased (float /*releaseVelocity*/)
{
    if (! damperLifted)
        return; // đã bị damper giữ (sustain đang tắt) rồi thì không làm gì thêm

    // Nếu sustain pedal không giữ, damper sẽ rơi thật (setDamperLifted(false)
    // được gọi từ PianoVoice/PedalResonanceEngine ngay sau lời gọi này).
}

void StringModel::setDamperLifted (bool lifted)
{
    damperLifted = lifted;
    // Khi damper rơi xuống: tăng tổn hao rất mạnh (mô phỏng nỉ damper ép vào dây)
    // -> âm tắt nhanh trong ~40-120ms tùy nốt.
    dampingTarget = lifted ? baseLossCoeff
                            : juce::jmap (frequencyHz, 27.5, 4186.0, 0.965, 0.85);
}

float StringModel::readDelayLine (const std::vector<float>& line, int writePos, float delaySamplesFrac) const
{
    // Nội suy tuyến tính đơn giản để đọc tại vị trí phân số trong delay-line.
    int len = (int) line.size();
    int base = writePos - 1;
    if (base < 0) base += len;
    float frac = delaySamplesFrac;
    int i0 = base;
    int i1 = (base + 1) % len;
    return line[(size_t) i0] * (1.0f - frac) + line[(size_t) i1] * frac;
}

float StringModel::processSample (float sympatheticInput) noexcept
{
    if (! active)
        return 0.0f;

    // --- Làm mượt hệ số damping khi damper đóng/mở (tránh click) ---
    lossCoeff += 0.002f * (dampingTarget - lossCoeff);

    int len = (int) rightGoing.size();

    // Đọc giá trị tại đầu ra hiện tại của mỗi delay-line
    int readR = (writeR + 1) % len;
    int readL = (writeL + 1) % len;
    float sampleR = rightGoing[(size_t) readR];
    float sampleL = leftGoing[(size_t) readL];

    // --- Bridge (ngựa đàn): sóng phải tới ngựa đàn, phản xạ thành sóng trái ---
    float lossedR = sampleR * lossCoeff + lossFilterStateR * (1.0f - lossCoeff);
    lossFilterStateR = lossedR;
    float reflectedAtBridge = lossedR * bridgeReflection;

    // --- Nut/agraffe: sóng trái tới đầu cố định, phản xạ thành sóng phải ---
    float lossedL = sampleL * lossCoeff + lossFilterStateL * (1.0f - lossCoeff);
    lossFilterStateL = lossedL;
    float reflectedAtNut = lossedL * nutReflection;

    // --- Dispersion filter chain áp cho sóng vừa phản xạ tại ngựa đàn ---
    float dispersed = reflectedAtBridge;
    for (auto& s : dispersionChain)
    {
        float y = -s.a * dispersed + s.x1 + s.a * s.y1;
        s.x1 = dispersed;
        s.y1 = y;
        dispersed = y;
    }

    // --- Bơm năng lượng cộng hưởng từ các dây khác (khi pedal giữ) ---
    float coupling = damperLifted ? sympatheticInput * 0.05f : 0.0f;

    // --- Ghi giá trị mới vào 2 đầu delay-line (điểm nối giữa 2 sóng) ---
    rightGoing[(size_t) writeR] = reflectedAtNut + coupling;
    leftGoing[(size_t) writeL]  = dispersed + coupling;

    writeR = (writeR + 1) % len;
    writeL = (writeL + 1) % len;

    // Đầu ra "pickup": tổng 2 sóng tại vị trí ngựa đàn, chính là y(x_bridge, t)
    float output = sampleR + sampleL;
    outputLevel = 0.999f * outputLevel + 0.001f * std::abs (output);

    if (outputLevel < 1.0e-5f && ! damperLifted)
        active = false; // voice đã tắt hẳn, có thể giải phóng

    return output;
}
