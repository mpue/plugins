/*
  ==============================================================================

    ConvolutionVisualizer.h
    A premium visualization for the CRV-1 convolution reverb. Renders the
    current impulse response as a beautifully drawn dual-channel waveform,
    overlaid with an exponential decay-curve fit, a flowing pulse of light
    that travels through the IR in time with incoming audio, and an animated
    "wave-pool" of expanding rings that represent reflections firing in the
    virtual space. Designed to look luxurious and to communicate the size,
    shape and balance of the reverb at a glance.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ConvolutionEngine.h"
#include <atomic>
#include <vector>

class ConvolutionVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit ConvolutionVisualizer (ConvolutionEngine& e) : engine (e)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        startTimerHz (32);
    }

    ~ConvolutionVisualizer() override = default;

    // Sets the impulse response that will be drawn. Performs reservoir
    // sampling down to a fixed render width on the calling thread, then
    // hands the result to the paint routine in a thread-safe way.
    void setImpulseResponse (const juce::AudioBuffer<float>& ir, double sampleRate)
    {
        const int N = ir.getNumSamples();
        if (N <= 0)
        {
            const juce::ScopedLock sl (irLock);
            irPeaksL.clear();
            irPeaksR.clear();
            irLengthSec = 0.0f;
            return;
        }

        const int target = 720; // visual resolution along the time axis
        std::vector<float> pL ((size_t) target, 0.0f);
        std::vector<float> pR ((size_t) target, 0.0f);

        const int hasR = ir.getNumChannels() >= 2 ? 1 : 0;
        const auto* L = ir.getReadPointer (0);
        const auto* R = hasR ? ir.getReadPointer (1) : L;

        const double samplesPerBucket = (double) N / (double) target;
        for (int b = 0; b < target; ++b)
        {
            const int s0 = (int) (b       * samplesPerBucket);
            const int s1 = juce::jmin (N, (int) ((b + 1) * samplesPerBucket));
            float maxL = 0.0f, maxR = 0.0f;
            for (int s = s0; s < s1; ++s)
            {
                maxL = juce::jmax (maxL, std::abs (L[s]));
                maxR = juce::jmax (maxR, std::abs (R[s]));
            }
            pL[(size_t) b] = maxL;
            pR[(size_t) b] = maxR;
        }

        {
            const juce::ScopedLock sl (irLock);
            irPeaksL  = std::move (pL);
            irPeaksR  = std::move (pR);
            irLengthSec = (float) ((double) N / sampleRate);
        }
    }

    void setIRInfoText (const juce::String& t)
    {
        const juce::ScopedLock sl (irLock);
        irInfoText = t;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background: rich deep blue gradient with vignette
        juce::ColourGradient bg (
            juce::Colour (0xff0a1320), bounds.getCentreX(), bounds.getCentreY(),
            juce::Colour (0xff020306), bounds.getRight(),  bounds.getBottom(), true);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 10.0f);

        // Subtle outline
        g.setColour (juce::Colour (0x224d9eff));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 10.0f, 1.0f);

        // Split into wave-pool top half and waveform bottom half
        auto top    = bounds.withTrimmedBottom (bounds.getHeight() * 0.42f).reduced (10.0f);
        auto bottom = bounds.withTrimmedTop    (bounds.getHeight() * 0.58f).reduced (10.0f, 8.0f);

        drawHallSilhouette (g, top);
        drawWavePool (g, top);
        drawImpulseResponse (g, bottom);
        drawLabels (g, bounds);
    }

    void resized() override {}

private:
    static constexpr int kMaxRings = 28;
    static constexpr int kEnvelopeSamples = 220;

    struct Ring
    {
        float age      = 0.0f;
        float lifetime = 1.0f;
        float intensity = 0.5f;
    };

    ConvolutionEngine& engine;

    juce::CriticalSection irLock;
    std::vector<float> irPeaksL, irPeaksR;
    float irLengthSec = 0.0f;
    juce::String irInfoText;

    std::array<Ring, kMaxRings> rings {};
    int nextRingSlot = 0;
    float emitAccumulator = 0.0f;
    float pulse = 0.0f;
    float wetSmooth = 0.0f;
    float playhead = 0.0f;          // 0..1 across IR; advances when audio comes in
    float playheadEnergy = 0.0f;    // brightness of the moving light pulse

    float envHistoryL[kEnvelopeSamples] {};
    float envHistoryR[kEnvelopeSamples] {};
    int   envWritePos = 0;

    void timerCallback() override
    {
        const float dt = 1.0f / 32.0f;
        const float input = engine.getInputEnergy();
        const float wetL  = engine.getWetEnergyL();
        const float wetR  = engine.getWetEnergyR();

        pulse     = 0.82f * pulse     + 0.18f * juce::jlimit (0.0f, 1.0f, input * 18.0f);
        wetSmooth = 0.80f * wetSmooth + 0.20f * juce::jlimit (0.0f, 1.0f, 0.5f * (wetL + wetR) * 18.0f);

        // Drive playhead: a soft "light" sweeping across the IR each time the
        // input pings the verb. Resets when input rises sharply.
        if (input > 0.012f)
        {
            playheadEnergy = juce::jmax (playheadEnergy, pulse);
            if (playhead > 0.6f) playhead = 0.0f; // re-trigger
        }
        const float irLen = juce::jmax (0.2f, irLengthSec);
        playhead += dt / irLen;
        if (playhead > 1.1f) playhead = 1.1f;
        playheadEnergy *= 0.96f;

        // Ring emission
        emitAccumulator += dt * (4.0f + 14.0f * pulse);
        while (emitAccumulator >= 1.0f && pulse > 0.03f)
        {
            emitAccumulator -= 1.0f;
            auto& r = rings[(size_t) nextRingSlot];
            nextRingSlot = (nextRingSlot + 1) % kMaxRings;
            r.age = 0.0f;
            r.lifetime = juce::jlimit (0.6f, 6.0f, irLengthSec * 0.55f);
            r.intensity = juce::jlimit (0.2f, 1.0f, pulse + 0.1f);
        }
        for (auto& r : rings)
            if (r.age <= r.lifetime + 0.5f) r.age += dt;

        envHistoryL[envWritePos] = wetL;
        envHistoryR[envWritePos] = wetR;
        envWritePos = (envWritePos + 1) % kEnvelopeSamples;

        repaint();
    }

    static juce::Path makeHallPath (juce::Rectangle<float> r)
    {
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

        juce::ColourGradient grad (
            juce::Colour (0x554d9eff), r.getCentreX(), r.getY() + r.getHeight() * 0.4f,
            juce::Colour (0x001a2438), r.getCentreX(), r.getBottom(), true);
        g.setGradientFill (grad);
        g.fillPath (path);

        g.setColour (juce::Colour (0x664d9eff));
        g.strokePath (path, juce::PathStrokeType (1.2f));

        // Perspective floor lines fading into the distance
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

        // Source: pulsing point of light at the centre of the hall
        const float sourceY = horizonY + (baseY - horizonY) * 0.15f;
        const float dotRadius = 5.0f + 9.0f * pulse;
        juce::Rectangle<float> dot (cx - dotRadius, sourceY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
        juce::ColourGradient halo (
            juce::Colour (0xff8fc0ff).withAlpha (0.85f), cx, sourceY,
            juce::Colour (0x00ffffff),                   cx + dotRadius * 3, sourceY, true);
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

            juce::Colour c = juce::Colour (0xff8fc0ff).interpolatedWith (juce::Colour (0xff2452a8), t);
            g.setColour (c.withAlpha (alpha));
            const float thickness = juce::jmax (1.0f, 2.5f * (1.0f - t));
            g.drawEllipse (cx - rx, sourceY - ry, rx * 2.0f, ry * 2.0f, thickness);
        }
    }

    void drawImpulseResponse (juce::Graphics& g, juce::Rectangle<float> r)
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
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4, r.getRight() - 4);

        // Draw waveform from cached peaks
        juce::ScopedLock sl (irLock);
        if (irPeaksL.empty())
        {
            g.setColour (juce::Colour (0xff445566));
            g.drawText ("Loading impulse response...", r,
                        juce::Justification::centred);
            return;
        }

        const int N = (int) irPeaksL.size();
        const float midY = r.getCentreY();
        const float halfH = r.getHeight() * 0.45f;

        // Filled silhouette (L)
        juce::Path fillL;
        fillL.startNewSubPath (r.getX(), midY);
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) (N - 1);
            const float x = r.getX() + t * r.getWidth();
            fillL.lineTo (x, midY - irPeaksL[(size_t) i] * halfH);
        }
        for (int i = N - 1; i >= 0; --i)
        {
            const float t = (float) i / (float) (N - 1);
            const float x = r.getX() + t * r.getWidth();
            fillL.lineTo (x, midY);
        }
        fillL.closeSubPath();

        juce::Path fillR;
        fillR.startNewSubPath (r.getX(), midY);
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) (N - 1);
            const float x = r.getX() + t * r.getWidth();
            fillR.lineTo (x, midY + irPeaksR[(size_t) i] * halfH);
        }
        for (int i = N - 1; i >= 0; --i)
        {
            const float t = (float) i / (float) (N - 1);
            const float x = r.getX() + t * r.getWidth();
            fillR.lineTo (x, midY);
        }
        fillR.closeSubPath();

        juce::ColourGradient topGrad (
            juce::Colour (0xcc8fc0ff), r.getX(), midY - halfH,
            juce::Colour (0x222452a8), r.getX(), midY, false);
        g.setGradientFill (topGrad);
        g.fillPath (fillL);

        juce::ColourGradient botGrad (
            juce::Colour (0x222452a8), r.getX(), midY,
            juce::Colour (0xccff66bb), r.getX(), midY + halfH, false);
        g.setGradientFill (botGrad);
        g.fillPath (fillR);

        // Bright outline strokes
        juce::Path topLine, botLine;
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) (N - 1);
            const float x = r.getX() + t * r.getWidth();
            if (i == 0) {
                topLine.startNewSubPath (x, midY - irPeaksL[(size_t) i] * halfH);
                botLine.startNewSubPath (x, midY + irPeaksR[(size_t) i] * halfH);
            } else {
                topLine.lineTo (x, midY - irPeaksL[(size_t) i] * halfH);
                botLine.lineTo (x, midY + irPeaksR[(size_t) i] * halfH);
            }
        }
        g.setColour (juce::Colour (0xffd6e6ff));
        g.strokePath (topLine, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colour (0xffffb8d8));
        g.strokePath (botLine, juce::PathStrokeType (1.2f));

        // Exponential decay envelope overlay (fit-line through the peaks)
        juce::Path env;
        // crude T60 estimate from peaks: time at which envelope crosses -60 dB
        float peak = 0.0f;
        for (auto v : irPeaksL) peak = juce::jmax (peak, v);
        for (auto v : irPeaksR) peak = juce::jmax (peak, v);
        if (peak > 1.0e-4f)
        {
            for (int i = 0; i < N; ++i)
            {
                const float t = (float) i / (float) (N - 1);
                const float maxLR = juce::jmax (irPeaksL[(size_t) i], irPeaksR[(size_t) i]);
                const float dB = juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, maxLR / peak));
                // Map dB [0..-60] to [midY-halfH .. midY-halfH*0.05]
                const float y = juce::jmap (juce::jlimit (-60.0f, 0.0f, dB),
                                            -60.0f, 0.0f,
                                            midY - 4.0f, midY - halfH);
                const float x = r.getX() + t * r.getWidth();
                if (i == 0) env.startNewSubPath (x, y);
                else        env.lineTo (x, y);
            }
            g.setColour (juce::Colour (0x553effa9));
            g.strokePath (env, juce::PathStrokeType (1.0f));
        }

        // Moving "playback head" — a soft column of light traveling across
        // the IR each time we get input. Animates the otherwise-static IR.
        const float ph = juce::jlimit (0.0f, 1.0f, playhead);
        const float hx = r.getX() + ph * r.getWidth();
        const float intensity = juce::jlimit (0.0f, 1.0f, playheadEnergy);
        if (intensity > 0.01f)
        {
            juce::ColourGradient sweep (
                juce::Colour (0xff4d9eff).withAlpha (intensity * 0.85f), hx, r.getY(),
                juce::Colour (0x004d9eff),                                hx + 30.0f, r.getY(), false);
            g.setGradientFill (sweep);
            g.fillRect (juce::Rectangle<float> (hx, r.getY() + 2.0f, 4.0f, r.getHeight() - 4.0f));

            juce::ColourGradient sweep2 (
                juce::Colour (0xff8fc0ff).withAlpha (intensity), hx, r.getCentreY(),
                juce::Colour (0x004d9eff),                       hx + 50.0f, r.getCentreY(), true);
            g.setGradientFill (sweep2);
            g.fillEllipse (hx - 25.0f, r.getCentreY() - 25.0f, 50.0f, 50.0f);
        }

        // Time markers
        g.setColour (juce::Colour (0xff7a8a9c));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        for (int i = 1; i < 4; ++i)
        {
            const float t = (float) i / 4.0f;
            const float secs = irLengthSec * t;
            const float x = r.getX() + t * r.getWidth();
            g.drawText (juce::String (secs, 1) + "s",
                        (int) (x - 22.0f), (int) (r.getBottom() - 16.0f), 44, 12,
                        juce::Justification::centred);
        }

        // IR info text
        g.setColour (juce::Colour (0xffb8c8dc));
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (irInfoText, r.reduced (10.0f, 4.0f), juce::Justification::topLeft);

        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("L", r.reduced (10.0f, 4.0f), juce::Justification::topRight);
        g.drawText ("R", r.reduced (10.0f, 4.0f), juce::Justification::bottomRight);
    }

    void drawLabels (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("CONVOLUTION FIELD", bounds.reduced (14.0f, 8.0f),
                    juce::Justification::topLeft);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionVisualizer)
};
