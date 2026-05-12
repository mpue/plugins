/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "GrainVisualizer.h"

class GS1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 public juce::ChangeListener
{
public:
    explicit GS1AudioProcessorEditor (GS1AudioProcessor&);
    ~GS1AudioProcessorEditor() override;

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

    GS1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    GrainVisualizer visualizer;

    // Header
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // Preset bar
    juce::ComboBox  presetCombo;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton saveBtn { "Save" };
    juce::TextButton deleteBtn { "Delete" };
    juce::TextButton folderBtn { "Folder" };

    // Source selector + octave
    juce::ComboBox  sourceCombo;
    juce::Label     sourceLabel;
    std::unique_ptr<APVTS::ComboBoxAttachment> sourceAttach;

    juce::Slider    octaveSlider;
    juce::Label     octaveLabel;
    std::unique_ptr<APVTS::SliderAttachment> octaveAttach;

    // Knobs - GRAIN row
    Knob positionKnob, sprayKnob, grainKnob, densityKnob, pitchKnob, pitchSprayKnob;
    // Knobs - MOTION & CHARACTER row
    Knob movementKnob, panKnob, reverseKnob, toneKnob, attackKnob, releaseKnob;
    // Knobs - SPACE row
    Knob lushKnob, spaceKnob, widthKnob, driveKnob, lfoKnob, volumeKnob;

    // Keyboard
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GS1AudioProcessorEditor)
};
