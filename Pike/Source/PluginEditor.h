/*
  ==============================================================================

    PluginEditor.h
    Top-level editor for Pike. Phase 1: branded, resizable shell that lays out
    the sections the synth will be built into. Uses ElegantDarkLookAndFeel.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"

//==============================================================================
/** A titled panel used as a placeholder for a future synth section. */
class SectionPanel : public juce::Component
{
public:
    explicit SectionPanel (juce::String titleText) : title (std::move (titleText)) {}

    void paint (juce::Graphics& g) override;

private:
    juce::String title;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionPanel)
};

//==============================================================================
class PikeAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit PikeAudioProcessorEditor (PikeAudioProcessor&);
    ~PikeAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void paintHeader (juce::Graphics&, juce::Rectangle<int> headerArea);

    PikeAudioProcessor& audioProcessor;

    ElegantDarkLookAndFeel lookAndFeel;

    // Placeholder sections that sketch the eventual layout.
    SectionPanel oscSection   { "OSCILLATORS" };
    SectionPanel mixerSection { "MIXER" };
    SectionPanel filterSection{ "FILTER" };
    SectionPanel envSection   { "ENVELOPES" };
    SectionPanel lfoSection   { "LFOs" };
    SectionPanel modSection   { "MOD MATRIX" };
    SectionPanel fxSection    { "FX" };
    SectionPanel arpSection   { "ARP" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PikeAudioProcessorEditor)
};
