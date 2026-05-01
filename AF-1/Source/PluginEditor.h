/*
  ==============================================================================

    AF-1 — Luxurious AutoFilter
    Editor (UI)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "FilterVisualizer.h"
#include "PresetManager.h"

class AF1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private PresetManager::Listener
{
public:
    explicit AF1AudioProcessorEditor (AF1AudioProcessor&);
    ~AF1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // PresetManager::Listener
    void presetListChanged() override;
    void currentPresetChanged (const juce::String& name) override;

    // ──────────────────────────────────────────────
    // Section panel container (for nice luxurious framing)
    struct Section : public juce::Component
    {
        juce::String title;
        explicit Section (const juce::String& t) : title (t) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();

            // Subtle inner panel
            juce::ColourGradient grad (juce::Colour (0xff272d36), r.getCentreX(), r.getY(),
                                       juce::Colour (0xff181c22), r.getCentreX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 8.0f);

            // Frame
            g.setColour (juce::Colour (0xff2c333d));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

            // Highlight top-edge
            g.setColour (juce::Colour (0x224d9eff));
            g.drawLine (r.getX() + 8.0f, r.getY() + 1.0f, r.getRight() - 8.0f, r.getY() + 1.0f, 1.0f);

            // Title plate
            const auto titleBox = r.removeFromTop (24.0f).reduced (10.0f, 4.0f);
            g.setColour (juce::Colour (0xff7da2d4));
            g.setFont (juce::Font (12.5f, juce::Font::bold));
            g.drawText (title.toUpperCase(), titleBox.toNearestInt(), juce::Justification::centredLeft);

            // Hairline under title
            g.setColour (juce::Colour (0xff262d36));
            g.drawLine (r.getX() + 8.0f, r.getY() + 28.0f, r.getRight() - 8.0f, r.getY() + 28.0f, 1.0f);
        }
    };

    // Labelled rotary slider with classy small caption underneath
    struct LabeledKnob : public juce::Component
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label  caption;

        explicit LabeledKnob (const juce::String& text)
        {
            slider.setPopupDisplayEnabled (true, false, nullptr);
            slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x00000000));
            slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour (0x00000000));
            slider.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffaab8c8));
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
            addAndMakeVisible (slider);

            caption.setText (text, juce::dontSendNotification);
            caption.setJustificationType (juce::Justification::centred);
            caption.setColour (juce::Label::textColourId, juce::Colour (0xff8b9bb0));
            caption.setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (caption);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            caption.setBounds (r.removeFromBottom (16));
            slider .setBounds (r);
        }
    };

    // ──────────────────────────────────────────────
    AF1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel laf;

    PresetManager presetManager;

    // Preset bar
    juce::Label       titleLabel;
    juce::Label       subtitleLabel;
    juce::ComboBox    presetCombo;
    juce::TextButton  prevButton   { "<" };
    juce::TextButton  nextButton   { ">" };
    juce::TextButton  saveAsButton { "Save As" };
    juce::TextButton  saveButton   { "Save" };
    juce::TextButton  deleteButton { "Delete" };
    juce::TextButton  initButton   { "Init" };

    // Filter visualizer
    FilterVisualizer visualizer;

    // Sections
    Section filterSection { "Filter" };
    Section lfoSection    { "LFO" };
    Section envSection    { "Envelope" };
    Section outSection    { "Output" };

    // Filter controls
    LabeledKnob cutoff    { "Cutoff" };
    LabeledKnob resonance { "Resonance" };
    LabeledKnob drive     { "Drive" };
    juce::ComboBox filterTypeBox;
    juce::ComboBox slopeBox;

    // LFO
    LabeledKnob lfoRate  { "Rate" };
    LabeledKnob lfoDepth { "Depth" };
    juce::ComboBox lfoShapeBox;

    // Env
    LabeledKnob envAmount  { "Amount" };
    LabeledKnob envAttack  { "Attack" };
    LabeledKnob envRelease { "Release" };

    // Output
    LabeledKnob mix    { "Mix" };
    LabeledKnob output { "Output" };

    // Attachments
    std::unique_ptr<SliderAttachment> aCutoff, aResonance, aDrive,
                                       aLfoRate, aLfoDepth,
                                       aEnvAmount, aEnvAttack, aEnvRelease,
                                       aMix, aOutput;
    std::unique_ptr<ComboAttachment>  aFilterType, aSlope, aLfoShape;

    // Helpers
    void setupKnob (LabeledKnob& k, const juce::String& paramId,
                    std::unique_ptr<SliderAttachment>& attachment);
    void setupCombo (juce::ComboBox& box, const juce::String& paramId,
                     const juce::StringArray& items,
                     std::unique_ptr<ComboAttachment>& attachment);

    void rebuildPresetCombo();
    void onPresetSelected();
    void onSaveAsClicked();
    void onSaveClicked();
    void onDeleteClicked();
    void onInitClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AF1AudioProcessorEditor)
};
