#include "PluginEditor.h"

AcousticPianoModelEditor::AcousticPianoModelEditor (AcousticPianoModelProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    configureSlider (hammerHardnessSlider, hammerHardnessLabel, "Hammer Hardness");
    configureSlider (sympatheticSlider, sympatheticLabel, "Sympathetic Resonance");
    configureSlider (reverbMixSlider, reverbMixLabel, "Reverb Mix");
    configureSlider (reverbSizeSlider, reverbSizeLabel, "Reverb Size");
    configureSlider (outputGainSlider, outputGainLabel, "Output Gain (dB)");
    configureSlider (unisonDetuneSlider, unisonDetuneLabel, "Unison Detune");

    auto& apvts = processorRef.apvts;
    hammerHardnessAttachment = std::make_unique<Attachment> (apvts, "hammerHardness", hammerHardnessSlider);
    sympatheticAttachment    = std::make_unique<Attachment> (apvts, "sympatheticAmount", sympatheticSlider);
    reverbMixAttachment      = std::make_unique<Attachment> (apvts, "reverbMix", reverbMixSlider);
    reverbSizeAttachment     = std::make_unique<Attachment> (apvts, "reverbSize", reverbSizeSlider);
    outputGainAttachment     = std::make_unique<Attachment> (apvts, "outputGain", outputGainSlider);
    unisonDetuneAttachment   = std::make_unique<Attachment> (apvts, "unisonDetune", unisonDetuneSlider);

    setSize (620, 320);
}

void AcousticPianoModelEditor::configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.attachToComponent (&slider, false);
    addAndMakeVisible (label);
}

void AcousticPianoModelEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient grad (juce::Colour (0xff2b1d12), 0.0f, 0.0f,
                                juce::Colour (0xff10090a), 0.0f, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillAll();

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText ("Acoustic Piano Model - Physical Modeling Synth",
                getLocalBounds().removeFromTop (40), juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("String Waveguide  |  Hammer/Damper Mechanics  |  Sustain Pedal Resonance  |  Convolution Reverb",
                getLocalBounds().removeFromTop (70).removeFromBottom (20), juce::Justification::centred);
}

void AcousticPianoModelEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (70); // chừa chỗ cho tiêu đề

    int numKnobs = 6;
    int knobWidth = area.getWidth() / numKnobs;

    auto place = [&] (juce::Slider& s)
    {
        auto slot = area.removeFromLeft (knobWidth).reduced (10, 30);
        s.setBounds (slot);
    };

    place (hammerHardnessSlider);
    place (sympatheticSlider);
    place (reverbMixSlider);
    place (reverbSizeSlider);
    place (outputGainSlider);
    place (unisonDetuneSlider);
}
