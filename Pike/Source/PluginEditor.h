/*
  ==============================================================================

    PluginEditor.h
    The editor uses a FIXED design layout (PikeContent, designW x designH) and
    scales it as a whole via an affine transform when the window is resized
    (constrained to the design aspect ratio) — no reflow, no scrolling.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "gui/PikeUI.h"
#include "gui/Visualisers.h"
#include "gui/OscillatorsPage.h"
#include "gui/FilterEnvPage.h"
#include "gui/ModPage.h"
#include "gui/MixEqPage.h"

//==============================================================================
/** All UI laid out at a fixed design size; the editor scales this component. */
class PikeContent : public juce::Component
{
public:
    explicit PikeContent (PikeAudioProcessor&);

    static constexpr int designW = 1100;
    static constexpr int designH = 720;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshPresets();
    void loadPresetAtComboId (int comboId);
    void stepPreset (int direction);
    void showSaveDialog();

    // MIDI Learn: right-click any parameter widget.
    void installMidiLearn (juce::Component& c);
    void handleMidiLearnClick (const juce::MouseEvent& e);
    void showMidiLearnMenu (const juce::String& paramId);

    struct LearnListener : juce::MouseListener
    {
        explicit LearnListener (PikeContent& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent& e) override { owner.handleMidiLearnClick (e); }
        PikeContent& owner;
    };
    LearnListener learnListener { *this };

    PikeAudioProcessor& audioProcessor;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    juce::ComboBox   presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    struct PresetItem { bool factory; juce::String name; };
    std::vector<PresetItem> presetItems;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    std::unique_ptr<pike::gui::Oscilloscope> scope;
    std::unique_ptr<pike::gui::LevelMeter>   meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PikeContent)
};

//==============================================================================
class PikeAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit PikeAudioProcessorEditor (PikeAudioProcessor&);
    ~PikeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PikeAudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;
    PikeContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PikeAudioProcessorEditor)
};
