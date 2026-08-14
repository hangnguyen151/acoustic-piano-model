#include "PluginProcessor.h"
#include "PluginEditor.h"

AcousticPianoModelProcessor::AcousticPianoModelProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int i = 0; i < maxVoices; ++i)
        voices.push_back (std::make_unique<PianoVoice> (pedalEngine));

    hammerHardnessParam    = apvts.getRawParameterValue ("hammerHardness");
    sympatheticAmountParam = apvts.getRawParameterValue ("sympatheticAmount");
    reverbMixParam          = apvts.getRawParameterValue ("reverbMix");
    reverbSizeParam         = apvts.getRawParameterValue ("reverbSize");
    outputGainParam         = apvts.getRawParameterValue ("outputGain");
    unisonDetuneParam       = apvts.getRawParameterValue ("unisonDetune");
}

juce::AudioProcessorValueTreeState::ParameterLayout AcousticPianoModelProcessor::createParameterLayout()
{
    using Param = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<Param> (
        juce::ParameterID { "hammerHardness", 1 }, "Hammer Hardness",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<Param> (
        juce::ParameterID { "sympatheticAmount", 1 }, "Sympathetic Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f));

    params.push_back (std::make_unique<Param> (
        juce::ParameterID { "reverbMix", 1 }, "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.28f));

    params.push_back (std::make_unique<Param> (
        juce::ParameterID { "reverbSize", 1 }, "Reverb Size",
        juce::NormalisableRange<float> (0.4f, 5.0f), 2.6f));

    params.push_back (std::make_unique<Param> (
        juce::ParameterID { "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f));

    params.push_back (std::make_unique<Param> (
        juce::ParameterID { "unisonDetune", 1 }, "Unison Detune (cents)",
        juce::NormalisableRange<float> (0.0f, 12.0f), 3.5f));

    return { params.begin(), params.end() };
}

void AcousticPianoModelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    pedalEngine.prepare (sampleRate);

    for (auto& v : voices)
        v->prepare (sampleRate, 0.0002f); // giá trị B mặc định, được ghi đè per-note trong startNote

    voiceMixBuffer.setSize (2, samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    reverb.prepare (spec);
}

bool AcousticPianoModelProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

PianoVoice* AcousticPianoModelProcessor::findFreeVoice (int /*midiNoteNumber*/)
{
    // Ưu tiên voice hoàn toàn im lặng (isVoiceActive() == false)
    for (auto& v : voices)
        if (! v->isVoiceActive())
            return v.get();

    // Không còn voice rảnh -> "steal" voice cũ nhất đang ở trạng thái release
    // (đã nhả phím, chỉ còn đuôi vang) để giảm ảnh hưởng nghe được.
    for (auto& v : voices)
        if (! v->isNoteHeldDown())
            return v.get();

    // Trường hợp cực đoan: tất cả voice đều đang giữ phím -> cướp voice đầu tiên.
    return voices.empty() ? nullptr : voices.front().get();
}

void AcousticPianoModelProcessor::handleMidiEvent (const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        if (auto* v = findFreeVoice (msg.getNoteNumber()))
        {
            v->setHammerHardness (hammerHardnessParam->load());
            v->setUnisonDetuneCents (unisonDetuneParam->load());
            v->startNote (msg.getNoteNumber(), msg.getFloatVelocity());
        }
    }
    else if (msg.isNoteOff())
    {
        for (auto& v : voices)
            if (v->isNoteHeldDown() && v->getCurrentMidiNote() == msg.getNoteNumber())
                v->stopNote (msg.getFloatVelocity());
    }
    else if (msg.isController() && msg.getControllerNumber() == 64) // Sustain pedal (CC64)
    {
        bool down = msg.getControllerValue() >= 64;
        pedalEngine.setSustainPedalDown (down);

        if (! down)
        {
            // Khi nhả pedal: mọi nốt đã buông phím trước đó phải để damper rơi
            // thật ngay bây giờ (giống hành vi piano thật: nhả pedal cắt hết
            // các nốt không còn được giữ phím).
            for (auto& v : voices)
                if (! v->isNoteHeldDown() && v->isVoiceActive())
                    v->stopNote (0.4f);
        }
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        for (auto& v : voices)
            v->stopNote (0.0f);
    }
}

void AcousticPianoModelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // --- Cập nhật tham số reverb (kích thước/damping) nếu người dùng vừa chỉnh ---
    static float lastReverbSize = -1.0f;
    float reqSize = reverbSizeParam->load();
    if (std::abs (reqSize - lastReverbSize) > 0.05f)
    {
        reverb.setSizeAndDamping (reqSize, 0.45f, currentSampleRate);
        lastReverbSize = reqSize;
    }
    reverb.setMix (reverbMixParam->load());
    pedalEngine.setResonanceAmount (sympatheticAmountParam->load());

    // --- Xử lý MIDI theo đúng vị trí sample trong block ---
    int samplePos = 0;
    auto midiIterator = midiMessages.cbegin();
    auto midiEnd = midiMessages.cend();

    voiceMixBuffer.setSize (buffer.getNumChannels(), numSamples, false, false, true);
    voiceMixBuffer.clear();

    while (samplePos < numSamples)
    {
        // Xử lý mọi sự kiện MIDI có timestamp <= samplePos hiện tại
        while (midiIterator != midiEnd && (*midiIterator).samplePosition <= samplePos)
        {
            handleMidiEvent ((*midiIterator).getMessage());
            ++midiIterator;
        }

        // --- Vòng lặp cộng hưởng bàn đạp: BẮT BUỘC đúng thứ tự ---
        pedalEngine.beginSample();

        float mono = 0.0f;
        for (auto& v : voices)
            if (v->isVoiceActive())
                mono += v->processOneSample();

        pedalEngine.finishSample();

        // Cộng thêm "soundboard wash" (màu âm bàn cộng hưởng khi pedal giữ)
        mono += pedalEngine.getSoundboardWashSample();

        // Soft clip nhẹ để tránh vỡ tiếng khi nhiều voice cộng dồn cùng lúc.
        mono = std::tanh (mono * 0.8f);

        for (int ch = 0; ch < voiceMixBuffer.getNumChannels(); ++ch)
            voiceMixBuffer.setSample (ch, samplePos, mono);

        ++samplePos;
    }

    // Xử lý các sự kiện MIDI còn sót lại ở cuối block (hiếm khi xảy ra)
    while (midiIterator != midiEnd)
    {
        handleMidiEvent ((*midiIterator).getMessage());
        ++midiIterator;
    }

    // --- Convolution reverb chất lượng cao trên toàn bộ master bus ---
    juce::dsp::AudioBlock<float> block (voiceMixBuffer);
    reverb.process (block);

    // --- Output gain và copy sang buffer output cuối cùng của plugin ---
    float gainLinear = juce::Decibels::decibelsToGain (outputGainParam->load());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        int srcCh = juce::jmin (ch, voiceMixBuffer.getNumChannels() - 1);
        buffer.copyFrom (ch, 0, voiceMixBuffer, srcCh, 0, numSamples);
        buffer.applyGain (ch, 0, numSamples, gainLinear);
    }
}

juce::AudioProcessorEditor* AcousticPianoModelProcessor::createEditor()
{
    return new AcousticPianoModelEditor (*this);
}

void AcousticPianoModelProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void AcousticPianoModelProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Bắt buộc bởi JUCE để plugin có thể được host tạo instance qua factory function.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcousticPianoModelProcessor();
}
