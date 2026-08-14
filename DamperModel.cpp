#include "DamperModel.h"

void DamperModel::prepare (double newSampleRate, double stringFrequencyHz)
{
    sampleRate = newSampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };

    liftNoiseFilter.prepare (spec);
    dropNoiseFilter.prepare (spec);
    thudFilter.prepare (spec);

    liftNoiseFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    dropNoiseFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    thudFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    // Tiếng nhấc/rơi damper có phổ tần cao hơn với dây cao, thấp hơn với dây trầm,
    // giống việc nỉ ma sát trên dây thép mảnh vs dây trầm bọc đồng.
    float centreFreq = (float) juce::jlimit (800.0, 5500.0, stringFrequencyHz * 3.0);
    liftNoiseFilter.setCutoffFrequency (centreFreq);
    liftNoiseFilter.setResonance (0.3f);

    dropNoiseFilter.setCutoffFrequency (centreFreq * 0.85f);
    dropNoiseFilter.setResonance (0.35f);

    thudFilter.setCutoffFrequency (180.0f);
    thudFilter.setResonance (0.5f);

    // Tốc độ tắt dần (decay) của các envelope nhiễu, tính theo hằng số thời gian ms
    auto decayCoeff = [this] (float ms)
    {
        return std::exp (-1.0f / (0.001f * ms * (float) sampleRate));
    };
    liftDecay = decayCoeff (10.0f);
    dropDecay = decayCoeff (35.0f);
    thudDecay = decayCoeff (18.0f);
}

void DamperModel::triggerLiftNoise (float velocity)
{
    liftEnvelope = 1.0f;
    liftGain = juce::jmap (juce::jlimit (0.0f, 1.0f, velocity), 0.0f, 1.0f, 0.01f, 0.06f);
}

void DamperModel::triggerDropNoise (float releaseVelocity)
{
    dropEnvelope = 1.0f;
    dropGain = juce::jmap (juce::jlimit (0.0f, 1.0f, releaseVelocity), 0.0f, 1.0f, 0.02f, 0.09f);

    thudEnvelope = 1.0f;
    thudGain = dropGain * 0.6f;
}

float DamperModel::getNextSample() noexcept
{
    float out = 0.0f;

    if (liftEnvelope > 1.0e-4f)
    {
        float noise = rng.nextFloat() * 2.0f - 1.0f;
        float filtered = liftNoiseFilter.processSample (0, noise);
        out += filtered * liftEnvelope * liftGain;
        liftEnvelope *= liftDecay;
    }

    if (dropEnvelope > 1.0e-4f)
    {
        float noise = rng.nextFloat() * 2.0f - 1.0f;
        float filtered = dropNoiseFilter.processSample (0, noise);
        out += filtered * dropEnvelope * dropGain;
        dropEnvelope *= dropDecay;
    }

    if (thudEnvelope > 1.0e-4f)
    {
        float noise = rng.nextFloat() * 2.0f - 1.0f;
        float filtered = thudFilter.processSample (0, noise);
        out += filtered * thudEnvelope * thudGain;
        thudEnvelope *= thudDecay;
    }

    return out;
}
