/*
  ==============================================================================

    KickEngine.h
    KM-1 Luxury Kick Machine — single-voice synthetic kick drum.
    A pitched body (sine ↔ triangle blend) with a fast exponential pitch
    envelope, a filtered noise click, an unpitched sub-octave layer, soft
    tanh drive, transient punch and a tilt EQ. Each trigger snapshots the
    current parameters and renders independently, so live UI tweaks never
    glitch a note that is already in flight.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class KickVoice
{
public:
    struct Params
    {
        float tuneHz          = 50.0f;
        float pitchAmtSemis   = 36.0f;
        float pitchTimeMs     = 35.0f;
        float bodyDecayMs     = 500.0f;
        float bodyShape       = 0.0f;   // 0 = sine, 1 = triangle
        float clickLevel      = 0.35f;
        float clickToneHz     = 2200.0f;
        float clickDecayMs    = 6.0f;
        float subLevel        = 0.45f;
        float drive           = 0.20f;
        float punch           = 0.30f;
        float tone            = 0.0f;
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
        ampEnv = pitchEnv = clickEnv = punchEnv = 0.0f;
        bodyPhase = subPhase = 0.0f;
        clickLP = 0.0f;
        tiltLow = 0.0f;
    }

    void trigger (const Params& p, float velocity = 1.0f) noexcept
    {
        params         = p;
        velocityScale  = juce::jlimit (0.0f, 1.0f, velocity);

        ampEnv         = 1.0f;
        pitchEnv       = 1.0f;
        clickEnv       = 1.0f;
        punchEnv       = 1.0f;

        bodyPhase      = 0.0f;
        subPhase       = 0.0f;
        clickLP        = 0.0f;
        tiltLow        = 0.0f;

        ampEnvCoeff    = decayCoeff (params.bodyDecayMs);
        pitchEnvCoeff  = decayCoeff (params.pitchTimeMs);
        clickEnvCoeff  = decayCoeff (params.clickDecayMs);
        punchEnvCoeff  = decayCoeff (10.0f);

        const float clickFc = juce::jlimit (200.0f, (float) sampleRate * 0.45f, params.clickToneHz);
        clickLPCoeff = std::exp (-juce::MathConstants<float>::twoPi * clickFc / (float) sampleRate);

        const float tiltFc = 700.0f;
        tiltCoeff = std::exp (-juce::MathConstants<float>::twoPi * tiltFc / (float) sampleRate);

        active = true;
    }

    bool isActive() const noexcept { return active; }

    float renderSample() noexcept
    {
        if (! active)
            return 0.0f;

        // ---------- pitch envelope -> instantaneous freq ----------
        const float pitchOctaves = (params.pitchAmtSemis / 12.0f) * pitchEnv;
        const float instFreq     = juce::jlimit (5.0f, (float) sampleRate * 0.45f,
                                                  params.tuneHz * std::pow (2.0f, pitchOctaves));

        // ---------- body oscillator (sine <-> tri blend) ----------
        bodyPhase += instFreq / (float) sampleRate;
        if (bodyPhase >= 1.0f) bodyPhase -= 1.0f;

        const float sineS = std::sin (juce::MathConstants<float>::twoPi * bodyPhase);
        const float triS  = makeTri (bodyPhase);
        const float bodyS = sineS * (1.0f - params.bodyShape) + triS * params.bodyShape;

        // ---------- sub osc (one octave below, slower amp env) ----------
        const float subFreq = params.tuneHz * 0.5f;
        subPhase += subFreq / (float) sampleRate;
        if (subPhase >= 1.0f) subPhase -= 1.0f;
        const float subS = std::sin (juce::MathConstants<float>::twoPi * subPhase) * params.subLevel;

        // ---------- click (LP-filtered noise burst) ----------
        const float noise = rng.nextFloat() * 2.0f - 1.0f;
        clickLP = clickLP * clickLPCoeff + noise * (1.0f - clickLPCoeff);
        const float clickS = clickLP * clickEnv * params.clickLevel * 1.4f;

        // ---------- mix layers ----------
        float mix = (bodyS + subS) * ampEnv + clickS;

        // ---------- punch (transient emphasis) ----------
        mix *= 1.0f + params.punch * punchEnv * 1.6f;

        // ---------- drive (soft tanh saturation) ----------
        const float k = 1.0f + params.drive * 5.0f;
        mix = std::tanh (mix * k) * (1.0f + params.drive * 0.6f);

        // ---------- tilt EQ ----------
        tiltLow = tiltLow * tiltCoeff + mix * (1.0f - tiltCoeff);
        const float low  = tiltLow;
        const float high = mix - tiltLow;
        mix = mix + low * params.tone * 0.7f - high * params.tone * 0.7f;

        // ---------- velocity / output ----------
        mix *= velocityScale * params.outputGainLin;

        // ---------- envelope advance ----------
        ampEnv   *= ampEnvCoeff;
        pitchEnv *= pitchEnvCoeff;
        clickEnv *= clickEnvCoeff;
        punchEnv *= punchEnvCoeff;

        if (ampEnv < 1.0e-5f && clickEnv < 1.0e-5f)
            active = false;

        return mix;
    }

    // Render a single shot into a mono buffer (used by the visualizer).
    void renderShot (float* buffer, int numSamples, const Params& p, float velocity = 1.0f)
    {
        prepare (sampleRate);
        trigger (p, velocity);

        for (int i = 0; i < numSamples; ++i)
            buffer[i] = renderSample();
    }

    double getSampleRate() const noexcept { return sampleRate; }

    static float makeTri (float phase) noexcept
    {
        const float p4 = phase * 4.0f;
        if (p4 < 1.0f) return p4;
        if (p4 < 3.0f) return 2.0f - p4;
        return p4 - 4.0f;
    }

private:
    float decayCoeff (float timeMs) const noexcept
    {
        const float t = juce::jmax (0.3f, timeMs);
        return std::exp (std::log (0.001f) / (t * 0.001f * (float) sampleRate));
    }

    Params params;
    double sampleRate = 44100.0;

    bool  active        = false;
    float velocityScale = 1.0f;

    float ampEnv = 0.0f, pitchEnv = 0.0f, clickEnv = 0.0f, punchEnv = 0.0f;
    float ampEnvCoeff = 0.0f, pitchEnvCoeff = 0.0f, clickEnvCoeff = 0.0f, punchEnvCoeff = 0.0f;

    float bodyPhase  = 0.0f, subPhase = 0.0f;
    float clickLP    = 0.0f, clickLPCoeff = 0.0f;
    float tiltLow    = 0.0f, tiltCoeff    = 0.0f;

    juce::Random rng;
};
