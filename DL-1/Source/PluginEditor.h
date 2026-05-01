/*
  ==============================================================================

    PluginEditor.h
    DL-1 — Luxury Stereo Delay Editor

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "DelayVisualizer.h"
#include "PresetManager.h"

class DL1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 public PresetManager::Listener
{
public:
    DL1AudioProcessorEditor (DL1AudioProcessor&);
    ~DL1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // PresetManager::Listener
    void presetListChanged() override;
    void currentPresetChanged(const juce::String& name) override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct LabeledKnob : public juce::Component
    {
        LabeledKnob(const juce::String& labelText)
        {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
            slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
            addAndMakeVisible(slider);

            label.setText(labelText, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setColour(juce::Label::textColourId, juce::Colour(0xff9bb6d8));
            label.setFont(juce::Font(11.0f, juce::Font::bold));
            addAndMakeVisible(label);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds(b.removeFromTop(16));
            slider.setBounds(b);
        }

        void paint(juce::Graphics& g) override
        {
            // subtle backplate
            auto b = getLocalBounds().toFloat().reduced(2.0f);
            g.setColour(juce::Colour(0x14ffffff));
            g.fillRoundedRectangle(b, 8.0f);
            g.setColour(juce::Colour(0x18b8d4f0));
            g.drawRoundedRectangle(b, 8.0f, 1.0f);
        }

        juce::Slider slider;
        juce::Label  label;
    };

    DL1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel laf;

    // Header
    juce::Label    titleLabel;
    juce::Label    subtitleLabel;
    juce::ComboBox presetCombo;
    juce::TextButton prevPresetBtn { "<" };
    juce::TextButton nextPresetBtn { ">" };
    juce::TextButton savePresetBtn { "Save" };
    juce::TextButton saveAsPresetBtn { "Save As" };
    juce::TextButton deletePresetBtn { "Delete" };

    // Visualizer
    std::unique_ptr<DelayVisualizer> visualizer;

    // Knobs - main row
    LabeledKnob inGainKnob   { "INPUT" };
    LabeledKnob timeLKnob    { "TIME L" };
    LabeledKnob timeRKnob    { "TIME R" };
    LabeledKnob feedbackKnob { "FEEDBACK" };
    LabeledKnob crossKnob    { "CROSSFEED" };
    LabeledKnob mixKnob      { "MIX" };
    LabeledKnob outGainKnob  { "OUTPUT" };

    // Knobs - tone & character row
    LabeledKnob lowCutKnob   { "LOW CUT" };
    LabeledKnob highCutKnob  { "HIGH CUT" };
    LabeledKnob driveKnob    { "DRIVE" };
    LabeledKnob modRateKnob  { "MOD RATE" };
    LabeledKnob modDepthKnob { "MOD DEPTH" };
    LabeledKnob widthKnob    { "WIDTH" };
    LabeledKnob duckingKnob  { "DUCKING" };

    // Sync controls
    juce::ToggleButton linkBtn  { "Link" };
    juce::ToggleButton syncBtn  { "Sync" };
    juce::ComboBox     divLCombo;
    juce::ComboBox     divRCombo;
    juce::Label        divLLabel { {}, "Div L" };
    juce::Label        divRLabel { {}, "Div R" };

    // Attachments
    std::unique_ptr<SliderAttachment> inGainAt, outGainAt, timeLAt, timeRAt, fbAt, xfAt, mixAt;
    std::unique_ptr<SliderAttachment> lowCutAt, highCutAt, driveAt, modRateAt, modDepthAt, widthAt, duckingAt;
    std::unique_ptr<ButtonAttachment> linkAt, syncAt;
    std::unique_ptr<ComboBoxAttachment> divLAt, divRAt;

    // Helpers
    void configureSlider(LabeledKnob& k, const juce::String& suffix);
    void rebuildPresetCombo();
    void showSaveAsDialog();
    void confirmAndDelete();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DL1AudioProcessorEditor)
};
