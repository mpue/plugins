/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "ReverbVisualizer.h"

class RV1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 public juce::ChangeListener
{
public:
    explicit RV1AudioProcessorEditor (RV1AudioProcessor&);
    ~RV1AudioProcessorEditor() override;

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

    RV1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    ReverbVisualizer visualizer;

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

    // Knobs
    Knob sizeKnob, decayKnob, dampKnob, predelayKnob, modKnob, mixKnob;

    // Secondary controls
    Knob diffKnob, widthKnob, lowKnob, highKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RV1AudioProcessorEditor)
};
