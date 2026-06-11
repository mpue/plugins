/*
  ==============================================================================

    PluginEditor.cpp
    ARP-1 Luxury Arpeggiator

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ARP1AudioProcessorEditor::ARP1AudioProcessorEditor (ARP1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetBar    (p.getPresetManager()),
      visualizer   (p.getEngine()),
      controlPanel (p.getEngine()),
      patternBar   (p.getPatternManager()),
      stepLane     (p.getEngine())
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (visualizer);
    addAndMakeVisible (controlPanel);
    addAndMakeVisible (patternBar);
    addAndMakeVisible (stepLane);

    // Editing the pattern (step lane or pattern-affecting controls) auto-saves
    // the current pattern and refreshes the views.
    auto patternEdited = [this]
    {
        audioProcessor.getPatternManager().markDirty();
        controlPanel.refreshFromEngine();
        stepLane.refresh();
        visualizer.repaint();
    };
    stepLane.onChanged          = patternEdited;
    controlPanel.onPatternChanged = patternEdited;

    // A preset load (or host state restore) refreshes every control + view.
    audioProcessor.getPresetManager().onPresetLoaded = [this]
    {
        controlPanel.refreshFromEngine();
        stepLane.refresh();
        visualizer.repaint();
    };

    setResizable (false, false);
    setSize (920, 696);
}

ARP1AudioProcessorEditor::~ARP1AudioProcessorEditor()
{
    audioProcessor.getPresetManager().onPresetLoaded = nullptr;
    setLookAndFeel (nullptr);
}

//==============================================================================
void ARP1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    juce::ColourGradient bg (juce::Colour (0xff141a23), 0.0f, 0.0f,
                              juce::Colour (0xff080b11), 0.0f, (float) bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    // Top sheen
    {
        juce::ColourGradient sheen (juce::Colour (0x184d9eff), bounds.getWidth() * 0.5f, -120.0f,
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

    g.setColour (juce::Colour (0xff9fd0ff));
    g.setFont (juce::Font (juce::FontOptions (24.0f).withStyle ("Bold")));
    g.drawText ("ARP-1",
                juce::Rectangle<int> (title.getX(), title.getY() + 2, 92, 28),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff4d9eff));
    g.fillRect (juce::Rectangle<int> (title.getX() + 90, title.getY() + 8, 1, 22));

    g.setColour (juce::Colour (0xffd8e6ff));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("LUXURY",
                juce::Rectangle<int> (title.getX() + 100, title.getY() + 6, 70, 12),
                juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff8aa6cc));
    g.drawText ("ARPEGGIATOR",
                juce::Rectangle<int> (title.getX() + 100, title.getY() + 18, 140, 12),
                juce::Justification::centredLeft);
}

void ARP1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header strip with preset bar on the right
    auto header = bounds.removeFromTop (62).reduced (12, 8);
    headerBounds = header;

    auto h = header;
    h.removeFromLeft (250);
    presetBar.setBounds (h.reduced (4, 0));

    bounds.removeFromTop (6);

    // Big visualiser
    auto vis = bounds.removeFromTop (280).reduced (12, 0);
    visualizer.setBounds (vis);

    bounds.removeFromTop (10);

    // Step lane at the very bottom, with the pattern selector strip above it
    auto lane = bounds.removeFromBottom (120).reduced (12, 0);
    lane.removeFromBottom (12);
    stepLane.setBounds (lane);

    auto pbar = bounds.removeFromBottom (34).reduced (12, 0);
    patternBar.setBounds (pbar);

    bounds.removeFromBottom (6);

    // Control panel fills the middle
    controlPanel.setBounds (bounds.reduced (12, 0));
}
