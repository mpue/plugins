/*
  ==============================================================================

    GranularEngine.h
    A polyphonic granular synthesizer engine built around six internally
    synthesised "luxury" source textures (Vocal, Strings, Choir, Bell, Glass,
    Air). Each held MIDI note spawns a continuous cloud of short windowed
    grains that are pitched, scattered and panned according to user
    parameters. The output is sweetened by a stereo ensemble chorus, a soft
    plate reverb, drive and width.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <atomic>

class GranularEngine
{
public:
    enum SourceType
    {
        SourceVocal = 0,
        SourceStrings,
        SourceChoir,
        SourceBell,
        SourceGlass,
        SourceAir,
        kNumSources
    };

    struct Parameters
    {
        // Source selection
        int   sourceIdx     = 0;       // 0..5

        // Grain shaping
        float position      = 0.30f;   // 0..1   normalized position in source
        float spray         = 0.20f;   // 0..1   random position deviation
        float grainSizeMs   = 120.0f;  // 20..400 ms
        float density       = 30.0f;   // 6..120  grains / sec / voice
        float pitchSemis    = 0.0f;    // -24..+24 semitones offset
        float pitchSpray    = 0.0f;    // 0..1 random pitch deviation (cents up to ~1200)
        float reverseProb   = 0.15f;   // 0..1 chance of reverse-played grain
        float panSpread     = 0.65f;   // 0..1 random pan
        float movement      = 0.30f;   // 0..1 LFO depth on position scrub

        // Envelope (in seconds)
        float attackSec     = 0.50f;
        float releaseSec    = 1.80f;

        // Tone
        float tone          = 0.50f;   // 0..1 tilt (low <-> high)
        float drive         = 0.15f;   // 0..1 soft saturation

        // FX
        float lushness      = 0.55f;   // 0..1 chorus
        float space         = 0.55f;   // 0..1 reverb
        float width         = 0.85f;   // 0..1 stereo width
        float volumeDb      = -6.0f;

        // Pitch
        int   octaveShift   = 0;       // -2..+2

        // Visualizer-only LFO speed for the source-scan needle
        float lfoRateHz     = 0.30f;
    };

    // Single grain that lives in a fixed pool; rebuilt on spawn.
    struct Grain
    {
        bool     active   = false;
        bool     reverse  = false;
        const float* source = nullptr;
        int      sourceLength = 0;
        double   readPos      = 0.0;    // fractional read position into source
        double   readInc      = 1.0;    // pitch ratio
        int      lifetimeSamples = 0;
        int      ageSamples      = 0;
        float    panL = 1.0f, panR = 1.0f;
        float    levelGain = 1.0f;      // includes voice envelope baseline
        float    sourcePos01 = 0.0f;    // for visualizer
        float    pitchRatioVis = 1.0f;  // for visualizer
        int      voiceIdx = 0;          // for visualizer / metering
    };

    GranularEngine()
    {
        for (auto& v : voices) v = Voice();
        grainPool.resize (kPoolSize);
        prepareSources (44100.0);
    }

    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;
        prepareSources (sr);
        chorus.prepare (sr);
        reverb.prepare (sr);
        smoothMaster.reset (sr, 0.04);
        smoothMaster.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.volumeDb));
        for (auto& v : voices) { v.envLevel = 0.0f; v.stage = Voice::Idle; v.phaseAccum = 0.0; }
        for (auto& g : grainPool) g.active = false;
        levelMeterL = levelMeterR = 0.0f;
        activityLevel = 0.0f;
        activeGrainCount.store (0);
        lfoPhase = 0.0;
    }

    void reset()
    {
        for (auto& g : grainPool) g.active = false;
        for (auto& v : voices) { v.envLevel = 0.0f; v.stage = Voice::Idle; v.phaseAccum = 0.0; }
        chorus.reset();
        reverb.reset();
        levelMeterL = levelMeterR = 0.0f;
        activityLevel = 0.0f;
        activeGrainCount.store (0);
    }

    void setParameters (const Parameters& p)
    {
        params = p;
        smoothMaster.setTargetValue (juce::Decibels::decibelsToGain (params.volumeDb));

        chorus.setRate ( juce::jmap (params.lushness, 0.18f, 0.85f) );
        chorus.setDepth ( juce::jmap (params.lushness, 0.0014f, 0.0085f) );
        chorus.setMix   ( juce::jmap (params.lushness, 0.20f, 0.85f) );
        chorus.setSpread ( juce::jmap (params.width, 0.25f, 1.0f) );

        reverb.setMix     (params.space);
        reverb.setSize    (juce::jmap (params.space, 0.55f, 0.92f));
        reverb.setDamping (juce::jmap (params.tone, 0.75f, 0.30f));
    }

    void processMidi (const juce::MidiBuffer& midi, int /*blockSize*/)
    {
        for (auto m : midi)
        {
            const auto& msg = m.getMessage();
            if      (msg.isNoteOn() && msg.getVelocity() > 0)  noteOn  (msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff() || msg.isNoteOn())        noteOff (msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) allNotesOff();
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        if (buffer.getNumChannels() < 2) return;

        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);

        const auto& src = sourceBuffers[(size_t) juce::jlimit (0, kNumSources - 1, params.sourceIdx)];
        const float* srcPtr = src.data();
        const int srcLen = (int) src.size();

        // Update LFO phase (one update per block)
        const double lfoInc = (double) params.lfoRateHz / sampleRate * (double) n;
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;

        const float lfoCur = std::sin ((float) (lfoPhase * juce::MathConstants<double>::twoPi));

        // Voice update + grain spawning
        for (int v = 0; v < kNumVoices; ++v)
        {
            auto& voice = voices[(size_t) v];
            if (voice.stage == Voice::Idle && voice.envLevel < 1e-5f) continue;

            // Envelope progression — block-rate
            const float blockSec = (float) n / (float) sampleRate;

            switch (voice.stage)
            {
                case Voice::Attack:
                {
                    const float a = juce::jmax (0.005f, params.attackSec);
                    voice.envLevel += blockSec / a;
                    if (voice.envLevel >= voice.target)
                    {
                        voice.envLevel = voice.target;
                        voice.stage = Voice::Sustain;
                    }
                    break;
                }
                case Voice::Sustain:
                    voice.envLevel = voice.target;
                    break;
                case Voice::Release:
                {
                    const float r = juce::jmax (0.020f, params.releaseSec);
                    voice.envLevel -= blockSec / r * voice.releaseAmp;
                    if (voice.envLevel <= 0.0001f)
                    {
                        voice.envLevel = 0.0f;
                        voice.stage = Voice::Idle;
                    }
                    break;
                }
                default: break;
            }

            // Grain spawning (time-accumulator based)
            if (voice.stage != Voice::Idle && voice.envLevel > 0.001f)
            {
                const float grainsPerSec = juce::jlimit (1.0f, 200.0f, params.density);
                voice.phaseAccum += (double) grainsPerSec * (double) n / sampleRate;
                while (voice.phaseAccum >= 1.0)
                {
                    voice.phaseAccum -= 1.0;
                    spawnGrain (voice, srcPtr, srcLen, lfoCur);
                }
            }
        }

        // Render all active grains
        renderGrains (L, R, n);

        // Soft master saturation
        if (params.drive > 0.001f)
        {
            const float drive = 1.0f + params.drive * 1.6f;
            const float comp  = 1.0f / std::tanh (drive);
            for (int i = 0; i < n; ++i)
            {
                L[i] = std::tanh (L[i] * drive) * comp;
                R[i] = std::tanh (R[i] * drive) * comp;
            }
        }

        // Tone tilt: a simple one-pole LP/HP tilt around 1kHz
        applyTone (L, R, n, params.tone);

        // Stereo chorus
        chorus.process (L, R, n);

        // Plate reverb
        reverb.process (L, R, n);

        // Width
        applyWidth (L, R, n, params.width);

        // Master gain & meter
        int aliveGrains = 0;
        for (auto& g : grainPool) if (g.active) ++aliveGrains;
        activeGrainCount.store (aliveGrains);

        for (int i = 0; i < n; ++i)
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

    // ===== Visualization helpers ============================================
    float getLevelL()       const noexcept { return levelMeterL; }
    float getLevelR()       const noexcept { return levelMeterR; }
    float getActivity()     const noexcept { return activityLevel; }
    int   getActiveGrains() const noexcept { return activeGrainCount.load(); }
    int   getActiveVoices() const noexcept
    {
        int n = 0; for (auto& v : voices) if (v.stage != Voice::Idle) ++n; return n;
    }
    float getLfoPhase()     const noexcept { return (float) lfoPhase; }

    // Source waveform (downsampled-on-demand) for the visualiser
    const std::vector<float>& getCurrentSourceBuffer() const noexcept
    {
        return sourceBuffers[(size_t) juce::jlimit (0, kNumSources - 1, params.sourceIdx)];
    }

    // Snapshot of currently active grains for visualizer (lock-free, readonly).
    struct GrainSnapshot
    {
        bool   active = false;
        float  pos01  = 0.0f;
        float  age01  = 0.0f;
        float  pitchRatio = 1.0f;
        float  pan    = 0.0f;
        int    voiceIdx = 0;
    };

    std::array<GrainSnapshot, 192> getGrainSnapshot() const noexcept
    {
        std::array<GrainSnapshot, 192> snap {};
        const int N = juce::jmin ((int) snap.size(), (int) grainPool.size());
        for (int i = 0; i < N; ++i)
        {
            const auto& g = grainPool[(size_t) i];
            snap[(size_t) i].active = g.active;
            if (! g.active) continue;
            snap[(size_t) i].pos01      = g.sourcePos01;
            snap[(size_t) i].age01      = g.lifetimeSamples > 0
                                            ? (float) g.ageSamples / (float) g.lifetimeSamples
                                            : 0.0f;
            snap[(size_t) i].pitchRatio = g.pitchRatioVis;
            snap[(size_t) i].pan        = g.panR - g.panL;
            snap[(size_t) i].voiceIdx   = g.voiceIdx;
        }
        return snap;
    }

    const Parameters& getParameters() const noexcept { return params; }

    static const char* getSourceName (int i)
    {
        static const char* names[kNumSources] =
            { "Vocal", "Strings", "Choir", "Bell", "Glass", "Air" };
        return names[juce::jlimit (0, kNumSources - 1, i)];
    }

private:
    static constexpr int kNumVoices  = 8;
    static constexpr int kPoolSize   = 192;
    static constexpr int kSourceLengthSec = 4;

    //===== Voice ==============================================================
    struct Voice
    {
        enum Stage { Idle, Attack, Sustain, Release };
        Stage  stage   = Idle;
        int    midiNote = 60;
        float  velocity = 1.0f;
        float  envLevel = 0.0f;
        float  target   = 0.0f;
        float  releaseAmp = 0.0f;   // captured envelope at note-off
        double phaseAccum = 0.0;    // for grain scheduling
        float  pitchRatio = 1.0f;   // 2^((note-60)/12) including octave
    };

    std::array<Voice, kNumVoices> voices;

    void noteOn (int note, float velocity)
    {
        // Steal voice playing this note first, otherwise the oldest released, otherwise lowest env.
        int idx = -1;
        for (int i = 0; i < kNumVoices; ++i)
            if (voices[(size_t) i].midiNote == note && voices[(size_t) i].stage != Voice::Idle)
            { idx = i; break; }

        if (idx < 0)
            for (int i = 0; i < kNumVoices; ++i)
                if (voices[(size_t) i].stage == Voice::Idle) { idx = i; break; }

        if (idx < 0)
        {
            float low = 1e9f; int s = 0;
            for (int i = 0; i < kNumVoices; ++i)
                if (voices[(size_t) i].envLevel < low) { low = voices[(size_t) i].envLevel; s = i; }
            idx = s;
        }

        auto& v = voices[(size_t) idx];
        v.midiNote   = note;
        v.velocity   = velocity;
        v.target     = juce::jlimit (0.0f, 1.0f, 0.35f + 0.65f * velocity);
        v.stage      = Voice::Attack;
        // pitchRatio: shifted by octave param + per-grain pitch knob
        const int shifted = note + params.octaveShift * 12;
        v.pitchRatio = std::pow (2.0f, (shifted - 60) / 12.0f);
        // (Don't reset envLevel: allows soft retrigger from current level)
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.stage != Voice::Idle && v.midiNote == note)
            {
                v.releaseAmp = juce::jmax (v.envLevel, 0.0001f);
                v.stage = Voice::Release;
            }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.stage != Voice::Idle)
            {
                v.releaseAmp = juce::jmax (v.envLevel, 0.0001f);
                v.stage = Voice::Release;
            }
    }

    //===== Grains =============================================================
    std::vector<Grain> grainPool;
    std::atomic<int>   activeGrainCount { 0 };

    int findFreeGrain() noexcept
    {
        for (int i = 0; i < (int) grainPool.size(); ++i)
            if (! grainPool[(size_t) i].active) return i;
        return -1;
    }

    void spawnGrain (Voice& voice, const float* srcPtr, int srcLen, float lfoCur)
    {
        const int idx = findFreeGrain();
        if (idx < 0) return;

        auto& g = grainPool[(size_t) idx];

        // Position with movement LFO + spray
        const float movementOffset = lfoCur * params.movement * 0.45f;
        const float sprayOffset = (random.nextFloat() * 2.0f - 1.0f) * params.spray * 0.45f;
        float pos01 = juce::jlimit (0.0f, 0.999f,
                                    params.position + movementOffset + sprayOffset);
        // wrap so we never read past the buffer
        if (pos01 < 0.0f) pos01 += 1.0f;
        if (pos01 >= 1.0f) pos01 -= 1.0f;

        // Pitch ratio: voice + offset + pitch spray (cents)
        const float sprayCents = (random.nextFloat() * 2.0f - 1.0f) * params.pitchSpray * 1200.0f;
        const float pitchSemis = params.pitchSemis;
        const float baseRatio  = voice.pitchRatio
                               * std::pow (2.0f, pitchSemis / 12.0f)
                               * std::pow (2.0f, sprayCents / 1200.0f);

        g.source       = srcPtr;
        g.sourceLength = srcLen;
        g.readPos      = (double) (pos01 * (float) srcLen);
        g.readInc      = (double) baseRatio;
        g.reverse      = (random.nextFloat() < params.reverseProb);
        if (g.reverse)
            g.readInc = -g.readInc;

        // Grain length in samples
        const int lenSamples = juce::jmax (32, (int) ((double) params.grainSizeMs * 0.001 * sampleRate));
        g.lifetimeSamples = lenSamples;
        g.ageSamples      = 0;

        // Stereo placement
        const float pan = (random.nextFloat() * 2.0f - 1.0f) * params.panSpread;
        const float a   = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        g.panL = std::cos (a) * juce::MathConstants<float>::sqrt2;
        g.panR = std::sin (a) * juce::MathConstants<float>::sqrt2;

        // Slightly randomize per-grain gain (humanization)
        const float jitter = 0.85f + 0.30f * random.nextFloat();
        g.levelGain = voice.envLevel * voice.velocity * jitter;

        g.sourcePos01    = pos01;
        g.pitchRatioVis  = baseRatio;
        g.voiceIdx       = (int) (&voice - &voices[0]);
        g.active         = true;
    }

    static inline float readSourceLinear (const float* src, int srcLen, double pos) noexcept
    {
        // Wrap into [0, srcLen)
        while (pos < 0.0)        pos += (double) srcLen;
        while (pos >= (double) srcLen) pos -= (double) srcLen;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % srcLen;
        const float frac = (float) (pos - (double) i0);
        return src[i0] * (1.0f - frac) + src[i1] * frac;
    }

    static inline float grainWindow (float t01) noexcept
    {
        // Tukey-ish: smooth raised cosine on both ends
        // Symmetric, peak at t=0.5
        const float pi = juce::MathConstants<float>::pi;
        return 0.5f - 0.5f * std::cos (2.0f * pi * juce::jlimit (0.0f, 1.0f, t01));
    }

    void renderGrains (float* L, float* R, int n)
    {
        for (auto& g : grainPool)
        {
            if (! g.active) continue;

            int remaining = g.lifetimeSamples - g.ageSamples;
            int toRender  = juce::jmin (n, remaining);

            for (int i = 0; i < toRender; ++i)
            {
                const float t01 = (float) (g.ageSamples + i) / (float) g.lifetimeSamples;
                const float wnd = grainWindow (t01);
                const float s   = readSourceLinear (g.source, g.sourceLength, g.readPos);
                const float v   = s * wnd * g.levelGain * 0.55f;
                L[i] += v * g.panL;
                R[i] += v * g.panR;
                g.readPos += g.readInc;
            }
            g.ageSamples += toRender;
            if (g.ageSamples >= g.lifetimeSamples)
                g.active = false;
        }
    }

    //===== Sources ============================================================
    std::array<std::vector<float>, kNumSources> sourceBuffers;
    bool sourcesReady = false;
    double sourcesGenSampleRate = 0.0;

    void prepareSources (double sr)
    {
        if (sourcesReady && std::abs (sr - sourcesGenSampleRate) < 1.0) return;
        sourcesGenSampleRate = sr;
        const int N = (int) (sr * (double) kSourceLengthSec);
        for (int s = 0; s < kNumSources; ++s)
            sourceBuffers[(size_t) s].assign ((size_t) N, 0.0f);

        generateSourceVocal   (sourceBuffers[SourceVocal],   sr);
        generateSourceStrings (sourceBuffers[SourceStrings], sr);
        generateSourceChoir   (sourceBuffers[SourceChoir],   sr);
        generateSourceBell    (sourceBuffers[SourceBell],    sr);
        generateSourceGlass   (sourceBuffers[SourceGlass],   sr);
        generateSourceAir     (sourceBuffers[SourceAir],     sr);

        sourcesReady = true;
    }

    static void normalize (std::vector<float>& buf, float peak = 0.85f)
    {
        float mx = 0.0001f;
        for (auto v : buf) mx = juce::jmax (mx, std::abs (v));
        const float g = peak / mx;
        for (auto& v : buf) v *= g;
    }

    static void fadeEdges (std::vector<float>& buf, double sr, double seconds = 0.05)
    {
        const int n = (int) buf.size();
        const int f = juce::jmin (n / 4, (int) (sr * seconds));
        for (int i = 0; i < f; ++i)
        {
            const float t = (float) i / (float) f;
            buf[(size_t) i]         *= t;
            buf[(size_t) (n - 1 - i)] *= t;
        }
    }

    // Vocal "Aah": three formants (700Hz, 1220Hz, 2600Hz) with slight vibrato + glottal harmonic stack
    static void generateSourceVocal (std::vector<float>& buf, double sr)
    {
        const int n = (int) buf.size();
        const float f0 = 138.0f;       // ~C#3
        const float vibratoHz = 5.0f;
        const float vibratoCents = 12.0f;

        // Formant filters: simple second-order resonators (biquad bandpass approximations)
        struct FmtFilter
        {
            float b0=1.0f, a1=0.0f, a2=0.0f, gain=1.0f;
            float z1=0.0f, z2=0.0f;
            void set (float fc, float Q, double sr)
            {
                const float w0 = 2.0f * juce::MathConstants<float>::pi * fc / (float) sr;
                const float alpha = std::sin (w0) / (2.0f * Q);
                const float cs = std::cos (w0);
                // Bandpass (constant skirt)
                const float a0 = 1.0f + alpha;
                b0 = alpha / a0;
                a1 = (-2.0f * cs) / a0;
                a2 = (1.0f - alpha) / a0;
                gain = 1.0f;
            }
            float process (float x)
            {
                const float y = b0 * x - a1 * z1 - a2 * z2;
                z2 = z1;
                z1 = y;
                return y * gain;
            }
        };

        FmtFilter f1, f2, f3;
        f1.set (700.0f, 12.0f, sr);  f1.gain = 1.0f;
        f2.set (1220.0f, 14.0f, sr); f2.gain = 0.65f;
        f3.set (2600.0f, 18.0f, sr); f3.gain = 0.40f;

        // Source: slowly drifting band-limited saw (additive harmonics)
        const int harmonics = 22;
        std::vector<float> sawHarmonicAmp ((size_t) harmonics, 0.0f);
        for (int h = 1; h <= harmonics; ++h)
            sawHarmonicAmp[(size_t)(h - 1)] = 1.0f / (float) h;

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) sr;

            // Slow drift in pitch (humanize)
            const float drift = std::sin (t * 0.21f * juce::MathConstants<float>::twoPi) * 0.5f
                              + std::sin (t * 0.07f * juce::MathConstants<float>::twoPi) * 0.3f;

            const float vib = std::sin (t * vibratoHz * juce::MathConstants<float>::twoPi);
            const float currentF0 = f0 * std::pow (2.0f, (vib * vibratoCents + drift * 8.0f) / 1200.0f);

            // Build glottal-like wave via additive
            float src = 0.0f;
            for (int h = 1; h <= harmonics; ++h)
            {
                const float fH = currentF0 * (float) h;
                if (fH > (float) sr * 0.45f) break;
                const float ph = std::fmod (fH * t, 1.0f);
                src += std::sin (ph * juce::MathConstants<float>::twoPi) * sawHarmonicAmp[(size_t)(h - 1)];
            }
            src *= 0.18f;

            // Apply formants
            const float vowel = f1.process (src) + f2.process (src) + f3.process (src);

            // Tiny breathiness
            const float breath = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * 0.015f;

            buf[(size_t) i] = vowel + breath;
        }
        fadeEdges (buf, sr, 0.04);
        normalize (buf, 0.80f);
    }

    static void generateSourceStrings (std::vector<float>& buf, double sr)
    {
        const int n = (int) buf.size();
        // Layered detuned saws + LP tone shaping
        struct Saw { double phase = 0.0; double inc = 0.0;
            void setF (double f, double sr) { inc = f / sr; }
            float next() { float v = (float) (2.0 * phase - 1.0); phase += inc; if (phase >= 1.0) phase -= 1.0; return v; }
        };

        const float f0 = 110.0f;        // A2
        const int   layers = 10;
        std::vector<Saw> saws ((size_t) layers);
        std::vector<float> spread ((size_t) layers);
        for (int i = 0; i < layers; ++i)
        {
            const float cents = ((float) i / (float) (layers - 1) - 0.5f) * 32.0f;
            const float f = f0 * std::pow (2.0f, cents / 1200.0f);
            saws[(size_t) i].setF (f, sr);
            spread[(size_t) i] = juce::Random::getSystemRandom().nextFloat();
            saws[(size_t) i].phase = juce::Random::getSystemRandom().nextFloat();
        }

        // One-pole LP for warmth (cutoff ~ 2.5kHz)
        float lpZ = 0.0f;
        const float lpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 2400.0f / (float) sr);

        // Bowed amplitude shape (slow swell)
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) sr;
            const float bow = 0.7f + 0.3f * std::sin (t * 0.13f * juce::MathConstants<float>::twoPi);

            float v = 0.0f;
            for (int s = 0; s < layers; ++s)
                v += saws[(size_t) s].next();
            v /= (float) layers;

            // Soft saturation
            v = std::tanh (v * 1.4f) * 0.7f;

            lpZ += lpCoef * (v - lpZ);

            buf[(size_t) i] = lpZ * bow;
        }

        fadeEdges (buf, sr, 0.06);
        normalize (buf, 0.80f);
    }

    static void generateSourceChoir (std::vector<float>& buf, double sr)
    {
        const int n = (int) buf.size();
        // Multiple voices each with own vibrato + slight pitch wandering. Output is a soft mass.
        struct Vox
        {
            float baseHz = 220.0f;
            float vibHz  = 5.0f;
            float vibAmt = 8.0f;
            float wanderHz = 0.07f;
            float wanderAmt = 14.0f;
            double phase = 0.0;
            void seed()
            {
                phase = juce::Random::getSystemRandom().nextFloat();
                vibHz = 4.5f + juce::Random::getSystemRandom().nextFloat() * 1.5f;
                wanderHz = 0.05f + juce::Random::getSystemRandom().nextFloat() * 0.12f;
            }
        };

        const int numVox = 14;
        std::vector<Vox> voxes ((size_t) numVox);
        const float chordRatios[5] = { 1.00f, 1.122f, 1.498f, 1.682f, 2.0f }; // root, M3, P5, M6, oct
        for (int i = 0; i < numVox; ++i)
        {
            voxes[(size_t) i].baseHz = 138.0f * chordRatios[i % 5];
            voxes[(size_t) i].seed();
        }

        // A low-pass to take the edge off
        float lpZ = 0.0f;
        const float lpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 2200.0f / (float) sr);

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) sr;
            float sum = 0.0f;
            for (auto& v : voxes)
            {
                const float vib  = std::sin (t * v.vibHz * juce::MathConstants<float>::twoPi);
                const float wand = std::sin (t * v.wanderHz * juce::MathConstants<float>::twoPi);
                const float cents = vib * v.vibAmt + wand * v.wanderAmt;
                const float f = v.baseHz * std::pow (2.0f, cents / 1200.0f);
                v.phase += (double) f / sr;
                if (v.phase >= 1.0) v.phase -= 1.0;
                // Aah-ish formant flavor (sine + 2nd + 3rd weak)
                const float ph = (float) v.phase;
                const float s = std::sin (ph * juce::MathConstants<float>::twoPi)
                              + 0.45f * std::sin (ph * 2.0f * juce::MathConstants<float>::twoPi)
                              + 0.20f * std::sin (ph * 3.0f * juce::MathConstants<float>::twoPi);
                sum += s;
            }
            sum /= (float) numVox;

            // Slow swell
            const float swell = 0.7f + 0.3f * std::sin (t * 0.10f * juce::MathConstants<float>::twoPi);
            sum *= swell;

            // Tiny breath
            const float breath = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * 0.012f;
            sum += breath;

            lpZ += lpCoef * (sum - lpZ);
            buf[(size_t) i] = lpZ;
        }
        fadeEdges (buf, sr, 0.07);
        normalize (buf, 0.78f);
    }

    static void generateSourceBell (std::vector<float>& buf, double sr)
    {
        const int n = (int) buf.size();
        // Inharmonic partials with exponential decays. Re-strike every ~1.5 sec for cyclic behaviour.
        struct Partial { float ratio; float amp; float decay; float phase = 0.0f; };
        const std::vector<Partial> partials = {
            { 0.50f, 0.40f, 1.6f }, { 1.00f, 1.00f, 2.2f },
            { 2.00f, 0.55f, 1.8f }, { 2.76f, 0.45f, 1.2f },
            { 3.49f, 0.35f, 1.0f }, { 4.20f, 0.30f, 0.9f },
            { 5.43f, 0.25f, 0.8f }, { 6.79f, 0.20f, 0.7f },
            { 8.21f, 0.18f, 0.6f }, { 10.50f, 0.12f, 0.5f }
        };

        const float fundamental = 220.0f;
        // Two strikes with overlap
        const float strikeTimes[3] = { 0.05f, 1.40f, 2.85f };

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) sr;
            float v = 0.0f;
            for (float st : strikeTimes)
            {
                const float age = t - st;
                if (age < 0.0f) continue;
                for (const auto& p : partials)
                {
                    const float a = p.amp * std::exp (-age / p.decay);
                    const float f = fundamental * p.ratio;
                    v += std::sin (age * f * juce::MathConstants<float>::twoPi) * a;
                }
            }
            buf[(size_t) i] = v * 0.20f;
        }
        fadeEdges (buf, sr, 0.02);
        normalize (buf, 0.85f);
    }

    static void generateSourceGlass (std::vector<float>& buf, double sr)
    {
        const int n = (int) buf.size();
        // Lots of high partials (2-3kHz region) + slow phasing + filtered noise
        struct Partial { float fc; float amp; float modHz; float modAmt; double phase = 0.0; };
        std::vector<Partial> partials;
        for (int i = 0; i < 12; ++i)
        {
            Partial p;
            p.fc = 1200.0f + (float) i * 350.0f
                   + juce::Random::getSystemRandom().nextFloat() * 60.0f;
            p.amp = 1.0f / (1.0f + (float) i * 0.18f);
            p.modHz = 0.10f + juce::Random::getSystemRandom().nextFloat() * 0.30f;
            p.modAmt = 8.0f + juce::Random::getSystemRandom().nextFloat() * 14.0f;
            p.phase = juce::Random::getSystemRandom().nextFloat();
            partials.push_back (p);
        }

        // Highpassed filtered noise for shimmer
        float hp1 = 0.0f, hp2 = 0.0f;
        const float hpA = std::exp (-2.0f * juce::MathConstants<float>::pi * 1400.0f / (float) sr);
        // very narrow bandpass via two cascaded HP
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) sr;
            float v = 0.0f;
            for (auto& p : partials)
            {
                const float m = std::sin (t * p.modHz * juce::MathConstants<float>::twoPi);
                const float fHere = p.fc * std::pow (2.0f, m * p.modAmt / 1200.0f);
                p.phase += (double) fHere / sr;
                if (p.phase >= 1.0) p.phase -= 1.0;
                v += std::sin ((float) p.phase * juce::MathConstants<float>::twoPi) * p.amp;
            }
            v /= (float) partials.size();

            // Add a touch of high noise
            const float nz = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);
            const float hpIn = nz - hp1; hp1 = nz - hp1 * hpA + hp1 * hpA;
            // simple double HP
            const float hpOut = hpIn - hp2 + hp2 * hpA;
            hp2 = hpOut;

            v += hpOut * 0.10f;

            // Slow swell
            const float swell = 0.55f + 0.45f * std::sin (t * 0.07f * juce::MathConstants<float>::twoPi);
            buf[(size_t) i] = v * swell;
        }
        fadeEdges (buf, sr, 0.06);
        normalize (buf, 0.78f);
    }

    static void generateSourceAir (std::vector<float>& buf, double sr)
    {
        const int n = (int) buf.size();
        // Filtered, evolving noise + hi-pitched whistle harmonics
        float lpZ = 0.0f;
        float bpZ1 = 0.0f, bpZ2 = 0.0f;
        const float lpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 1800.0f / (float) sr);

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) sr;
            const float modCutoff = 1.0f + 0.5f * std::sin (t * 0.3f * juce::MathConstants<float>::twoPi);
            const float coef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                                  * 1800.0f * modCutoff / (float) sr);

            const float n1 = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);
            lpZ += coef * (n1 - lpZ);

            // BP-ish (LP minus delayed LP)
            bpZ1 += lpCoef * 0.3f * (lpZ - bpZ1);
            bpZ2 = lpZ - bpZ1;

            // High whistle layered in
            const float whistleF = 1800.0f + 600.0f * std::sin (t * 0.12f * juce::MathConstants<float>::twoPi);
            const float whistle  = std::sin (t * whistleF * juce::MathConstants<float>::twoPi) * 0.10f;

            const float swell = 0.5f + 0.5f * std::sin (t * 0.16f * juce::MathConstants<float>::twoPi);

            buf[(size_t) i] = (bpZ2 * 1.4f + whistle) * swell;
        }
        fadeEdges (buf, sr, 0.10);
        normalize (buf, 0.70f);
    }

    //===== Tone tilt (LF/HF tilt around 1kHz) =================================
    float toneLpZL = 0.0f, toneLpZR = 0.0f;
    void applyTone (float* L, float* R, int n, float tone)
    {
        // tone < 0.5 -> darker (more LP)
        // tone > 0.5 -> brighter (more HP)
        const float pivot = (tone - 0.5f) * 2.0f; // -1..+1
        const float lpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 1100.0f / (float) sampleRate);

        const float lpMix = juce::jmax (0.0f, -pivot);   // 0..1
        const float hpMix = juce::jmax (0.0f,  pivot);   // 0..1

        for (int i = 0; i < n; ++i)
        {
            toneLpZL += lpCoef * (L[i] - toneLpZL);
            toneLpZR += lpCoef * (R[i] - toneLpZR);
            const float hpL = L[i] - toneLpZL;
            const float hpR = R[i] - toneLpZR;
            L[i] = L[i] * (1.0f - lpMix - hpMix) + toneLpZL * lpMix + (toneLpZL + hpL * 1.4f) * hpMix;
            R[i] = R[i] * (1.0f - lpMix - hpMix) + toneLpZR * lpMix + (toneLpZR + hpR * 1.4f) * hpMix;
        }
    }

    static void applyWidth (float* L, float* R, int n, float widthAmt)
    {
        const float w = juce::jlimit (0.0f, 1.0f, widthAmt);
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            const float s = 0.5f * (L[i] - R[i]) * w;
            L[i] = m + s;
            R[i] = m - s;
        }
    }

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
            bufSize = (int) (sr * 0.05);
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
            while (pos < 0) pos += (float) sz;
            int   i0 = (int) pos;
            int   i1 = (i0 + 1) % sz;
            float frac = pos - (float) i0;
            return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
        }

        void process (float* L, float* R, int n)
        {
            const double inc = (double) rateHz / sr;
            const float baseDelay = (float) (sr * 0.018f);
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

    //===== Reverb section =====================================================
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

    //===== State =============================================================
    Parameters params;
    double sampleRate = 44100.0;
    juce::Random random;

    Chorus        chorus;
    ReverbSection reverb;

    juce::SmoothedValue<float> smoothMaster;
    float levelMeterL = 0.0f, levelMeterR = 0.0f;
    float activityLevel = 0.0f;
    double lfoPhase = 0.0;
};
