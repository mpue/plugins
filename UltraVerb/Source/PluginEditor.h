/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "UI/MainUI.h"
#include "PluginProcessor.h"
#include "TrioLookAndFeel.h"

//==============================================================================
/**
*/
class UltraVerbAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    UltraVerbAudioProcessorEditor (UltraVerbAudioProcessor&);
    ~UltraVerbAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void timerCallback() override
    {
        mainUi->spectrum->setFFTData(audioProcessor.getFFTData());
        audioProcessor.getAnalyzer().performFFT(mainUi->particleSystemComponent.get());
    }

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    UltraVerbAudioProcessor& audioProcessor;
    std::unique_ptr<MainUI> mainUi;
    TrioLookAndFeel tlf;
    ResizableCornerComponent* resizer;
    ComponentBoundsConstrainer resizeLimits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UltraVerbAudioProcessorEditor)
};
