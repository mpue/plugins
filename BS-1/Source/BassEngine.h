/*
  ==============================================================================

    BassEngine.h
    BS-1 Luxury Bass Synthesizer — monophonic bass voice with glide.
    Two band-limited oscillators (tone-morph sine→tri→saw→square via PolyBLEP),
    a sub-octave sine, a noise layer, a Moog-style 4-pole ladder LP filter
    with envelope-controlled cutoff, ADSR amp envelope, soft saturation and
    a tilt/warmth tone control. Built for fat, musical, low-end weight.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// LadderFilter only exposes process(context); we want per-sample access so we
// can modulate the cutoff once per sample from the filter envelope.
struct LadderLP : public juce::dsp::LadderFilter<float>
{
    using juce::dsp::LadderFilter<float>::processSample;
    using juce::dsp::LadderFilter<float>::updateSmoothers;
};

class BassVoice
{
public:
    BassVoice() = default;

    struct Params
    {
        // ---- Tone / Oscillators ----
        float tone        = 0.55f;   // 0=sine, 0.33=tri, 0.66=saw, 1=square
        float drive       = 0.30f;   // 0..1 saturation/edge inside oscillator path
        float subLevel    = 0.55f;   // 0..1 sub osc (one octave below)
        float noiseLevel  = 0.02f;   // 0..1 grit
        int   octaveShift = 0;       // -2..+2 (in octaves)

        // ---- Filter ----
        float cutoffHz    = 350.0f;  // 30..14000
        float resonance   = 0.55f;   // 0..1 (mapped internally)
        float envAmount   = 0.80f;   // 0..1, scaled to several octaves of cutoff modulation
        float filterDecay = 280.0f;  // ms

        // ---- Amp envelope ----
        float ampAttack   = 6.0f;    // ms
        float ampSustain  = 0.85f;   // 0..1
        float ampRelease  = 220.0f;  // ms

        // ---- Voice ----
        float glideMs     = 60.0f;   // 0..1500
        float warmth      = 0.35f;   // tilt 0..1 (1 = warmer/darker low shelf, fewer highs)

        // ---- Master ----
        float outputGainLin = 1.0f;
    };

    void prepare (double newSampleRate, int /*samplesPerBlock*/)
    {
        sampleRate = newSampleRate;

        ladder.reset();
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = 1024;
        spec.numChannels      = 1;
        ladder.prepare (spec);
        ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
        ladder.setEnabled (true);

        warmthLow = 0.0f;
        warmthHigh = 0.0f;
        warmthCoeff = std::exp (-juce::MathConstants<float>::twoPi * 800.0f / (float) sampleRate);

        reset();
    }

    void reset() noexcept
    {
        gateOn = false;
        ampEnv = 0.0f;
        filtEnv = 0.0f;

        osc1Phase = 0.0f;
        osc2Phase = 0.5f;
        subPhase  = 0.0f;

        currentFreq = targetFreq = 110.0f;
        velocity = 1.0f;

        noteStack.clearQuick();

        ladder.reset();
        warmthLow = warmthHigh = 0.0f;
    }

    void noteOn (int midiNote, float vel) noexcept
    {
        // simple last-note priority (legato)
        noteStack.removeAllInstancesOf (midiNote);
        noteStack.add (midiNote);

        const float newFreq = midiNoteToHz (midiNote);
        targetFreq = newFreq;

        if (! gateOn)
        {
            currentFreq = newFreq;
            // re-trigger envelopes
            ampEnvStage  = AmpStage::attack;
            filtEnv      = 1.0f;
            velocity     = juce::jlimit (0.05f, 1.0f, vel);
        }
        else
        {
            // legato: keep envelopes, just glide
            velocity = juce::jlimit (0.05f, 1.0f, vel);
        }
        gateOn = true;
    }

    void noteOff (int midiNote) noexcept
    {
        noteStack.removeAllInstancesOf (midiNote);
        if (! noteStack.isEmpty())
        {
            // re-target to the last held note (mono legato)
            targetFreq = midiNoteToHz (noteStack.getLast());
        }
        else
        {
            gateOn = false;
            ampEnvStage = AmpStage::release;
        }
    }

    void allNotesOff() noexcept
    {
        noteStack.clearQuick();
        gateOn = false;
        ampEnvStage = AmpStage::release;
    }

    void setParams (const Params& p) noexcept
    {
        params = p;

        // pre-compute envelope coefficients (one-pole exponential)
        const float ar = juce::jmax (0.5f, p.ampAttack);
        const float rr = juce::jmax (0.5f, p.ampRelease);
        const float fd = juce::jmax (0.5f, p.filterDecay);

        ampAttackCoeff  = std::exp (-1.0f / (ar  * 0.001f * (float) sampleRate));
        ampReleaseCoeff = std::exp (-1.0f / (rr  * 0.001f * (float) sampleRate));
        filtDecayCoeff  = std::exp (std::log (0.001f) / (fd * 0.001f * (float) sampleRate));

        // glide coefficient (one-pole towards target)
        const float gms = juce::jmax (0.0f, p.glideMs);
        if (gms < 0.5f) glideCoeff = 0.0f; // instant
        else            glideCoeff = std::exp (-1.0f / (gms * 0.001f * (float) sampleRate));

        // warmth coefficient (tilt centre 800Hz)
        warmthCoeff = std::exp (-juce::MathConstants<float>::twoPi * 800.0f / (float) sampleRate);

        // configure ladder filter (cutoff updated per-sample with env)
        ladder.setResonance (juce::jlimit (0.0f, 0.92f, p.resonance * 0.9f));
        ladder.setDrive (1.0f + p.drive * 1.5f);
    }

    // Render a block into a stereo (or mono) buffer (additive — caller may pre-clear).
    void renderBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                      float* visBufferMono = nullptr)
    {
        const int numCh = buffer.getNumChannels();
        if (numSamples <= 0 || numCh == 0) return;

        // Normalised octave shift in semitones-as-multiplier
        const float octMul = std::pow (2.0f, (float) params.octaveShift);

        for (int n = 0; n < numSamples; ++n)
        {
            // ---- glide ----
            if (glideCoeff > 0.0f)
                currentFreq = targetFreq + (currentFreq - targetFreq) * glideCoeff;
            else
                currentFreq = targetFreq;

            // ---- amp envelope (A → S → R) ----
            switch (ampEnvStage)
            {
                case AmpStage::attack:
                    ampEnv = 1.0f + (ampEnv - 1.0f) * ampAttackCoeff;
                    if (ampEnv > 0.999f)
                    {
                        ampEnv = 1.0f;
                        ampEnvStage = AmpStage::sustain;
                    }
                    break;
                case AmpStage::sustain:
                    // hold at max while gateOn (no decay stage in this simple model — sustain follows gate)
                    {
                        const float target = params.ampSustain;
                        ampEnv = target + (ampEnv - target) * 0.9995f;
                    }
                    break;
                case AmpStage::release:
                    ampEnv *= ampReleaseCoeff;
                    if (ampEnv < 1.0e-5f) ampEnv = 0.0f;
                    break;
                case AmpStage::idle:
                    ampEnv = 0.0f;
                    break;
            }

            // ---- filter envelope (decay-only, retriggered on note-on) ----
            filtEnv *= filtDecayCoeff;

            // ---- oscillators ----
            const float baseFreq = juce::jlimit (10.0f, (float) sampleRate * 0.45f, currentFreq * octMul);
            const float phaseInc = baseFreq / (float) sampleRate;

            const float s1 = oscillate (osc1Phase, phaseInc, params.tone);
            osc1Phase += phaseInc; if (osc1Phase >= 1.0f) osc1Phase -= 1.0f;

            // OSC2: detuned slightly for fatness, blends in only when tone > 0.25 (after sine)
            const float det = 1.0019f;
            const float s2Inc = phaseInc * det;
            const float s2 = oscillate (osc2Phase, s2Inc, params.tone) * juce::jlimit (0.0f, 1.0f, (params.tone - 0.05f) * 1.4f);
            osc2Phase += s2Inc; if (osc2Phase >= 1.0f) osc2Phase -= 1.0f;

            const float subInc = phaseInc * 0.5f;
            const float subS = std::sin (juce::MathConstants<float>::twoPi * subPhase);
            subPhase += subInc; if (subPhase >= 1.0f) subPhase -= 1.0f;

            const float noiseS = (rng.nextFloat() * 2.0f - 1.0f);

            float sig = (s1 + s2 * 0.55f) * 0.5f
                      + subS * params.subLevel * 0.9f
                      + noiseS * params.noiseLevel * 0.35f;

            // pre-filter drive (subtle, asymmetric for warmth)
            const float k = 1.0f + params.drive * 2.0f;
            sig = std::tanh (sig * k) * (1.0f / std::tanh (k));

            // ---- filter cutoff with envelope modulation ----
            const float envOctaves = params.envAmount * 5.0f * filtEnv;
            const float fc = juce::jlimit (20.0f, (float) sampleRate * 0.45f,
                                            params.cutoffHz * std::pow (2.0f, envOctaves));
            ladder.setCutoffFrequencyHz (fc);
            ladder.updateSmoothers();
            const float filtered = ladder.processSample (sig, 0);

            // ---- warmth tilt EQ ----
            warmthLow = warmthLow * warmthCoeff + filtered * (1.0f - warmthCoeff);
            const float low  = warmthLow;
            const float high = filtered - warmthLow;
            const float warmthAmt = (params.warmth - 0.5f) * 1.4f; // -0.7 .. +0.7
            const float toned = filtered + low * warmthAmt - high * warmthAmt;

            // ---- amp + velocity + master ----
            const float out = toned * ampEnv * velocity * params.outputGainLin;

            // write to all channels
            for (int ch = 0; ch < numCh; ++ch)
                buffer.addSample (ch, startSample + n, out);

            if (visBufferMono != nullptr)
                visBufferMono[n] = out;
        }
    }

    bool isActive() const noexcept { return ampEnv > 1.0e-5f || gateOn; }

    double getSampleRate() const noexcept { return sampleRate; }

    Params getParams() const noexcept { return params; }

    // ---- offline preview render (for the scope) ----
    void renderPreview (float* mono, int numSamples, const Params& p,
                        int previewMidiNote = 36, double previewSr = 48000.0)
    {
        const double oldSr = sampleRate;
        Params oldP = params;
        const auto stack = noteStack;

        prepare (previewSr, numSamples);
        setParams (p);
        noteOn (previewMidiNote, 0.95f);

        const int gateLen = juce::jmin (numSamples, (int) (0.55 * previewSr));

        juce::AudioBuffer<float> tmp (1, numSamples);
        tmp.clear();
        renderBlock (tmp, 0, gateLen, mono);

        if (gateLen < numSamples)
        {
            noteOff (previewMidiNote);
            renderBlock (tmp, gateLen, numSamples - gateLen, mono ? mono + gateLen : nullptr);
        }

        // restore
        prepare (oldSr, numSamples);
        setParams (oldP);
        for (auto n : stack) noteOn (n, 1.0f);
    }

