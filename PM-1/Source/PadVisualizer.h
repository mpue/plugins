/*
  ==============================================================================

    PadVisualizer.h
    A luxurious cinematic visualization of the pad synthesizer. Combines:
      - A drifting aurora of layered, evolving sine waveforms whose
        density and color hue follow the synth character & brightness
      - Voice "stars" that pulse on note-on and drift along the pitch axis
      - A real-time stereo VU meter and an LFO indicator
      - A subtle starfield background for depth

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LuxuryPadSynth.h"

class PadVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit PadVisualizer (LuxuryPadSynth& s) : synth (s)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        startTimerHz (45);

        // Pre-populate stars
        for (auto& s : stars)
        {
            s.x = juce::Random::getSystemRandom().nextFloat();
            s.y = juce::Random::getSystemRandom().nextFloat();
            s.brightness = juce::Random::getSystemRandom().nextFloat() * 0.7f + 0.1f;
            s.twinkleSpeed = juce::Random::getSystemRandom().nextFloat() * 1.5f + 0.4f;
            s.twinklePhase = juce::Random::getSystemRandom().nextFloat() * 6.28f;
        }

        for (auto& w : aurora)
        {
            w.phase = juce::Random::getSystemRandom().nextFloat() * 6.28f;
            w.speed = juce::Random::getSystemRandom().nextFloat() * 0.10f + 0.04f;
            w.amp   = juce::Random::getSystemRandom().nextFloat() * 0.25f + 0.10f;
            w.freq  = juce::Random::getSystemRandom().nextFloat() * 1.6f + 0.8f;
            w.yOffset = juce::Random::getSystemRandom().nextFloat() * 0.8f - 0.4f;
        }
    }

    ~PadVisualizer() override = default;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background - deep cosmic gradient
        juce::ColourGradient bg (
            juce::Colour (0xff0d1430), bounds.getCentreX(), bounds.getY(),
            juce::Colour (0xff05080f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 10.0f);

        // Vignette overlay
        juce::ColourGradient vignette (
            juce::Colour (0x00000000), bounds.getCentreX(), bounds.getCentreY(),
            juce::Colour (0xaa000000), bounds.getRight(),   bounds.getBottom(), true);
        g.setGradientFill (vignette);
        g.fillRoundedRectangle (bounds, 10.0f);

        // Inner stroke
        g.setColour (juce::Colour (0x224d9eff));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 10.0f, 1.0f);

        drawStarfield (g, bounds);
        drawAurora    (g, bounds);
        drawVoiceStars(g, bounds);
        drawLevelBars (g, bounds);
        drawLfoOrb    (g, bounds);
        drawCornerLabels (g, bounds);
    }

    void resized() override {}

private:
    static constexpr int kNumAurora = 8;
    static constexpr int kNumStars  = 90;

    struct Star
    {
        float x = 0.5f, y = 0.5f;
        float brightness = 0.5f;
        float twinkleSpeed = 1.0f;
        float twinklePhase = 0.0f;
    };

    struct AuroraWave
    {
        float phase = 0.0f;
        float speed = 0.05f;
        float amp   = 0.2f;
        float freq  = 1.0f;
        float yOffset = 0.0f;
    };

    LuxuryPadSynth& synth;
    std::array<Star, kNumStars>      stars;
    std::array<AuroraWave, kNumAurora> aurora;
    float    pulse = 0.0f;
    float    lfoPhase = 0.0f;
    float    activitySmoothed = 0.0f;
    float    levelLSmoothed = 0.0f;
    float    levelRSmoothed = 0.0f;
    int      ticks = 0;

    // Voice star records (one per voice slot, updated from snapshot)
    struct VoiceStar
    {
        float displayY    = 0.5f;     // smoothed normalized pitch
        float targetY     = 0.5f;
        float displayX    = 0.5f;
        float floatPhase  = 0.0f;     // for tiny breathing motion
        float gain        = 0.0f;
        bool  on          = false;
    };
    std::array<VoiceStar, 16> voiceStars {};

    void timerCallback() override
    {
        ++ticks;
        const float dt = 1.0f / 45.0f;

        const float activity = synth.getActivity();
        const float lvlL     = synth.getLevelL();
        const float lvlR     = synth.getLevelR();

        activitySmoothed = 0.85f * activitySmoothed + 0.15f * juce::jlimit (0.0f, 1.0f, activity * 9.0f);
        levelLSmoothed   = 0.85f * levelLSmoothed   + 0.15f * juce::jlimit (0.0f, 1.0f, lvlL * 1.2f);
        levelRSmoothed   = 0.85f * levelRSmoothed   + 0.15f * juce::jlimit (0.0f, 1.0f, lvlR * 1.2f);
        pulse = 0.80f * pulse + 0.20f * activitySmoothed;

        const auto& params = synth.getParameters();
        lfoPhase += dt * params.lfoRateHz;
        if (lfoPhase > 1.0f) lfoPhase -= 1.0f;

        // Update voice stars
        const auto snap = synth.getVoiceSnapshot();
        for (size_t i = 0; i < voiceStars.size(); ++i)
        {
            voiceStars[i].targetY = 1.0f - snap[i].note01; // invert: higher note = top
            voiceStars[i].displayY += (voiceStars[i].targetY - voiceStars[i].displayY) * 0.18f;
            voiceStars[i].displayX  = 0.05f + 0.90f * ((float) i / (float) (voiceStars.size() - 1));
            voiceStars[i].gain      = 0.85f * voiceStars[i].gain + 0.15f * snap[i].gain;
            voiceStars[i].on        = snap[i].isOn;
            voiceStars[i].floatPhase += dt * (0.6f + (float) i * 0.07f);
        }

        // Aurora drift
        for (auto& w : aurora)
        {
            w.phase += dt * w.speed * juce::MathConstants<float>::twoPi
                       * (0.5f + 1.5f * params.movement);
            if (w.phase > 100.0f) w.phase -= 100.0f;
        }

        // Twinkle stars
        for (auto& s : stars)
        {
            s.twinklePhase += dt * s.twinkleSpeed;
        }

        repaint();
    }

    static juce::Colour mixColour (const juce::Colour& a, const juce::Colour& b, float t) noexcept
    {
        return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, t));
    }

    juce::Colour characterColour (float intensity) const noexcept
    {
        const auto& p = synth.getParameters();
        const auto cool   = juce::Colour (0xff5fb4ff);
        const auto warm   = juce::Colour (0xffff9966);
        const auto violet = juce::Colour (0xffb578ff);
        const auto amber  = juce::Colour (0xffffd27f);
        const auto green  = juce::Colour (0xff66e5b8);
        const auto teal   = juce::Colour (0xff48d3d3);

        juce::Colour base;
        switch (p.characterIdx)
        {
            case 0: base = cool;   break; // Warm Pad - blue
            case 1: base = amber;  break; // Bright Pad
            case 2: base = warm;   break; // Strings
            case 3: base = green;  break; // Choir
            case 4: base = teal;   break; // Glass
            case 5: base = violet; break; // Air
            default: base = cool;
        }

        // Brightness shifts hue toward white slightly
        return base.brighter (intensity * 0.4f);
    }

    void drawStarfield (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Random rng;
        for (size_t i = 0; i < stars.size(); ++i)
        {
            const auto& s = stars[i];
            const float twinkle = 0.4f + 0.6f * (0.5f + 0.5f * std::sin (s.twinklePhase));
            const float alpha   = juce::jlimit (0.0f, 1.0f, s.brightness * twinkle * 0.7f);
            const float radius  = 0.5f + s.brightness * 1.5f;
            const float x = r.getX() + s.x * r.getWidth();
            const float y = r.getY() + s.y * r.getHeight();
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        }
    }

    void drawAurora (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto& p = synth.getParameters();

        const float baseAmp = juce::jmap (activitySmoothed, 0.0f, 1.0f, 0.05f, 0.40f);
        const float yCentre = r.getCentreY();
        const float maxAmp  = r.getHeight() * 0.32f;

        // Each aurora wave is rendered as a soft glow region (gradient stroke)
        for (size_t k = 0; k < aurora.size(); ++k)
        {
            const auto& w = aurora[k];

            const float globalAmp = baseAmp * (1.0f + 0.4f * w.amp);
            const float layerY    = yCentre + w.yOffset * r.getHeight() * 0.20f;

            const float hueT = (float) k / (float) (aurora.size() - 1);
            const auto col   = characterColour (p.brightness)
                                .withMultipliedBrightness (0.7f + 0.6f * hueT)
                                .withAlpha (juce::jlimit (0.0f, 1.0f,
                                    0.10f + 0.45f * activitySmoothed * (1.0f - hueT * 0.4f)));

            // Build path
            juce::Path path;
            const int N = 96;
            for (int i = 0; i <= N; ++i)
            {
                const float t = (float) i / (float) N;
                const float x = r.getX() + t * r.getWidth();

                // Two layered sines for organic feel
                const float s1 = std::sin (t * w.freq * juce::MathConstants<float>::twoPi
                                           + w.phase);
                const float s2 = std::sin (t * w.freq * 1.7f * juce::MathConstants<float>::twoPi
                                           - w.phase * 1.3f);
                float yMod = (s1 * 0.7f + s2 * 0.3f) * maxAmp * globalAmp;

                // Add note-driven modulation: each active voice perturbs nearby points
                const auto snap = synth.getVoiceSnapshot();
                for (const auto& vs : snap)
                {
                    if (! vs.isOn || vs.gain < 0.01f) continue;
                    const float vt = vs.note01; // 0..1 horizontal position
                    const float dist = std::abs (t - vt);
                    if (dist < 0.20f)
                    {
                        const float bell = std::exp (- (dist * 8.0f) * (dist * 8.0f));
                        yMod -= bell * vs.gain * 12.0f;
                    }
                }

                const float y = layerY + yMod;

                if (i == 0) path.startNewSubPath (x, y);
                else        path.lineTo (x, y);
            }

            // Stroke with halo (draw three times with diminishing thickness)
            g.setColour (col.withMultipliedAlpha (0.30f));
            g.strokePath (path, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.withMultipliedAlpha (0.55f));
            g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.withMultipliedAlpha (1.0f).brighter (0.4f));
            g.strokePath (path, juce::PathStrokeType (0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        juce::ignoreUnused (yCentre);
    }

    void drawVoiceStars (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto& p = synth.getParameters();
        const float pad = 18.0f;
        auto inner = r.reduced (pad);

        for (auto& vs : voiceStars)
        {
            if (vs.gain < 0.005f && ! vs.on) continue;

            const float floatY = std::sin (vs.floatPhase) * 1.5f;
            const float x = inner.getX() + vs.displayX * inner.getWidth();
            const float y = inner.getY() + vs.displayY * inner.getHeight() + floatY;

            const float gain = juce::jlimit (0.0f, 1.0f, vs.gain);
            const float radius = 3.0f + 9.0f * gain;

            const auto col = characterColour (p.brightness).withMultipliedBrightness (1.0f + gain * 0.5f);

            // Glow halo
            juce::ColourGradient halo (
                col.withAlpha (0.6f * gain), x, y,
                col.withAlpha (0.0f),        x + radius * 4.5f, y + radius * 4.5f, true);
            g.setGradientFill (halo);
            g.fillEllipse (x - radius * 4.0f, y - radius * 4.0f, radius * 8.0f, radius * 8.0f);

            // Bright core
            g.setColour (juce::Colours::white.withAlpha (gain));
            g.fillEllipse (x - radius * 0.5f, y - radius * 0.5f, radius, radius);
            g.setColour (col.withAlpha (juce::jlimit (0.0f, 1.0f, gain * 1.2f)));
            g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 1.2f);
        }
    }

    void drawLevelBars (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Vertical stereo VU on the right side
        const float barW = 5.0f;
        const float barGap = 3.0f;
        const float pad = 14.0f;
        const auto colour = characterColour (synth.getParameters().brightness);

        juce::Rectangle<float> meterArea (r.getRight() - pad - (barW * 2 + barGap),
                                          r.getY() + pad,
                                          barW * 2 + barGap,
                                          r.getHeight() - pad * 2);

        auto drawBar = [&] (juce::Rectangle<float> b, float lvl)
        {
            // Background track
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
        const auto& p = synth.getParameters();
        const float pad = 14.0f;
        const float orbR = 14.0f;

        const float cx = r.getX() + pad + orbR;
        const float cy = r.getBottom() - pad - orbR;

        // Outer ring (LFO indicator)
        g.setColour (juce::Colour (0xff10171f));
        g.fillEllipse (cx - orbR, cy - orbR, orbR * 2, orbR * 2);
        g.setColour (juce::Colour (0xff202a38));
        g.drawEllipse (cx - orbR, cy - orbR, orbR * 2, orbR * 2, 1.0f);

        // LFO needle: angle follows lfoPhase
        const float angle = lfoPhase * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
        const float nx = cx + std::cos (angle) * (orbR - 4.0f);
        const float ny = cy + std::sin (angle) * (orbR - 4.0f);
        const auto colour = characterColour (p.brightness);
        g.setColour (colour.withAlpha (0.85f));
        g.drawLine (cx, cy, nx, ny, 1.4f);

        // Center dot
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
        g.drawText ("PAD FIELD", bounds.reduced (12.0f, 8.0f), juce::Justification::topLeft);

        const auto& p = synth.getParameters();
        const juce::StringArray characters { "Warm Pad", "Bright Pad", "Strings", "Choir", "Glass", "Air" };
        const auto charName = juce::isPositiveAndBelow (p.characterIdx, characters.size())
                                ? characters[p.characterIdx]
                                : juce::String();

        const int active = synth.getActiveVoices();
        const auto info = charName + "  -  " + juce::String (active) + " voices";

        g.setColour (juce::Colour (0xffaab8c8));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText (info, bounds.reduced (12.0f, 8.0f), juce::Justification::topRight);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadVisualizer)
};
