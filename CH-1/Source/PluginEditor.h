/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "ChorusVisualizer.h"
#include "PresetBar.h"

class CH1AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit CH1AudioProcessorEditor (CH1AudioProcessor&);
    ~CH1AudioProcessorEditor() override;

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

    CH1AudioProcessor& audioProcessor;

    ElegantDarkLookAndFeel lookAndFeel;

    // Header
    juce::Label   titleLabel;
    juce::Label   subtitleLabel;
    PresetBar     presetBar;

    // Visualizer
    ChorusVisualizer visualizer;

    // Knobs
    LabelledKnob rateKnob     { "Rate" };
    LabelledKnob depthKnob    { "Depth" };
    LabelledKnob delayKnob    { "Delay" };
    LabelledKnob feedbackKnob { "Feedback" };
    LabelledKnob widthKnob    { "Width" };
    LabelledKnob toneKnob     { "Tone" };
    LabelledKnob mixKnob      { "Mix" };
    LabelledKnob outKnob      { "Output" };

    juce::Label    voicesLabel;
    juce::ComboBox voicesBox;

    juce::ToggleButton bypassButton { "Bypass" };

    std::unique_ptr<SliderAttachment>   rateAtt, depthAtt, delayAtt, feedbackAtt,
                                        widthAtt, toneAtt, mixAtt, outAtt;
    std::unique_ptr<ComboBoxAttachment> voicesAtt;
    std::unique_ptr<ButtonAttachment>   bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CH1AudioProcessorEditor)
};
