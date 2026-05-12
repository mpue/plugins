/*
  ==============================================================================

    SnareEngine.h
    SN-1 Luxury Snare Machine — single-voice synthetic snare drum.

    Classic two-mode pitched body (sine + sine in inharmonic ratio) with a fast
    exponential pitch envelope, a band-passed noise tail with adjustable color
    and resonance ("buzz"), a separate high-frequency snap transient, soft tanh
    drive, and a final tilt EQ. Each trigger snapshots the parameters and
    renders independently so live UI tweaks never glitch a note in flight.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class SnareVoice
{
public:
    struct Params
    {
        // ---- Body (membrane) ----
        float tuneHz          = 200.0f;
        float pitchAmtSemis   = 8.0f;
        float pitchTimeMs     = 12.0f;
        float bodyDecayMs     = 110.0f;
        float bodyLevel       = 0.65f;

        // ---- Snares (noise) ----
        float noiseLevel      = 0.85f;
        float noiseColorHz    = 4500.0f;   // bandpass center
        float noiseDecayMs    = 220.0f;
        float buzzQ           = 1.6f;      // bandpass Q
        float snapLevel       = 0.55f;     // high-freq transient

        // ---- Master ----
        float drive           = 0.18f;
        float tone            = 0.0f;      // tilt EQ -1..+1
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
        ampEnv = pitchEnv = noiseEnv = snapEnv = bodyEnv = 0.0f;
        phase1 = phase2 = 0.0f;
        bandpass.reset();
        snapHP.reset();
        tiltLow = 0.0f;
    }

    void trigger (const Params& p, float velocity = 1.0f) noexcept
    {
        params         = p;
        velocityScale  = juce::jlimit (0.0f, 1.0f, velocity);

        bodyEnv  = 1.0f;
        pitchEnv = 1.0f;
        noiseEnv = 1.0f;
        snapEnv  = 1.0f;

        phase1 = phase2 = 0.0f;
        bandpass.reset();
        snapHP.reset();
        tiltLow = 0.0f;

        bodyEnvCoeff   = decayCoeff (params.bodyDecayMs);
        pitchEnvCoeff  = decayCoeff (params.pitchTimeMs);
        noiseEnvCoeff  = decayCoeff (params.noiseDecayMs);
        snapEnvCoeff   = decayCoeff (5.0f); // snap transient ~5ms

        const float fc = juce::jlimit (200.0f, (float) sampleRate * 0.45f, params.noiseColorHz);
        const float Q  = juce::jlimit (0.4f, 12.0f, params.buzzQ);
        bandpass.setBandpass (fc, Q, (float) sampleRate);

        // snap path: high-pass at ~4kHz - bright transient color
        snapHP.setCutoff (4500.0f, (float) sampleRate);

        // tilt EQ pivot ~ 700Hz
        const float tiltFc = 700.0f;
        tiltCoeff = std::exp (-juce::MathConstants<float>::twoPi * tiltFc / (float) sampleRate);

        active = true;
    }

    bool isActive() const noexcept { return active; }

    float renderSample() noexcept
    {
        if (! active)
            return 0.0f;

        // ---------- pitch envelope -> instantaneous body freq ----------
        const float pitchOctaves = (params.pitchAmtSemis / 12.0f) * pitchEnv;
        const float fund         = juce::jlimit (20.0f, (float) sampleRate * 0.45f,
                                                  params.tuneHz * std::pow (2.0f, pitchOctaves));

        // ---------- body: two sine modes (inharmonic ratio ~1.59) ----------
        phase1 += fund / (float) sampleRate;
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += (fund * 1.5874f) / (float) sampleRate;   // 2^(2/3) ≈ minor 6th
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        const float mode1 = std::sin (juce::MathConstants<float>::twoPi * phase1);
        const float mode2 = std::sin (juce::MathConstants<float>::twoPi * phase2) * 0.55f;
        const float bodyS = (mode1 + mode2) * params.bodyLevel * bodyEnv;

        // ---------- noise tail: bandpassed white noise ----------
        const float wn  = rng.nextFloat() * 2.0f - 1.0f;
        const float bp  = bandpass.process (wn);
        // boost factor to keep level roughly constant across Q values
        const float bpGain = 1.0f + std::sqrt (juce::jmax (0.4f, params.buzzQ));
        const float tailS = bp * bpGain * noiseEnv * params.noiseLevel * 0.9f;

        // ---------- snap: bright HP-filtered noise burst ----------
        const float wn2   = rng.nextFloat() * 2.0f - 1.0f;
        const float snS   = snapHP.process (wn2) * snapEnv * params.snapLevel * 1.2f;

        // ---------- mix layers ----------
        float mix = bodyS + tailS + snS;

        // ---------- drive (soft tanh saturation) ----------
        const float k = 1.0f + params.drive * 5.0f;
        mix = std::tanh (mix * k) * (1.0f + params.drive * 0.4f);

        // ---------- tilt EQ ----------
        tiltLow = tiltLow * tiltCoeff + mix * (1.0f - tiltCoeff);
        const float low  = tiltLow;
        const float high = mix - tiltLow;
        mix = mix + low * params.tone * 0.7f - high * params.tone * 0.7f;

        // ---------- velocity / output ----------
        mix *= velocityScale * params.outputGainLin;

        // ---------- envelope advance ----------
        bodyEnv  *= bodyEnvCoeff;
        pitchEnv *= pitchEnvCoeff;
        noiseEnv *= noiseEnvCoeff;
        snapEnv  *= snapEnvCoeff;

        if (bodyEnv < 1.0e-5f && noiseEnv < 1.0e-5f && snapEnv < 1.0e-5f)
            active = false;

        return mix;
    }

    // Render a single shot into a mono buffer (used by the visualiser).
    void renderShot (float* buffer, int numSamples, const Params& p, float velocity = 1.0f)
    {
        prepare (sampleRate);
        trigger (p, velocity);

        for (int i = 0; i < numSamples; ++i)
            buffer[i] = renderSample();
    }

    double getSampleRate() const noexcept { return sampleRate; }

private:
    float decayCoeff (float timeMs) const noexcept
    {
        const float t = juce::jmax (0.3f, timeMs);
        return std::exp (std::log (0.001f) / (t * 0.001f * (float) sampleRate));
    }

    // RBJ biquad bandpass (constant skirt gain)
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

    // Simple one-pole highpass (shape the snap)
    struct OnePoleHP
    {
        float a = 0.95f, lp = 0.0f;

        void setCutoff (float fc, float sr) noexcept
        {
            a = std::exp (-juce::MathConstants<float>::twoPi * fc / sr);
        }
        void reset() noexcept { lp = 0.0f; }
        float process (float in) noexcept
        {
            lp = lp * a + in * (1.0f - a);
            return in - lp;
        }
    };

    Params params;
    double sampleRate = 44100.0;

    bool  active        = false;
    float velocityScale = 1.0f;

    float ampEnv = 0.0f, bodyEnv = 0.0f, pitchEnv = 0.0f, noiseEnv = 0.0f, snapEnv = 0.0f;
    float bodyEnvCoeff = 0.0f, pitchEnvCoeff = 0.0f, noiseEnvCoeff = 0.0f, snapEnvCoeff = 0.0f;

    float phase1 = 0.0f, phase2 = 0.0f;

    BiquadBP   bandpass;
    OnePoleHP  snapHP;

    float tiltLow = 0.0f, tiltCoeff = 0.0f;

    juce::Random rng;
};
