/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "FlangerPhaserVisualizer.h"
#include "PresetBar.h"

class FP1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit FP1AudioProcessorEditor (FP1AudioProcessor&);
    ~FP1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct LabelledKnob : public juce::Component
    {
        LabelledKnob (const juce::String& name)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
            addAndMakeVisible (slider);

            label.setText (name, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::Font (12.0f, juce::Font::FontStyleFlags::plain));
            addAndMakeVisible (label);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (16));
            slider.setBounds (r);
        }

        juce::Slider slider;
        juce::Label  label;
    };

    void buildKnob (LabelledKnob& k, const juce::String& paramID,
                    std::unique_ptr<SliderAttachment>& attachment);

    void timerCallback() override;
    void setMode (int newMode);
    void syncModeButtons();

    FP1AudioProcessor& audioProcessor;

    ElegantDarkLookAndFeel lookAndFeel;

    // Header
    juce::Label   titleLabel;
    juce::Label   subtitleLabel;
    PresetBar     presetBar;

    // Visualizer
    FlangerPhaserVisualizer visualizer;

    // Mode selector — three big toggle-style buttons
    juce::TextButton flangerBtn { "Flanger" };
    juce::TextButton phaserBtn  { "Phaser"  };
    juce::TextButton hybridBtn  { "Hybrid"  };

    // Knobs
    LabelledKnob rateKnob     { "Rate" };
    LabelledKnob depthKnob    { "Depth" };
    LabelledKnob manualKnob   { "Manual" };
    LabelledKnob feedbackKnob { "Feedback" };
    LabelledKnob widthKnob    { "Width" };
    LabelledKnob toneKnob     { "Tone" };
    LabelledKnob mixKnob      { "Mix" };
    LabelledKnob outKnob      { "Output" };

    // Right-column selectors
    juce::Label    shapeLabel;
    juce::ComboBox shapeBox;
    juce::Label    stagesLabel;
    juce::ComboBox stagesBox;

    juce::ToggleButton bypassButton { "Bypass" };

    std::unique_ptr<SliderAttachment>   rateAtt, depthAtt, manualAtt, feedbackAtt,
                                        widthAtt, toneAtt, mixAtt, outAtt;
    std::unique_ptr<ComboBoxAttachment> shapeAtt, stagesAtt;
    std::unique_ptr<ButtonAttachment>   bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FP1AudioProcessorEditor)
};