private:
    enum class AmpStage { idle, attack, sustain, release };

    static float midiNoteToHz (int note) noexcept
    {
        return 440.0f * std::pow (2.0f, (note - 69) / 12.0f);
    }

    // PolyBLEP correction
    static inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    // Tone-morphed oscillator: sine → triangle → saw → square (band-limited where relevant)
    static inline float oscillate (float phase, float dt, float tone) noexcept
    {
        const float twoPi = juce::MathConstants<float>::twoPi;

        const float sineS = std::sin (twoPi * phase);

        // triangle from integrated square (band-limited not strictly needed but simple form)
        const float triS = 2.0f * std::abs (2.0f * (phase - std::floor (phase + 0.5f))) - 1.0f;

        // saw with polyBLEP
        float sawS = 2.0f * phase - 1.0f;
        sawS -= polyBlep (phase, dt);

        // square with polyBLEP (two correction terms half a period apart)
        float sqrS = phase < 0.5f ? 1.0f : -1.0f;
        sqrS += polyBlep (phase, dt);
        float ph2 = phase + 0.5f; if (ph2 >= 1.0f) ph2 -= 1.0f;
        sqrS -= polyBlep (ph2, dt);

        // morph piecewise between four targets
        if (tone <= 0.333f)
        {
            const float t = tone / 0.333f;
            return sineS * (1.0f - t) + triS * t;
        }
        if (tone <= 0.666f)
        {
            const float t = (tone - 0.333f) / 0.333f;
            return triS * (1.0f - t) + sawS * t;
        }
        const float t = juce::jlimit (0.0f, 1.0f, (tone - 0.666f) / 0.334f);
        return sawS * (1.0f - t) + sqrS * t;
    }

    Params params;
    double sampleRate = 44100.0;

    // voice state
    bool  gateOn   = false;
    float velocity = 1.0f;
    juce::Array<int> noteStack;

    float currentFreq = 110.0f, targetFreq = 110.0f;
    float glideCoeff = 0.0f;

    float osc1Phase = 0.0f, osc2Phase = 0.5f, subPhase = 0.0f;

    AmpStage ampEnvStage = AmpStage::idle;
    float ampEnv = 0.0f;
    float ampAttackCoeff = 0.0f, ampReleaseCoeff = 0.0f;

    float filtEnv = 0.0f;
    float filtDecayCoeff = 0.0f;

    LadderLP ladder;

    float warmthLow = 0.0f, warmthHigh = 0.0f, warmthCoeff = 0.0f;

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassVoice)
};
