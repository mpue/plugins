/*
  ==============================================================================

    PluginEditor.h
    SW-1 — Luxury Stereo Widener UI

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "StereoFieldVisualizer.h"

class SW1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 public juce::ChangeListener
{
public:
    explicit SW1AudioProcessorEditor (SW1AudioProcessor&);
    ~SW1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob
    {
        juce::Slider  slider;
        juce::Label   label;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };

    void configureKnob (Knob& k,
                        const juce::String& paramID,
                        const juce::String& displayName,
                        bool small = false);

    void rebuildPresetCombo();
    void onPresetComboChanged();
    void showSavePresetDialog();
    void showDeletePresetDialog();
    void openPresetsFolder();
    void layoutKnob (Knob& k, juce::Rectangle<int> r, int labelHeight = 16);
    void drawPanel  (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& caption);

    SW1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    StereoFieldVisualizer visualizer;

    // Header
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // Preset bar
    juce::ComboBox  presetCombo;
    juce::TextButton prevBtn   { "<" };
    juce::TextButton nextBtn   { ">" };
    juce::TextButton saveBtn   { "Save" };
    juce::TextButton deleteBtn { "Delete" };
    juce::TextButton folderBtn { "Folder" };

    // Main knobs
    Knob widthKnob, bassMonoKnob, shimmerKnob;
    Knob haasKnob, rotationKnob, outputKnob;

    // Multi-band
    Knob lowWidthKnob, midWidthKnob, highWidthKnob;
    Knob xLowKnob, xHighKnob;

    // Mix knob (bottom)
    Knob mixKnob;

    // Toggles
    juce::ToggleButton bassMonoOnToggle { "BASS MONO" };
    juce::ToggleButton bypassToggle     { "BYPASS" };
    juce::ToggleButton monoCheckToggle  { "MONO CHK" };
    std::unique_ptr<APVTS::ButtonAttachment> bassMonoOnAttach;
    std::unique_ptr<APVTS::ButtonAttachment> bypassAttach;
    std::unique_ptr<APVTS::ButtonAttachment> monoCheckAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SW1AudioProcessorEditor)
};
