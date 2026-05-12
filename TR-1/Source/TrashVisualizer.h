/*
  ==============================================================================

    TrashVisualizer.h
    A cinematic visualization of the TR-1 destruction. Three layers blend into
    a single glowing field:
      • A live transfer-curve scope (input vs. output through the saturator)
      • An animated debris field whose density reflects the damage meter
      • Dual horizontal level ribbons — clean (left) → trashed (right)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TrashEngine.h"

class TrashVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit TrashVisualizer (TrashEngine& e) : engine (e)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);

        for (auto& p : particles) respawnParticle (p, true);

        startTimerHz (45);
    }

    ~TrashVisualizer() override = default;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background — radial gradient with luxurious feel
        juce::ColourGradient bg (
            juce::Colour (0xff14182a), bounds.getCentreX(), bounds.getCentreY(),
            juce::Colour (0xff04050a), bounds.getRight(),  bounds.getBottom(), true);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 12.0f);

        // Subtle outer rim
        g.setColour (juce::Colour (0x224d9eff));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 12.0f, 1.2f);

        const auto inset = bounds.reduced (12.0f, 12.0f);

        const float damage = juce::jlimit (0.0f, 1.0f, engine.getDamageMeter() * 6.0f);

        drawGrid          (g, inset, damage);
        drawDebrisField   (g, inset, damage);
        drawTransferCurve (g, inset, damage);
        drawLevelRibbons  (g, inset);
        drawCornerLabels  (g, bounds, damage);
    }

    void resized() override {}

private:
    static constexpr int kNumParticles = 96;

    struct Particle
    {
        float x = 0.0f, y = 0.0f;        // 0..1 in the visualization area
        float vx = 0.0f, vy = 0.0f;
        float life = 0.0f, maxLife = 1.0f;
        float size = 1.0f;
        float hue  = 0.0f;
    };

    TrashEngine& engine;
    std::array<Particle, kNumParticles> particles {};
    juce::Random rng { 0xfeedbabe };
    int ticks = 0;
    float pulse = 0.0f;

    void respawnParticle (Particle& p, bool randomY = false)
    {
        p.x  = -0.05f - rng.nextFloat() * 0.1f;
        p.y  = randomY ? rng.nextFloat() : 0.45f + (rng.nextFloat() - 0.5f) * 0.22f;
        p.vx = 0.30f + rng.nextFloat() * 0.55f;
        p.vy = (rng.nextFloat() - 0.5f) * 0.05f;
        p.life = 0.0f;
        p.maxLife = 1.6f + rng.nextFloat() * 1.5f;
        p.size = 0.6f + rng.nextFloat() * 1.7f;
        p.hue  = rng.nextFloat();
    }

    void timerCallback() override
    {
        ++ticks;
        const float dt = 1.0f / 45.0f;

        const float dmg   = juce::jlimit (0.0f, 1.0f, engine.getDamageMeter() * 8.0f);
        const float input = juce::jlimit (0.0f, 1.0f,
            (engine.getInputLevelL() + engine.getInputLevelR()) * 5.0f);

        pulse = 0.85f * pulse + 0.15f * juce::jmax (input, dmg * 0.7f);

        for (auto& p : particles)
        {
            p.life += dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;

            // Drag, plus turbulence proportional to damage
            p.vx *= 0.985f;
            p.vy += (rng.nextFloat() - 0.5f) * 0.6f * dmg * dt;

            if (p.life > p.maxLife || p.x > 1.10f)
                respawnParticle (p, false);
        }

        repaint();
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> r, float damage)
    {
        // Faint horizon line at vertical centre
        g.setColour (juce::Colour (0xff1f2a3a).withAlpha (0.55f));
        const float midY = r.getCentreY();
        g.drawLine (r.getX(), midY, r.getRight(), midY, 1.0f);

        // Vertical grid: every 1/8th
        for (int i = 1; i < 8; ++i)
        {
            const float x = r.getX() + r.getWidth() * (i / 8.0f);
            g.setColour (juce::Colour (0xff182030).withAlpha (0.6f));
            g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
        }

        // Damage-driven horizon glow
        if (damage > 0.05f)
        {
            juce::ColourGradient horizonGlow (
                juce::Colour::fromHSV (0.62f - damage * 0.45f, 0.85f, 1.0f, 0.30f * damage),
                r.getCentreX(), midY,
                juce::Colour (0x00000000),
                r.getCentreX(), midY + r.getHeight() * 0.45f, false);
            g.setGradientFill (horizonGlow);
            g.fillRect (juce::Rectangle<float> (r.getX(), midY - 8.0f, r.getWidth(), 16.0f));
        }
    }

    void drawDebrisField (juce::Graphics& g, juce::Rectangle<float> r, float damage)
    {
        for (auto& p : particles)
        {
            if (p.life > p.maxLife) continue;
            const float t = p.life / p.maxLife;
            const float fade = juce::jlimit (0.0f, 1.0f, std::sin (t * juce::MathConstants<float>::pi));
            const float baseAlpha = 0.05f + 0.55f * damage;
            const float alpha = baseAlpha * fade;
            if (alpha < 0.005f) continue;

            const float px = r.getX() + p.x * r.getWidth();
            const float py = r.getY() + p.y * r.getHeight();
            const float sz = p.size * (1.5f + 3.5f * damage);

            // Hue ranges from cyan (0.55) to amber/red (0.05) as damage grows
            const float hue = juce::jmap (damage, 0.0f, 1.0f, 0.58f, 0.05f) + (p.hue - 0.5f) * 0.05f;
            const auto colour = juce::Colour::fromHSV (hue, 0.85f, 1.0f, alpha);

            // Soft halo
            juce::ColourGradient halo (
                colour.withAlpha (alpha * 0.6f), px, py,
                colour.withAlpha (0.0f),         px + sz * 2.0f, py, true);
            g.setGradientFill (halo);
            g.fillEllipse (px - sz * 2.0f, py - sz * 2.0f, sz * 4.0f, sz * 4.0f);

            g.setColour (colour);
            g.fillEllipse (px - sz * 0.5f, py - sz * 0.5f, sz, sz);
        }
    }

    void drawTransferCurve (juce::Graphics& g, juce::Rectangle<float> r, float damage)
    {
        // The transfer curve sits in a square cell on the left, gently highlighted.
        const float side = juce::jmin (r.getHeight() - 60.0f, r.getWidth() * 0.42f);
        juce::Rectangle<float> curveBox (r.getX(), r.getCentreY() - side * 0.5f, side, side);
        curveBox = curveBox.translated (8.0f, 0.0f);

        // Backing card
        g.setColour (juce::Colour (0xff0c1220).withAlpha (0.78f));
        g.fillRoundedRectangle (curveBox, 8.0f);
        g.setColour (juce::Colour (0xff223150));
        g.drawRoundedRectangle (curveBox, 8.0f, 1.0f);

        // Identity reference (dotted)
        g.setColour (juce::Colour (0xff334a6a).withAlpha (0.45f));
        const int steps = 20;
        for (int i = 0; i < steps; ++i)
        {
            const float t1 = (float) i / steps;
            const float t2 = (float) (i + 0.5f) / steps;
            g.drawLine (
                curveBox.getX() + t1 * curveBox.getWidth(),
                curveBox.getBottom() - t1 * curveBox.getHeight(),
                curveBox.getX() + t2 * curveBox.getWidth(),
                curveBox.getBottom() - t2 * curveBox.getHeight(),
                1.0f);
        }

        // Zero crossings
        g.setColour (juce::Colour (0xff223150));
        g.drawLine (curveBox.getX(), curveBox.getCentreY(), curveBox.getRight(), curveBox.getCentreY(), 1.0f);
        g.drawLine (curveBox.getCentreX(), curveBox.getY(), curveBox.getCentreX(), curveBox.getBottom(), 1.0f);

        // The actual transfer curve (sampled directly from the engine)
        juce::Path curve;
        const int N = 256;
        for (int i = 0; i <= N; ++i)
        {
            const float xn = (float) i / N * 2.0f - 1.0f;       // -1..+1
            const float yn = juce::jlimit (-1.4f, 1.4f, engine.sampleTransfer (xn));

            const float px = curveBox.getX() + (xn * 0.5f + 0.5f) * curveBox.getWidth();
            const float py = curveBox.getCentreY() - yn * curveBox.getHeight() * 0.42f;
            if (i == 0) curve.startNewSubPath (px, py);
            else        curve.lineTo (px, py);
        }

        // Glow under the curve
        const float glowAlpha = 0.35f + 0.5f * damage;
        const auto glowHi = juce::Colour::fromHSV (juce::jmap (damage, 0.0f, 1.0f, 0.58f, 0.07f),
                                                   0.9f, 1.0f, glowAlpha);
        const auto glowLo = juce::Colour::fromHSV (juce::jmap (damage, 0.0f, 1.0f, 0.62f, 0.04f),
                                                   0.9f, 1.0f, 0.05f);
        juce::ColourGradient curveGrad (glowHi, curveBox.getCentreX(), curveBox.getY(),
                                        glowLo, curveBox.getCentreX(), curveBox.getBottom(), false);
        g.setGradientFill (curveGrad);
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));

        // Bright crest on top
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.strokePath (curve, juce::PathStrokeType (0.7f, juce::PathStrokeType::curved));

        // Title strip
        g.setColour (juce::Colour (0xff8fa8c4).withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText ("TRANSFER CURVE", curveBox.reduced (8.0f, 6.0f), juce::Justification::topLeft);

        // Live signal dot riding the curve (animated phase from LFO)
        const float lfoPh = engine.getLfoPhase();
        const float xn = std::sin (lfoPh * juce::MathConstants<float>::twoPi) * pulse;
        const float yn = juce::jlimit (-1.0f, 1.0f, engine.sampleTransfer (xn));
        const float dx = curveBox.getX() + (xn * 0.5f + 0.5f) * curveBox.getWidth();
        const float dy = curveBox.getCentreY() - yn * curveBox.getHeight() * 0.42f;
        g.setColour (juce::Colour (0xffe6f0ff).withAlpha (0.85f));
        g.fillEllipse (dx - 3.0f, dy - 3.0f, 6.0f, 6.0f);
        juce::ColourGradient dotHalo (
            juce::Colour (0xffffffff).withAlpha (0.45f), dx, dy,
            juce::Colour (0x00ffffff),                  dx + 12.0f, dy, true);
        g.setGradientFill (dotHalo);
        g.fillEllipse (dx - 12.0f, dy - 12.0f, 24.0f, 24.0f);
    }

    void drawLevelRibbons (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // The ribbons live on the right side of the visualizer area
        const float side = juce::jmin (r.getHeight() - 60.0f, r.getWidth() * 0.42f);
        juce::Rectangle<float> ribbon (r.getRight() - r.getWidth() * 0.50f + 6.0f,
                                       r.getY() + 8.0f,
                                       r.getWidth() * 0.50f - 16.0f,
                                       side);

        g.setColour (juce::Colour (0xff0c1220).withAlpha (0.78f));
        g.fillRoundedRectangle (ribbon, 8.0f);
        g.setColour (juce::Colour (0xff223150));
        g.drawRoundedRectangle (ribbon, 8.0f, 1.0f);

        const float halfH = ribbon.getHeight() * 0.5f;
        const float midY  = ribbon.getCentreY();

        // CLEAN ribbon (top half) — input
        const float inLevel = juce::jlimit (0.0f, 1.0f,
            0.5f * (engine.getInputLevelL() + engine.getInputLevelR()) * 4.0f);

        juce::Path cleanShape;
        cleanShape.startNewSubPath (ribbon.getX(), midY);
        const int N = 64;
        for (int i = 0; i <= N; ++i)
        {
            const float t = (float) i / N;
            const float x = ribbon.getX() + t * ribbon.getWidth();
            const float wave = std::sin (t * juce::MathConstants<float>::twoPi * 6.0f
                                       + ticks * 0.12f);
            const float y = midY - halfH * 0.85f * wave * inLevel;
            cleanShape.lineTo (x, y);
        }
        cleanShape.lineTo (ribbon.getRight(), midY);
        g.setColour (juce::Colour (0xff8fc0ff).withAlpha (0.85f));
        g.strokePath (cleanShape, juce::PathStrokeType (1.8f));

        // TRASHED ribbon (bottom half) — output
        const float outLevel = juce::jlimit (0.0f, 1.0f,
            0.5f * (engine.getOutputLevelL() + engine.getOutputLevelR()) * 4.0f);
        const float damage = juce::jlimit (0.0f, 1.0f, engine.getDamageMeter() * 8.0f);

        juce::Path trashShape;
        trashShape.startNewSubPath (ribbon.getX(), midY);
        for (int i = 0; i <= N; ++i)
        {
            const float t = (float) i / N;
            const float x = ribbon.getX() + t * ribbon.getWidth();
            // Compose: clean carrier + harmonic distortion + chaotic detail tied to damage
            const float carrier = std::sin (t * juce::MathConstants<float>::twoPi * 6.0f
                                          + ticks * 0.12f);
            const float harm    = std::sin (t * juce::MathConstants<float>::twoPi * 18.0f
                                          + ticks * 0.18f);
            const float chaos   = (rng.nextFloat() - 0.5f);

            const float shaped = carrier * (1.0f - 0.4f * damage)
                               + harm * damage * 0.55f
                               + chaos * damage * 0.30f;

            const float y = midY + halfH * 0.85f * shaped * outLevel;
            trashShape.lineTo (x, y);
        }
        trashShape.lineTo (ribbon.getRight(), midY);

        // Hot gradient stroke
        const auto hotA = juce::Colour::fromHSV (juce::jmap (damage, 0.0f, 1.0f, 0.55f, 0.04f),
                                                 0.95f, 1.0f, 0.95f);
        const auto hotB = juce::Colour::fromHSV (juce::jmap (damage, 0.0f, 1.0f, 0.45f, 0.10f),
                                                 0.95f, 1.0f, 0.55f);
        juce::ColourGradient grad (hotA, ribbon.getX(), midY,
                                   hotB, ribbon.getRight(), midY, false);
        g.setGradientFill (grad);
        g.strokePath (trashShape, juce::PathStrokeType (2.0f));

        // Labels
        g.setColour (juce::Colour (0xff8fa8c4).withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText ("CLEAN",   ribbon.reduced (8.0f, 6.0f), juce::Justification::topLeft);
        g.drawText ("TRASHED", ribbon.reduced (8.0f, 6.0f), juce::Justification::bottomLeft);

        // Damage meter strip across the very bottom
        juce::Rectangle<float> meter (ribbon.getX(), ribbon.getBottom() + 6.0f,
                                      ribbon.getWidth(), 6.0f);
        g.setColour (juce::Colour (0xff1a2230));
        g.fillRoundedRectangle (meter, 3.0f);

        const auto fillCol = juce::Colour::fromHSV (juce::jmap (damage, 0.0f, 1.0f, 0.55f, 0.03f),
                                                    0.95f, 1.0f, 0.95f);
        g.setColour (fillCol);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * damage), 3.0f);
    }

    void drawCornerLabels (juce::Graphics& g, juce::Rectangle<float> bounds, float damage)
    {
        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("DESTRUCTION FIELD", bounds.reduced (16.0f, 10.0f), juce::Justification::topLeft);

        const auto rightLbl = bounds.reduced (16.0f, 10.0f);
        g.drawText (juce::String ("DAMAGE  ") + juce::String ((int) std::round (damage * 100.0f)) + "%",
                    rightLbl, juce::Justification::topRight);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrashVisualizer)
};
