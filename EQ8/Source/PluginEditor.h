/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "EQVisualizer.h"
#include "EQControlPanel.h"
#include "PresetBar.h"

//==============================================================================
/**
*/
class EQ8AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    EQ8AudioProcessorEditor (EQ8AudioProcessor&);
    ~EQ8AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQ8AudioProcessor& audioProcessor;

    ElegantDarkLookAndFeel elegantLookAndFeel;

    std::unique_ptr<PresetBar> presetBar;
    std::unique_ptr<EQVisualizer> eqVisualizer;
    std::unique_ptr<EQControlPanel> controlPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ8AudioProcessorEditor)
};
