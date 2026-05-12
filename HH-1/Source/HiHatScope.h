/*
  ==============================================================================

    HiHatScope.h
    Luxury hi-hat visualiser. Renders an offline preview of the current hi-hat
    parameters into a 1.6-second stereo buffer and draws it as a glowing twin
    phosphor waveform (L on top, R below) with a spectral "metal cluster"
    overlay showing the six oscillator bands, an HP/BP filter curve hint, an
    amplitude envelope contour and a flurry of sparkle particles that react
    to incoming triggers.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "HiHatEngine.h"

class HiHatScope : public juce::Component, private juce::Timer
{
public:
    HiHatScope()
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);

        previewVoiceL.prepare (previewSampleRate);
        previewVoiceR.prepare (previewSampleRate);
        rebuildPreview();

        startTimerHz (45);
    }

    ~HiHatScope() override = default;

    void setParams (const HiHatVoice::Params& p)
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

        // Spawn sparkle particles for the cymbal-shimmer effect
        const int n = 18 + (int) (velocity * 14.0f);
        for (int i = 0; i < n; ++i)
            sparks.push_back (makeSpark (velocity));
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

        auto traceArea = outer.reduced (16.0f, 26.0f);

        const float midY  = traceArea.getCentreY();
        const float halfH = traceArea.getHeight() * 0.42f;

        // ---- amplitude / time grid ----
        g.setColour (juce::Colour (0x14ffd2a8));
        for (int i = 1; i < 4; ++i)
        {
            const float y = traceArea.getY() + traceArea.getHeight() * (i / 4.0f);
            g.drawLine (traceArea.getX(), y, traceArea.getRight(), y, 0.7f);
        }
        g.setColour (juce::Colour (0x33ffd2a8));
        g.drawLine (traceArea.getX(), midY, traceArea.getRight(), midY, 1.0f);

        g.setColour (juce::Colour (0x10ffd2a8));
        for (int i = 1; i < 10; ++i)
        {
            const float x = traceArea.getX() + traceArea.getWidth() * (i / 10.0f);
            g.drawLine (x, traceArea.getY(), x, traceArea.getBottom(), 0.5f);
        }

        // ---- waveform paths (downsampled min/max envelope per channel) ----
        const int  N      = (int) traceArea.getWidth();
        const int  total  = previewLengthSamples;
        if (N <= 4 || total <= 4) return;

        const float xStep = traceArea.getWidth() / (float) N;

        std::vector<float> peakLPos ((size_t) N, 0.0f);
        std::vector<float> peakLNeg ((size_t) N, 0.0f);
        std::vector<float> peakRPos ((size_t) N, 0.0f);
        std::vector<float> peakRNeg ((size_t) N, 0.0f);

        for (int i = 0; i < N; ++i)
        {
            const int s0 = juce::jlimit (0, total - 1, (int) ((float) i / N * total));
            const int s1 = juce::jlimit (0, total - 1, (int) ((float) (i + 1) / N * total));
            float maxL = 0.0f, minL = 0.0f, maxR = 0.0f, minR = 0.0f;
            for (int s = s0; s < s1; ++s)
            {
                const float l = previewBufferL[(size_t) s];
                const float r = previewBufferR[(size_t) s];
                if (l > maxL) maxL = l;
                if (l < minL) minL = l;
                if (r > maxR) maxR = r;
                if (r < minR) minR = r;
            }
            peakLPos[(size_t) i] = juce::jlimit (-1.0f, 1.0f, maxL);
            peakLNeg[(size_t) i] = juce::jlimit (-1.0f, 1.0f, minL);
            peakRPos[(size_t) i] = juce::jlimit (-1.0f, 1.0f, maxR);
            peakRNeg[(size_t) i] = juce::jlimit (-1.0f, 1.0f, minR);
        }

        const float yL = midY - traceArea.getHeight() * 0.22f;
        const float yR = midY + traceArea.getHeight() * 0.22f;
        const float ampH = halfH * 0.55f;

        auto buildPaths = [&] (const std::vector<float>& pos,
                               const std::vector<float>& neg,
                               float yCenter,
                               juce::Path& topP, juce::Path& botP, juce::Path& fillP)
        {
            topP.startNewSubPath (traceArea.getX(), yCenter - pos[0] * ampH);
            botP.startNewSubPath (traceArea.getX(), yCenter - neg[0] * ampH);
            for (int i = 1; i < N; ++i)
            {
                const float x = traceArea.getX() + xStep * (float) i;
                topP.lineTo (x, yCenter - pos[(size_t) i] * ampH);
                botP.lineTo (x, yCenter - neg[(size_t) i] * ampH);
            }
            fillP.startNewSubPath (traceArea.getX(), yCenter - pos[0] * ampH);
            for (int i = 1; i < N; ++i)
            {
                const float x = traceArea.getX() + xStep * (float) i;
                fillP.lineTo (x, yCenter - pos[(size_t) i] * ampH);
            }
            for (int i = N - 1; i >= 0; --i)
            {
                const float x = traceArea.getX() + xStep * (float) i;
                fillP.lineTo (x, yCenter - neg[(size_t) i] * ampH);
            }
            fillP.closeSubPath();
        };

        juce::Path topL, botL, fillL, topR, botR, fillR;
        buildPaths (peakLPos, peakLNeg, yL, topL, botL, fillL);
        buildPaths (peakRPos, peakRNeg, yR, topR, botR, fillR);

        // gradient body fills - amber-gold gives the hi-hat its "brass" character
        juce::ColourGradient fillGrad (juce::Colour (0x88ffc97a), midY, traceArea.getY(),
                                        juce::Colour (0x22ff8a30), midY, traceArea.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (fillL);
        g.fillPath (fillR);

        auto strokeWith3Pass = [&g] (juce::Path& path)
        {
            g.setColour (juce::Colour (0x33ff8a30));
            g.strokePath (path, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0x66ffc97a));
            g.strokePath (path, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xfffff0d0));
            g.strokePath (path, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        strokeWith3Pass (topL);
        strokeWith3Pass (botL);
        strokeWith3Pass (topR);
        strokeWith3Pass (botR);

        // L / R labels
        g.setColour (juce::Colour (0x88ffc97a));
        g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
        g.drawText ("L", juce::Rectangle<int> ((int) traceArea.getX() + 4,
                                                 (int) (yL - 14.0f), 12, 12),
                    juce::Justification::centredLeft);
        g.drawText ("R", juce::Rectangle<int> ((int) traceArea.getX() + 4,
                                                 (int) (yR - 14.0f), 12, 12),
                    juce::Justification::centredLeft);

        // ---- amplitude envelope contour (cyan dashed) ----
        {
            juce::Path envPath;
            const float h     = juce::jlimit (0.0f, 1.0f, params.holdLevel);
            const float fast  = std::exp (std::log (0.001f) /
                                           juce::jmax (1.0f, params.decayMs * 0.001f * (float) previewSampleRate));
            const float slow  = std::exp (std::log (0.001f) /
                                           juce::jmax (1.0f, params.decayMs * (1.0f + h * 7.0f)
                                                              * 0.001f * (float) previewSampleRate));
            for (int i = 0; i < N; ++i)
            {
                const int   s0 = juce::jlimit (0, total - 1, (int) ((float) i / N * total));
                const float ef = std::pow (fast, (float) s0);
                const float es = std::pow (slow, (float) s0);
                const float e  = ef * (1.0f - h) + es * h;
                const float y  = traceArea.getY() + (1.0f - e) * traceArea.getHeight();
                const float x  = traceArea.getX() + xStep * (float) i;
                if (i == 0) envPath.startNewSubPath (x, y);
                else        envPath.lineTo (x, y);
            }
            g.setColour (juce::Colour (0x55a8e8ff));
            const float dash[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (envPath, envPath, dash, 2);
            g.strokePath (envPath, juce::PathStrokeType (1.2f));
        }

        // ---- spectral metal cluster overlay (six glowing vertical bands) ----
        {
            // Map a frequency 20..20000 Hz to a normalised x position in the trace area
            // using a logarithmic scale (perceptual layout, like a spectrum analyser).
            auto fToX = [&] (float f) -> float
            {
                const float fNorm = juce::jlimit (0.0f, 1.0f,
                                                    std::log10 (juce::jmax (1.0f, f / 20.0f))
                                                        / std::log10 (20000.0f / 20.0f));
                return traceArea.getX() + traceArea.getWidth() * fNorm;
            };

            static constexpr float baseRatios[6] = {
                1.0000f, 1.3420f, 1.6688f, 2.0000f, 2.5028f, 3.0300f
            };

            const float harm = juce::jlimit (0.0f, 1.0f, params.harmonics);

            for (int i = 0; i < 6; ++i)
            {
                const float ratio = juce::jmap (harm, 1.0f, baseRatios[i]);
                const float fHz   = params.tuneHz * ratio;
                const float x     = fToX (fHz);

                // attenuate visible weight if outside the HP cutoff (i.e. filtered out).
                const float hpAtten = juce::jlimit (0.15f, 1.0f,
                                                     fHz / juce::jmax (50.0f, params.hpCutoffHz));
                const float a       = 0.55f * params.metalLevel * hpAtten;

                g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.85f, 0.55f, a));
                g.drawLine (x, traceArea.getY() + 6.0f,
                            x, traceArea.getBottom() - 6.0f, 1.0f);

                // Glow halo
                g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.85f, 0.55f, a * 0.3f));
                g.drawLine (x - 1.0f, traceArea.getY() + 6.0f,
                            x - 1.0f, traceArea.getBottom() - 6.0f, 0.7f);
                g.drawLine (x + 1.0f, traceArea.getY() + 6.0f,
                            x + 1.0f, traceArea.getBottom() - 6.0f, 0.7f);
            }

            // HP cutoff marker
            const float xHp = fToX (params.hpCutoffHz);
            g.setColour (juce::Colour (0x553080d6));
            const float dashHp[] = { 3.0f, 3.0f };
            juce::Line<float> hpLine (xHp, traceArea.getY() + 4.0f,
                                       xHp, traceArea.getBottom() - 4.0f);
            juce::Path hpPath;
            hpPath.startNewSubPath (hpLine.getStart());
            hpPath.lineTo           (hpLine.getEnd());
            juce::Path dashedHp;
            juce::PathStrokeType (1.0f).createDashedStroke (dashedHp, hpPath, dashHp, 2);
            g.strokePath (dashedHp, juce::PathStrokeType (1.0f));

            // BP shimmer marker
            const float xBp = fToX (params.bpCutoffHz);
            g.setColour (juce::Colour (0x55ffaa55));
            juce::Path bpPath;
            bpPath.startNewSubPath (xBp, traceArea.getY() + 4.0f);
            bpPath.lineTo           (xBp, traceArea.getBottom() - 4.0f);
            juce::Path dashedBp;
            juce::PathStrokeType (1.0f).createDashedStroke (dashedBp, bpPath, dashHp, 2);
            g.strokePath (dashedBp, juce::PathStrokeType (1.0f));
        }

        // ---- live playhead ----
        {
            const juce::ScopedLock sl (playheadLock);
            if (playheadActive)
            {
                const float x = traceArea.getX() + traceArea.getWidth()
                                  * juce::jlimit (0.0f, 1.0f, playheadProgress);

                g.setColour (juce::Colour (0x55ffd09a));
                g.drawLine (x, traceArea.getY(), x, traceArea.getBottom(), 3.0f);
                g.setColour (juce::Colour (0xfffff0d0));
                g.drawLine (x, traceArea.getY(), x, traceArea.getBottom(), 1.0f);

                const float radius = 6.0f * playheadVelocity + 4.0f;
                g.setColour (juce::Colour (0x88fff0d0));
                g.fillEllipse (x - radius, midY - radius, radius * 2.0f, radius * 2.0f);
                g.setColour (juce::Colour (0xffffffff));
                g.fillEllipse (x - radius * 0.5f, midY - radius * 0.5f, radius, radius);
            }
        }

        // ---- sparkle particles (cymbal shimmer) ----
        for (auto& s : sparks)
        {
            if (s.life <= 0.0f) continue;
            const float a = juce::jlimit (0.0f, 1.0f, s.life) * 0.85f;
            const float x = juce::jmap (s.x, traceArea.getX(), traceArea.getRight());
            const float y = juce::jmap (s.y, traceArea.getY(), traceArea.getBottom());
            const float r = s.size;

            // glow
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.7f, a * 0.35f));
            g.fillEllipse (x - r * 1.8f, y - r * 1.8f, r * 3.6f, r * 3.6f);
            // core
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 1.0f, 0.95f, a));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }

        // ---- frame ----
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (outer, corner, 1.0f);

        // ---- header ----
        g.setColour (juce::Colour (0xaaffc97a));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("HI-HAT SCOPE",
                    juce::Rectangle<int> ((int) outer.getX() + 14, (int) outer.getY() + 6, 160, 14),
                    juce::Justification::centredLeft);

        // ---- live readout ----
        g.setColour (juce::Colour (0x88e8e8e8));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        const float bpKHz = params.bpCutoffHz / 1000.0f;
        const float hpKHz = params.hpCutoffHz / 1000.0f;
        const juce::String info =
            juce::String (params.tuneHz, 0) + " Hz   "
          + juce::String (params.decayMs, 0) + " ms   "
          + "HP " + juce::String (hpKHz, 2) + "k   "
          + "BP " + juce::String (bpKHz, 2) + "k   "
          + "peak " + juce::String (juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, previewPeak)), 1) + " dB";
        g.drawText (info,
                    juce::Rectangle<int> ((int) outer.getRight() - 460, (int) outer.getY() + 6, 446, 14),
                    juce::Justification::centredRight);
    }

    void resized() override {}

private:
    struct Spark
    {
        float x      = 0.5f;
        float y      = 0.5f;
        float vx     = 0.0f;
        float vy     = 0.0f;
        float life   = 1.0f;
        float decay  = 0.04f;
        float size   = 1.5f;
    };

    Spark makeSpark (float velocity)
    {
        Spark s;
        // start near the hit (left side of the trace)
        s.x     = 0.05f + rngVis.nextFloat() * 0.08f;
        s.y     = 0.5f + (rngVis.nextFloat() - 0.5f) * 0.6f;
        s.vx    = 0.005f + rngVis.nextFloat() * 0.02f * velocity;
        s.vy    = (rngVis.nextFloat() - 0.5f) * 0.018f;
        s.life  = 0.6f + rngVis.nextFloat() * 0.5f;
        s.decay = 0.018f + rngVis.nextFloat() * 0.025f;
        s.size  = 1.0f + rngVis.nextFloat() * 1.8f;
        return s;
    }

    void timerCallback() override
    {
        if (previewDirty)
        {
            rebuildPreview();
            previewDirty = false;
        }

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

        if (! sparks.empty())
        {
            for (auto& s : sparks)
            {
                if (s.life > 0.0f)
                {
                    s.x    += s.vx;
                    s.y    += s.vy;
                    s.vy   += 0.0008f;          // tiny gravity
                    s.life -= s.decay;
                    needsRepaint = true;
                }
            }
            sparks.erase (std::remove_if (sparks.begin(), sparks.end(),
                                            [] (const Spark& s) { return s.life <= 0.0f; }),
                           sparks.end());
        }

        // Always repaint at low rate to keep visuals "breathing"
        repaint();
        juce::ignoreUnused (needsRepaint);
    }

    void rebuildPreview()
    {
        // render stereo
        previewVoiceL.prepare (previewSampleRate);
        previewVoiceL.trigger (params, 1.0f);
        for (int i = 0; i < previewLengthSamples; ++i)
        {
            float l = 0.0f, r = 0.0f;
            previewVoiceL.renderStereo (l, r);
            previewBufferL[(size_t) i] = l;
            previewBufferR[(size_t) i] = r;
        }

        float peak = 0.0f;
        for (int i = 0; i < previewLengthSamples; ++i)
        {
            peak = juce::jmax (peak, std::abs (previewBufferL[(size_t) i]));
            peak = juce::jmax (peak, std::abs (previewBufferR[(size_t) i]));
        }
        previewPeak = peak;

        const float h    = juce::jlimit (0.0f, 1.0f, params.holdLevel);
        const float dur  = params.decayMs * (1.0f + h * 5.5f) / 1000.0f;
        playheadDurationSec = juce::jlimit (0.05f, 1.6f, dur * 1.2f);
    }

    static constexpr double previewSampleRate    = 44100.0;
    static constexpr int    previewLengthSamples = (int) (1.6 * 44100); // 1.6s

    std::array<float, previewLengthSamples> previewBufferL {};
    std::array<float, previewLengthSamples> previewBufferR {};

    HiHatVoice::Params params;
    HiHatVoice         previewVoiceL;
    HiHatVoice         previewVoiceR;   // unused but kept symmetrical for future
    bool               previewDirty = true;
    float              previewPeak  = 0.0f;

    juce::CriticalSection playheadLock;
    bool                  playheadActive   = false;
    float                 playheadProgress = 0.0f;
    float                 playheadVelocity = 1.0f;
    float                 playheadDurationSec = 0.6f;

    std::vector<Spark> sparks;
    juce::Random       rngVis;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HiHatScope)
};
