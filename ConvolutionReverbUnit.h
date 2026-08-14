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
*/
class ConvolutionReverbUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);

    /** Sinh và nạp một IR giả lập phòng hòa nhạc; sizeSeconds ~1.5-4.5s,
        damping 0..1 điều khiển tốc độ hấp thụ tần cao theo thời gian. */
    void generateAlgorithmicIR (float sizeSeconds, float damping, double sampleRate);

    /** Nạp IR từ file .wav thật (tùy chọn), nếu người dùng có sẵn thư viện IR. */
    void loadImpulseResponseFromFile (const juce::File& wavFile);

    void setMix (float wetAmount01) { mix = juce::jlimit (0.0f, 1.0f, wetAmount01); }
    void setSizeAndDamping (float sizeSeconds, float damping, double sampleRate);

    void process (juce::dsp::AudioBlock<float>& block);

    void reset() { convolution.reset(); }

private:
    juce::dsp::Convolution convolution;
    juce::dsp::ProcessSpec currentSpec {};
    float mix = 0.28f;

    // Buffer khô để trộn dry/wet thủ công (Convolution engine của JUCE xử lý
    // in-place theo block, nên ta giữ bản sao dry trước khi convolve).
    juce::AudioBuffer<float> dryBuffer;
};
