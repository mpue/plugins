/*
  ==============================================================================

    PluginEditor.h
    CD-1 Cinematic Drums — main editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"
#include "PresetBar.h"
#include "DrumScope.h"

class CD1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit CD1AudioProcessorEditor (CD1AudioProcessor&);
    ~CD1AudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

private:
    struct KnobControl
    {
        juce::Label  title;
        juce::Slider slider;
    };

    void configureKnob (KnobControl& k, const juce::String& titleText, juce::Colour accent);
    void layoutKnob   (KnobControl& k, juce::Rectangle<int> slot);

    void timerCallback() override;

    void drawHeader      (juce::Graphics& g, juce::Rectangle<int> r);
    void drawSectionPanel (juce::Graphics& g, juce::Rectangle<int> r,
                            const juce::String& title, juce::Colour accent) const;

    CD1AudioProcessor&     audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PresetBar  presetBar;
    DrumScope  scope;

    // ---- per-drum knob clusters (4 drums × 4 knobs) ----
    struct DrumStrip
    {
        juce::String name;
        juce::Colour accent;
        KnobControl  tune, decay, level, pan;
        juce::TextButton auditionBtn;
        juce::Rectangle<int> bounds;
    };
    DrumStrip drumStrips[cd1::NumDrums];

    // ---- master macro knobs ----
    KnobControl depth, impact, air, drive, width, size, tone, output;
    KnobControl rvSize, rvDamp, rvLow;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> tuneAtt[cd1::NumDrums];
    std::unique_ptr<SAtt> decayAtt[cd1::NumDrums];
    std::unique_ptr<SAtt> levelAtt[cd1::NumDrums];
    std::unique_ptr<SAtt> panAtt  [cd1::NumDrums];

    std::unique_ptr<SAtt> depthAtt, impactAtt, airAtt, driveAtt;
    std::unique_ptr<SAtt> widthAtt, sizeAtt,  toneAtt, outputAtt;
    std::unique_ptr<SAtt> rvSizeAtt, rvDampAtt, rvLowAtt;

    juce::Rectangle<int> drumPanelBounds;
    juce::Rectangle<int> macroPanelBounds;
    juce::Rectangle<int> reverbPanelBounds;

    std::vector<float> displayBufferL, displayBufferR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CD1AudioProcessorEditor)
};
