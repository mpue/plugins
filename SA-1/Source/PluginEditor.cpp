/*
  ==============================================================================

    PluginEditor.cpp
    SA-1 Luxury Quick Sampler

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SA1AudioProcessorEditor::SA1AudioProcessorEditor (SA1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetBar   (p.getPresetManager()),
      padGrid     (p),
      bigWaveform (p),
      paramPanel  (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (padGrid);
    addAndMakeVisible (bigWaveform);
    addAndMakeVisible (paramPanel);

    padGrid.onSelectedPadChanged = [this] (int idx) { onPadSelectionChanged (idx); };
    padGrid.onChanged            = [this]           { onAllPadsChanged(); };

    paramPanel.onPadStateChanged = [this]
    {
        padGrid.getPadComponent (padGrid.getSelectedPad()).repaint();
        bigWaveform.repaint();
    };

    audioProcessor.getPresetManager().onPresetLoaded = [this]
    {
        for (int i = 0; i < SA1::kNumPads; ++i)
            padGrid.getPadComponent (i).repaint();
        paramPanel.refreshFromPad();
        bigWaveform.repaint();
    };

    paramPanel.setSelectedPad (padGrid.getSelectedPad());
    bigWaveform.setSelectedPad (padGrid.getSelectedPad());

    setResizable (false, false);
    setSize (1180, 760);
    startTimerHz (45);
}

SA1AudioProcessorEditor::~SA1AudioProcessorEditor()
{
    audioProcessor.getPresetManager().onPresetLoaded = nullptr;
    setLookAndFeel (nullptr);
}

//==============================================================================
void SA1AudioProcessorEditor::onPadSelectionChanged (int padIndex)
{
    bigWaveform.setSelectedPad (padIndex);
    paramPanel .setSelectedPad (padIndex);
}

void SA1AudioProcessorEditor::onAllPadsChanged()
{
    bigWaveform.repaint();
    paramPanel.refreshFromPad();
}

void SA1AudioProcessorEditor::timerCallback()
{
    int padOut = -1;
    const int delta = audioProcessor.getEngine().consumeTriggerEvent (padOut);
    if (delta > 0 && juce::isPositiveAndBelow (padOut, SA1::kNumPads))
    {
        padGrid.getPadComponent (padOut).notifyTriggered (1.0f);
        bigWaveform.notifyTriggered (padOut, 1.0f);
    }
}

//==============================================================================
void SA1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    juce::ColourGradient bg (juce::Colour (0xff141a23), 0.0f, 0.0f,
                              juce::Colour (0xff080b11), 0.0f, (float) bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    // Sheen
    {
        juce::ColourGradient sheen (juce::Colour (0x18ffd4a0), bounds.getWidth() * 0.5f, -120.0f,
                                     juce::Colour (0x00000000), bounds.getWidth() * 0.5f, 280.0f, true);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    // Header bar
    auto header = headerBounds;
    juce::ColourGradient hbg (juce::Colour (0xff1d2533), header.getCentreX(), (float) header.getY(),
                               juce::Colour (0xff10141d), header.getCentreX(), (float) header.getBottom(), false);
    g.setGradientFill (hbg);
    g.fillRoundedRectangle (header.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff262e3a));
    g.drawRoundedRectangle (header.toFloat(), 8.0f, 1.0f);

    auto title = header.reduced (16, 6);

    // Logo
    g.setColour (juce::Colour (0xffffd29c));
    g.setFont (juce::Font (juce::FontOptions (24.0f).withStyle ("Bold")));
    g.drawText ("SA-1",
                juce::Rectangle<int> (title.getX(), title.getY() + 2, 80, 28),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffffa860));
    g.fillRect (juce::Rectangle<int> (title.getX() + 78, title.getY() + 8, 1, 22));

    g.setColour (juce::Colour (0xffe2c8a8));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("LUXURY",
                juce::Rectangle<int> (title.getX() + 88, title.getY() + 6, 70, 12),
                juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffc8a878));
    g.drawText ("QUICK SAMPLER",
                juce::Rectangle<int> (title.getX() + 88, title.getY() + 18, 140, 12),
                juce::Justification::centredLeft);
}

void SA1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header strip
    auto header = bounds.removeFromTop (62).reduced (12, 8);
    headerBounds = header;

    // Preset bar inside header right side
    auto h = header;
    h.removeFromLeft (240);
    presetBar.setBounds (h.reduced (4, 0));

    bounds.removeFromTop (8);

    // Big waveform — top row, full width less margin
    auto waveformArea = bounds.removeFromTop (240).reduced (12, 0);
    bigWaveform.setBounds (waveformArea);

    bounds.removeFromTop (12);

    // Below: pad grid (left) + parameter panel (right)
    auto bottom = bounds.reduced (12, 0);
    bottom.removeFromBottom (12);

    const int paramWidth = 380;
    auto padArea   = bottom.removeFromLeft (bottom.getWidth() - paramWidth - 12);
    bottom.removeFromLeft (12);
    auto paramArea = bottom;

    padGrid    .setBounds (padArea);
    paramPanel .setBounds (paramArea);
}
