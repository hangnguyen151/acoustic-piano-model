#pragma once
#include <juce_dsp/juce_dsp.h>

/**
    ConvolutionReverbUnit
    ----------------------
    Convolution reverb chất lượng cao đặt ở cuối chuỗi xử lý (master output
    bus), dùng juce::dsp::Convolution (partitioned FFT convolution, hiệu
    năng cao, độ trễ thấp phù hợp real-time).

    Vì plugin cần tự chứa (self-contained, không phụ thuộc file .wav bên
    ngoài), impulse response (IR) được TỰ SINH bằng thuật toán algorithmic
    IR generation mô phỏng một phòng hòa nhạc (concert hall) tự nhiên:
    nhiễu trắng được định hình bởi bao hình suy giảm mũ (exponential decay),
    lọc theo dải tần (early reflections dày đặc tần cao, đuôi vang tần thấp
    kéo dài hơn - giống hấp thụ âm học tự nhiên của phòng), cộng thêm một
    cụm early-reflections rời rạc mô phỏng phản xạ sớm từ tường/trần.

    Người dùng có thể thay bằng loadImpulseResponseFromFile() để nạp IR
    thật (ví dụ đo từ phòng hòa nhạc) nếu có sẵn.

    Ngoài reverb, lớp này còn có một bộ "Piano Tone Shaper" - 3 bộ lọc EQ
    (low-shelf ấm + peak trong trẻo + high-shelf giảm chói) đặt trên toàn
    bộ master bus (cả tín hiệu khô lẫn đuôi vang), mô phỏng màu âm mà bàn
    cộng hưởng (soundboard) và microphone thu âm piano thật tạo ra. Đây là
    yếu tố quan trọng giúp âm nghe "ấm, tròn" thay vì tiếng dây trần trụi
    kiểu synth/waveguide thô.
*/
class ConvolutionReverbUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);

    void generateAlgorithmicIR (float sizeSeconds, float damping, double sampleRate);

    void loadImpulseResponseFromFile (const juce::File& wavFile);

    void setMix (float wetAmount01) { mix = juce::jlimit (0.0f, 1.0f, wetAmount01); }
    void setSizeAndDamping (float sizeSeconds, float damping, double sampleRate);

    void process (juce::dsp::AudioBlock<float>& block);

    void reset() { convolution.reset(); }

private:
    void updateToneShaperCoefficients (double sampleRate);
    void processToneShaper (juce::dsp::AudioBlock<float>& block) noexcept;

    juce::dsp::Convolution convolution;
    juce::dsp::ProcessSpec currentSpec {};
    float mix = 0.28f;

    juce::AudioBuffer<float> dryBuffer;

    static constexpr int maxToneChannels = 2;
    static constexpr int numToneBands = 3;
    std::array<std::array<juce::dsp::IIR::Filter<float>, numToneBands>, maxToneChannels> toneFilters;
    juce::dsp::IIR::Coefficients<float>::Ptr lowShelfCoeffs, presencePeakCoeffs, highShelfCoeffs;
};
