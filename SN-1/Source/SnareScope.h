/*
  ==============================================================================

    SnareScope.h
    Luxury snare visualiser. Renders an offline preview of the current snare
    parameters into a 1.8-second buffer and draws it as a glowing,
    phosphor-style waveform with a filled body, a pitch contour overlay, a
    noise envelope contour and a row of "snare wires" below the trace that
    vibrate whenever a hit is triggered.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SnareEngine.h"

class SnareScope : public juce::Component, private juce::Timer
{
public:
    SnareScope()
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);

        previewVoice.prepare (previewSampleRate);
        rebuildPreview();

        startTimerHz (45);
    }

    ~SnareScope() override = default;

    void setParams (const SnareVoice::Params& p)
    {
        params = p;
        previewDirty = true;
    }

    void notifyTriggered (float velocity = 1.0f)
    {
        const juce::ScopedLock sl (playheadLock);
        playheadActive   = true;
        playheadProgress = 0.0f;
        playheadVelocity = juce::jlimit (0.1f, 1.0f, velocity);
        wireBuzz         = 1.0f;
    }

    void paint (juce::Graphics& g) override
    {
        const auto outer  = getLocalBounds().toFloat().reduced (1.0f);
        const float corner = 12.0f;

        // ---- panel background ----
        juce::ColourGradient bg (juce::Colour (0xff0e1620), outer.getCentreX(), outer.getY(),
                                  juce::Colour (0xff05080c), outer.getCentreX(), outer.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (outer, corner);

        {
            juce::ColourGradient v (juce::Colour (0x00000000), outer.getCentreX(), outer.getCentreY(),
                                     juce::Colour (0x66000000), outer.getRight(), outer.getBottom(), true);
            g.setGradientFill (v);
            g.fillRoundedRectangle (outer.reduced (1.0f), corner - 1.0f);
        }

        // Reserve a strip at the bottom for the snare-wire animation.
        const float wireBandH = 28.0f;
        auto traceArea = outer.reduced (16.0f, 26.0f);
        traceArea = traceArea.withTrimmedBottom (wireBandH);

        const float midY  = traceArea.getCentreY();
        const float halfH = traceArea.getHeight() * 0.45f;

        // ---- amplitude / time grid ----
        g.setColour (juce::Colour (0x12a8d0ff));
        for (int i = 1; i < 4; ++i)
        {
            const float y = traceArea.getY() + traceArea.getHeight() * (i / 4.0f);
            g.drawLine (traceArea.getX(), y, traceArea.getRight(), y, 0.7f);
        }
        g.setColour (juce::Colour (0x33a8d0ff));
        g.drawLine (traceArea.getX(), midY, traceArea.getRight(), midY, 1.0f);

        g.setColour (juce::Colour (0x10a8d0ff));
        for (int i = 1; i < 10; ++i)
        {
            const float x = traceArea.getX() + traceArea.getWidth() * (i / 10.0f);
            g.drawLine (x, traceArea.getY(), x, traceArea.getBottom(), 0.5f);
        }

        // ---- waveform path (downsampled mini/max envelope) ----
        const int  N      = (int) traceArea.getWidth();
        const int  total  = previewLengthSamples;
        if (N <= 4 || total <= 4) return;

        juce::Path topPath, botPath, fillPath;
        const float xStep = traceArea.getWidth() / (float) N;

        std::vector<float> peakPos ((size_t) N, 0.0f);
        std::vector<float> peakNeg ((size_t) N, 0.0f);

        for (int i = 0; i < N; ++i)
        {
            const int s0 = juce::jlimit (0, total - 1, (int) ((float) i / N * total));
            const int s1 = juce::jlimit (0, total - 1, (int) ((float) (i + 1) / N * total));
            float maxV = 0.0f, minV = 0.0f;
            for (int s = s0; s < s1; ++s)
            {
                const float v = previewBuffer[(size_t) s];
                if (v > maxV) maxV = v;
                if (v < minV) minV = v;
            }
            peakPos[(size_t) i] = juce::jlimit (-1.0f, 1.0f, maxV);
            peakNeg[(size_t) i] = juce::jlimit (-1.0f, 1.0f, minV);
        }

        topPath.startNewSubPath (traceArea.getX(), midY - peakPos[0] * halfH);
        botPath.startNewSubPath (traceArea.getX(), midY - peakNeg[0] * halfH);
        for (int i = 1; i < N; ++i)
        {
            const float x = traceArea.getX() + xStep * (float) i;
            topPath.lineTo (x, midY - peakPos[(size_t) i] * halfH);
            botPath.lineTo (x, midY - peakNeg[(size_t) i] * halfH);
        }

        // closed fill region between top and bottom envelope
        fillPath.startNewSubPath (traceArea.getX(), midY - peakPos[0] * halfH);
        for (int i = 1; i < N; ++i)
        {
            const float x = traceArea.getX() + xStep * (float) i;
            fillPath.lineTo (x, midY - peakPos[(size_t) i] * halfH);
        }
        for (int i = N - 1; i >= 0; --i)
        {
            const float x = traceArea.getX() + xStep * (float) i;
            fillPath.lineTo (x, midY - peakNeg[(size_t) i] * halfH);
        }
        fillPath.closeSubPath();

        // gradient body fill - blue shifted toward cyan-violet for snare
        juce::ColourGradient fillGrad (juce::Colour (0x886db6ff), midY, traceArea.getY(),
                                        juce::Colour (0x223080d6), midY, traceArea.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (fillPath);

        // outer glow stroke (3-pass phosphor)
        g.setColour (juce::Colour (0x223080d6));
        g.strokePath (topPath, juce::PathStrokeType (8.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (8.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0x556db6ff));
        g.strokePath (topPath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffe3f0ff));
        g.strokePath (topPath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ---- pitch contour overlay (orange) ----
        {
            juce::Path pitchPath;
            const float pitchSemis = params.pitchAmtSemis;
            const float coeff = std::exp (std::log (0.001f) /
                                           juce::jmax (1.0f, params.pitchTimeMs * 0.001f * (float) previewSampleRate));
            for (int i = 0; i < N; ++i)
            {
                const int   s0   = juce::jlimit (0, total - 1, (int) ((float) i / N * total));
                const float e    = std::pow (coeff, (float) s0);
                const float oct  = (pitchSemis / 12.0f) * e;
                const float fHz  = juce::jlimit (5.0f, 6000.0f,
                                                  params.tuneHz * std::pow (2.0f, oct));
                const float fNorm = juce::jlimit (0.0f, 1.0f,
                                                   std::log10 (juce::jmax (1.0f, fHz / 20.0f))
                                                   / std::log10 (1000.0f / 20.0f));
                const float y = traceArea.getY() + (1.0f - fNorm) * traceArea.getHeight() * 0.30f + 4.0f;
                const float x = traceArea.getX() + xStep * (float) i;
                if (i == 0) pitchPath.startNewSubPath (x, y);
                else        pitchPath.lineTo (x, y);
            }
            g.setColour (juce::Colour (0x66ffaa55));
            g.strokePath (pitchPath, juce::PathStrokeType (1.4f));
        }

        // ---- noise envelope contour (warm gold dashed) ----
        {
            juce::Path noisePath;
            const float coeff = std::exp (std::log (0.001f) /
                                           juce::jmax (1.0f, params.noiseDecayMs * 0.001f * (float) previewSampleRate));
            for (int i = 0; i < N; ++i)
            {
                const int   s0   = juce::jlimit (0, total - 1, (int) ((float) i / N * total));
                const float e    = std::pow (coeff, (float) s0);
                const float y    = traceArea.getBottom() - e * traceArea.getHeight() * 0.34f - 4.0f;
                const float x    = traceArea.getX() + xStep * (float) i;
                if (i == 0) noisePath.startNewSubPath (x, y);
                else        noisePath.lineTo (x, y);
            }
            g.setColour (juce::Colour (0x55ffd09a));
            const float dash[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (noisePath, noisePath, dash, 2);
            g.strokePath (noisePath, juce::PathStrokeType (1.2f));
        }

        // ---- live playhead ----
        {
            const juce::ScopedLock sl (playheadLock);
            if (playheadActive)
            {
                const float x = traceArea.getX() + traceArea.getWidth()
                                  * juce::jlimit (0.0f, 1.0f, playheadProgress);

                g.setColour (juce::Colour (0x556db6ff));
                g.drawLine (x, traceArea.getY(), x, traceArea.getBottom(), 3.0f);
                g.setColour (juce::Colour (0xffe8f3ff));
                g.drawLine (x, traceArea.getY(), x, traceArea.getBottom(), 1.0f);

                const float radius = 6.0f * playheadVelocity + 4.0f;
                g.setColour (juce::Colour (0x88e8f3ff));
                g.fillEllipse (x - radius, midY - radius, radius * 2.0f, radius * 2.0f);
                g.setColour (juce::Colour (0xffffffff));
                g.fillEllipse (x - radius * 0.5f, midY - radius * 0.5f, radius, radius);
            }
        }

        // ---- snare wire band (visualises the snares vibrating) ----
        {
            auto wires = juce::Rectangle<float> (traceArea.getX(),
                                                  traceArea.getBottom() + 6.0f,
                                                  traceArea.getWidth(),
                                                  wireBandH - 8.0f);

            // strip background
            g.setColour (juce::Colour (0x18ffffff));
            g.fillRoundedRectangle (wires.expanded (2.0f, 1.0f), 4.0f);

            const int   nWires = 6;
            const float buzzAmp = wireBuzz; // 0..1
            const float wAmp   = juce::jmap (juce::jlimit (0.0f, 1.0f, params.buzzQ / 8.0f),
                                              0.6f, 1.5f);

            for (int wi = 0; wi < nWires; ++wi)
            {
                const float y0 = wires.getY() + wires.getHeight() * (wi + 0.5f) / (float) nWires;

                juce::Path wp;
                const int steps = (int) wires.getWidth();
                for (int i = 0; i <= steps; i += 4)
                {
                    const float x = wires.getX() + (float) i;
                    const float ph = (float) i * 0.025f + (float) wi * 1.7f + wirePhase;
                    const float wave = std::sin (ph) * 0.6f + std::sin (ph * 1.7f + 1.1f) * 0.4f;
                    const float y = y0 + wave * 4.0f * buzzAmp * wAmp;
                    if (i == 0) wp.startNewSubPath (x, y);
                    else        wp.lineTo (x, y);
                }

                const auto col = juce::Colour::fromFloatRGBA (0.85f, 0.92f, 1.0f,
                                                               0.18f + 0.55f * buzzAmp);
                g.setColour (col);
                g.strokePath (wp, juce::PathStrokeType (0.9f));
            }
        }

        // ---- frame ----
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (outer, corner, 1.0f);

        // ---- header ----
        g.setColour (juce::Colour (0xaa6db6ff));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("SNARE SCOPE",
                    juce::Rectangle<int> ((int) outer.getX() + 14, (int) outer.getY() + 6, 140, 14),
                    juce::Justification::centredLeft);

        // ---- live readout ----
        g.setColour (juce::Colour (0x88e8e8e8));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        const float colorKHz = params.noiseColorHz / 1000.0f;
        const juce::String info =
            juce::String (params.tuneHz, 1) + " Hz   "
          + juce::String (params.bodyDecayMs, 0) + " ms body   "
          + juce::String (params.noiseDecayMs, 0) + " ms tail   "
          + juce::String (colorKHz, 2) + " kHz   "
          + "peak " + juce::String (juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, previewPeak)), 1) + " dB";
        g.drawText (info,
                    juce::Rectangle<int> ((int) outer.getRight() - 460, (int) outer.getY() + 6, 446, 14),
                    juce::Justification::centredRight);
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        if (previewDirty)
        {
            rebuildPreview();
            previewDirty = false;
        }

        // advance playhead and wire animation
        wirePhase += 0.45f;
        if (wirePhase > 1000.0f) wirePhase -= 1000.0f;

        bool needsRepaint = false;
        {
            const juce::ScopedLock sl (playheadLock);
            if (playheadActive)
            {
                const float dt = 1.0f / 45.0f;
                playheadProgress += dt / playheadDurationSec;
                if (playheadProgress >= 1.0f)
                    playheadActive = false;
                needsRepaint = true;
            }
        }

        // wireBuzz decays even after playhead finishes
        wireBuzz *= 0.93f;
        if (wireBuzz < 0.001f) wireBuzz = 0.0f;
        else                    needsRepaint = true;

        if (needsRepaint)
            repaint();
        else
            repaint(); // low-rate continual repaint keeps the look "alive"
    }

    void rebuildPreview()
    {
        previewVoice.renderShot (previewBuffer.data(), previewLengthSamples, params, 1.0f);

        float peak = 0.0f;
        for (int i = 0; i < previewLengthSamples; ++i)
            peak = juce::jmax (peak, std::abs (previewBuffer[(size_t) i]));

        previewPeak = peak;

        const float dur = juce::jmax (params.bodyDecayMs, params.noiseDecayMs) / 1000.0f;
        playheadDurationSec = juce::jlimit (0.05f, 1.8f, dur * 1.2f);
    }

    static constexpr double previewSampleRate    = 44100.0;
    static constexpr int    previewLengthSamples = (int) (1.8 * 44100); // 1.8s

    std::array<float, previewLengthSamples> previewBuffer {};
    SnareVoice::Params  params;
    SnareVoice          previewVoice;
    bool                previewDirty = true;
    float               previewPeak  = 0.0f;

    juce::CriticalSection playheadLock;
    bool                  playheadActive   = false;
    float                 playheadProgress = 0.0f;
    float                 playheadVelocity = 1.0f;
    float                 playheadDurationSec = 0.6f;

    float                 wireBuzz  = 0.0f;
    float                 wirePhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareScope)
};
