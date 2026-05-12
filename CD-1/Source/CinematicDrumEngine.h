/*
  ==============================================================================

    CinematicDrumEngine.h
    CD-1 Luxury Cinematic Drums — polyphonic, multi-layer drum engine.

    Four cinematic drum types share one synthesis core:
      0  BOOM   — ultra-low taiko / film-trailer kick (sub + body + slam)
      1  HIT    — wide tom-style impact (mid body, fast pitch sweep, tail)
      2  CRACK  — snare / clap-style impact (membrane + bandpassed noise)
      3  SUB    — pure deep rumble (sine sub + soft pitch droop, no transient)

    Each voice is a layered hit:
      body osc + detuned twin (de-thinning), sub osc one octave below,
      LP-filtered noise click, BP-filtered noise tail, soft tanh saturation,
      stereo Haas placement. Up to 8 voices ring simultaneously.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace cd1
{
    //==============================================================================
    // RBJ biquad bandpass — used for the snare / clap noise tail.
    struct BiquadBP
    {
        float b0 = 0, b1 = 0, b2 = 0;
        float a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        void set (float fc, float Q, float sr) noexcept
        {
            const float w0    = juce::MathConstants<float>::twoPi * juce::jlimit (20.0f, sr * 0.45f, fc) / sr;
            const float cw    = std::cos (w0);
            const float sw    = std::sin (w0);
            const float alpha = sw / (2.0f * juce::jmax (0.001f, Q));
            const float a0    = 1.0f + alpha;
            b0 =  alpha / a0;
            b1 =  0.0f;
            b2 = -alpha / a0;
            a1 = -2.0f * cw / a0;
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

    // One-pole LP — used for filtered noise click + body LP.
    struct OnePoleLP
    {
        float a = 0.0f, z = 0.0f;
        void set (float fc, float sr) noexcept
        {
            a = std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (10.0f, sr * 0.49f, fc) / sr);
        }
        void reset() noexcept { z = 0.0f; }
        float process (float in) noexcept
        {
            z = z * a + in * (1.0f - a);
            return z;
        }
    };

    //==============================================================================
    // Single drum voice.  A trigger snapshots its parameters and renders a
    // self-contained stereo drum hit until the envelopes have fully decayed.
    class DrumVoice
    {
    public:
        struct Params
        {
            // ---- Body (tonal membrane) ----
            float tuneHz         = 60.0f;
            float pitchAmtSemis  = 36.0f;
            float pitchTimeMs    = 35.0f;
            float bodyDecayMs    = 600.0f;
            float bodyShape      = 0.0f;     // 0=sine, 1=triangle
            float bodyDetuneCents = 6.0f;    // detune of twin osc (thickness)
            float bodyLevel      = 1.0f;

            // ---- Sub layer (one octave below) ----
            float subLevel       = 0.6f;
            float subDecayMs     = 800.0f;

            // ---- Transient click ----
            float clickLevel     = 0.4f;
            float clickToneHz    = 2200.0f;
            float clickDecayMs   = 7.0f;

            // ---- Noise tail (band-passed) ----
            float noiseLevel     = 0.0f;
            float noiseColorHz   = 3500.0f;
            float noiseQ         = 1.0f;
            float noiseDecayMs   = 250.0f;

            // ---- Per-voice master ----
            float drive          = 0.20f;
            float pan            = 0.0f;     // -1 = L, +1 = R
            float spread         = 0.5f;     // 0..1 stereo Haas spread
            float outputLin      = 1.0f;
        };

        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            reset();
        }

        void reset() noexcept
        {
            active = false;
            ageSamples = 0;
            ampEnv = pitchEnv = clickEnv = noiseEnv = subEnv = 0.0f;
            bodyPhase = bodyPhase2 = subPhase = 0.0f;
            clickLP.reset();
            bodyLP.reset();
            tail.reset();
            haasL = haasR = 0;
            std::fill (haasBuf.begin(), haasBuf.end(), 0.0f);
            haasWrite = 0;
        }

        void trigger (const Params& p, float velocity) noexcept
        {
            params         = p;
            velocityScale  = juce::jlimit (0.0f, 1.0f, velocity);

            ampEnv   = 1.0f;
            subEnv   = 1.0f;
            pitchEnv = 1.0f;
            clickEnv = 1.0f;
            noiseEnv = 1.0f;
            bodyPhase = bodyPhase2 = subPhase = 0.0f;
            ageSamples = 0;

            ampCoeff   = decayCoeff (params.bodyDecayMs);
            subCoeff   = decayCoeff (params.subDecayMs);
            pitchCoeff = decayCoeff (params.pitchTimeMs);
            clickCoeff = decayCoeff (params.clickDecayMs);
            noiseCoeff = decayCoeff (params.noiseDecayMs);

            clickLP.set (params.clickToneHz, (float) sampleRate);
            bodyLP .set (juce::jlimit (300.0f, 8000.0f, params.tuneHz * 80.0f), (float) sampleRate);
            tail   .set (params.noiseColorHz, params.noiseQ, (float) sampleRate);

            // Haas delays (0..18 samples) for stereo placement
            const float baseDelay = 18.0f * juce::jlimit (0.0f, 1.0f, params.spread);
            const int   leftD     = (int) std::round (baseDelay * juce::jmax (0.0f,  params.pan));
            const int   rightD    = (int) std::round (baseDelay * juce::jmax (0.0f, -params.pan));
            haasL = leftD;
            haasR = rightD;
            std::fill (haasBuf.begin(), haasBuf.end(), 0.0f);
            haasWrite = 0;

            active = true;
        }

        bool isActive() const noexcept { return active; }

        void renderSample (float& outL, float& outR) noexcept
        {
            if (! active) { outL = outR = 0.0f; return; }

            // ---- pitch envelope -> instantaneous body freq ----
            const float pitchOct = (params.pitchAmtSemis / 12.0f) * pitchEnv;
            const float fund     = juce::jlimit (5.0f, (float) sampleRate * 0.45f,
                                                  params.tuneHz * std::pow (2.0f, pitchOct));

            // ---- body: detuned twin oscillator (sine <-> tri blend) ----
            const float detuneRatio = std::pow (2.0f, params.bodyDetuneCents / 1200.0f);
            bodyPhase  += fund / (float) sampleRate;
            bodyPhase2 += (fund * detuneRatio) / (float) sampleRate;
            if (bodyPhase  >= 1.0f) bodyPhase  -= 1.0f;
            if (bodyPhase2 >= 1.0f) bodyPhase2 -= 1.0f;

            const float s1 = std::sin (juce::MathConstants<float>::twoPi * bodyPhase);
            const float s2 = std::sin (juce::MathConstants<float>::twoPi * bodyPhase2);
            const float t1 = makeTri (bodyPhase);
            const float bodyOsc = (s1 + s2) * 0.5f * (1.0f - params.bodyShape)
                                + t1 * params.bodyShape;
            const float body   = bodyLP.process (bodyOsc) * ampEnv * params.bodyLevel;

            // ---- sub octave: pure sine, slower decay ----
            const float subFreq = params.tuneHz * 0.5f;
            subPhase += subFreq / (float) sampleRate;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
            const float sub = std::sin (juce::MathConstants<float>::twoPi * subPhase) * subEnv * params.subLevel;

            // ---- click: LP-filtered noise burst ----
            const float wn1   = rng.nextFloat() * 2.0f - 1.0f;
            const float click = clickLP.process (wn1) * clickEnv * params.clickLevel * 1.6f;

            // ---- tail: band-passed noise (snare buzz) ----
            const float wn2  = rng.nextFloat() * 2.0f - 1.0f;
            const float bp   = tail.process (wn2);
            const float gainQ = 1.0f + std::sqrt (juce::jmax (0.4f, params.noiseQ));
            const float noise = bp * gainQ * noiseEnv * params.noiseLevel * 0.9f;

            // ---- mix layers ----
            float mix = body + sub + click + noise;

            // ---- soft saturation ----
            const float k = 1.0f + params.drive * 5.0f;
            mix = std::tanh (mix * k) * (1.0f + params.drive * 0.5f);

            // ---- velocity / output ----
            mix *= velocityScale * params.outputLin;

            // ---- stereo placement (Haas + pan trim) ----
            const float panLin = juce::jlimit (-1.0f, 1.0f, params.pan);
            const float gL = std::sqrt (0.5f * (1.0f - panLin));
            const float gR = std::sqrt (0.5f * (1.0f + panLin));

            haasBuf[(size_t) haasWrite] = mix;
            const int n = (int) haasBuf.size();
            const int readL = (haasWrite - haasL + n) % n;
            const int readR = (haasWrite - haasR + n) % n;
            const float dryL = haasBuf[(size_t) readL];
            const float dryR = haasBuf[(size_t) readR];
            haasWrite = (haasWrite + 1) % n;

            outL = dryL * gL * 1.414f;
            outR = dryR * gR * 1.414f;

            // ---- envelope advance ----
            ampEnv   *= ampCoeff;
            subEnv   *= subCoeff;
            pitchEnv *= pitchCoeff;
            clickEnv *= clickCoeff;
            noiseEnv *= noiseCoeff;
            ++ageSamples;

            if (ampEnv  < 1.0e-5f && subEnv   < 1.0e-5f
             && clickEnv < 1.0e-5f && noiseEnv < 1.0e-5f)
                active = false;
        }

        // Render to mono buffer for the visualizer (no Haas / pan).
        void renderShotMono (float* buffer, int numSamples,
                              const Params& p, float velocity = 1.0f)
        {
            // local snapshot to render without disturbing live state
            DrumVoice tmp;
            tmp.prepare (sampleRate);
            // disable Haas for the preview so the wave looks symmetrical
            Params pp = p;
            pp.spread = 0.0f;
            pp.pan    = 0.0f;
            tmp.trigger (pp, velocity);

            for (int i = 0; i < numSamples; ++i)
            {
                float L, R;
                tmp.renderSample (L, R);
                buffer[i] = (L + R) * 0.5f;
            }
        }

        int  getAgeSamples()  const noexcept { return ageSamples; }
        double getSampleRate() const noexcept { return sampleRate; }

    private:
        float decayCoeff (float timeMs) const noexcept
        {
            const float t = juce::jmax (0.3f, timeMs);
            return std::exp (std::log (0.001f) / (t * 0.001f * (float) sampleRate));
        }

        static float makeTri (float phase) noexcept
        {
            const float p4 = phase * 4.0f;
            if (p4 < 1.0f) return p4;
            if (p4 < 3.0f) return 2.0f - p4;
            return p4 - 4.0f;
        }

        Params params;
        double sampleRate = 44100.0;

        bool  active        = false;
        int   ageSamples    = 0;
        float velocityScale = 1.0f;

        float ampEnv = 0, subEnv = 0, pitchEnv = 0, clickEnv = 0, noiseEnv = 0;
        float ampCoeff = 0, subCoeff = 0, pitchCoeff = 0, clickCoeff = 0, noiseCoeff = 0;

        float bodyPhase = 0, bodyPhase2 = 0, subPhase = 0;

        OnePoleLP clickLP, bodyLP;
        BiquadBP  tail;

        // Haas micro-delay for stereo placement (≤ 18 samples ≈ 0.4 ms @ 44.1k)
        std::array<float, 32> haasBuf {};
        int   haasWrite = 0, haasL = 0, haasR = 0;

        juce::Random rng;
    };

    //==============================================================================
    // Drum types and per-drum visible parameters.
    enum DrumType { Boom = 0, Hit, Crack, Sub, NumDrums };

    inline const char* drumName (int idx) noexcept
    {
        switch (idx)
        {
            case Boom:  return "BOOM";
            case Hit:   return "HIT";
            case Crack: return "CRACK";
            case Sub:   return "SUB";
            default:    return "?";
        }
    }

    inline int defaultMidiNote (int idx) noexcept
    {
        // C1, D1, E1, F1 (MIDI 36..41)
        switch (idx)
        {
            case Boom:  return 36;   // C1
            case Hit:   return 38;   // D1
            case Crack: return 40;   // E1
            case Sub:   return 41;   // F1
            default:    return 36;
        }
    }

    // Macros applied across all drums.
    struct MasterMacros
    {
        float depth   = 0.5f;   // sub layer scale
        float impact  = 0.5f;   // transient scale
        float air     = 0.0f;   // -1..+1 high tilt
        float drive   = 0.2f;   // master saturation
        float width   = 0.7f;   // stereo Haas spread
        float size    = 0.4f;   // reverb wet
        float tone    = 0.0f;   // -1..+1 tilt EQ around 700Hz
        float outputDb = 0.0f;
    };

    //==============================================================================
    // Build the full DrumVoice::Params for a given drum type, taking per-drum
    // user knobs (tune/decay/level/pan) and the global master macros into
    // account.  This is what the engine snapshots at trigger time.
    inline DrumVoice::Params buildVoiceParams (int drumIdx,
                                                float tuneSemiOffset,
                                                float decayScale,
                                                float drumLevel,
                                                float pan,
                                                const MasterMacros& m) noexcept
    {
        DrumVoice::Params p;

        switch (drumIdx)
        {
            case Boom:
                p.tuneHz         = 45.0f;
                p.pitchAmtSemis  = 40.0f;
                p.pitchTimeMs    = 55.0f;
                p.bodyDecayMs    = 750.0f;
                p.bodyShape      = 0.05f;
                p.bodyDetuneCents = 8.0f;
                p.bodyLevel      = 1.10f;
                p.subLevel       = 0.85f;
                p.subDecayMs     = 1100.0f;
                p.clickLevel     = 0.30f;
                p.clickToneHz    = 1900.0f;
                p.clickDecayMs   = 9.0f;
                p.noiseLevel     = 0.05f;
                p.noiseColorHz   = 1200.0f;
                p.noiseQ         = 0.7f;
                p.noiseDecayMs   = 350.0f;
                p.drive          = 0.30f;
                break;

            case Hit:
                p.tuneHz         = 110.0f;
                p.pitchAmtSemis  = 24.0f;
                p.pitchTimeMs    = 30.0f;
                p.bodyDecayMs    = 380.0f;
                p.bodyShape      = 0.10f;
                p.bodyDetuneCents = 12.0f;
                p.bodyLevel      = 1.00f;
                p.subLevel       = 0.40f;
                p.subDecayMs     = 500.0f;
                p.clickLevel     = 0.55f;
                p.clickToneHz    = 2800.0f;
                p.clickDecayMs   = 6.0f;
                p.noiseLevel     = 0.20f;
                p.noiseColorHz   = 3000.0f;
                p.noiseQ         = 1.2f;
                p.noiseDecayMs   = 220.0f;
                p.drive          = 0.25f;
                break;

            case Crack:
                p.tuneHz         = 220.0f;
                p.pitchAmtSemis  = 10.0f;
                p.pitchTimeMs    = 14.0f;
                p.bodyDecayMs    = 130.0f;
                p.bodyShape      = 0.20f;
                p.bodyDetuneCents = 18.0f;
                p.bodyLevel      = 0.65f;
                p.subLevel       = 0.10f;
                p.subDecayMs     = 200.0f;
                p.clickLevel     = 0.65f;
                p.clickToneHz    = 4500.0f;
                p.clickDecayMs   = 4.0f;
                p.noiseLevel     = 0.95f;
                p.noiseColorHz   = 4800.0f;
                p.noiseQ         = 1.4f;
                p.noiseDecayMs   = 300.0f;
                p.drive          = 0.20f;
                break;

            case Sub:
                p.tuneHz         = 38.0f;
                p.pitchAmtSemis  = 14.0f;
                p.pitchTimeMs    = 80.0f;
                p.bodyDecayMs    = 1100.0f;
                p.bodyShape      = 0.0f;
                p.bodyDetuneCents = 4.0f;
                p.bodyLevel      = 1.05f;
                p.subLevel       = 0.95f;
                p.subDecayMs     = 1500.0f;
                p.clickLevel     = 0.05f;
                p.clickToneHz    = 1200.0f;
                p.clickDecayMs   = 12.0f;
                p.noiseLevel     = 0.0f;
                p.noiseColorHz   = 1000.0f;
                p.noiseQ         = 0.5f;
                p.noiseDecayMs   = 200.0f;
                p.drive          = 0.18f;
                break;

            default: break;
        }

        // ---- per-drum user offsets ----
        // tune: ±12 st around the drum's home freq
        p.tuneHz       = juce::jlimit (15.0f, 600.0f,
                                        p.tuneHz * std::pow (2.0f, tuneSemiOffset / 12.0f));
        // decay: 0.25× .. 2.5× scaling of body + sub + tail
        const float dScale = juce::jlimit (0.2f, 3.0f, decayScale);
        p.bodyDecayMs  *= dScale;
        p.subDecayMs   *= dScale;
        p.noiseDecayMs *= dScale;

        // ---- master macros into voice params ----
        // depth: scale sub layer level (0..1.5×)
        p.subLevel  *= juce::jlimit (0.0f, 1.5f, m.depth * 1.5f + 0.25f);
        // impact: scale transient + click amount
        p.clickLevel *= juce::jlimit (0.0f, 2.0f, m.impact * 1.7f + 0.3f);
        // drive: voice drive blends with master macro
        p.drive      = juce::jlimit (0.0f, 1.0f, p.drive + m.drive * 0.5f);
        // width: spread between voices
        p.spread     = juce::jlimit (0.0f, 1.0f, m.width);
        // pan from per-drum pan
        p.pan        = juce::jlimit (-1.0f, 1.0f, pan);
        // level
        p.outputLin  = juce::jlimit (0.0f, 2.0f, drumLevel);

        return p;
    }
} // namespace cd1
