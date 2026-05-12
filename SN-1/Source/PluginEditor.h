/*
  ==============================================================================

    PluginEditor.h
    SN-1 Luxury Snare Machine — main editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "PresetBar.h"
#include "SnareScope.h"

class SN1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private juce::Timer,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SN1AudioProcessorEditor (SN1AudioProcessor&);
    ~SN1AudioProcessorEditor() override;

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

    SN1AudioProcessor&     audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PresetBar  presetBar;
    SnareScope scope;

    juce::TextButton auditionButton;

    KnobControl tune, pitchAmt, pitchTime, bodyDecay, bodyLevel;
    KnobControl noiseLevel, noiseColor, noiseDecay, buzzQ, snapLevel;
    KnobControl drive, tone, output;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> tuneAtt, pitchAmtAtt, pitchTimeAtt, bodyDecayAtt, bodyLevelAtt;
    std::unique_ptr<SAtt> noiseLevelAtt, noiseColorAtt, noiseDecayAtt, buzzQAtt, snapLevelAtt;
    std::unique_ptr<SAtt> driveAtt, toneAtt, outputAtt;

    std::atomic<bool> paramsDirty { true };

    juce::Rectangle<int> tonePanelBounds;
    juce::Rectangle<int> snaresPanelBounds;
    juce::Rectangle<int> masterPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SN1AudioProcessorEditor)
};
