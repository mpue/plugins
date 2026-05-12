/*
  ==============================================================================

    LuxuryPadSynth.h
    A polyphonic pad synthesizer engine designed for lush, rich, evolving
    soundscapes. Each voice combines multiple detuned saw oscillators, a
    sub sine, a slow ADSR envelope, a stereo state-variable filter and a
    per-voice LFO. The global section provides ensemble chorus, ping-pong
    delay, plate-style reverb and a soft saturator.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

class LuxuryPadSynth
{
public:
    struct Parameters
    {
        // Tone
        float texture    = 0.50f;  // 0..1   detune & spread of layered oscillators
        float warmth     = 0.55f;  // 0..1   low end + filter resonance shaping
        float brightness = 0.55f;  // 0..1   filter cutoff
        float movement   = 0.35f;  // 0..1   LFO depth on filter
        float drive      = 0.20f;  // 0..1   soft saturation

        // Envelope (musical units)
        float attackSec  = 1.20f;
        float releaseSec = 2.20f;

        // Effects
        float lushness   = 0.55f;  // 0..1   chorus depth / rate
        float delaySend  = 0.18f;  // 0..1
        float space      = 0.45f;  // 0..1   reverb amount
        float width      = 0.85f;  // 0..1   stereo spread
        float volumeDb   = -6.0f;

        // Pitch / tuning
        int   octaveShift   = 0;     // -2..+2
        int   characterIdx  = 0;     // 0..5 voicing character

        // Modulation rate (slow shimmer)
        float lfoRateHz  = 0.35f;   // 0.05 .. 5
    };

    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;
        for (auto& v : voices) v.prepare (sr);
        chorus.prepare (sr);
        delay.prepare  (sr);
        reverb.prepare (sr);
        smoothMaster.reset (sr, 0.04);
        smoothMaster.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.volumeDb));
        levelMeterL = levelMeterR = 0.0f;
        spectrumPhase = 0.0f;
        juce::ignoreUnused (blockSize);
    }

    void reset()
    {
        for (auto& v : voices) v.reset();
        chorus.reset();
        delay.reset();
        reverb.reset();
        levelMeterL = levelMeterR = 0.0f;
        activityLevel = 0.0f;
    }

    void setParameters (const Parameters& p)
    {
        params = p;
        smoothMaster.setTargetValue (juce::Decibels::decibelsToGain (params.volumeDb));

        // Voice updates
        for (auto& v : voices)
        {
            v.setEnvelope (params.attackSec, params.releaseSec);
            v.setTexture (params.texture);
            v.setWarmth (params.warmth);
            v.setBrightness (params.brightness);
            v.setMovement (params.movement, params.lfoRateHz);
            v.setDrive (params.drive);
            v.setOctaveShift (params.octaveShift);
            v.setCharacter (params.characterIdx);
        }

        chorus.setRate ( juce::jmap (params.lushness, 0.20f, 0.80f) );
        chorus.setDepth ( juce::jmap (params.lushness, 0.0015f, 0.0085f) );
        chorus.setMix   ( juce::jmap (params.lushness, 0.30f, 0.85f) );
        chorus.setSpread ( juce::jmap (params.width, 0.25f, 1.0f) );

        delay.setSend (params.delaySend);
        delay.setFeedback ( juce::jmap (params.delaySend, 0.30f, 0.55f) );
        delay.setWidth (params.width);

        reverb.setMix (params.space);
        reverb.setSize (juce::jmap (params.space, 0.55f, 0.92f));
        reverb.setDamping (juce::jmap (params.warmth, 0.40f, 0.75f));
    }

    void processMidi (const juce::MidiBuffer& midi, int blockSize)
    {
        juce::ignoreUnused (blockSize);
        for (auto m : midi)
        {
            const auto& msg = m.getMessage();
            if (msg.isNoteOn())
                noteOn  (msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
                noteOff (msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                allNotesOff();
        }
    }

    // Process the buffer. The audio is fully synthesised inside (synth output).
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (buffer.getNumChannels() < 2) return;

        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);

        std::fill (L, L + numSamples, 0.0f);
        std::fill (R, R + numSamples, 0.0f);

        // Each voice writes its stereo output additively
        for (auto& v : voices)
            v.render (L, R, numSamples);

        // Master soft saturation (gentle, only audible with high drive)
        if (params.drive > 0.001f)
        {
            const float drive = 1.0f + params.drive * 1.4f;
            const float comp  = 1.0f / std::tanh (drive);
            for (int i = 0; i < numSamples; ++i)
            {
                L[i] = std::tanh (L[i] * drive) * comp;
                R[i] = std::tanh (R[i] * drive) * comp;
            }
        }

        // Stereo chorus
        chorus.process (L, R, numSamples);

        // Stereo ping-pong delay (returns wet+dry mix internally)
        delay.process (L, R, numSamples);

        // Plate reverb
        reverb.process (L, R, numSamples);

        // Stereo width adjustment (M/S)
        applyWidth (L, R, numSamples, params.width);

        // Master gain & metering
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = smoothMaster.getNextValue();
            L[i] *= g;
            R[i] *= g;

            const float al = std::abs (L[i]);
            const float ar = std::abs (R[i]);
            levelMeterL = juce::jmax (levelMeterL * 0.9985f, al);
            levelMeterR = juce::jmax (levelMeterR * 0.9985f, ar);
            activityLevel = juce::jmax (activityLevel * 0.9990f, 0.5f * (al + ar));
        }
    }

    // Visualization helpers
    float getLevelL()       const noexcept { return levelMeterL; }
    float getLevelR()       const noexcept { return levelMeterR; }
    float getActivity()     const noexcept { return activityLevel; }
    int   getActiveVoices() const noexcept
    {
        int n = 0; for (auto& v : voices) if (v.isActive()) ++n; return n;
    }

    // Returns up to 16 active voice notes, normalised pitch positions and gains.
    struct VoiceSnapshot { float note01; float gain; bool isOn; };
    std::array<VoiceSnapshot, 16> getVoiceSnapshot() const noexcept
    {
        std::array<VoiceSnapshot, 16> s {};
        for (size_t i = 0; i < voices.size() && i < s.size(); ++i)
        {
            s[i].note01 = juce::jlimit (0.0f, 1.0f, (voices[i].getMidiNote() - 24.0f) / 84.0f);
            s[i].gain   = voices[i].getEnvLevel();
            s[i].isOn   = voices[i].isActive();
        }
        return s;
    }

    const Parameters& getParameters() const noexcept { return params; }

private:
    static constexpr int kNumVoices = 16;
    static constexpr int kOscPerVoice = 6;

    //===== utility ============================================================
    static void applyWidth (float* L, float* R, int n, float widthAmt)
    {
        const float w = juce::jlimit (0.0f, 1.0f, widthAmt);
        const float side = w;
        const float mid  = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]) * mid;
            const float s = 0.5f * (L[i] - R[i]) * side;
            L[i] = m + s;
            R[i] = m - s;
        }
    }

    static inline float fastTanh (float x) noexcept
    {
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    //===== Oscillator =========================================================
    // A polyBLEP saw, sounds smoother and lush at low frequencies.
    struct PolyBlepSaw
    {
        double phase = 0.0;
        double inc   = 0.0;

        void setFrequency (double freq, double sr)
        {
            inc = freq / sr;
        }

        float process() noexcept
        {
            float value = 2.0f * (float) phase - 1.0f;
            value -= polyBlep ((float) phase, (float) inc);

            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            return value;
        }

        static float polyBlep (float t, float dt) noexcept
        {
            if (dt <= 0.0f) return 0.0f;
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
    };

    struct SineOsc
    {
        double phase = 0.0, inc = 0.0;
        void setFrequency (double f, double sr) { inc = f / sr; }
        float process() noexcept
        {
            float v = std::sin ((float) (phase * juce::MathConstants<double>::twoPi));
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            return v;
        }
    };

    //===== ADSR with smooth shaping ==========================================
    struct PadEnv
    {
        enum class Stage { Idle, Attack, Sustain, Release };
        Stage stage = Stage::Idle;

        float level   = 0.0f;
        float target  = 0.0f;
        float attackCoef = 0.0f;
        float releaseCoef = 0.0f;
        double sr = 44100.0;

        void prepare (double s) { sr = s; }

        void setTimes (float attackSec, float releaseSec)
        {
            attackCoef  = timeToCoef (juce::jmax (0.005f, attackSec));
            releaseCoef = timeToCoef (juce::jmax (0.020f, releaseSec));
        }

        void noteOn (float velocity)
        {
            stage  = Stage::Attack;
            target = juce::jlimit (0.0f, 1.0f, 0.4f + 0.6f * velocity);
            // ramp from current level - allows retriggering
        }

        void noteOff()
        {
            stage  = Stage::Release;
            target = 0.0f;
        }

        bool isActive() const noexcept { return stage != Stage::Idle; }

        float process() noexcept
        {
            switch (stage)
            {
                case Stage::Attack:
                    level += (target - level) * attackCoef;
                    if (std::abs (target - level) < 0.001f)
                    {
                        level = target;
                        stage = Stage::Sustain;
                    }
                    break;
                case Stage::Sustain:
                    level += (target - level) * 0.001f; // very slow drift
                    break;
                case Stage::Release:
                    level += (0.0f - level) * releaseCoef;
                    if (level < 0.00005f)
                    {
                        level = 0.0f;
                        stage = Stage::Idle;
                    }
                    break;
                case Stage::Idle:
                default:
                    return 0.0f;
            }
            return level;
        }

        float timeToCoef (float seconds) const noexcept
        {
            // exponential approach. coef ~ 1 - exp(-1 / (sr*tau))
            return 1.0f - std::exp (-1.0f / (float) (sr * seconds * 0.30f));
        }
    };

    //===== Per-voice State Variable Filter (TPT) ==============================
    struct SVF
    {
        double sr = 44100.0;
        float ic1eq = 0.0f, ic2eq = 0.0f;
        float g = 0.0f, k = 0.0f;
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

        void prepare (double s) { sr = s; ic1eq = ic2eq = 0.0f; setLP (1000.0f, 0.6f); }

        void setLP (float cutoffHz, float Q)
        {
            cutoffHz = juce::jlimit (20.0f, (float) sr * 0.45f, cutoffHz);
            Q = juce::jlimit (0.05f, 8.0f, Q);
            g = (float) std::tan (juce::MathConstants<double>::pi * cutoffHz / sr);
            k = 1.0f / Q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        float processLP (float v0) noexcept
        {
            const float v3 = v0 - ic2eq;
            const float v1 = a1 * ic1eq + a2 * v3;
            const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
            return v2;
        }
    };

    //===== Voice ==============================================================
    struct Voice
    {
        std::array<PolyBlepSaw, kOscPerVoice> saws {};
        SineOsc sub;
        SineOsc subFifth;
        PadEnv  env;
        SVF     filterL;
        SVF     filterR;
        double  sampleRate = 44100.0;

        // LFO for filter modulation
        double lfoPhase = 0.0;
        double lfoInc   = 0.0;
        float  movementDepth = 0.30f;

        // Configuration / parameters
        float baseFreq = 220.0f;
        int   midiNote = 60;
        float velocity = 1.0f;

        // Tone shaping
        float texture = 0.5f;
        float warmth  = 0.55f;
        float brightness = 0.55f;
        float driveAmt   = 0.0f;
        int   octaveShift = 0;
        int   character   = 0;

        // Stereo
        float panL = 1.0f, panR = 1.0f;

        void prepare (double sr)
        {
            sampleRate = sr;
            env.prepare (sr);
            filterL.prepare (sr);
            filterR.prepare (sr);
            // Random seed phases for richness
            for (size_t i = 0; i < saws.size(); ++i)
                saws[i].phase = juce::Random::getSystemRandom().nextFloat();
            sub.phase = juce::Random::getSystemRandom().nextFloat();
            subFifth.phase = juce::Random::getSystemRandom().nextFloat();
            lfoPhase = juce::Random::getSystemRandom().nextFloat();
        }

        void reset()
        {
            env.stage = PadEnv::Stage::Idle;
            env.level = 0.0f;
            filterL.ic1eq = filterL.ic2eq = 0.0f;
            filterR.ic1eq = filterR.ic2eq = 0.0f;
        }

        bool  isActive()    const noexcept { return env.isActive(); }
        float getEnvLevel() const noexcept { return env.level; }
        int   getMidiNote() const noexcept { return midiNote; }

        void noteOn (int note, float vel, float voicePan)
        {
            midiNote = note;
            velocity = vel;
            const float shifted = (float) (note + octaveShift * 12);
            baseFreq = (float) (440.0 * std::pow (2.0, (shifted - 69.0) / 12.0));
            env.noteOn (vel);
            updateOscFreqs();
            // Stereo placement
            const float p = juce::jlimit (-1.0f, 1.0f, voicePan);
            panL = std::cos ((p + 1.0f) * 0.25f * juce::MathConstants<float>::pi) * juce::MathConstants<float>::sqrt2;
            panR = std::sin ((p + 1.0f) * 0.25f * juce::MathConstants<float>::pi) * juce::MathConstants<float>::sqrt2;
        }

        void noteOff()
        {
            env.noteOff();
        }

        void setEnvelope (float a, float r) { env.setTimes (a, r); }
        void setOctaveShift (int o)         { octaveShift = juce::jlimit (-2, 2, o); updateOscFreqs(); }
        void setCharacter (int c)           { character = juce::jlimit (0, 5, c); updateOscFreqs(); }

        void setTexture (float t)    { texture = t; updateOscFreqs(); }
        void setWarmth  (float w)    { warmth  = w; }
        void setBrightness (float b) { brightness = b; }
        void setDrive (float d)      { driveAmt = d; }
        void setMovement (float depth, float rateHz)
        {
            movementDepth = depth;
            lfoInc = rateHz / sampleRate;
        }

        void updateOscFreqs()
        {
            // Detune amount in cents
            const float detuneCents = juce::jmap (texture, 1.0f, 22.0f);

            // Character-dependent voicing
            float weights[kOscPerVoice] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
            float subMix = 0.45f;
            float subFifthMix = 0.0f;

            switch (character)
            {
                case 0: // Warm Pad
                    subMix = 0.55f;
                    subFifthMix = 0.0f;
                    break;
                case 1: // Bright Pad
                    subMix = 0.30f;
                    weights[0] = 0.6f; weights[1] = 0.7f;
                    weights[2] = 1.0f; weights[3] = 1.0f;
                    weights[4] = 1.1f; weights[5] = 1.2f;
                    break;
                case 2: // Strings
                    subMix = 0.40f;
                    break;
                case 3: // Choir
                    subMix = 0.35f;
                    weights[0] = 0.7f; weights[5] = 0.7f; // softer edges
                    break;
                case 4: // Glass
                    subMix = 0.20f;
                    weights[0] = 0.6f; weights[1] = 0.8f; weights[2] = 1.0f;
                    weights[3] = 1.0f; weights[4] = 1.1f; weights[5] = 1.3f;
                    break;
                case 5: // Air
                    subMix = 0.30f;
                    subFifthMix = 0.10f;
                    weights[0] = 0.5f;
                    weights[1] = 0.7f;
                    weights[2] = 0.9f;
                    weights[3] = 0.9f;
                    weights[4] = 1.0f;
                    weights[5] = 1.0f;
                    break;
                default: break;
            }

            (void) weights; // captured by closure (we apply in render)

            // distribute saws around base frequency, alternating up/down
            const float spread[kOscPerVoice] = { -1.0f, 1.0f, -0.5f, 0.5f, -1.5f, 1.5f };
            for (size_t i = 0; i < saws.size(); ++i)
            {
                const float cents = spread[i] * detuneCents;
                const double f = baseFreq * std::pow (2.0, cents / 1200.0);
                saws[i].setFrequency (f, sampleRate);
            }
            sub.setFrequency (baseFreq * 0.5, sampleRate);
            subFifth.setFrequency (baseFreq * 0.75, sampleRate);

            // Cache
            voicingSubMix = subMix;
            voicingSubFifthMix = subFifthMix;
            for (int i = 0; i < kOscPerVoice; ++i) oscWeight[i] = weights[i];
        }

        float voicingSubMix = 0.45f;
        float voicingSubFifthMix = 0.0f;
        float oscWeight[kOscPerVoice] = {1,1,1,1,1,1};

        void render (float* L, float* R, int n)
        {
            if (! env.isActive() && env.level < 0.00005f) return;

            // Map brightness 0..1 to musical Hz range; scale by note pitch
            const float pitchFactor = baseFreq / 220.0f;

            for (int i = 0; i < n; ++i)
            {
                // Saw layer
                float sawSum = 0.0f;
                float weightSum = 0.0f;
                for (int o = 0; o < kOscPerVoice; ++o)
                {
                    sawSum += saws[(size_t) o].process() * oscWeight[o];
                    weightSum += oscWeight[o];
                }
                sawSum /= juce::jmax (1.0f, weightSum);

                // Sub layers
                const float subSig = sub.process() * voicingSubMix;
                const float fifthSig = subFifth.process() * voicingSubFifthMix;

                float core = sawSum * 0.7f + subSig * 0.6f + fifthSig * 0.4f;

                // LFO modulation on cutoff
                const float lfo = std::sin ((float) (lfoPhase * juce::MathConstants<double>::twoPi));
                lfoPhase += lfoInc; if (lfoPhase >= 1.0) lfoPhase -= 1.0;

                // Filter cutoff: brightness combined with note pitch + LFO
                float cutoff = juce::jmap (brightness, 250.0f, 6500.0f) * juce::jlimit (0.5f, 4.0f, std::pow (pitchFactor, 0.6f));
                cutoff *= std::pow (2.0f, lfo * movementDepth * 0.85f); // up to ~+/- 0.85 octaves
                cutoff = juce::jlimit (60.0f, (float) sampleRate * 0.42f, cutoff);

                const float Q = juce::jmap (warmth, 0.55f, 1.55f);

                // Update filter coefficients only every block-ish; here every sample for smooth modulation
                filterL.setLP (cutoff, Q);
                filterR.setLP (cutoff * 1.012f, Q); // slight stereo offset

                const float fl = filterL.processLP (core);
                const float fr = filterR.processLP (core * 0.985f); // tiny stereo asymmetry

                const float envVal = env.process();
                const float scale  = envVal * 0.16f; // headroom for many voices
                L[i] += fl * scale * panL;
                R[i] += fr * scale * panR;
            }
        }
    };

    //===== Stereo Chorus (3-tap ensemble) =====================================
    struct Chorus
    {
        double sr = 44100.0;
        std::vector<float> bufL, bufR;
        int writePos = 0;
        int bufSize  = 0;
        double phaseA = 0.0, phaseB = 0.33, phaseC = 0.66;
        float rateHz = 0.4f;
        float depthSec = 0.004f;
        float mix = 0.5f;
        float spread = 1.0f;

        void prepare (double s)
        {
            sr = s;
            bufSize = (int) (sr * 0.05); // 50 ms
            bufL.assign ((size_t) bufSize, 0.0f);
            bufR.assign ((size_t) bufSize, 0.0f);
            writePos = 0;
            phaseA = 0.0; phaseB = 0.33; phaseC = 0.66;
        }

        void reset() { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); }
        void setRate (float hz) { rateHz = hz; }
        void setDepth (float secs) { depthSec = secs; }
        void setMix (float m) { mix = juce::jlimit (0.0f, 1.0f, m); }
        void setSpread (float s) { spread = juce::jlimit (0.0f, 1.0f, s); }

        float readSample (const std::vector<float>& buf, float delaySamples)
        {
            const int sz = (int) buf.size();
            float pos = (float) writePos - delaySamples;
            while (pos < 0) pos += sz;
            int   i0 = (int) pos;
            int   i1 = (i0 + 1) % sz;
            float frac = pos - (float) i0;
            return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
        }

        void process (float* L, float* R, int n)
        {
            const double inc = rateHz / sr;
            const float baseDelay = (float) (sr * 0.018f); // 18 ms
            const float depthSamples = (float) (sr * depthSec);

            for (int i = 0; i < n; ++i)
            {
                bufL[(size_t) writePos] = L[i];
                bufR[(size_t) writePos] = R[i];

                const float lfoA = std::sin ((float) (phaseA * juce::MathConstants<double>::twoPi));
                const float lfoB = std::sin ((float) (phaseB * juce::MathConstants<double>::twoPi));
                const float lfoC = std::sin ((float) (phaseC * juce::MathConstants<double>::twoPi));

                const float dA = baseDelay + lfoA * depthSamples;
                const float dB = baseDelay + lfoB * depthSamples;
                const float dC = baseDelay + lfoC * depthSamples;

                // Three taps - distributed in stereo space according to spread
                const float aL = readSample (bufL, dA);
                const float aR = readSample (bufR, dA * 1.05f);
                const float bL = readSample (bufL, dB * 1.10f);
                const float bR = readSample (bufR, dB);
                const float cL = readSample (bufL, dC);
                const float cR = readSample (bufR, dC * 0.95f);

                const float wetL = (aL * (1.0f - 0.5f * spread) + bR * spread + cL * (0.7f - 0.3f * spread)) / 2.0f;
                const float wetR = (aR * (1.0f - 0.5f * spread) + bL * spread + cR * (0.7f - 0.3f * spread)) / 2.0f;

                L[i] = L[i] * (1.0f - mix) + wetL * mix;
                R[i] = R[i] * (1.0f - mix) + wetR * mix;

                writePos = (writePos + 1) % bufSize;
                phaseA += inc;          if (phaseA >= 1.0) phaseA -= 1.0;
                phaseB += inc * 1.07f;  if (phaseB >= 1.0) phaseB -= 1.0;
                phaseC += inc * 0.93f;  if (phaseC >= 1.0) phaseC -= 1.0;
            }
        }
    };

    //===== Stereo Delay (ping-pong) ==========================================
    struct PingPongDelay
    {
        double sr = 44100.0;
        std::vector<float> bufL, bufR;
        int writePos = 0;
        int bufSize  = 0;
        float send = 0.2f;
        float feedback = 0.4f;
        float widthAmt = 0.85f;
        float lpStateL = 0.0f, lpStateR = 0.0f;

        void prepare (double s)
        {
            sr = s;
            bufSize = (int) (sr * 1.5);
            bufL.assign ((size_t) bufSize, 0.0f);
            bufR.assign ((size_t) bufSize, 0.0f);
            writePos = 0;
            lpStateL = lpStateR = 0.0f;
        }

        void reset() { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); lpStateL = lpStateR = 0.0f; }
        void setSend (float s)     { send = juce::jlimit (0.0f, 1.0f, s); }
        void setFeedback (float f) { feedback = juce::jlimit (0.0f, 0.85f, f); }
        void setWidth (float w)    { widthAmt = juce::jlimit (0.0f, 1.0f, w); }

        float read (const std::vector<float>& buf, int delaySamples)
        {
            int sz = (int) buf.size();
            int idx = writePos - delaySamples;
            while (idx < 0) idx += sz;
            return buf[(size_t)(idx % sz)];
        }

        void process (float* L, float* R, int n)
        {
            if (send <= 0.001f) return;

            const int dL = (int) (sr * juce::jmap (widthAmt, 0.30f, 0.46f));
            const int dR = (int) (sr * juce::jmap (widthAmt, 0.40f, 0.60f));

            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i];
                const float inR = R[i];

                const float echoL = read (bufR, dL);
                const float echoR = read (bufL, dR);

                // Damped feedback (one-pole LP)
                const float fbL = echoL * feedback;
                const float fbR = echoR * feedback;
                lpStateL += 0.30f * (fbL - lpStateL);
                lpStateR += 0.30f * (fbR - lpStateR);

                bufL[(size_t) writePos] = inL + lpStateL;
                bufR[(size_t) writePos] = inR + lpStateR;

                L[i] = inL + echoL * send * 0.85f;
                R[i] = inR + echoR * send * 0.85f;

                writePos = (writePos + 1) % bufSize;
            }
        }
    };

    //===== Reverb (JUCE built-in for reliability) =============================
    struct ReverbSection
    {
        juce::Reverb rev;
        juce::Reverb::Parameters params;
        double sr = 44100.0;

        void prepare (double s)
        {
            sr = s;
            rev.setSampleRate (sr);
            params.roomSize = 0.85f;
            params.damping  = 0.55f;
            params.wetLevel = 0.40f;
            params.dryLevel = 1.00f;
            params.width    = 1.0f;
            params.freezeMode = 0.0f;
            rev.setParameters (params);
        }

        void reset() { rev.reset(); }

        void setMix (float m)
        {
            const float wet = juce::jlimit (0.0f, 0.85f, m);
            params.wetLevel = wet;
            params.dryLevel = 1.0f - wet * 0.5f;
            rev.setParameters (params);
        }

        void setSize (float s)
        {
            params.roomSize = juce::jlimit (0.0f, 0.99f, s);
            rev.setParameters (params);
        }

        void setDamping (float d)
        {
            params.damping = juce::jlimit (0.0f, 1.0f, d);
            rev.setParameters (params);
        }

        void process (float* L, float* R, int n)
        {
            rev.processStereo (L, R, n);
        }
    };

    //===== Voice management ===================================================
    void noteOn (int note, float velocity)
    {
        // Steal the oldest voice if all busy. Find inactive first.
        int idx = -1;
        for (int i = 0; i < kNumVoices; ++i)
            if (! voices[(size_t) i].isActive()) { idx = i; break; }

        if (idx < 0)
        {
            // Steal the voice with lowest envelope level
            float low = 1e9f; int s = 0;
            for (int i = 0; i < kNumVoices; ++i)
            {
                if (voices[(size_t) i].getEnvLevel() < low)
                {
                    low = voices[(size_t) i].getEnvLevel();
                    s = i;
                }
            }
            idx = s;
        }

        // Distribute voices across the stereo field
        const float pan = ((float) idx / (float) (kNumVoices - 1)) * 2.0f - 1.0f;
        // Reduce extremity
        const float voicePan = pan * 0.85f * juce::jmap (params.width, 0.30f, 1.0f);

        voices[(size_t) idx].setEnvelope (params.attackSec, params.releaseSec);
        voices[(size_t) idx].setTexture (params.texture);
        voices[(size_t) idx].setWarmth (params.warmth);
        voices[(size_t) idx].setBrightness (params.brightness);
        voices[(size_t) idx].setMovement (params.movement, params.lfoRateHz);
        voices[(size_t) idx].setDrive (params.drive);
        voices[(size_t) idx].setOctaveShift (params.octaveShift);
        voices[(size_t) idx].setCharacter (params.characterIdx);
        voices[(size_t) idx].noteOn (note, velocity, voicePan);
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.isActive() && v.getMidiNote() == note)
                v.noteOff();
    }

    void allNotesOff()
    {
        for (auto& v : voices) v.noteOff();
    }

    //===== state ==============================================================
    Parameters params;
    double sampleRate = 44100.0;
    std::array<Voice, kNumVoices> voices;
    Chorus        chorus;
    PingPongDelay delay;
    ReverbSection reverb;

    juce::SmoothedValue<float> smoothMaster;
    float levelMeterL = 0.0f, levelMeterR = 0.0f;
    float activityLevel = 0.0f;
    float spectrumPhase = 0.0f;
};
