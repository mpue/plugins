/*
  ==============================================================================

    PluginEditor.h
    SA-1 Luxury Quick Sampler — main editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "PresetBar.h"
#include "SamplerComponents.h"

class SA1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit SA1AudioProcessorEditor (SA1AudioProcessor&);
    ~SA1AudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void onPadSelectionChanged (int padIndex);
    void onAllPadsChanged();

    SA1AudioProcessor&     audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    SA1::PresetBar           presetBar;
    SA1::PadGrid             padGrid;
    SA1::BigWaveformDisplay  bigWaveform;
    SA1::PadParameterPanel   paramPanel;

    juce::Rectangle<int>     headerBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA1AudioProcessorEditor)
};
