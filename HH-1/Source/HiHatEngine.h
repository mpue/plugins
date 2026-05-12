/*
  ==============================================================================

    HiHatEngine.h
    HH-1 Luxury Hi-Hat Machine — single-voice synthetic hi-hat.

    Six inharmonic square-wave oscillators feed an aggressive high-pass /
    band-pass shaping chain (the classic 909-style metal cluster), layered
    with a band-passed noise bed for "air", a transient noise click for
    attack, a separate resonant shimmer band for sparkle, soft tanh drive,
    a tilt EQ and a mid/side width control for stereo image.

    Two amplitude envelopes (fast + slow) are crossfaded by the HOLD
    parameter to morph smoothly between closed and open hi-hat behaviour.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class HiHatVoice
{
public:
    struct Params
    {
        // ---- Metal cluster ----
        float tuneHz          = 800.0f;     // base oscillator frequency
        float metalLevel      = 0.85f;      // mix of metallic oscillators
        float harmonics       = 1.0f;       // 0..1 inharmonicity spread
        float hpCutoffHz      = 6500.0f;    // post-mix highpass
        float bpCutoffHz      = 9000.0f;    // shimmer bandpass center
        float shimmerQ        = 4.0f;       // bandpass Q

        // ---- Decay / texture ----
        float decayMs         = 90.0f;      // main amplitude decay (closed hat)
        float holdLevel       = 0.0f;       // 0..1, blends to slow tail (open)
        float noiseLevel      = 0.55f;      // air noise mix
        float noiseColorHz    = 7000.0f;    // air noise BP center

        // ---- Master ----
        float drive           = 0.20f;      // tanh drive
        float tone            = 0.0f;       // tilt -1..+1
        float width           = 0.55f;      // stereo width (M/S scale)
        float outputGainLin   = 1.0f;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        active = false;
        ampEnvFast = ampEnvSlow = attackEnv = 0.0f;
        for (auto& p : phasesL) p = 0.0f;
        for (auto& p : phasesR) p = 0.0f;
        hpL.reset(); hpR.reset();
        bpL.reset(); bpR.reset();
        noiseBP.reset();
        attackBP.reset();
        tiltLowL = tiltLowR = 0.0f;
    }

    void trigger (const Params& p, float velocity = 1.0f) noexcept
    {
        params = p;
        velocityScale = juce::jlimit (0.05f, 1.0f, velocity);

        ampEnvFast = 1.0f;
        ampEnvSlow = 1.0f;
        attackEnv  = 1.0f;

        // Slight phase variance per side to widen the metallic cluster.
        for (int i = 0; i < kNumOsc; ++i)
        {
            phasesL[i] = rng.nextFloat();
            phasesR[i] = rng.nextFloat();
        }

        const float hp = juce::jlimit (300.0f,  (float) sampleRate * 0.45f, params.hpCutoffHz);
        const float bp = juce::jlimit (1000.0f, (float) sampleRate * 0.45f, params.bpCutoffHz);
        const float Q  = juce::jlimit (0.5f, 12.0f, params.shimmerQ);
        const float nf = juce::jlimit (200.0f,  (float) sampleRate * 0.45f, params.noiseColorHz);

        hpL.setHighpass (hp, (float) sampleRate);
        hpR.setHighpass (hp, (float) sampleRate);
        bpL.setBandpass (bp, Q, (float) sampleRate);
        bpR.setBandpass (bp * 1.18f, juce::jmax (0.6f, Q * 0.8f), (float) sampleRate);
        noiseBP.setBandpass (nf, 1.4f, (float) sampleRate);
        attackBP.setBandpass (8500.0f, 1.0f, (float) sampleRate);

        ampFastCoeff = decayCoeff (params.decayMs);
        ampSlowCoeff = decayCoeff (params.decayMs * (1.0f + params.holdLevel * 7.0f));
        attackCoeff  = decayCoeff (3.0f);

        const float tiltFc = 1500.0f;
        tiltCoeff = std::exp (-juce::MathConstants<float>::twoPi * tiltFc / (float) sampleRate);

        active = true;
    }

    bool isActive() const noexcept { return active; }

    void renderStereo (float& outL, float& outR) noexcept
    {
        if (! active)
        {
            outL = outR = 0.0f;
            return;
        }

        // ---------- six metallic oscillators ----------
        // Inharmonic ratios chosen for a clean 909-style cluster.
        static constexpr float baseRatios[kNumOsc] = {
            1.0000f, 1.3420f, 1.6688f, 2.0000f, 2.5028f, 3.0300f
        };

        const float harm        = juce::jlimit (0.0f, 1.0f, params.harmonics);
        const float widthDetune = params.width * 0.012f;
        const float invSr       = 1.0f / (float) sampleRate;

        float oscL = 0.0f, oscR = 0.0f;
        for (int i = 0; i < kNumOsc; ++i)
        {
            // harmonics morphs from "all unison" (boring tone) to full cluster.
            const float ratio = juce::jmap (harm, 1.0f, baseRatios[i]);
            const float fund  = juce::jlimit (10.0f, (float) sampleRate * 0.45f,
                                                params.tuneHz * ratio);

            const float fL = fund * (1.0f - widthDetune);
            const float fR = fund * (1.0f + widthDetune);

            phasesL[i] += fL * invSr;
            if (phasesL[i] >= 1.0f) phasesL[i] -= 1.0f;
            phasesR[i] += fR * invSr;
            if (phasesR[i] >= 1.0f) phasesR[i] -= 1.0f;

            const float sqL = (phasesL[i] < 0.5f) ? 1.0f : -1.0f;
            const float sqR = (phasesR[i] < 0.5f) ? 1.0f : -1.0f;

            oscL += sqL;
            oscR += sqR;
        }

        constexpr float oscScale = 1.0f / (float) kNumOsc;
        oscL *= oscScale * params.metalLevel;
        oscR *= oscScale * params.metalLevel;

        // ---------- air noise (bandpassed white) ----------
        const float wn1 = rng.nextFloat() * 2.0f - 1.0f;
        const float wn2 = rng.nextFloat() * 2.0f - 1.0f;
        const float airL = noiseBP.process (wn1) * params.noiseLevel * 0.85f;
        const float airR = noiseBP.process (wn2) * params.noiseLevel * 0.85f;

        // ---------- attack click (very fast bandpassed noise burst) ----------
        const float wn3 = rng.nextFloat() * 2.0f - 1.0f;
        const float clk = attackBP.process (wn3) * attackEnv * 0.55f;

        float mixL = oscL + airL + clk;
        float mixR = oscR + airR + clk * 0.97f;

        // ---------- highpass ----------
        mixL = hpL.process (mixL);
        mixR = hpR.process (mixR);

        // ---------- shimmer band (resonant BP added on top) ----------
        const float shimL = bpL.process (mixL);
        const float shimR = bpR.process (mixR);

        const float shimmerGain = 0.4f + 0.4f * juce::jlimit (0.0f, 1.0f, params.shimmerQ / 12.0f);
        mixL = mixL * 0.78f + shimL * shimmerGain;
        mixR = mixR * 0.78f + shimR * shimmerGain;

        // ---------- amplitude envelope (fast/slow crossfade) ----------
        const float h    = juce::jlimit (0.0f, 1.0f, params.holdLevel);
        const float ampE = ampEnvFast * (1.0f - h) + ampEnvSlow * h;
        mixL *= ampE;
        mixR *= ampE;

        // ---------- soft tanh drive ----------
        const float k = 1.0f + params.drive * 4.5f;
        mixL = std::tanh (mixL * k) * (1.0f + params.drive * 0.35f);
        mixR = std::tanh (mixR * k) * (1.0f + params.drive * 0.35f);

        // ---------- tilt EQ around 1.5kHz ----------
        tiltLowL = tiltLowL * tiltCoeff + mixL * (1.0f - tiltCoeff);
        tiltLowR = tiltLowR * tiltCoeff + mixR * (1.0f - tiltCoeff);
        const float highL = mixL - tiltLowL;
        const float highR = mixR - tiltLowR;
        // tone > 0 -> brighter (boost highs), tone < 0 -> warmer (boost lows)
        mixL = mixL + highL * params.tone * 0.7f - tiltLowL * params.tone * 0.4f;
        mixR = mixR + highR * params.tone * 0.7f - tiltLowR * params.tone * 0.4f;

        // ---------- mid/side width ----------
        const float mid  = 0.5f * (mixL + mixR);
        const float side = 0.5f * (mixL - mixR);
        const float widthMul = 0.3f + params.width * 1.7f;
        mixL = mid + side * widthMul;
        mixR = mid - side * widthMul;

        // ---------- velocity / output ----------
        mixL *= velocityScale * params.outputGainLin;
        mixR *= velocityScale * params.outputGainLin;

        // ---------- envelope advance ----------
        ampEnvFast *= ampFastCoeff;
        ampEnvSlow *= ampSlowCoeff;
        attackEnv  *= attackCoeff;

        if (ampEnvFast < 1.0e-5f && ampEnvSlow < 1.0e-5f && attackEnv < 1.0e-5f)
            active = false;

        outL = mixL;
        outR = mixR;
    }

    // Render a single shot into a mono buffer (for the visualiser).
    void renderShotMono (float* buffer, int numSamples,
                         const Params& p, float velocity = 1.0f)
    {
        prepare (sampleRate);
        trigger (p, velocity);
        for (int i = 0; i < numSamples; ++i)
        {
            float l = 0.0f, r = 0.0f;
            renderStereo (l, r);
            buffer[i] = 0.5f * (l + r);
        }
    }

    double getSampleRate() const noexcept { return sampleRate; }

private:
    static constexpr int kNumOsc = 6;

    float decayCoeff (float timeMs) const noexcept
    {
        const float t = juce::jmax (0.5f, timeMs);
        return std::exp (std::log (0.001f) / (t * 0.001f * (float) sampleRate));
    }

    // RBJ biquad bandpass (constant skirt gain).
    struct BiquadBP
    {
        float b0 = 0, b1 = 0, b2 = 0;
        float a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        void setBandpass (float fc, float Q, float sr) noexcept
        {
            const float w0    = 2.0f * juce::MathConstants<float>::pi * fc / sr;
            const float cw    = std::cos (w0);
            const float sw    = std::sin (w0);
            const float alpha = sw / (2.0f * juce::jmax (0.001f, Q));
            const float a0    = 1.0f + alpha;

            b0 =   alpha / a0;
            b1 =   0.0f;
            b2 = - alpha / a0;
            a1 = - 2.0f * cw / a0;
            a2 = (1.0f - alpha) / a0;
        }

        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

        float process (float in) noexcept
        {
            const float y = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = in;
            y2 = y1; y1 = y;
            return y;
        }
    };

    // RBJ biquad highpass (-12 dB/oct).
    struct BiquadHP
    {
        float b0 = 0, b1 = 0, b2 = 0;
        float a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        void setHighpass (float fc, float sr) noexcept
        {
            const float Q     = 0.7071f;
            const float w0    = 2.0f * juce::MathConstants<float>::pi * fc / sr;
            const float cw    = std::cos (w0);
            const float sw    = std::sin (w0);
            const float alpha = sw / (2.0f * Q);
            const float a0    = 1.0f + alpha;

            b0 =  (1.0f + cw) / 2.0f / a0;
            b1 = -(1.0f + cw)        / a0;
            b2 =  (1.0f + cw) / 2.0f / a0;
            a1 = -2.0f * cw          / a0;
            a2 = (1.0f - alpha)      / a0;
        }

        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

        float process (float in) noexcept
        {
            const float y = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = in;
            y2 = y1; y1 = y;
            return y;
        }
    };

    Params params;
    double sampleRate = 44100.0;

    bool  active        = false;
    float velocityScale = 1.0f;

    float ampEnvFast = 0.0f, ampEnvSlow = 0.0f, attackEnv = 0.0f;
    float ampFastCoeff = 0.0f, ampSlowCoeff = 0.0f, attackCoeff = 0.0f;

    float phasesL[kNumOsc] {};
    float phasesR[kNumOsc] {};

    BiquadHP hpL, hpR;
    BiquadBP bpL, bpR;
    BiquadBP noiseBP, attackBP;

    float tiltLowL = 0.0f, tiltLowR = 0.0f, tiltCoeff = 0.0f;

    juce::Random rng;
};
