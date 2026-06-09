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

    PikeAudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PikeAudioProcessorEditor)
};
