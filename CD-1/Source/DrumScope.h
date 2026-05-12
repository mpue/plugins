/*
  ==============================================================================

    DrumScope.h
    Cinematic visualizer:
      • Top:    four luxury drum pads (BOOM / HIT / CRACK / SUB) with
                concentric impact rings that bloom on every trigger,
                phosphor glow + click-to-audition.
      • Bottom: a wide phosphor stereo waveform with horizon line, grid,
                impact spark and subtle vignette.

    The component is purely visual.  It receives:
      - notifyTrigger(drumIdx, velocity)   on every drum hit
      - pushAudio(buffer, n)               for the live waveform trail
      - setMasterPeak(L, R)                for meters under the pads

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "CinematicDrumEngine.h"

class DrumScope  : public juce::Component,
                   private juce::Timer
{
public:
    static constexpr int kNumPads = cd1::NumDrums;

    std::function<void (int padIdx, float velocity)> onPadClicked;

    DrumScope()
    {
        setOpaque (false);

        for (int i = 0; i < kNumPads; ++i)
            pads[i].name = cd1::drumName (i);

        // ring buffer for waveform (stereo)
        rb.assign (kRingSize * 2, 0.0f);
        startTimerHz (45);
    }

    ~DrumScope() override = default;

    //----------------------------------------------------------------------
    void notifyTrigger (int drumIdx, float velocity) noexcept
    {
        if (! juce::isPositiveAndBelow (drumIdx, kNumPads))
            return;

        const juce::ScopedLock sl (lock);
        auto& p = pads[drumIdx];
        Ring r;
        r.age      = 0.0f;
        r.life     = 0.55f + 0.25f * velocity;
        r.velocity = juce::jlimit (0.1f, 1.0f, velocity);
        if (p.rings.size() >= 6) p.rings.erase (p.rings.begin());
        p.rings.push_back (r);
        p.glow     = juce::jmin (1.0f, p.glow + 0.6f + 0.4f * velocity);
        p.lastVel  = juce::jlimit (0.0f, 1.0f, velocity);
        sparkAge   = 0.0f;
        sparkActive = true;
        sparkVel   = juce::jlimit (0.0f, 1.0f, velocity);
    }

    // Push a block of audio into the rolling display buffer.
    void pushAudio (const float* l, const float* r, int numSamples) noexcept
    {
        const juce::ScopedLock sl (lock);
        for (int i = 0; i < numSamples; ++i)
        {
            rb[(size_t)(rbWrite * 2 + 0)] = l[i];
            rb[(size_t)(rbWrite * 2 + 1)] = r[i];
            rbWrite = (rbWrite + 1) % kRingSize;
        }
    }

    void setMasterPeak (float pkL, float pkR) noexcept
    {
        meterL = juce::jmax (meterL * 0.78f, pkL);
        meterR = juce::jmax (meterR * 0.78f, pkR);
    }

    //----------------------------------------------------------------------
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int idx = padIndexAt (e.position);
        if (idx < 0) return;
        const float vel = juce::jlimit (0.4f, 1.0f,
                                         1.0f - (float)(e.y - padBoundsCache[(size_t)idx].getY())
                                                 / juce::jmax (1.0f, (float) padBoundsCache[(size_t)idx].getHeight()));
        if (onPadClicked)
            onPadClicked (idx, juce::jlimit (0.4f, 1.0f, vel + 0.2f));
    }

    //----------------------------------------------------------------------
    void paint (juce::Graphics& g) override
    {
        const auto outer = getLocalBounds().toFloat().reduced (1.0f);
        const float corner = 12.0f;

        // ---------- panel background ----------
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

        // ---------- header text ----------
        g.setColour (juce::Colour (0xaa4d9eff));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("CINEMATIC SCOPE",
                    juce::Rectangle<int> ((int) outer.getX() + 14, (int) outer.getY() + 6, 200, 14),
                    juce::Justification::centredLeft);

        // ---------- compute pad row + waveform area ----------
        auto inner = outer.reduced (12.0f, 22.0f);
        const float padRowH = juce::jmin (160.0f, inner.getHeight() * 0.60f);
        auto padRow  = inner.removeFromTop (padRowH);
        inner.removeFromTop (8.0f);
        auto waveArea = inner;

        // ---------- pads ----------
        const float padW   = padRow.getWidth() / (float) kNumPads;
        padBoundsCache.clear();

        for (int i = 0; i < kNumPads; ++i)
        {
            auto pr = juce::Rectangle<float> (padRow.getX() + padW * i, padRow.getY(),
                                                padW, padRow.getHeight()).reduced (8.0f, 4.0f);
            padBoundsCache.push_back (pr.toNearestInt());
            drawPad (g, pr, i);
        }

        // ---------- waveform ----------
        drawWaveform (g, waveArea);

        // ---------- frame ----------
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (outer, corner, 1.0f);

        // ---------- master meters (top right) ----------
        {
            const float mw = 60.0f, mh = 6.0f;
            auto mb = juce::Rectangle<float> (outer.getRight() - mw - 14.0f,
                                                outer.getY() + 8.0f, mw, mh);
            g.setColour (juce::Colour (0x401a2030));
            g.fillRoundedRectangle (mb, 2.0f);
            const float lL = juce::jlimit (0.0f, 1.0f, meterL);
            const float lR = juce::jlimit (0.0f, 1.0f, meterR);
            g.setColour (juce::Colour (0xcc4d9eff));
            g.fillRoundedRectangle (mb.withWidth (mb.getWidth() * lL).withHeight (mh * 0.45f), 2.0f);
            g.setColour (juce::Colour (0xccffaa55));
            g.fillRoundedRectangle (mb.withWidth (mb.getWidth() * lR)
                                       .withY (mb.getY() + mh * 0.55f)
                                       .withHeight (mh * 0.45f), 2.0f);
        }
    }

    void resized() override {}

private:
    //----------------------------------------------------------------------
    void timerCallback() override
    {
        const float dt = 1.0f / 45.0f;

        {
            const juce::ScopedLock sl (lock);

            for (auto& p : pads)
            {
                p.glow *= 0.88f;
                if (p.glow < 0.005f) p.glow = 0.0f;

                for (auto it = p.rings.begin(); it != p.rings.end(); )
                {
                    it->age += dt;
                    if (it->age >= it->life)
                        it = p.rings.erase (it);
                    else
                        ++it;
                }
            }

            if (sparkActive)
            {
                sparkAge += dt;
                if (sparkAge > 0.55f)
                {
                    sparkActive = false;
                    sparkAge    = 0.0f;
                }
            }
        }

        meterL *= 0.86f;
        meterR *= 0.86f;

        repaint();
    }

    int padIndexAt (juce::Point<float> p) const noexcept
    {
        for (int i = 0; i < (int) padBoundsCache.size(); ++i)
            if (padBoundsCache[(size_t) i].toFloat().contains (p))
                return i;
        return -1;
    }

    //----------------------------------------------------------------------
    void drawPad (juce::Graphics& g, juce::Rectangle<float> r, int idx) const
    {
        const auto& pad = pads[(size_t) idx];

        // ---- pad face: deep glass with radial sheen ----
        const float corner = 10.0f;
        juce::ColourGradient face (juce::Colour (0xff1c2533), r.getCentreX(), r.getY(),
                                    juce::Colour (0xff080d14), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (face);
        g.fillRoundedRectangle (r, corner);

        // glowing aura when triggered (blooms during pad.glow)
        if (pad.glow > 0.0f)
        {
            juce::ColourGradient aura (juce::Colour::fromFloatRGBA (0.30f, 0.62f, 1.0f, 0.55f * pad.glow),
                                        r.getCentreX(), r.getCentreY(),
                                        juce::Colour (0x00000000),
                                        r.getCentreX() + r.getWidth() * 0.55f, r.getCentreY(), true);
            g.setGradientFill (aura);
            g.fillRoundedRectangle (r.expanded (4.0f), corner + 2.0f);
        }

        // outer rim
        g.setColour (juce::Colour (0xff2a3344).withAlpha (0.85f));
        g.drawRoundedRectangle (r, corner, 1.2f);

        // ---- concentric impact rings ----
        const float cx = r.getCentreX();
        const float cy = r.getCentreY() + 6.0f;
        const float maxR = juce::jmin (r.getWidth(), r.getHeight()) * 0.45f;

        for (auto& ring : pad.rings)
        {
            const float t  = juce::jlimit (0.0f, 1.0f, ring.age / juce::jmax (0.001f, ring.life));
            const float rad = juce::jmap (t, 0.0f, 1.0f, maxR * 0.20f, maxR * 1.20f);
            const float alpha = (1.0f - t) * 0.85f * ring.velocity;
            const float thick = 1.4f + 1.5f * (1.0f - t);

            g.setColour (juce::Colour::fromFloatRGBA (0.32f, 0.66f, 1.0f, alpha * 0.35f));
            g.drawEllipse (cx - rad - 1.5f, cy - rad - 1.5f,
                           (rad + 1.5f) * 2.0f, (rad + 1.5f) * 2.0f, thick + 2.0f);
            g.setColour (juce::Colour::fromFloatRGBA (0.85f, 0.95f, 1.0f, alpha));
            g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, thick);
        }

        // ---- center dot bloom ----
        const float coreR = 6.0f + 12.0f * pad.glow;
        g.setColour (juce::Colour::fromFloatRGBA (0.30f, 0.62f, 1.0f, 0.30f * (0.4f + 0.6f * pad.glow)));
        g.fillEllipse (cx - coreR, cy - coreR, coreR * 2.0f, coreR * 2.0f);
        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 1.0f, 1.0f, 0.15f + 0.85f * pad.glow));
        g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);

        // ---- name label ----
        g.setColour (juce::Colour (0xff8aa6c8));
        g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
        g.drawText (pad.name,
                    juce::Rectangle<int> ((int) r.getX(), (int) r.getY() + 4,
                                           (int) r.getWidth(), 16),
                    juce::Justification::centred);

        // ---- velocity bar at bottom ----
        const float barH = 4.0f;
        const float barW = r.getWidth() - 16.0f;
        auto bar = juce::Rectangle<float> (r.getX() + 8.0f, r.getBottom() - 14.0f, barW, barH);
        g.setColour (juce::Colour (0x401a2030));
        g.fillRoundedRectangle (bar, 2.0f);

        const float v = juce::jlimit (0.0f, 1.0f, pad.lastVel * (0.4f + 0.6f * pad.glow));
        g.setColour (juce::Colour::fromFloatRGBA (0.30f, 0.62f, 1.0f, 0.85f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * v), 2.0f);
    }

    //----------------------------------------------------------------------
    void drawWaveform (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        // panel bg for waveform sub-area
        const float corner = 8.0f;
        juce::ColourGradient bg (juce::Colour (0xff0a121b), area.getCentreX(), area.getY(),
                                  juce::Colour (0xff03070b), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (area, corner);
        g.setColour (juce::Colour (0xff1d2532));
        g.drawRoundedRectangle (area, corner, 0.8f);

        const float midY = area.getCentreY();
        const float halfH = area.getHeight() * 0.42f;

        // ---- grid ----
        g.setColour (juce::Colour (0x22a8d0ff));
        g.drawLine (area.getX(), midY, area.getRight(), midY, 0.8f);
        g.setColour (juce::Colour (0x10a8d0ff));
        for (int i = 1; i < 8; ++i)
        {
            const float x = area.getX() + area.getWidth() * (i / 8.0f);
            g.drawLine (x, area.getY() + 6, x, area.getBottom() - 6, 0.4f);
        }

        // ---- waveform path (downsampled peak envelope, two stereo lanes) ----
        const int N = juce::jmax (32, (int) area.getWidth());
        if (N <= 4) return;

        std::vector<float> pkPosL (N, 0.0f), pkNegL (N, 0.0f);
        std::vector<float> pkPosR (N, 0.0f), pkNegR (N, 0.0f);

        {
            const juce::ScopedLock sl (lock);
            const int total = kRingSize;
            // newest sample at rbWrite-1; we'll walk the buffer from oldest (rbWrite) → newest
            for (int i = 0; i < N; ++i)
            {
                const int s0 = (int) ((float) i / N * total);
                const int s1 = (int) ((float) (i + 1) / N * total);
                float maxL = 0, minL = 0, maxR = 0, minR = 0;
                for (int s = s0; s < s1; ++s)
                {
                    const int idx = (rbWrite + s) % total;
                    const float vL = rb[(size_t)(idx * 2 + 0)];
                    const float vR = rb[(size_t)(idx * 2 + 1)];
                    if (vL > maxL) maxL = vL; if (vL < minL) minL = vL;
                    if (vR > maxR) maxR = vR; if (vR < minR) minR = vR;
                }
                pkPosL[(size_t) i] = juce::jlimit (-1.0f, 1.0f, maxL);
                pkNegL[(size_t) i] = juce::jlimit (-1.0f, 1.0f, minL);
                pkPosR[(size_t) i] = juce::jlimit (-1.0f, 1.0f, maxR);
                pkNegR[(size_t) i] = juce::jlimit (-1.0f, 1.0f, minR);
            }
        }

        const float xStep = area.getWidth() / (float) N;

        auto buildEnvelopePath = [&] (juce::Path& fill, juce::Path& topLine, juce::Path& botLine,
                                       const std::vector<float>& pP, const std::vector<float>& pN)
        {
            topLine.startNewSubPath (area.getX(), midY - pP[0] * halfH);
            botLine.startNewSubPath (area.getX(), midY - pN[0] * halfH);
            for (int i = 1; i < N; ++i)
            {
                const float x = area.getX() + xStep * (float) i;
                topLine.lineTo (x, midY - pP[(size_t) i] * halfH);
                botLine.lineTo (x, midY - pN[(size_t) i] * halfH);
            }
            fill.startNewSubPath (area.getX(), midY - pP[0] * halfH);
            for (int i = 1; i < N; ++i)
                fill.lineTo (area.getX() + xStep * i, midY - pP[(size_t) i] * halfH);
            for (int i = N - 1; i >= 0; --i)
                fill.lineTo (area.getX() + xStep * i, midY - pN[(size_t) i] * halfH);
            fill.closeSubPath();
        };

        juce::Path fillL, topL, botL;
        juce::Path fillR, topR, botR;
        buildEnvelopePath (fillL, topL, botL, pkPosL, pkNegL);
        buildEnvelopePath (fillR, topR, botR, pkPosR, pkNegR);

        // ---- left channel: blue ----
        {
            juce::ColourGradient grad (juce::Colour (0x884d9eff), midY, area.getY(),
                                        juce::Colour (0x224d9eff), midY, area.getBottom(), false);
            g.setGradientFill (grad);
            g.fillPath (fillL);
            g.setColour (juce::Colour (0x224d9eff));
            g.strokePath (topL, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
            g.strokePath (botL, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
            g.setColour (juce::Colour (0xffd2e7ff));
            g.strokePath (topL, juce::PathStrokeType (1.1f));
            g.strokePath (botL, juce::PathStrokeType (1.1f));
        }

        // ---- right channel: warm amber, slightly transparent ----
        {
            juce::ColourGradient grad (juce::Colour (0x66ffaa55), midY, area.getY(),
                                        juce::Colour (0x22ffaa55), midY, area.getBottom(), false);
            g.setGradientFill (grad);
            g.fillPath (fillR);
            g.setColour (juce::Colour (0x33ffaa55));
            g.strokePath (topR, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved));
            g.strokePath (botR, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved));
            g.setColour (juce::Colour (0xffffe0c0));
            g.strokePath (topR, juce::PathStrokeType (0.8f));
            g.strokePath (botR, juce::PathStrokeType (0.8f));
        }

        // ---- spark on hit (right edge flash) ----
        if (sparkActive)
        {
            const float t = juce::jlimit (0.0f, 1.0f, sparkAge / 0.55f);
            const float a = (1.0f - t) * 0.7f * sparkVel;
            juce::ColourGradient sp (juce::Colour::fromFloatRGBA (0.7f, 0.9f, 1.0f, a),
                                      area.getRight(), midY,
                                      juce::Colour (0x00000000),
                                      area.getRight() - 90.0f, midY, true);
            g.setGradientFill (sp);
            g.fillRect (area);
        }
    }

    //----------------------------------------------------------------------
    struct Ring   { float age = 0, life = 0.5f, velocity = 1.0f; };
    struct PadVis
    {
        juce::String name;
        std::vector<Ring> rings;
        float glow      = 0.0f;
        float lastVel   = 0.0f;
    };

    PadVis pads[kNumPads];
    std::vector<juce::Rectangle<int>> padBoundsCache;

    static constexpr int kRingSize = 8192;
    std::vector<float> rb;
    int rbWrite = 0;

    float meterL = 0.0f, meterR = 0.0f;

    bool  sparkActive = false;
    float sparkAge    = 0.0f;
    float sparkVel    = 0.0f;

    juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumScope)
};
