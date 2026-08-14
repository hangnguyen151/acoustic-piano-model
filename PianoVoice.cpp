#include "PianoVoice.h"

PianoVoice::PianoVoice (PedalResonanceEngine& sharedPedalEngine)
    : pedalEngine (sharedPedalEngine)
{
}

void PianoVoice::prepare (double newSampleRate, float inharmonicityCoeffB)
{
    sampleRate = newSampleRate;
    hammer.prepare (sampleRate);

    for (int i = 0; i < maxUnisonStrings; ++i)
        strings[(size_t) i].prepare (sampleRate, 440.0, inharmonicityCoeffB);
}

void PianoVoice::startNote (int midiNoteNumber, float velocity)
{
    currentMidiNote = midiNoteNumber;
    noteVelocity = velocity;
    noteIsOn = true;

    double baseFreq = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

    // Hệ số inharmonicity B tăng dần với nốt cao/nốt trầm quấn dây đồng,
    // xấp xỉ đường cong thực nghiệm quan sát trên piano thật.
    float noteRatio = juce::jlimit (0.0f, 1.0f, (float) (midiNoteNumber - 21) / 87.0f); // A0..C8
    float inharmonicity = juce::jmap (noteRatio, 0.00008f, 0.02f);

    damper.prepare (sampleRate, baseFreq);

    // Cấu hình unison: nốt trầm 1 dây, nốt trung 2 dây, nốt cao 3 dây,
    // giống cấu trúc dây thật của piano grand.
    numUnisonStrings = (midiNoteNumber < 40) ? 1 : (midiNoteNumber < 88 ? 2 : 3);

    for (int i = 0; i < numUnisonStrings; ++i)
    {
        float detuneRatio = 1.0f;
        if (numUnisonStrings > 1)
        {
            float spread = (float) i - (float) (numUnisonStrings - 1) * 0.5f;
            float cents = spread * unisonDetuneCents;
            detuneRatio = std::pow (2.0f, cents / 1200.0f);
        }
        strings[(size_t) i].prepare (sampleRate, baseFreq * (double) detuneRatio, inharmonicity);
    }

    float strikePos = juce::jmap (noteRatio, 0.16f, 0.08f);

    hammer.trigger (velocity, hammerHardness);
    for (int i = 0; i < numUnisonStrings; ++i)
        strings[(size_t) i].hammerStrike (velocity, hammerHardness, strikePos);

    damper.triggerLiftNoise (velocity);
}

void PianoVoice::stopNote (float releaseVelocity)
{
    noteIsOn = false;

    bool sustainHeld = pedalEngine.isSustainDown();

    for (int i = 0; i < numUnisonStrings; ++i)
    {
        strings[(size_t) i].keyReleased (releaseVelocity);
        if (! sustainHeld)
            strings[(size_t) i].setDamperLifted (false); // damper rơi thật -> tắt âm nhanh
    }

    if (! sustainHeld)
        damper.triggerDropNoise (releaseVelocity);
}

bool PianoVoice::isVoiceActive() const noexcept
{
    if (noteIsOn)
        return true;

    for (int i = 0; i < numUnisonStrings; ++i)
        if (strings[(size_t) i].isActive())
            return true;

    return damper.isActive();
}

float PianoVoice::processOneSample() noexcept
{
    if (! isVoiceActive())
        return 0.0f;

    // Khi sustain pedal đang giữ, mọi dây (kể cả nốt đã nhả phím) vẫn được
    // damper nhấc lên để tiếp tục tham gia cộng hưởng.
    if (pedalEngine.isSustainDown())
        for (int i = 0; i < numUnisonStrings; ++i)
            strings[(size_t) i].setDamperLifted (true);

    // 1) Bơm lực búa (nếu còn đang tiếp xúc dây) vào tất cả dây unison.
    if (hammer.contactActive())
    {
        float force = hammer.getNextSample();
        for (int i = 0; i < numUnisonStrings; ++i)
            strings[(size_t) i].injectExcitation (force);
    }

    // 2) Lấy hồi tiếp cộng hưởng đã tính từ sample trước (dùng chung toàn bus).
    float resonanceFeedback = pedalEngine.getResonanceFeedback();

    // 3) Xử lý từng dây unison, cộng dồn thành 1 nốt, đồng thời góp năng
    //    lượng vào bus cộng hưởng chung cho sample hiện tại.
    float noteOutput = 0.0f;
    for (int i = 0; i < numUnisonStrings; ++i)
    {
        float stringOut = strings[(size_t) i].processSample (resonanceFeedback);
        noteOutput += stringOut;
        pedalEngine.accumulate (stringOut, strings[(size_t) i].canResonateSympathetically());
    }
    if (numUnisonStrings > 1)
        noteOutput *= 1.0f / std::sqrt ((float) numUnisonStrings);

    // 4) Cộng thêm nhiễu cơ học (damper lift/drop noise, key-off thud).
    noteOutput += damper.getNextSample();

    return noteOutput * (0.55f + 0.45f * noteVelocity);
}
