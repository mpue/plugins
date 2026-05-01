/*
  ==============================================================================

    EQControlPanel.h
    Created: Control Panel for individual EQ bands
    Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "EQBand.h"

class EQBandControl : public juce::Component
{
public:
    EQBandControl(int bandIndex, EQBand& band) : index(bandIndex), eqBand(band)
    {
        addAndMakeVisible(frequencySlider);
        frequencySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        frequencySlider.setRange(20.0, 20000.0, 1.0);
        frequencySlider.setSkewFactor(0.3);
        frequencySlider.setValue(band.getFrequency());
        frequencySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        frequencySlider.setTextValueSuffix(" Hz");
        frequencySlider.onValueChange = [this] { updateBandFromSliders(); };

        addAndMakeVisible(gainSlider);
        gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        gainSlider.setRange(-24.0, 24.0, 0.1);
        gainSlider.setValue(band.getGain());
        gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        gainSlider.setTextValueSuffix(" dB");
        gainSlider.onValueChange = [this] { updateBandFromSliders(); };

        addAndMakeVisible(qSlider);
        qSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        qSlider.setRange(0.1, 10.0, 0.01);
        qSlider.setValue(band.getQ());
        qSlider.setSkewFactor(0.5);
        qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        qSlider.onValueChange = [this] { updateBandFromSliders(); };

        addAndMakeVisible(enableButton);
        enableButton.setButtonText("On");
        enableButton.setToggleState(band.isEnabled(), juce::dontSendNotification);
        enableButton.onClick = [this] { updateBandFromSliders(); };

        addAndMakeVisible(freqLabel);
        freqLabel.setText("Freq", juce::dontSendNotification);
        freqLabel.setJustificationType(juce::Justification::centred);
        freqLabel.setFont(juce::Font(12.0f));

        addAndMakeVisible(gainLabel);
        gainLabel.setText("Gain", juce::dontSendNotification);
        gainLabel.setJustificationType(juce::Justification::centred);
        gainLabel.setFont(juce::Font(12.0f));

        addAndMakeVisible(qLabel);
        qLabel.setText("Q", juce::dontSendNotification);
        qLabel.setJustificationType(juce::Justification::centred);
        qLabel.setFont(juce::Font(12.0f));

        addAndMakeVisible(bandLabel);
        juce::String bandName;
        switch (band.getType())
        {
            case EQBand::LowShelf: bandName = "Low Shelf"; break;
            case EQBand::HighShelf: bandName = "High Shelf"; break;
            case EQBand::Peak: bandName = "Band " + juce::String(bandIndex + 1); break;
        }
        bandLabel.setText(bandName, juce::dontSendNotification);
        bandLabel.setJustificationType(juce::Justification::centred);
        bandLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        juce::Colour bandColour = juce::Colour(0xff4d9eff);
        if (eqBand.getType() == EQBand::LowShelf)
            bandColour = juce::Colour(0xffff6b6b);
        else if (eqBand.getType() == EQBand::HighShelf)
            bandColour = juce::Colour(0xff4ecdc4);

        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

        g.setColour(bandColour.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1), 8.0f, 2.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        auto labelArea = bounds.removeFromTop(25);
        bandLabel.setBounds(labelArea);

        bounds.removeFromTop(5);

        auto enableArea = bounds.removeFromTop(30);
        enableButton.setBounds(enableArea.reduced(20, 0));

        bounds.removeFromTop(10);

        auto row1 = bounds.removeFromTop(25);
        freqLabel.setBounds(row1);

        auto freqArea = bounds.removeFromTop(85);
        frequencySlider.setBounds(freqArea);

        bounds.removeFromTop(5);

        auto row2 = bounds.removeFromTop(25);
        gainLabel.setBounds(row2);

        auto gainArea = bounds.removeFromTop(85);
        gainSlider.setBounds(gainArea);

        bounds.removeFromTop(5);

        auto row3 = bounds.removeFromTop(25);
        qLabel.setBounds(row3);

        auto qArea = bounds.removeFromTop(85);
        qSlider.setBounds(qArea);
    }

    void updateFromBand()
    {
        frequencySlider.setValue(eqBand.getFrequency(), juce::dontSendNotification);
        gainSlider.setValue(eqBand.getGain(), juce::dontSendNotification);
        qSlider.setValue(eqBand.getQ(), juce::dontSendNotification);
        enableButton.setToggleState(eqBand.isEnabled(), juce::dontSendNotification);
    }

    std::function<void()> onParameterChanged;

private:
    void updateBandFromSliders()
    {
        eqBand.setFrequency(static_cast<float>(frequencySlider.getValue()));
        eqBand.setGain(static_cast<float>(gainSlider.getValue()));
        eqBand.setQ(static_cast<float>(qSlider.getValue()));
        eqBand.setEnabled(enableButton.getToggleState());

        if (onParameterChanged)
            onParameterChanged();
    }

    int index;
    EQBand& eqBand;

    juce::Slider frequencySlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::ToggleButton enableButton;

    juce::Label freqLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Label bandLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQBandControl)
};

class EQControlPanel : public juce::Component
{
public:
    EQControlPanel(std::array<EQBand, 8>& bands)
    {
        for (int i = 0; i < 8; ++i)
        {
            auto* control = new EQBandControl(i, bands[i]);
            bandControls.add(control);
            addAndMakeVisible(control);

            control->onParameterChanged = [this] 
            {
                if (onAnyParameterChanged)
                    onAnyParameterChanged();
            };
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int numBands = bandControls.size();
        int controlWidth = bounds.getWidth() / numBands;

        for (int i = 0; i < numBands; ++i)
        {
            auto controlBounds = bounds.removeFromLeft(controlWidth).reduced(5);
            bandControls[i]->setBounds(controlBounds);
        }
    }

    void updateAllControls()
    {
        for (auto* control : bandControls)
            control->updateFromBand();
    }

    std::function<void()> onAnyParameterChanged;

private:
    juce::OwnedArray<EQBandControl> bandControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQControlPanel)
};
