/*
  ==============================================================================

    PluginEditor.h
    ARP-1 Luxury Arpeggiator

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "ArpComponents.h"
#include "PresetBar.h"
#include "PatternBar.h"

//==============================================================================
class ARP1AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    ARP1AudioProcessorEditor (ARP1AudioProcessor&);
    ~ARP1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ARP1AudioProcessor& audioProcessor;

    ElegantDarkLookAndFeel lookAndFeel;

    ARP1::PresetBar      presetBar;
    ARP1::ArpVisualizer  visualizer;
    ARP1::ControlPanel   controlPanel;
    ARP1::PatternBar     patternBar;
    ARP1::StepLane       stepLane;

    juce::Rectangle<int> headerBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARP1AudioProcessorEditor)
};
