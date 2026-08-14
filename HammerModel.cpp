#include "HammerModel.h"

void HammerModel::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

void HammerModel::trigger (float velocity, float hardness)
{
    velocity = juce::jlimit (0.0f, 1.0f, velocity);
    hardness = juce::jlimit (0.0f, 1.0f, hardness);

    sampleIndex = 0;
    amplitude = velocity * velocity; // năng lượng va chạm ~ v^2

    // Búa cứng hơn -> hệ số phi tuyến p lớn hơn -> xung nhọn hơn.
    shapeExponent = juce::jmap (hardness, 0.0f, 1.0f, 1.6f, 3.8f);

    // Thời gian tiếp xúc búa-dây: búa mềm tiếp xúc lâu hơn (vài ms),
    // búa cứng tiếp xúc rất ngắn (~0.5-1.5ms), và đánh mạnh cũng rút ngắn
    // thời gian tiếp xúc (giống hành vi vật lý thật).
    float contactMs = juce::jmap (hardness, 0.0f, 1.0f, 4.5f, 0.8f)
                       * juce::jmap (velocity, 0.0f, 1.0f, 1.15f, 0.85f);
    contactLengthSamples = juce::jmax (8, (int) std::round (contactMs * 0.001 * sampleRate));

    // Búa cứng -> lọc thông thấp mở rộng hơn (giữ nhiều harmonics cao) ->
    // lpCoeff gần 1 nghĩa là ít làm mượt.
    lpCoeff = juce::jmap (hardness, 0.0f, 1.0f, 0.15f, 0.85f);
    lpState = 0.0f;
}

float HammerModel::getNextSample() noexcept
{
    if (sampleIndex >= contactLengthSamples)
        return 0.0f;

    // Vị trí chuẩn hóa trong cửa sổ tiếp xúc, từ 0 -> 1
    float t = (float) sampleIndex / (float) juce::jmax (1, contactLengthSamples - 1);

    // Hình dạng xung: nửa sin nâng lên lũy thừa p, mô phỏng F(x) = K*x^p
    // dọc theo quá trình nén rồi nhả của nỉ búa (raised-sine đối xứng).
    float raisedSine = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * t));
    float shaped = std::pow (raisedSine, shapeExponent);

    float force = amplitude * shaped;

    // Lọc thông thấp một cực để bo tròn xung theo độ cứng búa.
    lpState += lpCoeff * (force - lpState);

    ++sampleIndex;
    return lpState;
}
