/*
  ==============================================================================

    EQVisualizer.h
    Created: Visual EQ Display with Spectrum and Response Curve
    Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "EQBand.h"
#include "SpectrumAnalyzer.h"

class EQVisualizer : public juce::Component, private juce::Timer
{
public:
    EQVisualizer(std::array<EQBand, 8>& bands, SpectrumAnalyzer& analyzer)
        : eqBands(bands), spectrumAnalyzer(analyzer)
    {
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.fillAll(juce::Colour(0xff1a1a1a));

        drawGrid(g, bounds);
        drawSpectrum(g, bounds);
        drawResponseCurve(g, bounds);
        drawBandHandles(g, bounds);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto pos = e.getPosition().toFloat();

        for (int i = 0; i < 8; ++i)
        {
            auto handlePos = getHandlePosition(i);
            float distance = pos.getDistanceFrom(handlePos);

            if (distance < 15.0f)
            {
                draggedBand = i;
                return;
            }
        }

        draggedBand = -1;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (draggedBand >= 0 && draggedBand < 8)
        {
            auto bounds = getLocalBounds().toFloat();
            float x = e.position.x;
            float y = e.position.y;

            float freq = xToFrequency(x, bounds);
            float gain = yToGain(y, bounds);

            if (e.mods.isShiftDown())
            {
                float currentQ = eqBands[draggedBand].getQ();
                float deltaY = e.getDistanceFromDragStartY();
                float newQ = juce::jlimit(0.1f, 10.0f, currentQ - deltaY * 0.01f);
                eqBands[draggedBand].setQ(newQ);
            }
            else
            {
                eqBands[draggedBand].setFrequency(freq);
                eqBands[draggedBand].setGain(gain);
            }

            repaint();

            if (onBandChanged)
                onBandChanged(draggedBand);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        draggedBand = -1;
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        auto pos = e.getPosition().toFloat();

        for (int i = 0; i < 8; ++i)
        {
            auto handlePos = getHandlePosition(i);
            float distance = pos.getDistanceFrom(handlePos);

            if (distance < 15.0f)
            {
                eqBands[i].setGain(0.0f);
                repaint();

                if (onBandChanged)
                    onBandChanged(i);
                return;
            }
        }
    }

    std::function<void(int)> onBandChanged;

private:
    void timerCallback() override
    {
        repaint();
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        g.setColour(juce::Colour(0xff2a2a2a));

        const std::vector<float> freqs = {30, 60, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};

        for (float freq : freqs)
        {
            float x = frequencyToX(freq, bounds.toFloat());
            g.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(bounds.getHeight()));

            g.setColour(juce::Colour(0xff666666));
            g.setFont(10.0f);
            juce::String label = freq >= 1000 ? juce::String(freq / 1000.0f, 1) + "k" : juce::String(static_cast<int>(freq));
            g.drawText(label, static_cast<int>(x) - 20, bounds.getHeight() - 20, 40, 15, juce::Justification::centred);
            g.setColour(juce::Colour(0xff2a2a2a));
        }

        const std::vector<float> gains = {-24, -18, -12, -6, 0, 6, 12, 18, 24};

        for (float gain : gains)
        {
            float y = gainToY(gain, bounds.toFloat());
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(bounds.getWidth()));

            if (gain == 0.0f)
            {
                g.setColour(juce::Colour(0xff404040));
                g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(bounds.getWidth()));
                g.setColour(juce::Colour(0xff2a2a2a));
            }

            g.setColour(juce::Colour(0xff666666));
            g.setFont(10.0f);
            g.drawText(juce::String(static_cast<int>(gain)) + " dB", 5, static_cast<int>(y) - 7, 50, 15, juce::Justification::left);
            g.setColour(juce::Colour(0xff2a2a2a));
        }
    }

    void drawSpectrum(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto& scopeData = spectrumAnalyzer.getScopeData();
        int numBins = spectrumAnalyzer.getNumBins();

        juce::Path spectrumPath;
        bool firstPoint = true;

        for (int i = 1; i < numBins; ++i)
        {
            float freq = spectrumAnalyzer.binToFrequency(i);
            if (freq < 20.0f || freq > 20000.0f)
                continue;

            float level = scopeData[i];
            float x = frequencyToX(freq, bounds.toFloat());
            float gainDb = juce::jmap(level, 0.0f, 1.0f, -60.0f, 0.0f);
            float y = gainToY(gainDb, bounds.toFloat());

            if (firstPoint)
            {
                spectrumPath.startNewSubPath(x, bounds.getBottom());
                spectrumPath.lineTo(x, y);
                firstPoint = false;
            }
            else
            {
                spectrumPath.lineTo(x, y);
            }
        }

        if (!firstPoint)
        {
            spectrumPath.lineTo(bounds.getRight(), bounds.getBottom());
            spectrumPath.closeSubPath();

            g.setColour(juce::Colour(0xff2a52a8).withAlpha(0.15f));
            g.fillPath(spectrumPath);
        }
    }

    void drawResponseCurve(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        juce::Path responsePath;
        bool firstPoint = true;

        const int numPoints = 500;
        for (int i = 0; i < numPoints; ++i)
        {
            float t = static_cast<float>(i) / (numPoints - 1);
            float freq = 20.0f * std::pow(1000.0f, t);

            std::complex<float> response(1.0f, 0.0f);
            for (auto& band : eqBands)
            {
                if (band.isEnabled())
                    response *= band.getFrequencyResponse(freq);
            }

            float magnitude = std::abs(response);
            float gainDb = juce::Decibels::gainToDecibels(magnitude);
            gainDb = juce::jlimit(-30.0f, 30.0f, gainDb);

            float x = frequencyToX(freq, bounds.toFloat());
            float y = gainToY(gainDb, bounds.toFloat());

            if (firstPoint)
            {
                responsePath.startNewSubPath(x, y);
                firstPoint = false;
            }
            else
            {
                responsePath.lineTo(x, y);
            }
        }

        g.setColour(juce::Colour(0xff4d9eff));
        g.strokePath(responsePath, juce::PathStrokeType(2.5f));

        g.setColour(juce::Colour(0xff4d9eff).withAlpha(0.1f));
        juce::Path fillPath = responsePath;
        fillPath.lineTo(bounds.getRight(), gainToY(0, bounds.toFloat()));
        fillPath.lineTo(bounds.getX(), gainToY(0, bounds.toFloat()));
        fillPath.closeSubPath();
        g.fillPath(fillPath);
    }

    void drawBandHandles(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (!eqBands[i].isEnabled())
                continue;

            auto pos = getHandlePosition(i);

            bool isBeingDragged = (i == draggedBand);
            float size = isBeingDragged ? 14.0f : 10.0f;

            juce::Colour handleColour = juce::Colour(0xff4d9eff);
            if (eqBands[i].getType() == EQBand::LowShelf)
                handleColour = juce::Colour(0xffff6b6b);
            else if (eqBands[i].getType() == EQBand::HighShelf)
                handleColour = juce::Colour(0xff4ecdc4);

            g.setColour(handleColour.withAlpha(0.3f));
            g.fillEllipse(pos.x - size * 2, pos.y - size * 2, size * 4, size * 4);

            g.setColour(handleColour);
            g.fillEllipse(pos.x - size / 2, pos.y - size / 2, size, size);

            g.setColour(juce::Colour(0xff1a1a1a));
            g.fillEllipse(pos.x - size / 4, pos.y - size / 4, size / 2, size / 2);

            if (isBeingDragged || pos.getDistanceFrom(getMouseXYRelative().toFloat()) < 20.0f)
            {
                g.setColour(juce::Colours::white);
                g.setFont(11.0f);
                juce::String label = juce::String(static_cast<int>(eqBands[i].getFrequency())) + " Hz\n" +
                                   juce::String(eqBands[i].getGain(), 1) + " dB\n" +
                                   "Q: " + juce::String(eqBands[i].getQ(), 2);

                int labelWidth = 80;
                int labelHeight = 50;
                int labelX = static_cast<int>(pos.x) - labelWidth / 2;
                int labelY = static_cast<int>(pos.y) - 60;

                g.setColour(juce::Colour(0xff2a2a2a).withAlpha(0.9f));
                g.fillRoundedRectangle(labelX, labelY, labelWidth, labelHeight, 4.0f);

                g.setColour(handleColour);
                g.drawRoundedRectangle(labelX, labelY, labelWidth, labelHeight, 4.0f, 1.5f);

                g.setColour(juce::Colours::white);
                g.drawText(label, labelX, labelY, labelWidth, labelHeight, juce::Justification::centred);
            }
        }
    }

    juce::Point<float> getHandlePosition(int bandIndex)
    {
        if (bandIndex < 0 || bandIndex >= 8)
            return {0, 0};

        auto bounds = getLocalBounds().toFloat();
        float x = frequencyToX(eqBands[bandIndex].getFrequency(), bounds);
        float y = gainToY(eqBands[bandIndex].getGain(), bounds);

        return {x, y};
    }

    float frequencyToX(float frequency, juce::Rectangle<float> bounds)
    {
        float minFreq = std::log10(20.0f);
        float maxFreq = std::log10(20000.0f);
        float logFreq = std::log10(juce::jlimit(20.0f, 20000.0f, frequency));

        float t = (logFreq - minFreq) / (maxFreq - minFreq);
        return bounds.getX() + t * bounds.getWidth();
    }

    float xToFrequency(float x, juce::Rectangle<float> bounds)
    {
        float t = (x - bounds.getX()) / bounds.getWidth();
        t = juce::jlimit(0.0f, 1.0f, t);

        float minFreq = std::log10(20.0f);
        float maxFreq = std::log10(20000.0f);
        float logFreq = minFreq + t * (maxFreq - minFreq);

        return std::pow(10.0f, logFreq);
    }

    float gainToY(float gain, juce::Rectangle<float> bounds)
    {
        float t = juce::jmap(gain, -24.0f, 24.0f, 1.0f, 0.0f);
        return bounds.getY() + t * bounds.getHeight();
    }

    float yToGain(float y, juce::Rectangle<float> bounds)
    {
        float t = (y - bounds.getY()) / bounds.getHeight();
        return juce::jmap(t, 1.0f, 0.0f, -24.0f, 24.0f);
    }

    std::array<EQBand, 8>& eqBands;
    SpectrumAnalyzer& spectrumAnalyzer;
    int draggedBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQVisualizer)
};
