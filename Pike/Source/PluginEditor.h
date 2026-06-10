/*
  ==============================================================================

    PluginEditor.h
    Top-level editor: branded header + a TabbedComponent of parameter pages.
    Every control is bound to the APVTS via the data-driven PikeUI framework.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "gui/PikeUI.h"
#include "gui/Visualisers.h"
#include "gui/FilterEnvPage.h"

//==============================================================================
class PikeAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit PikeAudioProcessorEditor (PikeAudioProcessor&);
    ~PikeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void addPage (const juce::String& name, const std::vector<pike::gui::GroupSpec>& specs);

    void refreshPresets();
    void loadPresetAtComboId (int comboId);
    void stepPreset (int direction);
    void showSaveDialog();

    PikeAudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    // Preset bar.
    juce::ComboBox  presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    struct PresetItem { bool factory; juce::String name; };
    std::vector<PresetItem> presetItems;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    // Header eyecatchers.
    std::unique_ptr<pike::gui::Oscilloscope> scope;
    std::unique_ptr<pike::gui::LevelMeter>   meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PikeAudioProcessorEditor)
};
