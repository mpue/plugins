/*
  ==============================================================================

    FlangerPhaserVisualizer.h
    Live frequency-response display of the current flanger / phaser state.
    Renders the moving comb / notch curve plus an animated LFO trail and a
    tasteful HUD with rate / depth / mode readouts.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <complex>
#include "FlangerPhaserEngine.h"

class FlangerPhaserVisualizer : public juce::Component,
                                private juce::Timer
{
public:
    FlangerPhaserVisualizer (FlangerPhaserEngine& e) : engine (e)
    {
        setOpaque (false);
        startTimerHz (45);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        const float corner = 10.0f;

        // Background
        juce::ColourGradient bg (juce::Colour (0xff121826), bounds.getX(), bounds.getY(),
                                 juce::Colour (0xff080b12), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, corner);

        // Inner highlight
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (bounds.getX() + corner, bounds.getY() + 0.5f,
                    bounds.getRight() - corner, bounds.getY() + 0.5f, 1.0f);

        // Outline
        g.setColour (juce::Colour (0xff2a3344));
        g.drawRoundedRectangle (bounds, corner, 1.0f);

        const auto inner = bounds.reduced (12.0f, 14.0f);

        // Reserve a slim strip on the right for the LFO trail
        const float trailW = juce::jmin (140.0f, inner.getWidth() * 0.22f);
        auto trailRect    = inner.withLeft (inner.getRight() - trailW);
        auto responseRect = inner.withRight (trailRect.getX() - 10.0f);

        drawSpectrumGrid (g, responseRect);
        drawResponseCurve (g, responseRect);
        drawLfoTrail (g, trailRect);
        drawHud (g, inner);
    }

    void resized() override {}

private:
    void timerCallback() override { repaint(); }

    static constexpr float fMinHz = 30.0f;
    static constexpr float fMaxHz = 20000.0f;

    static float xForHz (float hz, juce::Rectangle<float> r)
    {
        const float t = (std::log (hz) - std::log (fMinHz))
                      / (std::log (fMaxHz) - std::log (fMinHz));
        return r.getX() + juce::jlimit (0.0f, 1.0f, t) * r.getWidth();
    }

    void drawSpectrumGrid (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Horizontal centre / amplitude markers
        g.setColour (juce::Colour (0xff1a2230));
        for (int i = 1; i < 5; ++i)
        {
            const float y = r.getY() + r.getHeight() * (float) i / 5.0f;
            g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
        }

        // Vertical decade lines + sub-decade ticks
        const std::array<float, 4> majors = { 100.0f, 1000.0f, 10000.0f, 20000.0f };
        const std::array<float, 27> ticks = {
            30,40,50,60,70,80,90,
            100,200,300,400,500,600,700,800,900,
            1000,2000,3000,4000,5000,6000,7000,8000,9000,
            10000,15000
        };

        g.setColour (juce::Colour (0xff141d2c));
        for (auto f : ticks)
        {
            if (f >= fMinHz && f <= fMaxHz)
            {
                const float x = xForHz (f, r);
                g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
            }
        }

        g.setColour (juce::Colour (0xff263246));
        for (auto f : majors)
        {
            if (f >= fMinHz && f <= fMaxHz)
            {
                const float x = xForHz (f, r);
                g.drawLine (x, r.getY(), x, r.getBottom(), 1.2f);
            }
        }

        // Decade labels
        g.setColour (juce::Colour (0xff556a8a));
        g.setFont (juce::Font (10.0f, juce::Font::FontStyleFlags::plain));
        const std::array<std::pair<float, juce::String>, 4> labels = {
            std::make_pair (100.0f,  juce::String ("100")),
            std::make_pair (1000.0f, juce::String ("1k")),
            std::make_pair (5000.0f, juce::String ("5k")),
            std::make_pair (10000.0f,juce::String ("10k"))
        };
        for (auto& l : labels)
        {
            const float x = xForHz (l.first, r);
            g.drawText (l.second,
                        juce::Rectangle<float> (x - 18.0f, r.getBottom() - 12.0f, 36.0f, 12.0f),
                        juce::Justification::centredBottom, false);
        }

        // 0 dB line
        g.setColour (juce::Colour (0xff1f2a3a));
        const float midY = r.getCentreY();
        g.drawLine (r.getX(), midY, r.getRight(), midY, 1.0f);
    }

    // Compute the current effect's frequency-response magnitude in dB at frequency hz
    // for a given channel (0 or 1). Models flanger comb and/or phaser cascade.
    float responseDb (float hz, int channel) const
    {
        const auto fs   = (float) engine.getSampleRate();
        const auto mode = engine.getMode();

        // Magnitude (linear) of wet-only path. We render the dry+wet combination so we hear comb shape.
        std::complex<float> H (0.0f, 0.0f);

        // ---- Flanger contribution ----
        if (mode == FlangerPhaserEngine::Mode::Flanger || mode == FlangerPhaserEngine::Mode::Hybrid)
        {
            const float dMs   = engine.getCurrentFlangerDelayMs (channel);
            const float dSmp  = juce::jmax (1.0f, dMs * 0.001f * fs);
            const float omega = juce::MathConstants<float>::twoPi * hz / fs;
            const float fb    = engine.getFeedbackValue();
            // Comb with feedback:  H_w = e^{-jωD} / (1 - fb * e^{-jωD})
            const std::complex<float> z = std::polar (1.0f, -omega * dSmp);
            const std::complex<float> denom = std::complex<float> (1.0f, 0.0f) - fb * z;
            std::complex<float> hf = z;
            if (std::abs (denom) > 1e-6f)
                hf = z / denom;

            const float w = (mode == FlangerPhaserEngine::Mode::Hybrid) ? 0.7f : 1.0f;
            H += hf * w;
        }

        // ---- Phaser contribution ----
        if (mode == FlangerPhaserEngine::Mode::Phaser || mode == FlangerPhaserEngine::Mode::Hybrid)
        {
            const float fHz = engine.getCurrentPhaserHz (channel);
            const float piFoverFs = juce::MathConstants<float>::pi * fHz / fs;
            const float t = std::tan (piFoverFs);
            const float a = (t - 1.0f) / (t + 1.0f);

            // First-order all-pass H_a(z) = (a + z^-1) / (1 + a z^-1)
            const float omega = juce::MathConstants<float>::twoPi * hz / fs;
            const std::complex<float> z1 = std::polar (1.0f, -omega);
            const std::complex<float> num = std::complex<float> (a, 0.0f) + z1;
            const std::complex<float> den = std::complex<float> (1.0f, 0.0f) + a * z1;
            std::complex<float> ap = num / den;

            // Cascade
            std::complex<float> hp = ap;
            for (int s = 1; s < engine.getNumStages(); ++s)
                hp *= ap;

            // Feedback approximation: hp / (1 - fb * hp) — perceptually matches the engine's behaviour
            const float fb    = engine.getFeedbackValue() * 0.6f;
            std::complex<float> denom = std::complex<float> (1.0f, 0.0f) - fb * hp;
            if (std::abs (denom) > 1e-6f)
                hp = hp / denom;

            const float w = (mode == FlangerPhaserEngine::Mode::Hybrid) ? 0.7f : 1.0f;
            H += hp * w;
        }

        // Mix with dry: y = (1-mix)*x + mix*wet
        const float mix = engine.getMixValue();
        const std::complex<float> dry (1.0f - mix, 0.0f);
        const std::complex<float> wet = mix * H;
        const std::complex<float> total = dry + wet;

        const float mag = std::abs (total);
        const float db  = 20.0f * std::log10 (juce::jmax (1.0e-4f, mag));
        return db;
    }

    void drawResponseCurve (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const int   numPoints = juce::jmax (96, (int) r.getWidth());
        const float dbMin = -28.0f;
        const float dbMax =  18.0f;

        auto buildPath = [&] (int channel) -> juce::Path
        {
            juce::Path p;
            for (int i = 0; i <= numPoints; ++i)
            {
                const float t = (float) i / (float) numPoints;
                const float lf = std::log (fMinHz) + t * (std::log (fMaxHz) - std::log (fMinHz));
                const float hz = std::exp (lf);

                const float db = responseDb (hz, channel);
                const float dbN = juce::jlimit (0.0f, 1.0f, (db - dbMin) / (dbMax - dbMin));
                const float x = r.getX() + t * r.getWidth();
                const float y = r.getBottom() - dbN * r.getHeight();

                if (i == 0) p.startNewSubPath (x, y);
                else        p.lineTo (x, y);
            }
            return p;
        };

        // Channel 1 (cyan) under, channel 0 (blue) on top — gives a stereo "split" feel
        const juce::Colour colR (0xff42d6c0); // teal
        const juce::Colour colL (0xff4d9eff); // blue

        // Filled area under L curve for that lush, glowing look
        auto pL = buildPath (0);
        {
            juce::Path fill = pL;
            fill.lineTo (r.getRight(), r.getBottom());
            fill.lineTo (r.getX(),     r.getBottom());
            fill.closeSubPath();

            juce::ColourGradient grad (colL.withAlpha (0.32f), r.getX(), r.getY(),
                                       colL.withAlpha (0.0f),  r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillPath (fill);
        }

        // Right channel — soft glow + thin line
        auto pR = buildPath (1);
        g.setColour (colR.withAlpha (0.22f));
        g.strokePath (pR, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        g.setColour (colR.withAlpha (0.85f));
        g.strokePath (pR, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        // Left channel — strong glow + bright line
        g.setColour (colL.withAlpha (0.30f));
        g.strokePath (pL, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        g.setColour (colL);
        g.strokePath (pL, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        // Notch / peak markers — find local minima of L curve and draw tiny dots
        drawNotchMarkers (g, r, dbMin, dbMax);
    }

    void drawNotchMarkers (juce::Graphics& g, juce::Rectangle<float> r,
                           float dbMin, float dbMax)
    {
        const int N = 240;
        std::vector<float> mags ((size_t) (N + 1));
        for (int i = 0; i <= N; ++i)
        {
            const float t = (float) i / (float) N;
            const float lf = std::log (fMinHz) + t * (std::log (fMaxHz) - std::log (fMinHz));
            mags[(size_t) i] = responseDb (std::exp (lf), 0);
        }

        for (int i = 2; i < N - 2; ++i)
        {
            const float a = mags[(size_t) (i - 1)];
            const float b = mags[(size_t) i];
            const float c = mags[(size_t) (i + 1)];
            // Local minimum (notch)
            if (b < a && b < c && b < -6.0f)
            {
                const float t = (float) i / (float) N;
                const float dbN = juce::jlimit (0.0f, 1.0f, (b - dbMin) / (dbMax - dbMin));
                const float x = r.getX() + t * r.getWidth();
                const float y = r.getBottom() - dbN * r.getHeight();
                g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.18f));
                g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
                g.setColour (juce::Colour (0xff8fc7ff));
                g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
            }
        }
    }

    void drawLfoTrail (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Backplate
        g.setColour (juce::Colour (0xff0e131e));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (0xff202b3d));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        // Centre line
        g.setColour (juce::Colour (0xff1c2434));
        g.drawLine (r.getX() + 6.0f, r.getCentreY(), r.getRight() - 6.0f, r.getCentreY(), 1.0f);

        const float midY  = r.getCentreY();
        const float halfH = r.getHeight() * 0.42f;

        const float rate     = engine.getRateValue();
        const float depthN   = engine.getDepthValue();
        const float phase0   = engine.getLfoPhase();
        const float cycles   = juce::jlimit (1.5f, 8.0f, 0.8f + rate * 1.5f);

        const auto shape = engine.getLfoShape();

        auto sampleLfo = [shape] (float ph01) -> float
        {
            const float twoPi = juce::MathConstants<float>::twoPi;
            switch (shape)
            {
                case FlangerPhaserEngine::LfoShape::Triangle:
                {
                    const float t = ph01 < 0.5f ? ph01 * 2.0f : (1.0f - ph01) * 2.0f;
                    return t * 2.0f - 1.0f;
                }
                case FlangerPhaserEngine::LfoShape::Drift:
                case FlangerPhaserEngine::LfoShape::Sine:
                default:
                    return std::sin (twoPi * ph01);
            }
        };

        // Two LFO traces for L/R
        for (int ch = 0; ch < 2; ++ch)
        {
            const juce::Colour col = ch == 0
                ? juce::Colour (0xff4d9eff)
                : juce::Colour (0xff42d6c0);

            const float chOffset = ch == 0 ? 0.0f : 0.25f;

            juce::Path p;
            const int numPts = juce::jmax (32, (int) r.getWidth());
            for (int i = 0; i <= numPts; ++i)
            {
                const float t = (float) i / (float) numPts;
                float ph = phase0 + chOffset + t * cycles;
                ph -= std::floor (ph);
                const float v = sampleLfo (ph);
                const float x = r.getX() + 6.0f + t * (r.getWidth() - 12.0f);
                const float y = midY - v * halfH * (0.25f + 0.75f * depthN);
                if (i == 0) p.startNewSubPath (x, y);
                else        p.lineTo (x, y);
            }
            g.setColour (col.withAlpha (0.20f));
            g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            g.setColour (col.withAlpha (0.85f));
            g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }

        // Heading text
        g.setColour (juce::Colour (0xff7a99c0));
        g.setFont (juce::Font (10.0f, juce::Font::FontStyleFlags::plain));
        g.drawText ("LFO", r.reduced (8, 4), juce::Justification::topLeft, false);
    }

    void drawHud (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto mode = engine.getMode();
        juce::String modeName;
        switch (mode)
        {
            case FlangerPhaserEngine::Mode::Flanger: modeName = "FLANGER"; break;
            case FlangerPhaserEngine::Mode::Phaser:  modeName = "PHASER";  break;
            default:                                 modeName = "HYBRID";  break;
        }

        g.setColour (juce::Colour (0xff8fc7ff));
        g.setFont (juce::Font (12.0f, juce::Font::FontStyleFlags::bold));
        g.drawText (modeName, r.removeFromTop (16).toNearestInt(),
                    juce::Justification::topLeft, false);

        g.setColour (juce::Colour (0xff7a99c0));
        g.setFont (juce::Font (10.5f, juce::Font::FontStyleFlags::plain));
        const auto bottom = getLocalBounds().reduced (16, 8).removeFromBottom (16);
        const juce::String stats =
              juce::String (engine.getRateValue(), 2) + " Hz   |   "
            + juce::String ((int) (engine.getDepthValue() * 100.0f)) + "% depth   |   "
            + juce::String ((int) (engine.getFeedbackValue() * 100.0f)) + "% fb";
        g.drawText (stats, bottom, juce::Justification::bottomRight, false);
    }

    FlangerPhaserEngine& engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangerPhaserVisualizer)
};
