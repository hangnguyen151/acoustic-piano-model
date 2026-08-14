#include "ConvolutionReverbUnit.h"

void ConvolutionReverbUnit::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSpec = spec;
    convolution.prepare (spec);
    dryBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
    generateAlgorithmicIR (2.6f, 0.45f, spec.sampleRate);
}

void ConvolutionReverbUnit::generateAlgorithmicIR (float sizeSeconds, float damping, double sampleRate)
{
    const int numSamples = juce::jmax (64, (int) std::round (sizeSeconds * sampleRate));
    const int numChannels = 2;

    juce::AudioBuffer<float> ir (numChannels, numSamples);
    ir.clear();

    juce::Random rng;
    damping = juce::jlimit (0.0f, 1.0f, damping);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = ir.getWritePointer (ch);

        // --- 1) Early reflections: cụm xung rời rạc trong ~80ms đầu, biên độ
        //         giảm dần, vị trí lệch nhẹ giữa 2 kênh để tạo độ rộng stereo. ---
        int numEarlyTaps = 22;
        for (int t = 0; t < numEarlyTaps; ++t)
        {
            float tapTimeSec = (float) t / (float) numEarlyTaps * 0.08f
                                * (1.0f + (ch == 1 ? 0.015f : 0.0f));
            int idx = (int) (tapTimeSec * (float) sampleRate);
            if (idx < numSamples)
            {
                float amp = std::pow (0.82f, (float) t) * (0.6f + 0.4f * rng.nextFloat());
                data[idx] += (rng.nextFloat() * 2.0f - 1.0f) * amp;
            }
        }

        // --- 2) Late diffuse tail: nhiễu trắng nhân bao hình suy giảm mũ,
        //         lọc thông thấp một cực mà hệ số cắt giảm dần theo thời gian
        //         (mô phỏng hấp thụ âm học: tần cao tắt nhanh hơn tần thấp). ---
        float lpState = 0.0f;
        float t60 = sizeSeconds; // thời gian để suy giảm ~60dB (xấp xỉ)
        for (int n = 0; n < numSamples; ++n)
        {
            float timeSec = (float) n / (float) sampleRate;
            float envelope = std::exp (-6.907755f * timeSec / juce::jmax (0.05f, t60)); // -60dB tại t60

            // Hệ số lọc thông thấp giảm dần theo thời gian -> đuôi vang càng
            // về sau càng "tối" (ít treble), giống hấp thụ vật liệu phòng thật.
            float lpCoeff = juce::jmap (envelope, 0.0f, 1.0f,
                                         0.05f + 0.15f * (1.0f - damping),
                                         0.85f);
            float noise = rng.nextFloat() * 2.0f - 1.0f;
            lpState += lpCoeff * (noise - lpState);

            data[n] += lpState * envelope * 0.9f;
        }
    }

    convolution.loadImpulseResponse (std::move (ir),
                                      sampleRate,
                                      juce::dsp::Convolution::Stereo::yes,
                                      juce::dsp::Convolution::Trim::no,
                                      juce::dsp::Convolution::Normalise::yes);
}

void ConvolutionReverbUnit::loadImpulseResponseFromFile (const juce::File& wavFile)
{
    if (! wavFile.existsAsFile())
        return;

    convolution.loadImpulseResponse (wavFile,
                                      juce::dsp::Convolution::Stereo::yes,
                                      juce::dsp::Convolution::Trim::yes,
                                      0 /* no length limit */,
                                      juce::dsp::Convolution::Normalise::yes);
}

void ConvolutionReverbUnit::setSizeAndDamping (float sizeSeconds, float damping, double sampleRate)
{
    generateAlgorithmicIR (sizeSeconds, damping, sampleRate);
}

void ConvolutionReverbUnit::process (juce::dsp::AudioBlock<float>& block)
{
    const int numCh = (int) block.getNumChannels();
    const int numSamples = (int) block.getNumSamples();

    // Lưu bản sao khô (dry) để trộn thủ công sau khi convolve (ướt/wet).
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), numSamples);

    convolution.process (juce::dsp::ProcessContextReplacing<float> (block));

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet = block.getChannelPointer ((size_t) ch);
        auto* dry = dryBuffer.getReadPointer (ch);
        for (int n = 0; n < numSamples; ++n)
            wet[n] = dry[n] * (1.0f - mix) + wet[n] * mix;
    }
}
