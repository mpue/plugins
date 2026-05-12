/*
  ==============================================================================

    PluginEditor.h
    HH-1 Luxury Hi-Hat Machine — main editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "PresetBar.h"
#include "HiHatScope.h"

class HH1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private juce::Timer,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit HH1AudioProcessorEditor (HH1AudioProcessor&);
    ~HH1AudioProcessorEditor() override;

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

    HH1AudioProcessor&     audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PresetBar  presetBar;
    HiHatScope scope;

    juce::TextButton auditionButton;

    KnobControl tune, metal, harmonics, hpCut, bpCut, shimmerQ;
    KnobControl decay, hold, noise, color;
    KnobControl drive, tone, width, output;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> tuneAtt, metalAtt, harmonicsAtt, hpCutAtt, bpCutAtt, shimmerQAtt;
    std::unique_ptr<SAtt> decayAtt, holdAtt, noiseAtt, colorAtt;
    std::unique_ptr<SAtt> driveAtt, toneAtt, widthAtt, outputAtt;

    std::atomic<bool> paramsDirty { true };

    juce::Rectangle<int> metalPanelBounds;
    juce::Rectangle<int> texturePanelBounds;
    juce::Rectangle<int> masterPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HH1AudioProcessorEditor)
};
