/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "ConvolutionVisualizer.h"

class CRV1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                  public juce::ChangeListener
{
public:
    explicit CRV1AudioProcessorEditor (CRV1AudioProcessor&);
    ~CRV1AudioProcessorEditor() override;

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
    void rebuildIRCombo();
    void onPresetComboChanged();
    void onIRComboChanged();
    void showSavePresetDialog();
    void showDeletePresetDialog();
    void openPresetsFolder();
    void loadCustomIRFromDisk();
    void deleteSelectedUserIR();
    void openIRFolder();

    void irRenderedFromProcessor (const juce::AudioBuffer<float>& ir,
                                  double sampleRate,
                                  const juce::String& info);

    CRV1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    ConvolutionVisualizer visualizer;

    // Header
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // Preset bar
    juce::ComboBox  presetCombo;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton saveBtn { "Save" };
    juce::TextButton deleteBtn { "Delete" };
    juce::TextButton folderBtn { "Folder" };

    // IR selector row
    juce::Label    irLabel;
    juce::ComboBox irCombo;
    juce::TextButton loadIRBtn  { "Load IR..." };
    juce::TextButton delIRBtn   { "Delete IR" };
    juce::TextButton irFolderBtn { "IR Folder" };

    // Knobs
    Knob sizeKnob, decayKnob, predelayKnob, modKnob, mixKnob;
    Knob lowKnob, highKnob, widthKnob, outputKnob;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CRV1AudioProcessorEditor)
};
