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
#include "GlassStyle.h"

namespace pike::gui
{
    namespace col
    {
        const juce::Colour accent { 0xff4d9eff };
        const juce::Colour panel  { 0xff181b22 };
        const juce::Colour line    { 0xff404040 };
    }

    /** Dark glass background for plot panels (keeps the plot area readable). */
    inline void fillGlassPlot (juce::Graphics& g, juce::Rectangle<float> b, float corner)
    {
        fillGlassPanel (g, b, corner, juce::Colour (0xff222833), juce::Colour (0xff090c11), 0.10f);
    }

    //==============================================================================
    class LevelMeter : public juce::Component, private juce::Timer
    {
    public:
        explicit LevelMeter (VisualState& vs) : vis (vs) { startTimerHz (30); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            fillGlassPlot (g, b, 4.0f);

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
            fillGlassPlot (g, b, 4.0f);
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
    /** Live magnitude-response plot of the state-variable filter, computed from
        the current Type / Slope / Cutoff / Resonance parameters. */
    class FilterResponse : public juce::Component, private juce::Timer
    {
    public:
        FilterResponse (juce::AudioProcessorValueTreeState& s,
                        juce::String typeId, juce::String slopeId,
                        juce::String cutoffId, juce::String resoId)
            : state (s), tId (typeId), slId (slopeId), cId (cutoffId), rId (resoId)
        {
            startTimerHz (30);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (1.0f);
            fillGlassPlot (g, b, 4.0f);

            g.setColour (col::accent.withAlpha (0.8f));
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText ("Filter Response", b.reduced (6, 3).removeFromTop (12),
                        juce::Justification::topLeft, false);

            auto plot = b.reduced (8.0f, 6.0f).withTrimmedTop (12.0f);

            // 0 dB reference line.
            const float zeroY = dbToY (0.0f, plot);
            g.setColour (col::line.withAlpha (0.6f));
            g.drawHorizontalLine ((int) zeroY, plot.getX(), plot.getRight());

            // Octave grid lines (100 Hz, 1 kHz, 10 kHz).
            for (double f : { 100.0, 1000.0, 10000.0 })
            {
                const float x = freqToX (f, plot);
                g.setColour (col::line.withAlpha (0.25f));
                g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            }

            const int   type   = (int) get (tId);
            const bool  s24    = get (slId) > 0.5f;
            const float cutoff = get (cId);
            const float reso   = get (rId);

            juce::Path curve;
            const int w = juce::jmax (2, (int) plot.getWidth());
            for (int x = 0; x <= w; ++x)
            {
                const double f  = xToFreq ((float) x / w);
                const float  db = magnitudeDb (f, type, s24, cutoff, reso);
                const float  y  = dbToY (db, plot);
                const float  px = plot.getX() + (float) x;
                if (x == 0) curve.startNewSubPath (px, y);
                else        curve.lineTo (px, y);
            }

            // Filled area under the curve.
            juce::Path fill = curve;
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            juce::ColourGradient grad (col::accent.withAlpha (0.28f), plot.getX(), plot.getY(),
                                       col::accent.withAlpha (0.02f), plot.getX(), plot.getBottom(), false);
            g.setGradientFill (grad);
            g.fillPath (fill);

            g.setColour (col::accent);
            g.strokePath (curve, juce::PathStrokeType (1.6f));

            // Cutoff marker.
            const float cx = freqToX (cutoff, plot);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawVerticalLine ((int) cx, plot.getY(), plot.getBottom());
        }

    private:
        float get (const juce::String& id) const
        {
            if (auto* p = state.getRawParameterValue (id)) return p->load();
            return 0.0f;
        }

        void timerCallback() override
        {
            const float sum = get (tId) + get (slId) + get (cId) * 0.001f + get (rId);
            if (std::abs (sum - lastSum) > 1.0e-6f) { lastSum = sum; repaint(); }
        }

        static constexpr double fMin = 20.0, fMax = 20000.0;
        static constexpr float  dbMax = 18.0f, dbMin = -36.0f;

        static double xToFreq (float norm) { return fMin * std::pow (fMax / fMin, (double) norm); }

        float freqToX (double f, juce::Rectangle<float> plot) const
        {
            const double n = std::log (f / fMin) / std::log (fMax / fMin);
            return plot.getX() + (float) (juce::jlimit (0.0, 1.0, n) * plot.getWidth());
        }

        float dbToY (float db, juce::Rectangle<float> plot) const
        {
            const float n = (juce::jlimit (dbMin, dbMax, db) - dbMin) / (dbMax - dbMin);
            return plot.getBottom() - n * plot.getHeight();
        }

        static float magnitudeDb (double freq, int type, bool slope24, float cutoffHz, float reso)
        {
            constexpr double pi = 3.14159265358979323846;
            const double wc = 2.0 * pi * juce::jlimit (10.0f, 20000.0f, cutoffHz);
            const double w  = 2.0 * pi * freq;

            const double k = reso >= 0.999 ? 0.02 : 1.0 / (0.5 * std::pow (200.0, (double) reso));

            auto stageMag = [&] (double kk)
            {
                const double re = wc * wc - w * w;
                const double im = kk * wc * w;
                const double den = std::sqrt (re * re + im * im);
                if (type == 0) return (wc * wc) / den;     // LP
                if (type == 2) return (w * w) / den;       // HP
                return (kk * wc * w) / den;                // BP
            };

            double mag = stageMag (k);
            if (slope24)
                mag *= stageMag (1.41421356);              // Butterworth second stage
            return 20.0f * std::log10 ((float) juce::jmax (1.0e-5, mag));
        }

        juce::AudioProcessorValueTreeState& state;
        juce::String tId, slId, cId, rId;
        float lastSum = -1.0f;
    };

    //==============================================================================
    /** Draws one cycle of an oscillator's currently selected waveform, reading
        the wave / pulse-width / wavetable-position parameters. */
    class WaveformDisplay : public juce::Component, private juce::Timer
    {
    public:
        WaveformDisplay (juce::AudioProcessorValueTreeState& s,
                         juce::String waveId, juce::String pwId, juce::String wtPosId)
            : state (s), wId (waveId), pwId (pwId), wtId (wtPosId)
        {
            startTimerHz (20);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (1.0f);
            fillGlassPanel (g, b, 4.0f, juce::Colour (0xff222833), juce::Colour (0xff090c11), 0.08f);

            auto plot = b.reduced (8.0f, 6.0f);
            g.setColour (col::line.withAlpha (0.4f));
            g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

            const int   wave = (int) get (wId);
            const float pw   = get (pwId);
            const float wt   = get (wtId);

            const float midY = plot.getCentreY();
            const float amp  = plot.getHeight() * 0.42f;
            const int   w    = juce::jmax (2, (int) plot.getWidth());

            juce::Path path;
            for (int x = 0; x <= w; ++x)
            {
                const float p = (float) x / (float) w;             // 0..1 phase
                const float v = juce::jlimit (-1.0f, 1.0f, sample (wave, p, pw, wt));
                const float px = plot.getX() + (float) x;
                const float py = midY - v * amp;
                if (x == 0) path.startNewSubPath (px, py);
                else        path.lineTo (px, py);
            }

            g.setColour (col::accent.withAlpha (0.25f));
            g.strokePath (path, juce::PathStrokeType (3.0f));
            g.setColour (col::accent);
            g.strokePath (path, juce::PathStrokeType (1.6f));
        }

    private:
        float get (const juce::String& id) const
        {
            if (auto* p = state.getRawParameterValue (id)) return p->load();
            return 0.0f;
        }

        void timerCallback() override
        {
            const float s = get (wId) * 1000.0f + get (pwId) + get (wtId);
            if (std::abs (s - lastSum) > 1.0e-6f) { lastSum = s; repaint(); }
        }

        // Single waveform shapes over one cycle phase p in [0,1).
        static float basicShape (int wave, float p, float pw)
        {
            constexpr float twoPi = 6.2831853f;
            switch (wave)
            {
                case 0: return std::sin (twoPi * p);                       // Sine
                case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);          // Triangle
                case 2: return 2.0f * p - 1.0f;                            // Saw
                case 3: return p < pw ? 1.0f : -1.0f;                      // Pulse
                default: return std::sin (twoPi * p);
            }
        }

        // Wavetable morphs sine -> tri -> saw -> square (matching the bank).
        static float wavetableShape (float p, float wt)
        {
            const float fpos = juce::jlimit (0.0f, 1.0f, wt) * 3.0f;
            const int   f0   = (int) fpos;
            const int   f1   = juce::jmin (f0 + 1, 3);
            const float ff   = fpos - (float) f0;
            auto frame = [&] (int f) -> float
            {
                switch (f)
                {
                    case 0: return std::sin (6.2831853f * p);
                    case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);
                    case 2: return 2.0f * p - 1.0f;
                    default: return p < 0.5f ? 1.0f : -1.0f;
                }
            };
            return frame (f0) + ff * (frame (f1) - frame (f0));
        }

        static float sample (int wave, float p, float pw, float wt)
        {
            return wave == 4 ? wavetableShape (p, wt) : basicShape (wave, p, pw);
        }

        juce::AudioProcessorValueTreeState& state;
        juce::String wId, pwId, wtId;
        float lastSum = -1.0e9f;
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
            fillGlassPlot (g, b, 4.0f);

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

            // Playhead with a soft radial glow.
            if (headVisible)
            {
                const float hx = juce::jlimit (plot.getX(), ex, plot.getX() + headX);
                const float hy = top + (1.0f - headVal) * (bottom - top);

                const float r = 13.0f;
                juce::ColourGradient glow (col::accent.withAlpha (0.7f), hx, hy,
                                           col::accent.withAlpha (0.0f), hx + r, hy, true);
                g.setGradientFill (glow);
                g.fillEllipse (hx - r, hy - r, 2.0f * r, 2.0f * r);

                g.setColour (col::accent.brighter (0.4f));
                g.fillEllipse (hx - 4.0f, hy - 4.0f, 8.0f, 8.0f);
                g.setColour (juce::Colours::white);
                g.fillEllipse (hx - 2.0f, hy - 2.0f, 4.0f, 4.0f);
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
