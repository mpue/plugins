/*
  ==============================================================================

    PluginEditor.h
    BS-1 Luxury Bass Synthesizer — main editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "PresetBar.h"
#include "BassScope.h"

class BS1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit BS1AudioProcessorEditor (BS1AudioProcessor&);
    ~BS1AudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

private:
    struct KnobControl
    {
        juce::Label  title;
        juce::Slider slider;
    };

    void configureKnob (KnobControl& k, const juce::String& titleText);
    void layoutKnob   (KnobControl& k, juce::Rectangle<int> slot);

    void timerCallback() override;
    void drawSectionPanel (juce::Graphics& g, juce::Rectangle<int> r,
                           const juce::String& title) const;

    BS1AudioProcessor&     audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PresetBar                   presetBar;
    BassScope                   scope;
    juce::MidiKeyboardComponent keyboard;

    juce::TextButton auditionButton;

    KnobControl tone, drive, subLevel, noiseLevel;
    KnobControl cutoff, resonance, envAmount, filterDecay;
    KnobControl ampAttack, ampSustain, ampRelease;
    KnobControl glide, warmth, output;

    juce::ComboBox octaveBox;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SAtt> toneAtt, driveAtt, subAtt, noiseAtt;
    std::unique_ptr<SAtt> cutoffAtt, resoAtt, envAtt, decAtt;
    std::unique_ptr<SAtt> atkAtt, susAtt, relAtt;
    std::unique_ptr<SAtt> glideAtt, warmthAtt, outAtt;
    std::unique_ptr<BAtt> octAtt;

    juce::Rectangle<int> tonePanelBounds;
    juce::Rectangle<int> filterPanelBounds;
    juce::Rectangle<int> envPanelBounds;
    juce::Rectangle<int> masterPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BS1AudioProcessorEditor)
};
