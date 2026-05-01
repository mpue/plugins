/*
  ==============================================================================

    ChorusVisualizer.h
    Animated visual of the chorus voices: LFO ribbons + delay-time playheads.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ChorusEngine.h"

class ChorusVisualizer : public juce::Component,
                         private juce::Timer
{
public:
    ChorusVisualizer (ChorusEngine& e) : engine (e)
    {
        setOpaque (false);
        startTimerHz (45);
    }

    ~ChorusVisualizer() override = default;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        const float corner = 8.0f;

        // Backplate — soft vertical gradient
        juce::ColourGradient bg (juce::Colour (0xff111722), bounds.getX(), bounds.getY(),
                                 juce::Colour (0xff0a0d14), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, corner);

        // Subtle inner highlight on top edge
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawLine (bounds.getX() + corner, bounds.getY() + 0.5f,
                    bounds.getRight() - corner, bounds.getY() + 0.5f, 1.0f);

        // Outline
        g.setColour (juce::Colour (0xff2a3344));
        g.drawRoundedRectangle (bounds, corner, 1.0f);

        const auto inner = bounds.reduced (10.0f, 12.0f);
        drawGrid (g, inner);
        drawVoices (g, inner);
        drawHud (g, inner);
    }

    void resized() override {}

private:
    void timerCallback() override { repaint(); }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Horizontal centre line
        g.setColour (juce::Colour (0xff1f2a3a));
        const float midY = r.getCentreY();
        g.drawLine (r.getX(), midY, r.getRight(), midY, 1.0f);

        // Faint vertical grid lines
        const int divisions = 8;
        g.setColour (juce::Colour (0xff182233));
        for (int i = 1; i < divisions; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) divisions;
            g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
        }
    }

    void drawVoices (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const int   nVoices  = juce::jlimit (1, ChorusEngine::kMaxVoices, engine.getNumVoices());
        const float midY     = r.getCentreY();
        const float halfH    = r.getHeight() * 0.42f;
        const float depthN   = juce::jlimit (0.0f, 1.0f, engine.getDepthValue() / 15.0f);
        const float rate     = engine.getRateValue();

        // How many cycles of the slowest LFO to draw across the width
        const float cyclesAcross = juce::jlimit (1.0f, 6.0f, 0.6f + rate * 1.2f);

        // Voice colours (cool blues with a touch of cyan / violet for depth)
        const juce::Colour voiceCols[ChorusEngine::kMaxVoices] = {
            juce::Colour (0xff4d9eff),
            juce::Colour (0xff6fc0ff),
            juce::Colour (0xff9b7dff),
            juce::Colour (0xff42d6c0)
        };

        const float voicePhaseOffset[ChorusEngine::kMaxVoices] = { 0.0f, 0.25f, 0.5f, 0.75f };

        const int   numPoints = juce::jmax (32, (int) r.getWidth());
        const float phase0    = engine.getLfoPhase (0); // master phase reference

        for (int v = 0; v < nVoices; ++v)
        {
            juce::Path p;
            for (int i = 0; i <= numPoints; ++i)
            {
                const float t = (float) i / (float) numPoints;
                // Time travels left -> right; phase advances accordingly
                const float ph = phase0 + voicePhaseOffset[v] + t * cyclesAcross;
                const float lfo = std::sin (juce::MathConstants<float>::twoPi * ph);
                const float x = r.getX() + t * r.getWidth();
                const float y = midY - lfo * halfH * (0.25f + 0.75f * depthN);
                if (i == 0) p.startNewSubPath (x, y);
                else        p.lineTo (x, y);
            }

            const auto col = voiceCols[v];

            // Soft halo behind the line
            g.setColour (col.withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
            // Crisp line
            g.setColour (col.withAlpha (0.92f));
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

            // Playhead dot at the right edge — current modulation value
            {
                const float ph = phase0 + voicePhaseOffset[v];
                const float lfo = std::sin (juce::MathConstants<float>::twoPi * ph);
                const float x = r.getRight() - 2.0f;
                const float y = midY - lfo * halfH * (0.25f + 0.75f * depthN);

                g.setColour (col.withAlpha (0.35f));
                g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
                g.setColour (col);
                g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.fillEllipse (x - 1.2f, y - 1.2f, 2.4f, 2.4f);
            }
        }
    }

    void drawHud (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Top-left label
        g.setColour (juce::Colour (0xff7a99c0));
        g.setFont (juce::Font (11.0f, juce::Font::FontStyleFlags::plain));
        g.drawText ("LFO  /  voices: " + juce::String (engine.getNumVoices()),
                    r.removeFromTop (14).toNearestInt(), juce::Justification::topLeft);

        // Bottom-right rate / depth readout
        auto bottom = getLocalBounds().reduced (14, 6).removeFromBottom (16);
        g.setColour (juce::Colour (0xff7a99c0));
        g.setFont (juce::Font (11.0f, juce::Font::FontStyleFlags::plain));
        g.drawText (juce::String (engine.getRateValue(), 2) + " Hz   |   "
                    + juce::String (engine.getDepthValue(), 1) + " ms",
                    bottom, juce::Justification::bottomRight);
    }

    ChorusEngine& engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusVisualizer)
};
