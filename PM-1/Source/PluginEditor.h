/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "PadVisualizer.h"

class PM1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 public juce::ChangeListener
{
public:
    explicit PM1AudioProcessorEditor (PM1AudioProcessor&);
    ~PM1AudioProcessorEditor() override;

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

    PM1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PadVisualizer visualizer;

    // Top header
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

    // Octave selector (slider 1-px increments)
    juce::Slider octaveSlider;
    juce::Label  octaveLabel;
    std::unique_ptr<APVTS::SliderAttachment> octaveAttach;

    // Main knobs - logical grouping
    // TONE row
    Knob textureKnob, warmthKnob, brightKnob, driveKnob;
    // ENVELOPE row
    Knob attackKnob, releaseKnob, movementKnob, lfoKnob;
    // SPACE row
    Knob lushnessKnob, delayKnob, spaceKnob, widthKnob;
    // OUT
    Knob volumeKnob;

    // On-screen keyboard
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PM1AudioProcessorEditor)
};
