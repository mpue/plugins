/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
UltraVerbAudioProcessorEditor::UltraVerbAudioProcessorEditor (UltraVerbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), resizer(new ResizableCornerComponent(this, &resizeLimits))
{
    juce::Desktop::getInstance().setGlobalScaleFactor(.75f);
    setLookAndFeel(&tlf);
    setSize (1280,1150);
    mainUi.reset(new MainUI(p));
    addAndMakeVisible(mainUi.get());
    setResizable(false, true);
    resizeLimits.setSizeLimits(800, 600, 1280, 1150); // Set minimum and maximum sizes

    addAndMakeVisible(resizer);
    resizer->setBounds(getWidth() - 16, getHeight() - 16, 16, 16); // Position it in the bottom right corner

    startTimerHz(30);
}

UltraVerbAudioProcessorEditor::~UltraVerbAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    delete resizer;
    resizer = nullptr;
    mainUi = nullptr;
}

//==============================================================================
void UltraVerbAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::darkgrey.darker());

}

void UltraVerbAudioProcessorEditor::resized()
{
    resizer->setBounds(getWidth() - 16, getHeight() - 16, 16, 16);
}
