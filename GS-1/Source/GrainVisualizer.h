/*
  ==============================================================================

    GrainVisualizer.h
    Cinematic visualization for GS-1. Three layered displays in one canvas:

      1. SOURCE WAVEFORM (top half) - the currently selected internal source
         displayed as a downsampled minmax wave. A cursor indicates the
         current "Position" knob value, and a translucent band around it
         shows the "Spray" range. Active grains are shown as moving glowing
         tick-marks at the actual sample positions they are reading from.

      2. GRAIN CLOUD (bottom half) - every active grain is rendered as a
         glowing soft particle, x = horizontal source-position, y = pitch
         ratio (mapped to the vertical axis), color = voice index
         (rainbow). A short trail of recent grains gives a sense of
         continuous motion.

      3. UI OVERLAYS - corner labels (source name / grain count / voice
         count), stereo VU meters on the right, an LFO orb in the lower-
         left corner, a cosmic starfield + vignette to give depth.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GranularEngine.h"
#include <array>

class GrainVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit GrainVisualizer (GranularEngine& engine) : eng (engine)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        startTimerHz (45);

        for (auto& s : stars)
        {
            s.x = juce::Random::getSystemRandom().nextFloat();
            s.y = juce::Random::getSystemRandom().nextFloat();
            s.brightness = juce::Random::getSystemRandom().nextFloat() * 0.7f + 0.10f;
            s.twinkleSpeed = juce::Random::getSystemRandom().nextFloat() * 1.5f + 0.4f;
            s.twinklePhase = juce::Random::getSystemRandom().nextFloat() * 6.28f;
        }

        cachedSourceIdx = -1;
    }

    ~GrainVisualizer() override = default;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // --- Background: cosmic gradient ----------------------------------
        juce::ColourGradient bg (
            juce::Colour (0xff0d1430), bounds.getCentreX(), bounds.getY(),
            juce::Colour (0xff05080f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 10.0f);

        // Vignette
        juce::ColourGradient vignette (
            juce::Colour (0x00000000), bounds.getCentreX(), bounds.getCentreY(),
            juce::Colour (0xaa000000), bounds.getRight(),   bounds.getBottom(), true);
        g.setGradientFill (vignette);
        g.fillRoundedRectangle (bounds, 10.0f);

        // Inner stroke
        g.setColour (juce::Colour (0x224d9eff));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 10.0f, 1.0f);

        // Subtle starfield in background
        drawStarfield (g, bounds);

        // Layout: top = source wave (50%), bottom = grain cloud
        const float headerH = 22.0f;
        const float pad     = 14.0f;
        auto inner = bounds.reduced (pad, pad + 4.0f);
        inner.removeFromTop (headerH);

        const float gap     = 12.0f;
        const float waveH   = (inner.getHeight() - gap) * 0.50f;
        auto waveArea  = inner.removeFromTop (waveH);
        inner.removeFromTop (gap);
        auto cloudArea = inner;

        drawSourcePanel (g, waveArea);
        drawGrainCloud  (g, cloudArea);
        drawLevelBars   (g, bounds);
        drawLfoOrb      (g, bounds);
        drawCornerLabels(g, bounds);
    }

    void resized() override {}

private:
    static constexpr int kNumStars = 70;

    struct Star
    {
        float x = 0.5f, y = 0.5f;
        float brightness = 0.5f;
        float twinkleSpeed = 1.0f;
        float twinklePhase = 0.0f;
    };

    GranularEngine& eng;

    std::array<Star, kNumStars> stars;

    // For source waveform mini caching
    int   cachedSourceIdx = -1;
    int   cachedSrcLength = 0;
    std::vector<float> minPeaks, maxPeaks;
    int   cacheBins = 0;

    // Smoothed visualizer state
    float activitySmoothed = 0.0f;
    float levelLSmoothed = 0.0f;
    float levelRSmoothed = 0.0f;
    float positionSmoothed = 0.30f;
    float spraySmoothed    = 0.20f;

    // Trail of recent grain positions
    struct TrailPoint
    {
        float x = 0.0f, y = 0.0f;
        float life = 1.0f;          // 1 = fresh, fades to 0
        int   voiceIdx = 0;
    };
    std::vector<TrailPoint> trail;

    int ticks = 0;

    void timerCallback() override
    {
        ++ticks;

        const float activity = eng.getActivity();
        const float lvlL     = eng.getLevelL();
        const float lvlR     = eng.getLevelR();

        activitySmoothed = 0.85f * activitySmoothed + 0.15f * juce::jlimit (0.0f, 1.0f, activity * 9.0f);
        levelLSmoothed   = 0.85f * levelLSmoothed   + 0.15f * juce::jlimit (0.0f, 1.0f, lvlL * 1.2f);
        levelRSmoothed   = 0.85f * levelRSmoothed   + 0.15f * juce::jlimit (0.0f, 1.0f, lvlR * 1.2f);

        const auto& p = eng.getParameters();
        positionSmoothed = 0.80f * positionSmoothed + 0.20f * juce::jlimit (0.0f, 1.0f, p.position);
        spraySmoothed    = 0.80f * spraySmoothed    + 0.20f * juce::jlimit (0.0f, 1.0f, p.spray);

        // Capture current grain positions into the trail
        const auto snap = eng.getGrainSnapshot();
        for (const auto& gs : snap)
        {
            if (! gs.active) continue;
            // age01 < 0.5 = grain is fresh
            if (gs.age01 < 0.05f)
            {
                TrailPoint tp;
                tp.x = juce::jlimit (0.0f, 1.0f, gs.pos01);
                // Map pitch ratio to vertical axis: ratio 0.25..4.0 => 1..0
                const float pitch01 = juce::jlimit (0.0f, 1.0f,
                                        std::log2 (juce::jmax (0.25f, gs.pitchRatio) / 0.25f) / 4.0f);
                tp.y = 1.0f - pitch01;
                tp.life = 1.0f;
                tp.voiceIdx = gs.voiceIdx;
                trail.push_back (tp);
            }
        }

        // Decay trail
        for (auto& tp : trail) tp.life -= 0.025f;
        trail.erase (std::remove_if (trail.begin(), trail.end(),
                                     [] (const TrailPoint& t) { return t.life <= 0.0f; }),
                     trail.end());
        if (trail.size() > 800) trail.erase (trail.begin(), trail.begin() + (long) (trail.size() - 800));

        // Twinkle stars
        const float dt = 1.0f / 45.0f;
        for (auto& s : stars) s.twinklePhase += dt * s.twinkleSpeed;

        // Refresh cached source minmax if source changed
        const int srcIdx = p.sourceIdx;
        const auto& src = eng.getCurrentSourceBuffer();
        if (srcIdx != cachedSourceIdx
            || (int) src.size() != cachedSrcLength
            || cacheBins == 0
            || cacheBins != juce::jmax (1, getWidth() - 32))
        {
            rebuildSourceCache (src, srcIdx);
        }

        repaint();
    }

    void rebuildSourceCache (const std::vector<float>& src, int srcIdx)
    {
        const int targetBins = juce::jmax (1, getWidth() - 32);
        if (src.empty() || targetBins <= 0)
        {
            minPeaks.clear(); maxPeaks.clear();
            cachedSrcLength = 0; cachedSourceIdx = srcIdx; cacheBins = 0;
            return;
        }
        minPeaks.assign ((size_t) targetBins, 0.0f);
        maxPeaks.assign ((size_t) targetBins, 0.0f);
        const int srcLen = (int) src.size();
        const float perBin = (float) srcLen / (float) targetBins;
        for (int b = 0; b < targetBins; ++b)
        {
            const int s0 = (int) (b * perBin);
            const int s1 = juce::jmin (srcLen, (int) ((b + 1) * perBin));
            float lo = 0.0f, hi = 0.0f;
            for (int i = s0; i < s1; ++i)
            {
                const float v = src[(size_t) i];
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
            minPeaks[(size_t) b] = lo;
            maxPeaks[(size_t) b] = hi;
        }
        cachedSourceIdx = srcIdx;
        cachedSrcLength = srcLen;
        cacheBins = targetBins;
    }

    static juce::Colour voiceColour (int voiceIdx)
    {
        // Distribute hues across 8 voices
        const float hue = juce::jlimit (0.0f, 1.0f, ((float) (voiceIdx % 8)) / 8.0f);
        return juce::Colour::fromHSV (hue, 0.55f, 1.0f, 1.0f);
    }

    void drawStarfield (juce::Graphics& g, juce::Rectangle<float> r)
    {
        for (const auto& s : stars)
        {
            const float twinkle = 0.4f + 0.6f * (0.5f + 0.5f * std::sin (s.twinklePhase));
            const float alpha   = juce::jlimit (0.0f, 1.0f, s.brightness * twinkle * 0.65f);
            const float radius  = 0.5f + s.brightness * 1.4f;
            const float x = r.getX() + s.x * r.getWidth();
            const float y = r.getY() + s.y * r.getHeight();
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        }
    }

    void drawSourcePanel (juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Panel BG with subtle inner shadow
        g.setColour (juce::Colour (0xff0a0f17));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (juce::Colour (0xff1a2230));
        g.drawRoundedRectangle (area, 6.0f, 1.0f);

        const float yc = area.getCentreY();
        const float halfH = area.getHeight() * 0.42f;

        // Centre baseline
        g.setColour (juce::Colour (0xff1a2230));
        g.drawHorizontalLine ((int) yc, area.getX() + 4.0f, area.getRight() - 4.0f);

        if (minPeaks.empty() || maxPeaks.empty())
            return;

        // Draw waveform as filled polygon
        const int nBins = (int) minPeaks.size();
        const float xLeft  = area.getX() + 4.0f;
        const float drawW  = area.getWidth() - 8.0f;
        const float perPx  = drawW / (float) nBins;

        juce::Path wavePath;
        wavePath.startNewSubPath (xLeft, yc);
        for (int i = 0; i < nBins; ++i)
        {
            const float x = xLeft + (float) i * perPx;
            const float vMax = juce::jlimit (-1.0f, 1.0f, maxPeaks[(size_t) i]);
            wavePath.lineTo (x, yc - vMax * halfH);
        }
        for (int i = nBins - 1; i >= 0; --i)
        {
            const float x = xLeft + (float) i * perPx;
            const float vMin = juce::jlimit (-1.0f, 1.0f, minPeaks[(size_t) i]);
            wavePath.lineTo (x, yc - vMin * halfH);
        }
        wavePath.closeSubPath();

        // Wave fill gradient
        juce::ColourGradient waveGrad (
            juce::Colour (0xff4d9eff).withAlpha (0.55f), area.getCentreX(), area.getY(),
            juce::Colour (0xff264c88).withAlpha (0.45f), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill (waveGrad);
        g.fillPath (wavePath);

        // Wave outline
        g.setColour (juce::Colour (0xffaad8ff).withAlpha (0.8f));
        g.strokePath (wavePath, juce::PathStrokeType (0.8f));

        // --- Spray window band (translucent) ------------------------------
        const float pos = juce::jlimit (0.0f, 1.0f, positionSmoothed);
        const float spr = juce::jlimit (0.0f, 1.0f, spraySmoothed);
        const float wPxLeft  = juce::jlimit (xLeft, xLeft + drawW,
                                              xLeft + (pos - spr * 0.5f) * drawW);
        const float wPxRight = juce::jlimit (xLeft, xLeft + drawW,
                                              xLeft + (pos + spr * 0.5f) * drawW);
        if (wPxRight > wPxLeft + 0.5f)
        {
            juce::Rectangle<float> band (wPxLeft, area.getY() + 3.0f,
                                          wPxRight - wPxLeft, area.getHeight() - 6.0f);
            g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.10f));
            g.fillRoundedRectangle (band, 3.0f);
            g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.30f));
            g.drawRoundedRectangle (band, 3.0f, 0.8f);
        }

        // --- Position cursor ----------------------------------------------
        const float cursorX = xLeft + pos * drawW;
        g.setColour (juce::Colour (0xffe6f0ff).withAlpha (0.85f));
        g.drawLine (cursorX, area.getY() + 4.0f, cursorX, area.getBottom() - 4.0f, 1.4f);
        // Cursor handle dot
        g.setColour (juce::Colour (0xff4d9eff));
        g.fillEllipse (cursorX - 3.0f, yc - 3.0f, 6.0f, 6.0f);

        // --- Grain reading-position tick marks ----------------------------
        const auto snap = eng.getGrainSnapshot();
        for (const auto& gs : snap)
        {
            if (! gs.active) continue;
            const float gx = xLeft + juce::jlimit (0.0f, 1.0f, gs.pos01) * drawW;
            const float life = 1.0f - juce::jlimit (0.0f, 1.0f, gs.age01);
            const auto col = voiceColour (gs.voiceIdx).withAlpha (life * 0.85f);

            // Vertical glowing tick
            g.setColour (col);
            g.drawLine (gx, area.getY() + 4.0f, gx, area.getBottom() - 4.0f, 0.6f);
            // Bright dot at the centre baseline
            const float dotR = 1.6f + 1.4f * life;
            g.setColour (col.brighter (0.4f));
            g.fillEllipse (gx - dotR, yc - dotR, dotR * 2.0f, dotR * 2.0f);
        }

        // Source name label (top-left of waveform)
        const auto& p = eng.getParameters();
        g.setColour (juce::Colour (0xffaab8c8));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (juce::String ("SOURCE - ") + GranularEngine::getSourceName (p.sourceIdx),
                    (int) area.getX() + 8, (int) area.getY() + 4, 220, 12,
                    juce::Justification::topLeft);
    }

    void drawGrainCloud (juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Panel BG
        g.setColour (juce::Colour (0xff0a0f17));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (juce::Colour (0xff1a2230));
        g.drawRoundedRectangle (area, 6.0f, 1.0f);

        const auto inner = area.reduced (6.0f);

        // Grid (subtle)
        g.setColour (juce::Colour (0xff111a25));
        for (int i = 1; i < 4; ++i)
        {
            const float gx = inner.getX() + inner.getWidth() * (float) i / 4.0f;
            g.drawLine (gx, inner.getY(), gx, inner.getBottom(), 0.6f);
        }
        for (int i = 1; i < 3; ++i)
        {
            const float gy = inner.getY() + inner.getHeight() * (float) i / 3.0f;
            g.drawLine (inner.getX(), gy, inner.getRight(), gy, 0.6f);
        }

        // Center pitch line (unison ratio = 1.0 -> y = 0.75 since range is 0.25..4)
        // Actually: pitch01 = log2(ratio/0.25)/4 -> ratio=1.0 -> log2(4)/4 = 0.5 -> y = 0.5
        {
            const float gy = inner.getY() + inner.getHeight() * 0.5f;
            g.setColour (juce::Colour (0x554d9eff));
            g.drawLine (inner.getX(), gy, inner.getRight(), gy, 0.8f);
        }

        // Render trail (faded particles)
        for (const auto& tp : trail)
        {
            const float x = inner.getX() + tp.x * inner.getWidth();
            const float y = inner.getY() + tp.y * inner.getHeight();
            const auto col = voiceColour (tp.voiceIdx).withAlpha (juce::jlimit (0.0f, 0.8f, tp.life * 0.6f));
            const float r = 1.5f + tp.life * 1.4f;
            g.setColour (col);
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }

        // Render currently active grains as soft glowing particles
        const auto snap = eng.getGrainSnapshot();
        for (const auto& gs : snap)
        {
            if (! gs.active) continue;
            const float pitch01 = juce::jlimit (0.0f, 1.0f,
                                    std::log2 (juce::jmax (0.25f, gs.pitchRatio) / 0.25f) / 4.0f);
            const float x = inner.getX() + juce::jlimit (0.0f, 1.0f, gs.pos01) * inner.getWidth();
            const float y = inner.getY() + (1.0f - pitch01) * inner.getHeight();
            const float life = 1.0f - juce::jlimit (0.0f, 1.0f, gs.age01);

            const auto col = voiceColour (gs.voiceIdx).withAlpha (juce::jlimit (0.0f, 1.0f, life));

            // Outer glow
            const float coreR = 2.5f + life * 4.5f;
            const float haloR = coreR * 4.0f;
            juce::ColourGradient halo (
                col.withAlpha (0.55f * life), x, y,
                col.withAlpha (0.0f),         x + haloR, y + haloR, true);
            g.setGradientFill (halo);
            g.fillEllipse (x - haloR, y - haloR, haloR * 2.0f, haloR * 2.0f);

            // Core
            g.setColour (juce::Colours::white.withAlpha (life * 0.85f));
            g.fillEllipse (x - coreR * 0.5f, y - coreR * 0.5f, coreR, coreR);
            g.setColour (col);
            g.drawEllipse (x - coreR, y - coreR, coreR * 2.0f, coreR * 2.0f, 1.0f);
        }

        // Axis labels (very subtle)
        g.setColour (juce::Colour (0xff5a6a80));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("position",
                    (int) inner.getX(), (int) (inner.getBottom() - 12),
                    (int) inner.getWidth(), 12,
                    juce::Justification::centredBottom);

        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (
            -juce::MathConstants<float>::halfPi,
            inner.getX() + 8.0f,
            inner.getCentreY()));
        g.drawText ("pitch",
                    (int) (inner.getX() - 30), (int) inner.getCentreY() - 6,
                    60, 12,
                    juce::Justification::centred);
        g.restoreState();
    }

    void drawLevelBars (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float barW = 5.0f;
        const float barGap = 3.0f;
        const float pad = 14.0f;
        const auto colour = juce::Colour (0xff4d9eff);

        juce::Rectangle<float> meterArea (r.getRight() - pad - (barW * 2 + barGap),
                                          r.getY() + pad,
                                          barW * 2 + barGap,
                                          r.getHeight() - pad * 2);

        auto drawBar = [&] (juce::Rectangle<float> b, float lvl)
        {
            g.setColour (juce::Colour (0xff10171f));
            g.fillRoundedRectangle (b, 2.0f);
            g.setColour (juce::Colour (0xff202a38));
            g.drawRoundedRectangle (b, 2.0f, 0.8f);

            const float fillH = b.getHeight() * juce::jlimit (0.0f, 1.0f, lvl);
            juce::Rectangle<float> fill (b.getX(), b.getBottom() - fillH, b.getWidth(), fillH);

            juce::ColourGradient grad (
                colour.brighter (0.2f), fill.getCentreX(), fill.getY(),
                colour.darker (0.4f),   fill.getCentreX(), fill.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, 2.0f);
        };

        drawBar (meterArea.removeFromLeft (barW), levelLSmoothed);
        meterArea.removeFromLeft (barGap);
        drawBar (meterArea.removeFromLeft (barW), levelRSmoothed);
    }

    void drawLfoOrb (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float pad = 14.0f;
        const float orbR = 14.0f;
        const float cx = r.getX() + pad + orbR;
        const float cy = r.getBottom() - pad - orbR;

        g.setColour (juce::Colour (0xff10171f));
        g.fillEllipse (cx - orbR, cy - orbR, orbR * 2, orbR * 2);
        g.setColour (juce::Colour (0xff202a38));
        g.drawEllipse (cx - orbR, cy - orbR, orbR * 2, orbR * 2, 1.0f);

        const float angle = eng.getLfoPhase() * juce::MathConstants<float>::twoPi
                            - juce::MathConstants<float>::halfPi;
        const float nx = cx + std::cos (angle) * (orbR - 4.0f);
        const float ny = cy + std::sin (angle) * (orbR - 4.0f);
        const auto colour = juce::Colour (0xff4d9eff);
        g.setColour (colour.withAlpha (0.85f));
        g.drawLine (cx, cy, nx, ny, 1.4f);

        g.setColour (colour.brighter (0.3f));
        g.fillEllipse (cx - 1.6f, cy - 1.6f, 3.2f, 3.2f);

        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText ("LFO", (int) (cx - 14), (int) (cy + orbR + 3), 28, 12, juce::Justification::centred);
    }

    void drawCornerLabels (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("GRAIN FIELD", bounds.reduced (12.0f, 8.0f), juce::Justification::topLeft);

        const int grains = eng.getActiveGrains();
        const int voices = eng.getActiveVoices();
        const auto info = juce::String (voices) + " voices  -  "
                        + juce::String (grains) + " grains";

        g.setColour (juce::Colour (0xffaab8c8));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText (info, bounds.reduced (12.0f, 8.0f), juce::Justification::topRight);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainVisualizer)
};
