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
#include "gui/MsegPage.h"
#include "gui/MixEqPage.h"
#include "gui/MidiLearnOverlay.h"
#include "Licensing/ActivationOverlay.h"

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

    /** Invoked after the license state changes from here (e.g. a deactivation),
        so the editor can re-show the activation overlay and re-mute audio. */
    void setLicenseChangedCallback (std::function<void()> cb) { onLicenseChanged = std::move (cb); }

private:
    void showLicenseMenu();
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

    std::vector<juce::Component::SafePointer<juce::Component>> learnWidgets;
    std::unique_ptr<pike::gui::MidiLearnOverlay> learnOverlay;

    PikeAudioProcessor& audioProcessor;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    juce::ComboBox   presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    juce::TextButton licenseButton { "License" };
    std::function<void()> onLicenseChanged;
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
    /** Shows the modal-style activation overlay over the whole editor and wires
        up the success callback (hide overlay + refresh the processor's gate). */
    void showLicenseOverlay (bool reactivation);

    PikeAudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;
    PikeContent content;
    std::unique_ptr<pike::gui::ActivationOverlay> licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PikeAudioProcessorEditor)
};
