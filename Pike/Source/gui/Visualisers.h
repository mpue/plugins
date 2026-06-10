/*
  ==============================================================================

    Visualisers.h
    Animated, polling display widgets driven by the processor's VisualState:
      - LevelMeter     : stereo output meter with peak-hold ballistics
      - Oscilloscope   : output waveform with a soft glow
      - EnvelopeDisplay: ADSR curve with a playhead that follows note triggers

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "VisualState.h"

namespace pike::gui
{
    namespace col
    {
        const juce::Colour accent { 0xff4d9eff };
        const juce::Colour panel  { 0xff181b22 };
        const juce::Colour line    { 0xff404040 };
    }

    //==============================================================================
    class LevelMeter : public juce::Component, private juce::Timer
    {
    public:
        explicit LevelMeter (VisualState& vs) : vis (vs) { startTimerHz (30); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (col::panel);
            g.fillRoundedRectangle (b, 3.0f);

            const float gap = 3.0f;
            const float bw  = (b.getWidth() - 3.0f * gap) * 0.5f;
            drawBar (g, { b.getX() + gap,            b.getY() + 2.0f, bw, b.getHeight() - 4.0f }, dispL);
            drawBar (g, { b.getX() + 2.0f * gap + bw, b.getY() + 2.0f, bw, b.getHeight() - 4.0f }, dispR);
        }

    private:
        void timerCallback() override
        {
            auto ballistic = [] (float target, float& disp, float& hold, int& holdCtr)
            {
                disp = target > disp ? target : disp * 0.82f + target * 0.18f;
                if (disp >= hold) { hold = disp; holdCtr = 30; }
                else if (--holdCtr <= 0) hold = juce::jmax (disp, hold - 0.02f);
            };
            ballistic (vis.meterL.load (std::memory_order_relaxed), dispL, holdL, holdCtrL);
            ballistic (vis.meterR.load (std::memory_order_relaxed), dispR, holdR, holdCtrR);
            repaint();
        }

        void drawBar (juce::Graphics& g, juce::Rectangle<float> r, float level)
        {
            const float v = std::sqrt (juce::jlimit (0.0f, 1.0f, level));   // visual scaling

            g.setColour (juce::Colour (0xff101216));
            g.fillRoundedRectangle (r, 2.0f);

            auto fill = r.withTop (r.getBottom() - v * r.getHeight());
            juce::ColourGradient grad (juce::Colour (0xff2a8cff), r.getX(), r.getBottom(),
                                       juce::Colour (0xffff4d4d), r.getX(), r.getY(), false);
            grad.addColour (0.7, juce::Colour (0xff4de2ff));
            grad.addColour (0.85, juce::Colour (0xffe2ff4d));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, 2.0f);
        }

        VisualState& vis;
        float dispL = 0, dispR = 0, holdL = 0, holdR = 0;
        int   holdCtrL = 0, holdCtrR = 0;
    };

    //==============================================================================
    class Oscilloscope : public juce::Component, private juce::Timer
    {
    public:
        explicit Oscilloscope (VisualState& vs) : vis (vs) { startTimerHz (45); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (col::panel);
            g.fillRoundedRectangle (b, 3.0f);
            g.setColour (col::line.withAlpha (0.5f));
            g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());

            const int   start = vis.scopeWrite.load (std::memory_order_relaxed);
            const int   N     = VisualState::scopeSize;
            const float midY  = b.getCentreY();
            const float amp   = b.getHeight() * 0.45f;
            const int   w     = juce::jmax (1, (int) b.getWidth());

            juce::Path path;
            for (int x = 0; x < w; ++x)
            {
                const int idx = (start + (int) ((float) x / w * N)) & (N - 1);
                const float s = juce::jlimit (-1.0f, 1.0f, vis.scope[idx]);
                const float y = midY - s * amp;
                if (x == 0) path.startNewSubPath (b.getX() + (float) x, y);
                else        path.lineTo (b.getX() + (float) x, y);
            }

            g.setColour (col::accent.withAlpha (0.25f));
            g.strokePath (path, juce::PathStrokeType (3.0f));   // glow
            g.setColour (col::accent);
            g.strokePath (path, juce::PathStrokeType (1.4f));
        }

    private:
        void timerCallback() override { repaint(); }
        VisualState& vis;
    };

    //==============================================================================
    class EnvelopeDisplay : public juce::Component, private juce::Timer
    {
    public:
        EnvelopeDisplay (juce::AudioProcessorValueTreeState& s, VisualState& vs,
                         juce::String a, juce::String d, juce::String sus, juce::String r,
                         juce::String titleText)
            : state (s), vis (vs), aId (a), dId (d), sId (sus), rId (r), title (titleText)
        {
            startTimerHz (60);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (col::panel);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (col::line);
            g.drawRoundedRectangle (b, 4.0f, 1.0f);

            g.setColour (col::accent.withAlpha (0.8f));
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText (title, b.reduced (6, 3).removeFromTop (12), juce::Justification::topLeft, false);

            auto plot = b.reduced (8.0f, 6.0f).withTrimmedTop (10.0f);
            const float A = get (aId), D = get (dId), S = get (sId), R = get (rId);

            const float top = plot.getY(), bottom = plot.getBottom();
            const float sustainW = plot.getWidth() * 0.22f;
            const float tScale = (plot.getWidth() - sustainW) / juce::jmax (0.01f, A + D + R);
            const float ax = plot.getX() + A * tScale;
            const float dx = ax + D * tScale;
            const float sx = dx + sustainW;
            const float ex = sx + R * tScale;
            const float sY = top + (1.0f - S) * (bottom - top);

            juce::Path env;
            env.startNewSubPath (plot.getX(), bottom);
            env.lineTo (ax, top);
            env.lineTo (dx, sY);
            env.lineTo (sx, sY);
            env.lineTo (ex, bottom);

            // Filled area under the curve.
            juce::Path fill = env;
            fill.lineTo (plot.getX(), bottom);
            fill.closeSubPath();
            juce::ColourGradient grad (col::accent.withAlpha (0.30f), plot.getX(), top,
                                       col::accent.withAlpha (0.02f), plot.getX(), bottom, false);
            g.setGradientFill (grad);
            g.fillPath (fill);

            g.setColour (col::accent);
            g.strokePath (env, juce::PathStrokeType (1.6f));

            // Playhead.
            if (headVisible)
            {
                const float hx = juce::jlimit (plot.getX(), ex, plot.getX() + headX);
                const float hy = top + (1.0f - headVal) * (bottom - top);
                g.setColour (col::accent.withAlpha (0.25f));
                g.fillEllipse (hx - 5.0f, hy - 5.0f, 10.0f, 10.0f);
                g.setColour (juce::Colours::white);
                g.fillEllipse (hx - 2.5f, hy - 2.5f, 5.0f, 5.0f);
            }
        }

    private:
        float get (const juce::String& id) const
        {
            if (auto* p = state.getRawParameterValue (id)) return p->load();
            return 0.0f;
        }

        void timerCallback() override
        {
            const float dt = 1.0f / 60.0f;
            const float A = get (aId), D = get (dId), S = get (sId), R = get (rId);

            // Redraw the curve when a knob changes, even when no note is playing.
            const float sum = A + D + S + R;
            bool needsRepaint = std::abs (sum - lastParamSum) > 1.0e-6f;
            lastParamSum = sum;

            const int t = vis.triggerId.load (std::memory_order_relaxed);
            if (t != lastTrigger)
            {
                lastTrigger = t;
                pt = 0.0f; released = false; headVisible = true; releaseLevel = S;
            }

            const bool gate = vis.gate.load (std::memory_order_relaxed);
            if (! gate && ! released && headVisible)
            {
                released = true;
                releaseStart = pt;
                releaseLevel = currentValue (pt, A, D, S);
            }

            if (headVisible)
            {
                pt += dt;
                updateHead (A, D, S, R);
                needsRepaint = true;
            }

            if (needsRepaint)
                repaint();
        }

        float currentValue (float t, float A, float D, float S) const
        {
            if (t < A)      return A > 0 ? t / A : 1.0f;
            if (t < A + D)  return 1.0f - (1.0f - S) * (D > 0 ? (t - A) / D : 1.0f);
            return S;
        }

        void updateHead (float A, float D, float S, float R)
        {
            // Reproduce the curve's x-mapping (must match paint()).
            auto plotW = (float) getWidth() - 18.0f;
            const float sustainW = plotW * 0.22f;
            const float tScale = (plotW - sustainW) / juce::jmax (0.01f, A + D + R);

            if (! released)
            {
                const float held = pt;
                if (held < A + D + sustainHold)
                {
                    headVal = currentValue (held, A, D, S);
                    const float x = juce::jmin (held, A + D) * tScale
                                  + (held > A + D ? juce::jmin (held - (A + D), sustainHold) / juce::jmax (0.0001f, sustainHold) * sustainW : 0.0f);
                    headX = x;
                }
                else { headVal = S; headX = (A + D) * tScale + sustainW; }
            }
            else
            {
                const float rt = pt - releaseStart;
                if (rt >= R) { headVisible = false; return; }
                headVal = releaseLevel * (1.0f - (R > 0 ? rt / R : 1.0f));
                headX   = (A + D) * tScale + sustainW + rt * tScale;
            }
        }

        static constexpr float sustainHold = 0.5f;   // seconds the head crosses the plateau

        juce::AudioProcessorValueTreeState& state;
        VisualState& vis;
        juce::String aId, dId, sId, rId, title;

        int   lastTrigger = -1;
        float pt = 0.0f, releaseStart = 0.0f, releaseLevel = 0.0f;
        bool  released = false, headVisible = false;
        float headX = 0.0f, headVal = 0.0f;
        float lastParamSum = -1.0f;
    };
}
