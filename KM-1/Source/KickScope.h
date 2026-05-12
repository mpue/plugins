/*
  ==============================================================================

    KickScope.h
    Luxury kick visualiser. Renders an offline preview of the current kick
    parameters into a 1.5-second buffer and draws it as a glowing,
    phosphor-style waveform with a filled body, an envelope contour and a
    thin pitch curve overlay. A live playhead sweeps across the trace
    whenever the user actually plays a note.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "KickEngine.h"

class PluginProcessor;

class KickScope : public juce::Component, private juce::Timer
{
public:
    KickScope()
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);

        previewVoice.prepare (previewSampleRate);
        rebuildPreview();

        startTimerHz (45);
    }

    ~KickScope() override = default;

    // Called from the editor whenever any parameter changes.
    void setParams (const KickVoice::Params& p)
    {
        params = p;
        previewDirty = true;
    }

    // Called whenever a kick is actually triggered (audio-thread → editor).
    void notifyTriggered (float velocity = 1.0f)
    {
        const juce::ScopedLock sl (playheadLock);
        playheadActive   = true;
        playheadProgress = 0.0f;
        playheadVelocity = juce::jlimit (0.1f, 1.0f, velocity);
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

        // soft inner vignette
        {
            juce::ColourGradient v (juce::Colour (0x00000000), outer.getCentreX(), outer.getCentreY(),
                                     juce::Colour (0x66000000), outer.getRight(), outer.getBottom(), true);
            g.setGradientFill (v);
            g.fillRoundedRectangle (outer.reduced (1.0f), corner - 1.0f);
        }

        const auto area  = outer.reduced (16.0f, 26.0f);
        const float midY = area.getCentreY();
        const float halfH = area.getHeight() * 0.45f;

        // ---- amplitude / time grid ----
        g.setColour (juce::Colour (0x12a8d0ff));
        for (int i = 1; i < 4; ++i)
        {
            const float y = area.getY() + area.getHeight() * (i / 4.0f);
            g.drawLine (area.getX(), y, area.getRight(), y, 0.7f);
        }
        g.setColour (juce::Colour (0x33a8d0ff));
        g.drawLine (area.getX(), midY, area.getRight(), midY, 1.0f);

        g.setColour (juce::Colour (0x10a8d0ff));
        for (int i = 1; i < 10; ++i)
        {
            const float x = area.getX() + area.getWidth() * (i / 10.0f);
            g.drawLine (x, area.getY(), x, area.getBottom(), 0.5f);
        }

        // ---- waveform path (downsampled mini/max envelope) ----
        const int  N      = (int) area.getWidth();
        const int  total  = previewLengthSamples;
        if (N <= 4 || total <= 4) return;

        juce::Path topPath, botPath, fillPath;
        const float xStep = area.getWidth() / (float) N;

        std::vector<float> peakPos (N, 0.0f);
        std::vector<float> peakNeg (N, 0.0f);

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

        // build positive top / negative bottom paths
        topPath.startNewSubPath (area.getX(), midY - peakPos[0] * halfH);
        botPath.startNewSubPath (area.getX(), midY - peakNeg[0] * halfH);
        for (int i = 1; i < N; ++i)
        {
            const float x = area.getX() + xStep * (float) i;
            topPath.lineTo (x, midY - peakPos[(size_t) i] * halfH);
            botPath.lineTo (x, midY - peakNeg[(size_t) i] * halfH);
        }

        // closed fill region between top and bottom envelope
        fillPath.startNewSubPath (area.getX(), midY - peakPos[0] * halfH);
        for (int i = 1; i < N; ++i)
        {
            const float x = area.getX() + xStep * (float) i;
            fillPath.lineTo (x, midY - peakPos[(size_t) i] * halfH);
        }
        for (int i = N - 1; i >= 0; --i)
        {
            const float x = area.getX() + xStep * (float) i;
            fillPath.lineTo (x, midY - peakNeg[(size_t) i] * halfH);
        }
        fillPath.closeSubPath();

        // gradient body fill
        juce::ColourGradient fillGrad (juce::Colour (0x884d9eff), midY, area.getY(),
                                        juce::Colour (0x224d9eff), midY, area.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (fillPath);

        // outer glow stroke (3-pass phosphor)
        g.setColour (juce::Colour (0x224d9eff));
        g.strokePath (topPath, juce::PathStrokeType (8.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (8.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0x554d9eff));
        g.strokePath (topPath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffd2e7ff));
        g.strokePath (topPath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ---- pitch contour overlay ----
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
                const float fHz  = juce::jlimit (5.0f, 4000.0f,
                                                  params.tuneHz * std::pow (2.0f, oct));
                const float fNorm = juce::jlimit (0.0f, 1.0f,
                                                   std::log10 (juce::jmax (1.0f, fHz / 20.0f))
                                                   / std::log10 (1000.0f / 20.0f));
                const float y = area.getY() + (1.0f - fNorm) * area.getHeight() * 0.30f + 4.0f;
                const float x = area.getX() + xStep * (float) i;
                if (i == 0) pitchPath.startNewSubPath (x, y);
                else        pitchPath.lineTo (x, y);
            }
            g.setColour (juce::Colour (0x66ffaa55));
            g.strokePath (pitchPath, juce::PathStrokeType (1.4f));
        }

        // ---- live playhead ----
        {
            const juce::ScopedLock sl (playheadLock);
            if (playheadActive)
            {
                const float x = area.getX() + area.getWidth() * juce::jlimit (0.0f, 1.0f, playheadProgress);

                g.setColour (juce::Colour (0x554d9eff));
                g.drawLine (x, area.getY(), x, area.getBottom(), 3.0f);
                g.setColour (juce::Colour (0xffe8f3ff));
                g.drawLine (x, area.getY(), x, area.getBottom(), 1.0f);

                const float radius = 6.0f * playheadVelocity + 4.0f;
                g.setColour (juce::Colour (0x88e8f3ff));
                g.fillEllipse (x - radius, midY - radius, radius * 2.0f, radius * 2.0f);
                g.setColour (juce::Colour (0xffffffff));
                g.fillEllipse (x - radius * 0.5f, midY - radius * 0.5f, radius, radius);
            }
        }

        // ---- frame ----
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (outer, corner, 1.0f);

        // ---- header ----
        g.setColour (juce::Colour (0xaa4d9eff));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("KICK SCOPE",
                    juce::Rectangle<int> ((int) outer.getX() + 14, (int) outer.getY() + 6, 120, 14),
                    juce::Justification::centredLeft);

        // ---- live readout ----
        g.setColour (juce::Colour (0x88e8e8e8));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        const juce::String info =
            juce::String (params.tuneHz, 1) + " Hz   "
          + "+" + juce::String (params.pitchAmtSemis, 0) + " st   "
          + juce::String (params.bodyDecayMs, 0) + " ms   "
          + "peak " + juce::String (juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, previewPeak)), 1) + " dB";
        g.drawText (info,
                    juce::Rectangle<int> ((int) outer.getRight() - 360, (int) outer.getY() + 6, 346, 14),
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

        // advance playhead
        bool needsRepaint = previewDirty;
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

        if (needsRepaint || true) // always repaint at low rate to keep look alive
            repaint();
    }

    void rebuildPreview()
    {
        previewVoice.renderShot (previewBuffer.data(), previewLengthSamples, params, 1.0f);

        float peak = 0.0f;
        for (int i = 0; i < previewLengthSamples; ++i)
            peak = juce::jmax (peak, std::abs (previewBuffer[(size_t) i]));

        previewPeak = peak;

        playheadDurationSec = juce::jlimit (0.05f, 1.5f, (params.bodyDecayMs / 1000.0f) * 1.2f);
    }

    static constexpr double previewSampleRate = 44100.0;
    static constexpr int    previewLengthSamples = (int) (1.5 * 44100); // 1.5s

    std::array<float, previewLengthSamples> previewBuffer {};
    KickVoice::Params  params;
    KickVoice          previewVoice;
    bool               previewDirty = true;
    float              previewPeak  = 0.0f;

    juce::CriticalSection playheadLock;
    bool                  playheadActive   = false;
    float                 playheadProgress = 0.0f;
    float                 playheadVelocity = 1.0f;
    float                 playheadDurationSec = 0.6f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickScope)
};
