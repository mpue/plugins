/*
  ==============================================================================

    CrushScope.h
    Phosphor-style oscilloscope visualising the bitcrusher action: a faint
    dry waveform, a glowing wet waveform on top, the difference between
    them shaded in, and (when bit depth is low) horizontal lines marking
    the actual quantisation steps. Triggered on a rising zero-crossing in
    the dry signal so the display stays rock-stable while audio plays.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class CrushScope : public juce::Component, private juce::Timer
{
public:
    explicit CrushScope (BC1AudioProcessor& p) : processor (p)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        startTimerHz (45);
    }

    ~CrushScope() override = default;

    void paint (juce::Graphics& g) override
    {
        const auto outer = getLocalBounds().toFloat().reduced (1.0f);
        const float corner = 10.0f;

        // ---- panel background ----
        juce::ColourGradient bg (juce::Colour (0xff0e1620), outer.getCentreX(), outer.getY(),
                                  juce::Colour (0xff05080c), outer.getCentreX(), outer.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (outer, corner);

        // soft inner vignette
        {
            juce::ColourGradient v (juce::Colour (0x00000000), outer.getCentreX(), outer.getCentreY(),
                                     juce::Colour (0x55000000), outer.getRight(), outer.getBottom(), true);
            g.setGradientFill (v);
            g.fillRoundedRectangle (outer.reduced (1.0f), corner - 1.0f);
        }

        // ---- working area ----
        const auto area = outer.reduced (12.0f, 14.0f);
        const float midY = area.getCentreY();
        const float halfH = area.getHeight() * 0.45f;

        // ---- amplitude / time grid ----
        g.setColour (juce::Colour (0x14a8d0ff));
        for (int i = 1; i < 4; ++i)
        {
            const float y = area.getY() + area.getHeight() * (i / 4.0f);
            g.drawLine (area.getX(), y, area.getRight(), y, 0.7f);
        }
        g.setColour (juce::Colour (0x33a8d0ff));
        g.drawLine (area.getX(), midY, area.getRight(), midY, 1.0f);

        g.setColour (juce::Colour (0x10a8d0ff));
        for (int i = 1; i < 8; ++i)
        {
            const float x = area.getX() + area.getWidth() * (i / 8.0f);
            g.drawLine (x, area.getY(), x, area.getBottom(), 0.5f);
        }

        // ---- bit-depth quantisation guides ----
        const float bits = processor.getCurrentBits();
        if (bits < 8.5f)
        {
            const int   steps = juce::jmax (2, (int) std::round (std::pow (2.0f, bits)));
            const float qStep = 2.0f / (float) (steps - 1);
            g.setColour (juce::Colour (0x224d9eff));
            for (int s = 0; s < steps; ++s)
            {
                const float v = -1.0f + s * qStep;
                const float y = midY - juce::jlimit (-1.0f, 1.0f, v) * halfH;
                g.drawLine (area.getX(), y, area.getRight(), y, 0.5f);
            }
        }

        // ---- waveform paths ----
        juce::Path dryPath, wetPath;
        const int N = displaySamples;
        const float xStep = area.getWidth() / (float) (N - 1);

        for (int i = 0; i < N; ++i)
        {
            const float x = area.getX() + xStep * (float) i;
            const float dy = midY - juce::jlimit (-1.0f, 1.0f, dryDisplay[(size_t) i]) * halfH;
            const float wy = midY - juce::jlimit (-1.0f, 1.0f, wetDisplay[(size_t) i]) * halfH;
            if (i == 0)
            {
                dryPath.startNewSubPath (x, dy);
                wetPath.startNewSubPath (x, wy);
            }
            else
            {
                dryPath.lineTo (x, dy);
                wetPath.lineTo (x, wy);
            }
        }

        // ---- shaded crush region (between dry and wet) ----
        {
            juce::Path diff;
            diff.startNewSubPath (area.getX(), midY - juce::jlimit (-1.0f, 1.0f, dryDisplay[0]) * halfH);
            for (int i = 1; i < N; ++i)
            {
                const float x = area.getX() + xStep * (float) i;
                diff.lineTo (x, midY - juce::jlimit (-1.0f, 1.0f, dryDisplay[(size_t) i]) * halfH);
            }
            for (int i = N - 1; i >= 0; --i)
            {
                const float x = area.getX() + xStep * (float) i;
                diff.lineTo (x, midY - juce::jlimit (-1.0f, 1.0f, wetDisplay[(size_t) i]) * halfH);
            }
            diff.closeSubPath();

            g.setColour (juce::Colour (0x224d9eff));
            g.fillPath (diff);
        }

        // ---- dry curve ----
        g.setColour (juce::Colour (0x66a0c8ff));
        g.strokePath (dryPath, juce::PathStrokeType (1.0f));

        // ---- wet curve, three-pass phosphor glow ----
        g.setColour (juce::Colour (0x224d9eff));
        g.strokePath (wetPath, juce::PathStrokeType (8.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0x554d9eff));
        g.strokePath (wetPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffd2e7ff));
        g.strokePath (wetPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ---- bypass dim overlay ----
        if (processor.getCurrentBypass())
        {
            g.setColour (juce::Colour (0x99000000));
            g.fillRoundedRectangle (outer, corner);
            g.setColour (juce::Colour (0xffe8e8e8));
            g.setFont (juce::Font (juce::FontOptions (16.0f).withStyle ("Bold")));
            g.drawText ("BYPASSED", outer.toNearestInt(), juce::Justification::centred);
        }

        // ---- frame ----
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (outer, corner, 1.0f);

        // ---- header ----
        g.setColour (juce::Colour (0xaa4d9eff));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("CRUSH SCOPE",
                    juce::Rectangle<int> ((int) outer.getX() + 14, (int) outer.getY() + 6, 120, 14),
                    juce::Justification::centredLeft);

        // ---- live readout ----
        g.setColour (juce::Colour (0x88e8e8e8));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        const float currentRate = processor.getCurrentRate();
        const juce::String rateStr = currentRate >= 1000.0f
            ? juce::String (currentRate / 1000.0f, 2) + " kHz"
            : juce::String ((int) std::round (currentRate)) + " Hz";
        const juce::String info = juce::String (processor.getCurrentBits(), 2) + " bit  /  " + rateStr;
        g.drawText (info,
                    juce::Rectangle<int> ((int) outer.getRight() - 184, (int) outer.getY() + 6, 170, 14),
                    juce::Justification::centredRight);
    }

    void resized() override {}

private:
    static constexpr int displaySamples = 768;
    static constexpr int searchSpan     = 2048;

    void timerCallback() override
    {
        std::array<float, searchSpan> dry {};
        std::array<float, searchSpan> wet {};
        processor.copyScope (dry.data(), wet.data(), searchSpan);

        // pick a stable trigger – most recent rising zero crossing in the dry buffer
        int trig = searchSpan - displaySamples - 1;
        for (int i = trig; i > 1; --i)
        {
            if (dry[(size_t) (i - 1)] <= 0.0f && dry[(size_t) i] > 0.0f)
            {
                trig = i;
                break;
            }
        }

        constexpr float smoothing = 0.65f; // visual softening of the trace
        for (int i = 0; i < displaySamples; ++i)
        {
            const int srcIdx = juce::jlimit (0, searchSpan - 1, trig + i);
            const float d = dry[(size_t) srcIdx];
            const float w = wet[(size_t) srcIdx];
            dryDisplay[(size_t) i] = dryDisplay[(size_t) i] * smoothing + d * (1.0f - smoothing);
            wetDisplay[(size_t) i] = wetDisplay[(size_t) i] * smoothing + w * (1.0f - smoothing);
        }

        repaint();
    }

    BC1AudioProcessor& processor;

    std::array<float, displaySamples> dryDisplay {};
    std::array<float, displaySamples> wetDisplay {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrushScope)
};
