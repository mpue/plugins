/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kHeaderHeight = 56;
    constexpr int kEditorHeight = 320;
    constexpr int kEditorMinW = 720;
}

MicroModAudioProcessorEditor::MicroModAudioProcessorEditor (MicroModAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&laf);

    titleLabel.setText ("MicroMod", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8f0ff));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Micro Modular Synthesizer", juce::dontSendNotification);
    subtitleLabel.setFont (juce::FontOptions (12.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7a99c0));
    addAndMakeVisible (subtitleLabel);

    addButton.onClick = [this] { showAddMenu(); };
    addAndMakeVisible (addButton);

    grid = std::make_unique<ModuleGrid> (audioProcessor);
    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible (viewport);

    audioProcessor.addChainListener (this);

    setResizable (true, true);
    setResizeLimits (kEditorMinW, kEditorHeight, 4096, kEditorHeight + 200);
    setSize (1000, kEditorHeight);
}

MicroModAudioProcessorEditor::~MicroModAudioProcessorEditor()
{
    audioProcessor.removeChainListener (this);
    setLookAndFeel (nullptr);
}

void MicroModAudioProcessorEditor::chainChanged()
{
    resized();
}

void MicroModAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));

    auto header = getLocalBounds().removeFromTop (kHeaderHeight);
    juce::ColourGradient grad (juce::Colour (0xff202636), 0, 0,
                               juce::Colour (0xff14171e), 0, (float) kHeaderHeight, false);
    g.setGradientFill (grad);
    g.fillRect (header);

    g.setColour (juce::Colour (0xff2a3245));
    g.fillRect (0, kHeaderHeight - 1, getWidth(), 1);
}

void MicroModAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kHeaderHeight);

    titleLabel.setBounds    (header.removeFromLeft (180).reduced (16, 6).withTrimmedTop (4));
    subtitleLabel.setBounds (titleLabel.getBounds().withY (titleLabel.getBottom() - 4).withHeight (16).withWidth (260));

    auto right = header.removeFromRight (160).reduced (12, 12);
    addButton.setBounds (right);

    viewport.setBounds (r);
    if (grid)
    {
        const int w = juce::jmax (grid->getRequiredWidth(), viewport.getWidth());
        const int h = viewport.getHeight() - viewport.getScrollBarThickness();
        grid->setSize (w, h);
    }
}

void MicroModAudioProcessorEditor::showAddMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < (int) mm::ModuleType::NumTypes; ++i)
    {
        const auto t = (mm::ModuleType) i;
        const bool full = ! audioProcessor.canAdd (t);
        juce::PopupMenu::Item item (mm::moduleTypeName (t)
            + (full ? " (max 4)" : ""));
        item.itemID = i + 1;
        item.isEnabled = ! full;
        menu.addItem (item);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton),
                        [this] (int result)
                        {
                            if (result <= 0) return;
                            audioProcessor.addModule ((mm::ModuleType) (result - 1));
                        });
}
