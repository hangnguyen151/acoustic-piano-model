#include "StringModel.h"

void StringModel::prepare (double newSampleRate, double midiNoteFrequencyHz, float inharmonicityCoeffB)
{
    sampleRate = newSampleRate;
    frequencyHz = midiNoteFrequencyHz;

    double totalDelaySamples = sampleRate / frequencyHz;
    double oneWayDelay = totalDelaySamples * 0.5;

    lineLength = juce::jmax (4, (int) std::floor (oneWayDelay));
    fracDelay  = (float) (oneWayDelay - (double) lineLength);

    rightGoing.assign ((size_t) lineLength + 4, 0.0f);
    leftGoing.assign  ((size_t) lineLength + 4, 0.0f);
    writeR = writeL = 0;
    allpassFracState = 0.0f;

    baseLossCoeff = (float) juce::jmap (frequencyHz, 27.5, 4186.0, 0.99985, 0.9950);
    lossCoeff = baseLossCoeff;
    dampingTarget = baseLossCoeff;

    updateDispersionFilters (inharmonicityCoeffB);

    warmthCoeff = (float) juce::jmap (frequencyHz, 27.5, 4186.0, 0.9, 0.55);
    warmthFilterState = 0.0f;

    strikePositionRatio = 0.12f;
    active = false;
    damperLifted = false;
}

void StringModel::updateDispersionFilters (float B)
{
    float a = juce::jlimit (-0.22f, 0.22f, B * 3.0f - 0.015f);
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

    strikePosSamples = juce::jmax (1, (int) std::round (strikePositionRatio * (float) lineLength));

    for (auto& v : rightGoing) v *= 0.15f;
    for (auto& v : leftGoing)  v *= 0.15f;
}

void StringModel::injectExcitation (float forceSample) noexcept
{
    rightGoing[(size_t) writeR] += forceSample * 0.5f;
    leftGoing[(size_t) writeL]  += forceSample * 0.5f;
}

void StringModel::keyReleased (float /*releaseVelocity*/)
{
    if (! damperLifted)
        return;
}

void StringModel::setDamperLifted (bool lifted)
{
    damperLifted = lifted;
    dampingTarget = lifted ? baseLossCoeff
                            : juce::jmap (frequencyHz, 27.5, 4186.0, 0.965, 0.85);
}

float StringModel::readDelayLine (const std::vector<float>& line, int writePos, float delaySamplesFrac) const
{
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

    lossCoeff += 0.002f * (dampingTarget - lossCoeff);

    int len = (int) rightGoing.size();

    int readR = (writeR + 1) % len;
    int readL = (writeL + 1) % len;
    float sampleR = rightGoing[(size_t) readR];
    float sampleL = leftGoing[(size_t) readL];

    float lossedR = sampleR * lossCoeff + lossFilterStateR * (1.0f - lossCoeff);
    lossFilterStateR = lossedR;
    float reflectedAtBridge = lossedR * bridgeReflection;

    float lossedL = sampleL * lossCoeff + lossFilterStateL * (1.0f - lossCoeff);
    lossFilterStateL = lossedL;
    float reflectedAtNut = lossedL * nutReflection;

    float dispersed = reflectedAtBridge;
    for (auto& s : dispersionChain)
    {
        float y = -s.a * dispersed + s.x1 + s.a * s.y1;
        s.x1 = dispersed;
        s.y1 = y;
        dispersed = y;
    }

    float coupling = damperLifted ? sympatheticInput * 0.05f : 0.0f;

    rightGoing[(size_t) writeR] = reflectedAtNut + coupling;
    leftGoing[(size_t) writeL]  = dispersed + coupling;

    writeR = (writeR + 1) % len;
    writeL = (writeL + 1) % len;

    float output = sampleR + sampleL;

    warmthFilterState += warmthCoeff * (output - warmthFilterState);
    output = warmthFilterState;

    outputLevel = 0.999f * outputLevel + 0.001f * std::abs (output);

    if (outputLevel < 1.0e-5f && ! damperLifted)
        active = false;

    return output;
}
