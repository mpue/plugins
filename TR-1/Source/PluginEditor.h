/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "TrashVisualizer.h"

class TR1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 public juce::ChangeListener
{
public:
    explicit TR1AudioProcessorEditor (TR1AudioProcessor&);
    ~TR1AudioProcessorEditor() override;

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

    void configureKnob (Knob& k, const juce::String& paramID, const juce::String& displayName);
    void rebuildPresetCombo();
    void onPresetComboChanged();
    void showSavePresetDialog();
    void showDeletePresetDialog();
    void openPresetsFolder();

    TR1AudioProcessor&     audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    TrashVisualizer visualizer;

    // Header
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // Preset bar
    juce::ComboBox  presetCombo;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton saveBtn { "Save" };
    juce::TextButton deleteBtn { "Delete" };
    juce::TextButton folderBtn { "Folder" };

    // Character selector
    juce::ComboBox  characterCombo;
    juce::Label     characterLabel;
    std::unique_ptr<APVTS::ComboBoxAttachment> characterAttach;

    // Primary knobs (top row, big)
    Knob driveKnob, crunchKnob, toneKnob, bodyKnob, motionKnob, mixKnob;

    // Secondary controls (bottom row)
    Knob textureKnob, motionRateKnob, ageKnob, widthKnob, outputKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TR1AudioProcessorEditor)
};
