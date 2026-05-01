/*
  ==============================================================================

    DelayVisualizer.h
    Animated delay visualization for DL-1.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DelayEngine.h"

class DelayVisualizer : public juce::Component, private juce::Timer
{
public:
    DelayVisualizer(DelayEngine::VisualState& state) : visState(state)
    {
        startTimerHz(60);
    }

    ~DelayVisualizer() override = default;

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.0f);

        // Background — deep, slightly gradiented panel
        juce::ColourGradient bg(juce::Colour(0xff0e1218), b.getCentreX(), b.getY(),
                                juce::Colour(0xff05080c), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(b, 10.0f);

        // Inner glow border
        g.setColour(juce::Colour(0xff1f2a3a));
        g.drawRoundedRectangle(b, 10.0f, 1.2f);

        // Time grid (subtle vertical lines every 250 ms up to 2500 ms)
        const float maxTimeMs = 2500.0f;
        g.setColour(juce::Colour(0x14ffffff));
        for (int t = 250; t < (int)maxTimeMs; t += 250)
        {
            const float xv = b.getX() + (t / maxTimeMs) * b.getWidth();
            g.drawLine(xv, b.getY() + 6.0f, xv, b.getBottom() - 6.0f, (t % 1000 == 0) ? 1.0f : 0.5f);
        }

        // Channel lanes
        const float laneHeight = b.getHeight() * 0.42f;
        const auto laneL = juce::Rectangle<float>(b.getX(), b.getY() + b.getHeight() * 0.06f,
                                                  b.getWidth(), laneHeight);
        const auto laneR = juce::Rectangle<float>(b.getX(), b.getBottom() - laneHeight - b.getHeight() * 0.06f,
                                                  b.getWidth(), laneHeight);

        drawLane(g, laneL, true );
        drawLane(g, laneR, false);

        // Center axis
        g.setColour(juce::Colour(0x33ffffff));
        g.drawLine(b.getX() + 4.0f, b.getCentreY(), b.getRight() - 4.0f, b.getCentreY(), 0.5f);

        // Title
        g.setColour(juce::Colour(0x88a8c7e8));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("STEREO DELAY VISUALIZER", b.reduced(8.0f, 4.0f),
                   juce::Justification::topLeft, false);

        // Time labels
        g.setColour(juce::Colour(0x55b8d4f0));
        g.setFont(juce::Font(9.0f));
        for (int t = 500; t <= 2000; t += 500)
        {
            const float xv = b.getX() + (t / maxTimeMs) * b.getWidth();
            g.drawText(juce::String(t) + " ms",
                       (int)xv - 22, (int)b.getCentreY() - 7, 44, 14,
                       juce::Justification::centred, false);
        }
    }

    void resized() override {}

private:
    void drawLane(juce::Graphics& g, juce::Rectangle<float> lane, bool isLeft)
    {
        const float maxTimeMs = 2500.0f;
        const float baseY     = lane.getCentreY();
        const float dryX      = lane.getX() + 8.0f;

        const float dly       = isLeft ? visState.delayMsL.load() : visState.delayMsR.load();
        const float fb        = visState.feedback.load();
        const float xfb       = visState.crossfeed.load();
        const float modDepth  = visState.modDepth.load();
        const float modPhase  = visState.modPhase.load();
        const float wetLevel  = isLeft ? visState.wetLevelL.load() : visState.wetLevelR.load();
        const float inLevel   = isLeft ? visState.inputLevelL.load() : visState.inputLevelR.load();

        const juce::Colour primary = isLeft ? juce::Colour(0xff4d9eff) : juce::Colour(0xff7fbfff);

        // Lane background subtle
        g.setColour(juce::Colour(0x10ffffff));
        g.fillRoundedRectangle(lane.reduced(2.0f, 6.0f), 6.0f);

        // Lane label
        g.setColour(primary.withAlpha(0.7f));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(isLeft ? "L" : "R", lane.reduced(6.0f, 0.0f).toNearestInt(),
                   juce::Justification::topRight, false);

        // Source pulse at left edge (input "tap zero")
        const float dryRadius = 6.0f + 22.0f * juce::jlimit(0.0f, 1.0f, inLevel);
        g.setColour(juce::Colour(0xffe8f0ff).withAlpha(juce::jlimit(0.05f, 0.65f, inLevel * 1.5f)));
        g.fillEllipse(dryX - dryRadius, baseY - dryRadius, dryRadius * 2.0f, dryRadius * 2.0f);
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.85f));
        g.fillEllipse(dryX - 3.0f, baseY - 3.0f, 6.0f, 6.0f);

        // Compute and draw delay taps. Each tap n is at time (n+1)*dly with amplitude fb^n
        const int   maxTaps = 16;
        float       amp     = 1.0f;

        juce::Path connectingLine;
        connectingLine.startNewSubPath(dryX, baseY);

        for (int n = 0; n < maxTaps; ++n)
        {
            const float tMs = dly * (n + 1);
            if (tMs > maxTimeMs) break;

            amp *= (n == 0) ? 1.0f : fb;
            const float displayedAmp = juce::jlimit(0.0f, 1.0f, amp);

            if (displayedAmp < 0.01f) break;

            const float modWobble = 1.0f + modDepth * 0.04f *
                                    std::sin((modPhase + n * 0.1f) * juce::MathConstants<float>::twoPi);
            const float xv = lane.getX() + (tMs / maxTimeMs) * lane.getWidth() * modWobble;

            const float maxBarH = lane.getHeight() * 0.45f;
            const float barH    = displayedAmp * maxBarH;

            // Glow halo (bigger when wet output is loud)
            const float glow    = juce::jlimit(0.0f, 1.0f, wetLevel * 1.4f) * displayedAmp;
            if (glow > 0.02f)
            {
                juce::ColourGradient halo(
                    primary.withAlpha(0.35f * glow), xv, baseY,
                    primary.withAlpha(0.0f),         xv + 22.0f, baseY, true);
                g.setGradientFill(halo);
                g.fillEllipse(xv - 22.0f, baseY - 22.0f, 44.0f, 44.0f);
            }

            // Vertical bar (impulse-response style)
            juce::ColourGradient barGrad(
                primary.withAlpha(0.95f), xv, baseY - barH,
                primary.withAlpha(0.10f), xv, baseY + barH, false);
            g.setGradientFill(barGrad);
            g.fillRect(juce::Rectangle<float>(xv - 1.5f, baseY - barH, 3.0f, barH * 2.0f));

            // Tap dot
            const float dotR = 3.5f + 4.0f * displayedAmp + 6.0f * glow;
            g.setColour(primary.brighter(0.4f).withAlpha(0.9f));
            g.fillEllipse(xv - dotR, baseY - dotR, dotR * 2.0f, dotR * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.fillEllipse(xv - 1.5f, baseY - 1.5f, 3.0f, 3.0f);

            // Crossfeed indicator: tiny arrow from L tap to R lane (and vice versa)
            if (xfb > 0.02f && n < 4)
            {
                const float arrowAlpha = juce::jlimit(0.0f, 0.45f, xfb * displayedAmp * 0.6f);
                g.setColour(primary.withAlpha(arrowAlpha));
                const float arrowLenY = isLeft ? 22.0f : -22.0f;
                g.drawLine(xv, baseY, xv + 4.0f, baseY + arrowLenY, 1.5f);
            }

            connectingLine.lineTo(xv, baseY);
        }

        // Decay envelope line (smooth curve through tap tops)
        if (! connectingLine.isEmpty())
        {
            // build envelope path
            juce::Path env;
            env.startNewSubPath(dryX, baseY);
            float a = 1.0f;
            for (int n = 0; n < maxTaps; ++n)
            {
                const float tMs = dly * (n + 1);
                if (tMs > maxTimeMs) break;
                a *= (n == 0) ? 1.0f : fb;
                if (a < 0.005f) break;
                const float xv = lane.getX() + (tMs / maxTimeMs) * lane.getWidth();
                const float maxBarH = lane.getHeight() * 0.45f;
                const float barTop  = baseY - juce::jlimit(0.0f, 1.0f, a) * maxBarH;
                env.lineTo(xv, barTop);
            }
            g.setColour(primary.withAlpha(0.35f));
            g.strokePath(env, juce::PathStrokeType(1.2f));
        }
    }

    void timerCallback() override { repaint(); }

    DelayEngine::VisualState& visState;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayVisualizer)
};
