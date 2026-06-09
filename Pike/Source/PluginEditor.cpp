/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int   kHeaderHeight = 56;
    constexpr int   kPadding      = 10;
    constexpr float kPanelCorner  = 6.0f;

    const juce::Colour kBackground { 0xff1a1a1a };
    const juce::Colour kPanelFill  { 0xff222222 };
    const juce::Colour kPanelLine  { 0xff404040 };
    const juce::Colour kAccent     { 0xff4d9eff };
    const juce::Colour kTextDim    { 0xff888888 };
}

//==============================================================================
void SectionPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    // Panel body with a subtle vertical gradient for depth.
    juce::ColourGradient body (kPanelFill.brighter (0.04f), bounds.getX(), bounds.getY(),
                               kPanelFill.darker (0.10f),   bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (bounds, kPanelCorner);

    g.setColour (kPanelLine);
    g.drawRoundedRectangle (bounds, kPanelCorner, 1.0f);

    // Section title bar.
    auto titleArea = bounds.removeFromTop (24.0f).reduced (10.0f, 0.0f);
    g.setColour (kAccent);
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText (title, titleArea, juce::Justification::centredLeft, false);

    // Accent underline under the title.
    g.setColour (kAccent.withAlpha (0.35f));
    g.drawLine (bounds.getX() + 10.0f, bounds.getY() + 24.0f,
                bounds.getRight() - 10.0f, bounds.getY() + 24.0f, 1.0f);
}

//==============================================================================
PikeAudioProcessorEditor::PikeAudioProcessorEditor (PikeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* s : { &oscSection, &mixerSection, &filterSection, &envSection,
                     &lfoSection, &modSection, &fxSection, &arpSection })
        addAndMakeVisible (*s);

    setResizable (true, true);
    setResizeLimits (900, 560, 2400, 1500);
    getConstrainer()->setFixedAspectRatio (1100.0 / 680.0);
    setSize (1100, 680);
}

PikeAudioProcessorEditor::~PikeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void PikeAudioProcessorEditor::paintHeader (juce::Graphics& g, juce::Rectangle<int> headerArea)
{
    auto area = headerArea.toFloat();

    juce::ColourGradient header (juce::Colour (0xff262b36), area.getX(), area.getY(),
                                 juce::Colour (0xff15171c), area.getX(), area.getBottom(), false);
    g.setGradientFill (header);
    g.fillRect (area);

    g.setColour (kAccent.withAlpha (0.7f));
    g.drawLine (area.getX(), area.getBottom() - 1.0f, area.getRight(), area.getBottom() - 1.0f, 2.0f);

    auto text = area.reduced (16.0f, 0.0f);

    // Logo / name.
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (28.0f, juce::Font::bold));
    g.drawText ("PIKE", text.removeFromLeft (110.0f), juce::Justification::centredLeft, false);

    // Subtitle.
    g.setColour (kTextDim);
    g.setFont (juce::Font (13.0f));
    g.drawText ("Polyphonic Hybrid Synthesizer", text,
                juce::Justification::centredLeft, false);

    // Version, right-aligned.
    g.setColour (kAccent);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("v" JucePlugin_VersionString, area.reduced (16.0f, 0.0f),
                juce::Justification::centredRight, false);
}

void PikeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
    paintHeader (g, getLocalBounds().removeFromTop (kHeaderHeight));
}

void PikeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (kHeaderHeight);
    area.reduce (kPadding, kPadding);

    // Bottom row: FX + Arp span the width.
    auto bottom = area.removeFromBottom (juce::roundToInt (area.getHeight() * 0.26f));
    area.removeFromBottom (kPadding);
    fxSection .setBounds (bottom.removeFromLeft (juce::roundToInt (bottom.getWidth() * 0.7f))
                                .withTrimmedRight (kPadding));
    arpSection.setBounds (bottom);

    // Upper block split into three columns.
    auto colW = (area.getWidth() - 2 * kPadding) / 3;

    auto left = area.removeFromLeft (colW);
    area.removeFromLeft (kPadding);
    auto mid  = area.removeFromLeft (colW);
    area.removeFromLeft (kPadding);
    auto right = area;

    // Left column: oscillators over mixer.
    oscSection  .setBounds (left.removeFromTop (juce::roundToInt (left.getHeight() * 0.62f))
                                 .withTrimmedBottom (kPadding));
    mixerSection.setBounds (left);

    // Middle column: filter over envelopes.
    filterSection.setBounds (mid.removeFromTop (juce::roundToInt (mid.getHeight() * 0.42f))
                                 .withTrimmedBottom (kPadding));
    envSection   .setBounds (mid);

    // Right column: LFOs over mod matrix.
    lfoSection.setBounds (right.removeFromTop (juce::roundToInt (right.getHeight() * 0.42f))
                                .withTrimmedBottom (kPadding));
    modSection.setBounds (right);
}
