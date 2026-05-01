/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CH1AudioProcessorEditor::CH1AudioProcessorEditor (CH1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetBar (p.getPresetManager()),
      visualizer (p.getEngine())
{
    setLookAndFeel (&lookAndFeel);

    // Header labels
    titleLabel.setText ("CH-1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (28.0f, juce::Font::FontStyleFlags::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8f0ff));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Stereo Chorus", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (12.0f, juce::Font::FontStyleFlags::plain));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7a99c0));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (visualizer);

    auto& apvts = audioProcessor.getAPVTS();

    // Knobs + APVTS attachments
    buildKnob (rateKnob,     "rate",     rateAtt);
    buildKnob (depthKnob,    "depth",    depthAtt);
    buildKnob (delayKnob,    "delay",    delayAtt);
    buildKnob (feedbackKnob, "feedback", feedbackAtt);
    buildKnob (widthKnob,    "width",    widthAtt);
    buildKnob (toneKnob,     "tone",     toneAtt);
    buildKnob (mixKnob,      "mix",      mixAtt);
    buildKnob (outKnob,      "gain",     outAtt);

    // Voices selector
    voicesLabel.setText ("Voices", juce::dontSendNotification);
    voicesLabel.setJustificationType (juce::Justification::centred);
    voicesLabel.setFont (juce::Font (12.0f));
    addAndMakeVisible (voicesLabel);

    voicesBox.addItem ("1", 1);
    voicesBox.addItem ("2", 2);
    voicesBox.addItem ("3", 3);
    voicesBox.addItem ("4", 4);
    addAndMakeVisible (voicesBox);
    voicesAtt = std::make_unique<ComboBoxAttachment> (apvts, "voices", voicesBox);

    // Bypass
    addAndMakeVisible (bypassButton);
    bypassAtt = std::make_unique<ButtonAttachment> (apvts, "bypass", bypassButton);

    setSize (760, 520);
}

CH1AudioProcessorEditor::~CH1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CH1AudioProcessorEditor::buildKnob (LabelledKnob& k, const juce::String& paramID,
                                         std::unique_ptr<SliderAttachment>& attachment)
{
    addAndMakeVisible (k);
    attachment = std::make_unique<SliderAttachment> (audioProcessor.getAPVTS(), paramID, k.slider);
}

//==============================================================================
void CH1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Deep, slightly blue background gradient
    juce::ColourGradient bg (juce::Colour (0xff1a2030), bounds.getX(), bounds.getY(),
                             juce::Colour (0xff0d1118), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    // Top-edge accent
    g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.25f));
    g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, bounds.getWidth(), 1.5f));

    // Section divider line under the header
    g.setColour (juce::Colour (0xff2a3344));
    g.drawLine (12.0f, 60.0f, bounds.getWidth() - 12.0f, 60.0f, 1.0f);

    // Card background under the knob row
    auto card = getLocalBounds().reduced (10).removeFromBottom (220).toFloat().reduced (2.0f);
    g.setColour (juce::Colour (0xff141a26));
    g.fillRoundedRectangle (card, 10.0f);
    g.setColour (juce::Colour (0xff2a3344));
    g.drawRoundedRectangle (card, 10.0f, 1.0f);
}

void CH1AudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);

    // Header row
    auto header = r.removeFromTop (50);
    auto titleArea = header.removeFromLeft (220);
    titleLabel   .setBounds (titleArea.removeFromTop (32));
    subtitleLabel.setBounds (titleArea);

    // Preset bar takes the rest of the header on the right
    header.removeFromLeft (10);
    presetBar.setBounds (header.reduced (0, 6));

    r.removeFromTop (8); // gap below header divider

    // Visualizer
    auto visArea = r.removeFromTop (200);
    visualizer.setBounds (visArea);

    r.removeFromTop (6);

    // Bottom controls card area
    auto controls = r.reduced (8, 8);

    // Right column: voices + bypass + output
    auto right = controls.removeFromRight (110);
    {
        auto vSection = right.removeFromTop (70);
        voicesLabel.setBounds (vSection.removeFromTop (16));
        voicesBox  .setBounds (vSection.reduced (4, 4));

        right.removeFromTop (6);
        bypassButton.setBounds (right.removeFromTop (28).reduced (4, 2));
        right.removeFromTop (6);
        outKnob.setBounds (right);
    }

    controls.removeFromRight (10);

    // Knob grid: 7 knobs across the remaining width
    LabelledKnob* knobs[] = { &rateKnob, &depthKnob, &delayKnob, &feedbackKnob,
                              &widthKnob, &toneKnob, &mixKnob };
    const int n = juce::numElementsInArray (knobs);
    const int knobW = controls.getWidth() / n;

    for (int i = 0; i < n; ++i)
    {
        auto cell = controls.removeFromLeft (knobW).reduced (2, 4);
        knobs[i]->setBounds (cell);
    }
}
