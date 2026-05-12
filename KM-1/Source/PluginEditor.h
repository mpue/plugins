/*
  ==============================================================================

    PluginEditor.h
    KM-1 Luxury Kick Machine — main editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "PresetBar.h"
#include "KickScope.h"

class KM1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private juce::Timer,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit KM1AudioProcessorEditor (KM1AudioProcessor&);
    ~KM1AudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

private:
    struct KnobControl
    {
        juce::Label  title;
        juce::Slider slider;
    };

    void configureKnob (KnobControl& k, const juce::String& titleText);
    void layoutKnob   (KnobControl& k, juce::Rectangle<int> slot);

    void timerCallback() override;
    void parameterChanged (const juce::String& id, float newValue) override;

    void pushParamsToScope();
    void drawSectionPanel (juce::Graphics& g, juce::Rectangle<int> r,
                           const juce::String& title) const;

    KM1AudioProcessor&     audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PresetBar  presetBar;
    KickScope  scope;

    juce::TextButton auditionButton;

    KnobControl tune, pitchAmt, pitchTime, bodyDecay, bodyShape;
    KnobControl clickLevel, clickTone, subLevel;
    KnobControl drive, punch, tone, output;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> tuneAtt, pitchAmtAtt, pitchTimeAtt, bodyDecayAtt, bodyShapeAtt;
    std::unique_ptr<SAtt> clickLevelAtt, clickToneAtt, subLevelAtt;
    std::unique_ptr<SAtt> driveAtt, punchAtt, toneAtt, outputAtt;

    std::atomic<bool> paramsDirty { true };

    juce::Rectangle<int> bodyPanelBounds;
    juce::Rectangle<int> clickPanelBounds;
    juce::Rectangle<int> masterPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KM1AudioProcessorEditor)
};
