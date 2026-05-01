/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "PitchVisualizer.h"

class PS1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 public juce::ChangeListener
{
public:
    explicit PS1AudioProcessorEditor (PS1AudioProcessor&);
    ~PS1AudioProcessorEditor() override;

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

    void configureKnob (Knob& k, const juce::String& paramID, const juce::String& displayName,
                        const juce::String& suffix = juce::String());
    void rebuildPresetCombo();
    void onPresetComboChanged();
    void showSavePresetDialog();
    void showDeletePresetDialog();
    void openPresetsFolder();
    void addQuickButtons();

    PS1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PitchVisualizer visualizer;

    // Top header
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // Preset bar
    juce::ComboBox  presetCombo;
    juce::TextButton prevBtn  { "<" };
    juce::TextButton nextBtn  { ">" };
    juce::TextButton saveBtn  { "Save" };
    juce::TextButton deleteBtn{ "Delete" };
    juce::TextButton folderBtn{ "Folder" };

    // Quick interval buttons (just set the pitch parameter)
    struct QuickButton { juce::TextButton btn; int semitones = 0; };
    std::vector<std::unique_ptr<QuickButton>> quickButtons;

    // Character selector
    juce::ComboBox characterCombo;
    juce::Label    characterLabel;
    std::unique_ptr<APVTS::ComboBoxAttachment> characterAttach;

    // Main control row
    Knob pitchKnob, fineKnob, formantKnob, widthKnob, feedbackKnob, mixKnob;

    // Bottom secondary row
    Knob lowKnob, highKnob, qualityKnob, driveKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PS1AudioProcessorEditor)
};
