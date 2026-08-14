#include "ConvolutionReverbUnit.h"

void ConvolutionReverbUnit::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSpec = spec;
    convolution.prepare (spec);
    dryBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

    generateAlgorithmicIR (2.6f, 0.6f, spec.sampleRate);

    updateToneShaperCoefficients (spec.sampleRate);
    for (auto& channelBands : toneFilters)
        for (auto& f : channelBands)
            f.reset();
}

void ConvolutionReverbUnit::updateToneShaperCoefficients (double sampleRate)
{
    lowShelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 180.0f, 0.7f, juce::Decibels::decibelsToGain (3.0f));

    presencePeakCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 2200.0f, 0.9f, juce::Decibels::decibelsToGain (2.0f));

    highShelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 6500.0f, 0.7f, juce::Decibels::decibelsToGain (-3.5f));

    for (auto& channelBands : toneFilters)
    {
        channelBands[0].coefficients = lowShelfCoeffs;
        channelBands[1].coefficients = presencePeakCoeffs;
        channelBands[2].coefficients = highShelfCoeffs;
    }
}

void ConvolutionReverbUnit::processToneShaper (juce::dsp::AudioBlock<float>& block) noexcept
{
    const int numCh = juce::jmin ((int) block.getNumChannels(), maxToneChannels);
    const int numSamples = (int) block.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = block.getChannelPointer ((size_t) ch);
        for (int n = 0; n < numSamples; ++n)
        {
            float s = data[n];
            for (auto& f : toneFilters[(size_t) ch])
                s = f.processSample (s);
            data[n] = s;
        }
    }
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

        float lpState = 0.0f;
        float t60 = sizeSeconds;
        for (int n = 0; n < numSamples; ++n)
        {
            float timeSec = (float) n / (float) sampleRate;
            float envelope = std::exp (-6.907755f * timeSec / juce::jmax (0.05f, t60));

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
                                      0,
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

    processToneShaper (block);
}
