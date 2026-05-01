/*
  ==============================================================================

    PluginEditor.h
    BC-1 Luxury Bitcrusher editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "CrushScope.h"
#include "PresetBar.h"

class BC1AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit BC1AudioProcessorEditor (BC1AudioProcessor&);
    ~BC1AudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

private:
    struct KnobControl
    {
        juce::Label   title;
        juce::Slider  slider;
    };

    void configureKnob (KnobControl& k, const juce::String& titleText);
    void layoutKnob   (KnobControl& k, juce::Rectangle<int> slot);

    BC1AudioProcessor&     audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PresetBar  presetBar;
    CrushScope scope;

    juce::ToggleButton bypassButton;

    KnobControl drive, bits, rate, dither, tone, mix, output;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SAtt> driveAtt, bitsAtt, rateAtt, ditherAtt, toneAtt, mixAtt, outputAtt;
    std::unique_ptr<BAtt> bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BC1AudioProcessorEditor)
};
