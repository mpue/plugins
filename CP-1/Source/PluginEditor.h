/*
  ==============================================================================
    CP-1 Compressor — Editor
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"

class CP1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    CP1AudioProcessorEditor (CP1AudioProcessor&);
    ~CP1AudioProcessorEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    // Custom meters (kept inside the editor — read live from the processor)
    class GRMeter : public juce::Component
    {
    public:
        float grDb { 0.0f };           // positive dB
        float maxDb { 24.0f };
        void paint (juce::Graphics&) override;
    };

    class LevelMeter : public juce::Component
    {
    public:
        float levelDb { -60.0f };
        float minDb { -60.0f };
        float maxDb {   6.0f };
        void paint (juce::Graphics&) override;
    };

    //==============================================================================
    void timerCallback() override;

    void styleRotary  (juce::Slider&);
    void styleCaption (juce::Label&, const juce::String& text);
    void stylePillToggle (juce::TextButton&, const juce::String& text);
    void populatePresetBox();
    void updatePresetBox();

    CP1AudioProcessor& proc;
    ElegantDarkLookAndFeel laf;

    // Header
    juce::Label       titleLabel;
    juce::Label       subtitleLabel;
    juce::TextButton  bypassBtn;

    // Preset bar
    juce::TextButton  presetPrevBtn, presetNextBtn, presetSaveBtn, presetDeleteBtn;
    juce::ComboBox    presetBox;

    // Main knobs
    juce::Slider thresholdKnob, ratioKnob, attackKnob, releaseKnob,
                 kneeKnob, makeupKnob, mixKnob;
    juce::Label  thresholdLbl, ratioLbl, attackLbl, releaseLbl,
                 kneeLbl,      makeupLbl, mixLbl;

    // Sidechain
    juce::GroupComponent scGroup;
    juce::Slider         scHpfKnob;
    juce::Label          scHpfLbl;
    juce::TextButton     extScBtn, scListenBtn;
    juce::Label          scStatusLbl;

    // Global / Detector
    juce::GroupComponent ctrlGroup;
    juce::ComboBox       detectorBox;
    juce::Label          detectorLbl;
    juce::TextButton     stereoLinkBtn, autoRelBtn;

    // Meters
    juce::GroupComponent meterGroup;
    GRMeter              grMeter;
    LevelMeter           inputMeter, outputMeter;
    juce::Label          inMeterLbl, outMeterLbl, grMeterLbl;

    // Attachments
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAtt> thrAtt, ratioAtt, atkAtt, relAtt,
                                kneeAtt, makeupAtt, mixAtt, scHpfAtt;
    std::unique_ptr<ButtonAtt> bypassAtt, extScAtt, scListenAtt,
                                stereoLinkAtt, autoRelAtt;
    std::unique_ptr<ComboAtt>  detAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CP1AudioProcessorEditor)
};
