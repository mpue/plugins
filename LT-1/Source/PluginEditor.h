/*
  ==============================================================================

    LT-1 — Luxury Limiter
    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "ElegantDarkLookAndFeel.h"

//==============================================================================
// Vertical level meter with peak hold and a soft glow.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    enum class Style { input, output, gainReduction };

    LevelMeter (std::atomic<float>& src, Style s, juce::Colour accent);
    ~LevelMeter() override;

    void paint (juce::Graphics& g) override;

    /** For GR meters: the peak-hold tail so the user can see brief pumps. */
    void setPeakSource (std::atomic<float>* peakSrc) { peakSource = peakSrc; }

private:
    void timerCallback() override;

    std::atomic<float>& source;
    std::atomic<float>* peakSource = nullptr;
    Style style;
    juce::Colour accentColour;

    float displayed = 0.0f;
    float peakDisplay = 0.0f;
};

//==============================================================================
// Real-time scope: shows the input waveform overlaid with the gain-reduction
// envelope so the limiter's effect is clearly visible.
class LimiterScope : public juce::Component, private juce::Timer
{
public:
    explicit LimiterScope (LT1AudioProcessor& p);
    ~LimiterScope() override;

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    LT1AudioProcessor& processor;
    std::vector<float> inSnap, outSnap, gainSnap;
};

//==============================================================================
// One labelled knob with its APVTS attachment, used many times in the layout.
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (juce::AudioProcessorValueTreeState& s,
                 const juce::String& paramId,
                 const juce::String& displayName);

    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// The little "preset" toolbar at the top: combo, ◀ ▶, Save, Save As, Delete, Init.
class PresetBar : public juce::Component, private juce::ChangeListener
{
public:
    PresetBar (LT1AudioProcessor& p);
    ~PresetBar() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshList();

    LT1AudioProcessor& processor;
    juce::ComboBox     presetBox;
    juce::TextButton   prevButton  { "<" };
    juce::TextButton   nextButton  { ">" };
    juce::TextButton   saveButton  { "Save" };
    juce::TextButton   saveAsButton{ "Save As..." };
    juce::TextButton   deleteButton{ "Delete" };
    juce::TextButton   initButton  { "Init" };
};

//==============================================================================
class LT1AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit LT1AudioProcessorEditor (LT1AudioProcessor&);
    ~LT1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LT1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PresetBar  presetBar;

    LabeledKnob threshold, ceiling, release, knee, inGain, outGain;

    juce::ToggleButton lookaheadToggle  { "Lookahead" };
    juce::ToggleButton autoReleaseToggle{ "Auto Release" };
    juce::ToggleButton stereoLinkToggle { "Stereo Link" };
    juce::ToggleButton bypassToggle     { "Bypass" };

    using BAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<BAttach> lookaheadAttach, autoReleaseAttach, stereoLinkAttach, bypassAttach;

    LevelMeter meterInL,  meterInR;
    LevelMeter meterOutL, meterOutR;
    LevelMeter meterGR;

    LimiterScope scope;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LT1AudioProcessorEditor)
};
