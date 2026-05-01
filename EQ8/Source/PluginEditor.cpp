/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EQ8AudioProcessorEditor::EQ8AudioProcessorEditor (EQ8AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel(&elegantLookAndFeel);

    auto& bands = audioProcessor.getEQBands();
    auto& analyzer = audioProcessor.getSpectrumAnalyzer();

    presetBar = std::make_unique<PresetBar>(audioProcessor.getPresetManager());
    addAndMakeVisible(*presetBar);

    presetBar->onPresetChanged = [this]()
    {
        if (controlPanel)
            controlPanel->updateAllControls();
        if (eqVisualizer)
            eqVisualizer->repaint();
    };

    eqVisualizer = std::make_unique<EQVisualizer>(bands, analyzer);
    addAndMakeVisible(*eqVisualizer);

    eqVisualizer->onBandChanged = [this](int bandIndex)
    {
        if (controlPanel)
            controlPanel->updateAllControls();
    };

    controlPanel = std::make_unique<EQControlPanel>(bands);
    addAndMakeVisible(*controlPanel);

    controlPanel->onAnyParameterChanged = [this]()
    {
        if (eqVisualizer)
            eqVisualizer->repaint();
    };

    setSize(1400, 800);
    setResizable(true, true);
    setResizeLimits(1000, 600, 2000, 1200);
}

EQ8AudioProcessorEditor::~EQ8AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void EQ8AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(elegantLookAndFeel.findColour(juce::ResizableWindow::backgroundColourId));
}

void EQ8AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    presetBar->setBounds(bounds.removeFromTop(36));
    bounds.removeFromTop(8);

    auto visualizerHeight = bounds.getHeight() * 0.55f;
    auto visualizerArea = bounds.removeFromTop(static_cast<int>(visualizerHeight));
    eqVisualizer->setBounds(visualizerArea);

    bounds.removeFromTop(10);

    controlPanel->setBounds(bounds);
}
