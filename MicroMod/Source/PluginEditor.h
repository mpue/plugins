/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ModuleGrid.h"
#include "ElegantDarkLookAndFeel.h"

class MicroModAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                      public ChainListener
{
public:
    MicroModAudioProcessorEditor (MicroModAudioProcessor&);
    ~MicroModAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void chainChanged() override;

private:
    void showAddMenu();

    MicroModAudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel  laf;

    juce::Label   titleLabel;
    juce::Label   subtitleLabel;
    juce::TextButton addButton { "+ Add Module" };

    juce::Viewport viewport;
    std::unique_ptr<ModuleGrid> grid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroModAudioProcessorEditor)
};
