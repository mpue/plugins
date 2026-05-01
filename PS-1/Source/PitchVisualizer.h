/*
  ==============================================================================

    PitchVisualizer.h
    A luxurious, real-time visualization of the pitch shifter's behaviour.
    Combines a frequency-morphing waveform ribbon, a polished mini piano
    keyboard showing input -> output note mapping with a glowing
    Bezier connector, an interval readout (Octave Up, Perfect Fifth...),
    shimmer particles when feedback is engaged, and live input/output
    level pulses.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LuxuryPitchShifter.h"

class PitchVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit PitchVisualizer (LuxuryPitchShifter& ps) : shifter (ps)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        startTimerHz (60);

        for (auto& p : particles)
            p.life = 0.0f;
    }

    ~PitchVisualizer() override = default;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        // Outer panel — deep gradient with vignette
        {
            juce::ColourGradient bg (
                juce::Colour (0xff0a1118), bounds.getCentreX(), bounds.getCentreY(),
                juce::Colour (0xff020306), bounds.getRight(),  bounds.getBottom(), true);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (bounds, 10.0f);

            g.setColour (juce::Colour (0x224d9eff));
            g.drawRoundedRectangle (bounds.reduced (1.0f), 10.0f, 1.0f);
        }

        const auto inner = bounds.reduced (16.0f);

        // Layout regions
        auto topArea = inner;
        auto kbArea  = topArea.removeFromBottom (juce::jmax (110.0f, inner.getHeight() * 0.32f));
        topArea.removeFromBottom (10.0f);

        // Wave ribbon area takes top half of topArea
        auto ribbonArea = topArea;
        auto centerArea = ribbonArea.removeFromBottom (ribbonArea.getHeight() * 0.45f);

        drawRibbon       (g, ribbonArea);
        drawCentralReadout (g, centerArea);
        drawKeyboard     (g, kbArea);
        drawParticles    (g, ribbonArea.withY (ribbonArea.getY()).withHeight (ribbonArea.getHeight() + centerArea.getHeight()));
        drawCornerLabel  (g, bounds);
    }

private:
    static constexpr int   kMaxParticles = 64;
    static constexpr int   kRibbonResolution = 256;

    struct Particle
    {
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        float life = 0.0f, maxLife = 1.0f;
        float size = 2.0f;
    };

    LuxuryPitchShifter& shifter;
    std::array<Particle, kMaxParticles> particles {};
    int   nextParticleSlot = 0;
    float emitAccum   = 0.0f;
    float pulseSmooth = 0.0f;
    float wetSmooth   = 0.0f;
    float ribbonPhase = 0.0f;
    float displayedSemi = 0.0f;
    int   ticks = 0;

    void timerCallback() override
    {
        ++ticks;
        const float dt = 1.0f / 60.0f;

        const float in  = shifter.getInputEnergy();
        const float wet = 0.5f * (shifter.getWetEnergyL() + shifter.getWetEnergyR());

        pulseSmooth = 0.85f * pulseSmooth + 0.15f * juce::jlimit (0.0f, 1.0f, in  * 14.0f);
        wetSmooth   = 0.80f * wetSmooth   + 0.20f * juce::jlimit (0.0f, 1.0f, wet * 14.0f);

        // Smoothly track the live shifter semitones
        const float liveSemi = shifter.getCurrentSemitones();
        displayedSemi += 0.18f * (liveSemi - displayedSemi);

        // Ribbon phase increment depends on current pitch ratio (visual rhythm)
        const float ratio = juce::jlimit (0.25f, 4.0f, shifter.getCurrentPitchRatio());
        ribbonPhase += dt * (1.0f + 0.5f * ratio);
        if (ribbonPhase > juce::MathConstants<float>::twoPi * 4.0f)
            ribbonPhase -= juce::MathConstants<float>::twoPi * 4.0f;

        // Emit shimmer particles when feedback is active
        const float fb = shifter.getParameters().feedback;
        emitAccum += dt * (8.0f + 50.0f * fb * (0.4f + pulseSmooth));
        while (emitAccum >= 1.0f && fb > 0.05f)
        {
            emitAccum -= 1.0f;
            spawnParticle();
        }

        // Update particles
        for (auto& p : particles)
        {
            if (p.life <= 0.0f) continue;
            p.life -= dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vy -= 18.0f * dt; // float upward
        }

        repaint();
    }

    void spawnParticle()
    {
        const auto inner = getLocalBounds().toFloat().reduced (16.0f);
        auto& p = particles[(size_t) nextParticleSlot];
        nextParticleSlot = (nextParticleSlot + 1) % kMaxParticles;

        const float cx = inner.getCentreX();
        const float baseY = inner.getY() + inner.getHeight() * 0.45f;

        auto& rnd = juce::Random::getSystemRandom();
        p.x = cx + rnd.nextFloat() * 200.0f - 100.0f;
        p.y = baseY + rnd.nextFloat() * 12.0f;
        p.vx = (rnd.nextFloat() - 0.5f) * 30.0f;
        p.vy = -20.0f - rnd.nextFloat() * 50.0f;
        p.maxLife = 1.4f + rnd.nextFloat() * 1.2f;
        p.life    = p.maxLife;
        p.size    = 1.6f + rnd.nextFloat() * 2.6f;
    }

    void drawRibbon (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float midY = r.getCentreY();
        const float amp  = juce::jmin (r.getHeight() * 0.42f, 56.0f);

        const float ratio = juce::jlimit (0.25f, 4.0f, shifter.getCurrentPitchRatio());
        // Two sine waves that morph between input and output frequency across the ribbon width.
        // freq1 = input frequency factor (constant 1.0)
        // freq2 = output (pitch ratio applied)

        juce::Path inputWave, outputWave;
        const int N = kRibbonResolution;

        for (int i = 0; i <= N; ++i)
        {
            float t = (float) i / (float) N;
            float x = r.getX() + r.getWidth() * t;

            // Input wave: gentle sine animated rightward
            float angIn  = ribbonPhase * 1.5f + t * juce::MathConstants<float>::twoPi * 4.0f;
            float yIn    = midY + std::sin (angIn) * amp * (0.4f + 0.6f * pulseSmooth);

            // Output wave: frequency scaled by ratio, slightly phase-offset
            float angOut = ribbonPhase * 1.5f * ratio + t * juce::MathConstants<float>::twoPi * 4.0f * ratio;
            float yOut   = midY + std::sin (angOut) * amp * (0.4f + 0.6f * wetSmooth);

            if (i == 0) { inputWave.startNewSubPath (x, yIn); outputWave.startNewSubPath (x, yOut); }
            else        { inputWave.lineTo (x, yIn);          outputWave.lineTo (x, yOut); }
        }

        // Soft horizon line behind
        g.setColour (juce::Colour (0xff14202d));
        g.drawHorizontalLine ((int) midY, r.getX(), r.getRight());

        // Input — translucent silver
        g.setColour (juce::Colour (0xffaab8c8).withAlpha (0.35f));
        g.strokePath (inputWave, juce::PathStrokeType (1.2f));

        // Output — glowing blue, with a faint outer halo
        {
            auto outerGlow = juce::Colour (0xff4d9eff).withAlpha (0.30f);
            g.setColour (outerGlow);
            g.strokePath (outputWave, juce::PathStrokeType (4.0f));
            g.setColour (juce::Colour (0xffb3d6ff));
            g.strokePath (outputWave, juce::PathStrokeType (1.6f));
        }

        // Side fade to suggest motion
        {
            juce::ColourGradient leftFade  (juce::Colour (0xff0a1118).withAlpha (1.0f), r.getX(), r.getCentreY(),
                                            juce::Colour (0xff0a1118).withAlpha (0.0f), r.getX() + 80.0f, r.getCentreY(), false);
            g.setGradientFill (leftFade);
            g.fillRect (r.removeFromLeft (80.0f));
        }
    }

    void drawCentralReadout (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float semi = displayedSemi;
        const float absSemi = std::abs (semi);
        juce::String main  = (semi > 0.05f ? juce::String ("+") : juce::String());
        if (absSemi < 0.05f) main = juce::String ("0");
        else                 main += juce::String ((int) std::round (semi));

        // Big number
        g.setColour (juce::Colour (0xffe6f0ff));
        g.setFont (juce::Font (juce::FontOptions (44.0f, juce::Font::bold)));
        auto numArea = r;
        g.drawFittedText (main, numArea.toNearestInt(), juce::Justification::centred, 1);

        // 'semitones' suffix
        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        auto suffix = r.translated (0.0f, 28.0f);
        g.drawFittedText ("SEMITONES", suffix.toNearestInt(), juce::Justification::centred, 1);

        // Interval description above number
        auto interval = describeInterval (semi);
        g.setColour (juce::Colour (0xff4d9eff));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        auto labelArea = r.translated (0.0f, -28.0f);
        g.drawFittedText (interval, labelArea.toNearestInt(), juce::Justification::centred, 1);
    }

    static juce::String describeInterval (float semi)
    {
        const int s = (int) std::round (semi);
        if (s == 0) return "UNISON";
        const int abs = std::abs (s);
        const int oct  = abs / 12;
        const int rem  = abs % 12;

        static const char* names[12] = {
            "UNISON", "MINOR 2nd", "MAJOR 2nd", "MINOR 3rd", "MAJOR 3rd", "PERFECT 4th",
            "TRITONE", "PERFECT 5th", "MINOR 6th", "MAJOR 6th", "MINOR 7th", "MAJOR 7th"
        };

        juce::String name;
        if (oct == 0)
        {
            name = names[rem];
        }
        else if (rem == 0)
        {
            name = (oct == 1) ? "OCTAVE" : (juce::String (oct) + " OCTAVES");
        }
        else
        {
            name = juce::String (oct) + "OCT + " + juce::String (names[rem]);
        }
        return juce::String ((s > 0) ? "+ " : "- ") + name;
    }

    void drawKeyboard (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff10171f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (0xff2a3340));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        const auto kb = r.reduced (10.0f, 14.0f);

        // Show 4 octaves around input note (A4 = MIDI 69 reference)
        const int firstMidi = 36;  // C2
        const int lastMidi  = 96;  // C7
        const int totalNotes = lastMidi - firstMidi + 1;
        // Count white keys
        int whiteCount = 0;
        for (int m = firstMidi; m <= lastMidi; ++m)
            if (! isBlack (m)) ++whiteCount;

        const float whiteW = kb.getWidth() / (float) whiteCount;
        const float whiteH = kb.getHeight();
        const float blackW = whiteW * 0.62f;
        const float blackH = whiteH * 0.62f;

        // Reference / shifted notes
        const int refMidi = 60; // C4 reference
        const int shiftedMidi = juce::jlimit (firstMidi, lastMidi, refMidi + (int) std::round (displayedSemi));

        // First pass: white keys
        std::vector<juce::Rectangle<float>> noteRects ((size_t) totalNotes);
        float cursorX = kb.getX();
        for (int m = firstMidi; m <= lastMidi; ++m)
        {
            if (isBlack (m)) continue;
            juce::Rectangle<float> kr (cursorX, kb.getY(), whiteW, whiteH);
            noteRects[(size_t) (m - firstMidi)] = kr;

            // White key body: pearly gradient
            juce::ColourGradient g1 (
                juce::Colour (0xfff0f3f7), kr.getCentreX(), kr.getY(),
                juce::Colour (0xffc5cdd6), kr.getCentreX(), kr.getBottom(), false);
            g.setGradientFill (g1);
            g.fillRoundedRectangle (kr.reduced (1.0f, 0.0f), 2.0f);

            // Subtle vertical separator
            g.setColour (juce::Colour (0xff4a525b));
            g.drawVerticalLine ((int) kr.getRight() - 1, kr.getY(), kr.getBottom());

            cursorX += whiteW;
        }

        // Second pass: black keys positioned on top of white seam
        cursorX = kb.getX();
        for (int m = firstMidi; m <= lastMidi; ++m)
        {
            if (isBlack (m))
            {
                juce::Rectangle<float> kr (cursorX - blackW * 0.5f, kb.getY(), blackW, blackH);
                noteRects[(size_t) (m - firstMidi)] = kr;

                juce::ColourGradient g1 (
                    juce::Colour (0xff1c2230), kr.getCentreX(), kr.getY(),
                    juce::Colour (0xff05080f), kr.getCentreX(), kr.getBottom(), false);
                g.setGradientFill (g1);
                g.fillRoundedRectangle (kr, 2.0f);

                g.setColour (juce::Colour (0xff111720));
                g.drawRoundedRectangle (kr, 2.0f, 0.8f);
            }
            else
            {
                cursorX += whiteW;
            }
        }

        // Highlight reference key (silver)
        highlightKey (g, noteRects[(size_t) (refMidi - firstMidi)], isBlack (refMidi),
                      juce::Colour (0xffe6f0ff), 0.60f + 0.40f * pulseSmooth);

        // Highlight shifted key (blue glow, scaled by wet level)
        highlightKey (g, noteRects[(size_t) (shiftedMidi - firstMidi)], isBlack (shiftedMidi),
                      juce::Colour (0xff4d9eff), 0.40f + 0.60f * wetSmooth);

        // Glow connector arc between the two keys
        if (refMidi != shiftedMidi)
            drawConnector (g, noteRects[(size_t) (refMidi - firstMidi)],
                              noteRects[(size_t) (shiftedMidi - firstMidi)],
                              kb.getY());

        // Note labels at C positions
        g.setColour (juce::Colour (0xff8fa8c4).withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        for (int m = firstMidi; m <= lastMidi; ++m)
        {
            if ((m % 12) != 0) continue;
            const auto& kr = noteRects[(size_t) (m - firstMidi)];
            if (kr.isEmpty()) continue;
            const int oct = m / 12 - 1;
            g.drawText (juce::String ("C") + juce::String (oct),
                        kr.withTrimmedTop (kr.getHeight() - 16.0f).reduced (1.0f, 1.0f),
                        juce::Justification::centred);
        }
    }

    static bool isBlack (int midiNote)
    {
        const int p = ((midiNote % 12) + 12) % 12;
        return p == 1 || p == 3 || p == 6 || p == 8 || p == 10;
    }

    void highlightKey (juce::Graphics& g, juce::Rectangle<float> kr, bool black,
                       juce::Colour colour, float intensity)
    {
        intensity = juce::jlimit (0.0f, 1.0f, intensity);

        // Outer halo
        g.setColour (colour.withAlpha (0.35f * intensity));
        g.fillRoundedRectangle (kr.expanded (3.0f), 4.0f);

        // Filled key
        const float a1 = (black ? 0.85f : 0.78f);
        juce::ColourGradient gr (
            colour.withAlpha (a1 * intensity), kr.getCentreX(), kr.getY(),
            colour.withMultipliedBrightness (0.5f).withAlpha (0.45f * intensity),
            kr.getCentreX(), kr.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (kr.reduced (1.0f), 2.0f);

        // Inner highlight
        g.setColour (colour.withAlpha (0.85f * intensity));
        g.drawRoundedRectangle (kr.reduced (1.0f), 2.0f, 1.4f);
    }

    void drawConnector (juce::Graphics& g, juce::Rectangle<float> a,
                        juce::Rectangle<float> b, float topLine)
    {
        const float ax = a.getCentreX();
        const float bx = b.getCentreX();
        const float arcHeight = juce::jmin (60.0f, std::abs (bx - ax) * 0.35f + 20.0f);

        juce::Path arc;
        arc.startNewSubPath (ax, topLine - 4.0f);
        arc.cubicTo (ax, topLine - arcHeight,
                     bx, topLine - arcHeight,
                     bx, topLine - 4.0f);

        // Glow
        g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.25f));
        g.strokePath (arc, juce::PathStrokeType (5.0f));
        g.setColour (juce::Colour (0xff8fc0ff));
        g.strokePath (arc, juce::PathStrokeType (1.6f));

        // Arrow at destination
        const float dir = (bx > ax) ? 1.0f : -1.0f;
        juce::Path arrow;
        arrow.startNewSubPath (bx - dir * 6.0f, topLine - 10.0f);
        arrow.lineTo (bx, topLine - 4.0f);
        arrow.lineTo (bx - dir * 6.0f, topLine + 2.0f);
        g.setColour (juce::Colour (0xffb3d6ff));
        g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawParticles (juce::Graphics& g, juce::Rectangle<float> r)
    {
        for (auto& p : particles)
        {
            if (p.life <= 0.0f) continue;
            const float t = p.life / juce::jmax (0.0001f, p.maxLife);
            const float a = juce::jlimit (0.0f, 1.0f, t);
            // Particle colour fades from bright cyan to deep blue
            const auto col = juce::Colour (0xff8fc0ff).interpolatedWith (juce::Colour (0xff2452a8), 1.0f - t);
            g.setColour (col.withAlpha (a * 0.85f));
            g.fillEllipse (p.x - p.size, p.y - p.size, p.size * 2.0f, p.size * 2.0f);

            // Halo
            g.setColour (col.withAlpha (a * 0.20f));
            g.fillEllipse (p.x - p.size * 2.5f, p.y - p.size * 2.5f, p.size * 5.0f, p.size * 5.0f);
        }
        juce::ignoreUnused (r);
    }

    void drawCornerLabel (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("PITCH FIELD", bounds.reduced (16.0f, 12.0f), juce::Justification::topLeft);

        // Show grain size & pitch ratio
        const float ratio = shifter.getCurrentPitchRatio();
        const float grainMs = shifter.getGrainSizeMs();
        juce::String txt = "x" + juce::String (ratio, 3) + "   "
                         + juce::String (grainMs, 0) + " ms grain";
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.setColour (juce::Colour (0xff7a8aa3));
        g.drawText (txt, bounds.reduced (16.0f, 12.0f), juce::Justification::topRight);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchVisualizer)
};
