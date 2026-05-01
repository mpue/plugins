/*
  ==============================================================================

    ReverbVisualizer.h
    A luxurious animated visualization of the reverb tail. Combines an
    expanding "wave pool" of reflections, a real-time decay envelope,
    and a stereo wet-level pulse.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LuxuryReverb.h"

class ReverbVisualizer : public juce::Component, private juce::Timer
{
public:
    ReverbVisualizer (LuxuryReverb& r) : reverb (r)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    ~ReverbVisualizer() override = default;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background: deep blue/black gradient with vignette
        juce::ColourGradient bg (
            juce::Colour (0xff0a1118), bounds.getCentreX(), bounds.getCentreY(),
            juce::Colour (0xff020306), bounds.getRight(),  bounds.getBottom(), true);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 10.0f);

        // Subtle inner glow
        g.setColour (juce::Colour (0x224d9eff));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 10.0f, 1.0f);

        // Split into top (wave pool) and bottom (decay envelope)
        auto top    = bounds.withTrimmedBottom (bounds.getHeight() * 0.42f).reduced (8.0f);
        auto bottom = bounds.withTrimmedTop    (bounds.getHeight() * 0.58f).reduced (8.0f, 6.0f);

        drawHallSilhouette (g, top);
        drawWavePool (g, top);
        drawDecayEnvelope (g, bottom);
        drawCornerLabels (g, bounds);
    }

    void resized() override {}

private:
    static constexpr int kEnvelopeSamples = 256;
    static constexpr int kMaxRings = 32;

    struct Ring
    {
        float age      = 0.0f; // seconds
        float lifetime = 1.0f;
        float intensity = 0.5f;
        float angle     = 0.0f;
    };

    LuxuryReverb& reverb;
    std::array<Ring, kMaxRings> rings {};
    int   nextRingSlot = 0;
    float emitAccumulator = 0.0f;
    float pulse = 0.0f;
    float envHistoryL[kEnvelopeSamples] {};
    float envHistoryR[kEnvelopeSamples] {};
    int   envWritePos = 0;
    int   ticks = 0;

    void timerCallback() override
    {
        ++ticks;
        const float dt = 1.0f / 30.0f;
        const float input = reverb.getInputEnergy();
        const float wetL  = reverb.getWetEnergyL();
        const float wetR  = reverb.getWetEnergyR();
        const float t60   = juce::jlimit (0.05f, 25.0f, reverb.getApproxT60());

        // Pulse: smoothed input level
        pulse = 0.85f * pulse + 0.15f * juce::jlimit (0.0f, 1.0f, input * 14.0f);

        // Emit rings based on input + diffusion
        emitAccumulator += dt * (4.0f + 12.0f * pulse);
        while (emitAccumulator >= 1.0f && pulse > 0.04f)
        {
            emitAccumulator -= 1.0f;
            auto& r = rings[(size_t) nextRingSlot];
            nextRingSlot = (nextRingSlot + 1) % kMaxRings;
            r.age = 0.0f;
            r.lifetime = juce::jlimit (0.6f, 6.0f, t60 * 0.5f);
            r.intensity = juce::jlimit (0.2f, 1.0f, pulse + 0.1f);
            r.angle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
        }

        // Age existing rings
        for (auto& r : rings)
            if (r.age <= r.lifetime + 0.5f)
                r.age += dt;

        // Append to envelope history
        envHistoryL[envWritePos] = wetL;
        envHistoryR[envWritePos] = wetR;
        envWritePos = (envWritePos + 1) % kEnvelopeSamples;

        repaint();
    }

    static juce::Path makeHallPath (juce::Rectangle<float> r)
    {
        // Stylised concert-hall silhouette: rounded top, perspective floor lines suggested
        juce::Path p;
        const float w = r.getWidth();
        const float h = r.getHeight();
        const float cx = r.getCentreX();
        const float baseY = r.getBottom();
        const float topY  = r.getY() + h * 0.18f;

        p.startNewSubPath (cx - w * 0.45f, baseY);
        p.lineTo (cx - w * 0.45f, baseY - h * 0.25f);
        p.quadraticTo (cx - w * 0.40f, topY + h * 0.15f, cx - w * 0.20f, topY + h * 0.05f);
        p.quadraticTo (cx, topY - h * 0.05f, cx + w * 0.20f, topY + h * 0.05f);
        p.quadraticTo (cx + w * 0.40f, topY + h * 0.15f, cx + w * 0.45f, baseY - h * 0.25f);
        p.lineTo (cx + w * 0.45f, baseY);
        p.closeSubPath();
        return p;
    }

    void drawHallSilhouette (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto path = makeHallPath (r);

        // Soft glowing fill
        juce::ColourGradient grad (
            juce::Colour (0x554d9eff), r.getCentreX(), r.getY() + r.getHeight() * 0.4f,
            juce::Colour (0x001a2438), r.getCentreX(), r.getBottom(), true);
        g.setGradientFill (grad);
        g.fillPath (path);

        // Faint outline
        g.setColour (juce::Colour (0x664d9eff));
        g.strokePath (path, juce::PathStrokeType (1.2f));

        // Perspective floor lines
        const float baseY = r.getBottom();
        const float cx    = r.getCentreX();
        const float horizonY = r.getY() + r.getHeight() * 0.55f;
        for (int i = 1; i < 8; ++i)
        {
            float t = i / 8.0f;
            float y = juce::jmap (t, 0.0f, 1.0f, horizonY, baseY);
            float fade = std::pow (t, 0.7f) * 0.35f;
            float halfWidth = juce::jmap (t, 0.0f, 1.0f, 6.0f, r.getWidth() * 0.45f);
            g.setColour (juce::Colour (0xff4d9eff).withAlpha (fade));
            g.drawLine (cx - halfWidth, y, cx + halfWidth, y, 1.0f);
        }

        // Source dot at center, pulsing with input
        const float sourceY = horizonY + (baseY - horizonY) * 0.15f;
        const float dotRadius = 5.0f + 8.0f * pulse;
        juce::Rectangle<float> dot (cx - dotRadius, sourceY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
        juce::ColourGradient halo (
            juce::Colour (0xff8fc0ff).withAlpha (0.85f), cx, sourceY,
            juce::Colour (0x00ffffff),                  cx + dotRadius * 3, sourceY, true);
        g.setGradientFill (halo);
        g.fillEllipse (dot.expanded (dotRadius * 2.0f));
        g.setColour (juce::Colour (0xffe6f0ff));
        g.fillEllipse (dot);
    }

    void drawWavePool (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float cx = r.getCentreX();
        const float horizonY = r.getY() + r.getHeight() * 0.55f;
        const float baseY    = r.getBottom();
        const float sourceY  = horizonY + (baseY - horizonY) * 0.15f;

        const float maxRadiusX = r.getWidth() * 0.48f;
        const float maxRadiusY = (baseY - sourceY) * 0.92f;

        for (auto& ring : rings)
        {
            if (ring.age <= 0.0f || ring.age > ring.lifetime) continue;
            const float t = ring.age / ring.lifetime;
            if (t > 1.0f) continue;

            const float rx = maxRadiusX * t;
            const float ry = maxRadiusY * t;
            const float alpha = (1.0f - t) * ring.intensity * 0.85f;
            if (alpha < 0.01f) continue;

            // Color shifts from bright cyan to deep blue as it fades
            juce::Colour c = juce::Colour (0xff8fc0ff).interpolatedWith (juce::Colour (0xff2452a8), t);
            g.setColour (c.withAlpha (alpha));
            const float thickness = juce::jmax (1.0f, 2.5f * (1.0f - t));
            g.drawEllipse (cx - rx, sourceY - ry, rx * 2.0f, ry * 2.0f, thickness);
        }
    }

    void drawDecayEnvelope (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Frame
        g.setColour (juce::Colour (0xff10171f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (0xff2a3340));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);

        // Time grid
        g.setColour (juce::Colour (0xff1f2a36));
        for (int i = 1; i < 8; ++i)
        {
            float x = r.getX() + r.getWidth() * (i / 8.0f);
            g.drawLine (x, r.getY() + 4, x, r.getBottom() - 4, 1.0f);
        }
        for (int i = 1; i < 4; ++i)
        {
            float y = r.getY() + r.getHeight() * (i / 4.0f);
            g.drawLine (r.getX() + 4, y, r.getRight() - 4, y, 1.0f);
        }

        const float t60 = juce::jlimit (0.1f, 25.0f, reverb.getApproxT60());
        const float window = juce::jmin (10.0f, t60 * 1.2f); // seconds visible

        // Theoretical decay envelope: e^(-t * 6.91 / T60)
        juce::Path top, fill;
        const int N = (int) r.getWidth();
        const float topY = r.getY() + 6.0f;
        const float botY = r.getBottom() - 4.0f;
        const float h    = botY - topY;

        const float pulseLevel = juce::jlimit (0.0f, 1.0f, pulse + 0.15f);

        for (int i = 0; i <= N; ++i)
        {
            float t = (float) i / (float) juce::jmax (1, N);
            float secs = t * window;
            float env = std::exp (-secs * 6.9078f / t60) * pulseLevel;
            float y = botY - env * h;

            float x = r.getX() + i;
            if (i == 0) { top.startNewSubPath (x, y); fill.startNewSubPath (x, botY); fill.lineTo (x, y); }
            else        { top.lineTo (x, y); fill.lineTo (x, y); }
        }
        fill.lineTo (r.getRight(), botY);
        fill.closeSubPath();

        // Glow fill
        juce::ColourGradient grad (
            juce::Colour (0x884d9eff), r.getX(), topY,
            juce::Colour (0x114d9eff), r.getX(), botY, false);
        g.setGradientFill (grad);
        g.fillPath (fill);

        // Top stroke
        g.setColour (juce::Colour (0xff8fc0ff));
        g.strokePath (top, juce::PathStrokeType (1.6f));

        // Real-time wet history overlay (left = old, right = now)
        const int historyCount = kEnvelopeSamples;
        juce::Path liveL, liveR;
        for (int i = 0; i < historyCount; ++i)
        {
            int idx = (envWritePos + i) % historyCount;
            float t = (float) i / (float) (historyCount - 1);
            float xpos = r.getX() + r.getWidth() * t;
            float vL = juce::jlimit (0.0f, 1.0f, envHistoryL[(size_t) idx] * 12.0f);
            float vR = juce::jlimit (0.0f, 1.0f, envHistoryR[(size_t) idx] * 12.0f);
            float yL = botY - vL * h;
            float yR = botY - vR * h;
            if (i == 0) { liveL.startNewSubPath (xpos, yL); liveR.startNewSubPath (xpos, yR); }
            else        { liveL.lineTo (xpos, yL);          liveR.lineTo (xpos, yR); }
        }
        g.setColour (juce::Colour (0xddff8866).withAlpha (0.6f));
        g.strokePath (liveL, juce::PathStrokeType (1.0f));
        g.setColour (juce::Colour (0xddff66bb).withAlpha (0.6f));
        g.strokePath (liveR, juce::PathStrokeType (1.0f));

        // T60 readout
        g.setColour (juce::Colour (0xffaab8c8));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (juce::String ("T60 ") + juce::String (t60, 2) + " s",
                    r.reduced (8.0f, 4.0f), juce::Justification::topLeft);
        g.drawText (juce::String (window, 1) + " s window",
                    r.reduced (8.0f, 4.0f), juce::Justification::topRight);
    }

    void drawCornerLabels (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("REVERB FIELD", bounds.reduced (12.0f, 8.0f), juce::Justification::topLeft);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbVisualizer)
};
