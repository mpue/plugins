/*
  ==============================================================================

    SamplerEngine.h
    SA-1 Luxury Quick Sampler — multi-pad drum sampling engine.

    Holds 16 drum pads, each with one stereo sample, AHR envelope, pitch,
    pan, gain, choke group and reverse playback. Polyphonic voice pool
    with cubic-Hermite interpolation provides clean playback regardless
    of pitch ratio.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace SA1
{
    static constexpr int kNumPads     = 16;
    static constexpr int kMaxVoices   = 32;
    static constexpr int kThumbPoints = 256;

    struct PadSlotInfo
    {
        const char* shortName;   // Displayed on pad
        const char* longName;    // Displayed in big view
        int         midiNote;    // GM drum mapping (key zero-velocity if not assigned)
        int         chokeGroup;  // 0 = none; pads in same group choke each other
        juce::uint32 colour;     // Accent ARGB tint for this pad slot
    };

    inline const PadSlotInfo& getDefaultSlot (int padIndex) noexcept
    {
        // 4x4 layout, GM-style drum mapping
        static const PadSlotInfo slots[kNumPads] =
        {
            // Row 1 — kicks / snares / claps
            { "KICK",   "KICK",          36, 0, 0xffff8a3a },
            { "SNARE",  "SNARE",         38, 0, 0xffff5a5a },
            { "CLAP",   "CLAP",          39, 0, 0xfff48fb1 },
            { "RIM",    "RIM SHOT",      37, 0, 0xfff8b878 },
            // Row 2 — hats / cymbals
            { "CH",     "CLOSED HAT",    42, 1, 0xff8be9fd },
            { "OH",     "OPEN HAT",      46, 1, 0xff64d2ff },
            { "RIDE",   "RIDE",          51, 0, 0xffe9c46a },
            { "CRASH",  "CRASH",         49, 0, 0xfff4c95f },
            // Row 3 — toms
            { "TOM L",  "LOW TOM",       43, 0, 0xff7ee787 },
            { "TOM M",  "MID TOM",       47, 0, 0xff9be19a },
            { "TOM H",  "HIGH TOM",      50, 0, 0xffb6e3b1 },
            { "COWBL",  "COWBELL",       56, 0, 0xfffabd50 },
            // Row 4 — perc / fx
            { "SHKR",   "SHAKER",        70, 0, 0xffd0a878 },
            { "TAMB",   "TAMBOURINE",    54, 0, 0xffc78a8f },
            { "PERC",   "PERC",          75, 0, 0xffa78bfa },
            { "FX",     "FX",            76, 0, 0xff7c9bff }
        };
        return slots[juce::jlimit (0, kNumPads - 1, padIndex)];
    }

    //==============================================================================
    /** All per-pad parameters that can be saved and recalled. */
    struct PadState
    {
        juce::String filePath;       // empty if not loaded
        juce::String displayName;    // sample's short name (without ext)

        float gainDb       = 0.0f;   // -60..+12
        float pan          = 0.0f;   // -1..+1
        float pitchSemis   = 0.0f;   // -24..+24
        float fineCents    = 0.0f;   // -100..+100
        float attackMs     = 1.0f;   // 0..500
        float releaseMs    = 200.0f; // 5..4000 (release of the AHR)
        float startPos     = 0.0f;   // 0..1 of file
        bool  reverse      = false;
        int   chokeGroup   = 0;      // overrides default if > 0; 0 = use slot default
        int   midiNote     = -1;     // -1 = use slot default

        void resetSample() noexcept
        {
            filePath = {};
            displayName = {};
        }
    };

    //==============================================================================
    /** A single stereo voice that plays one pad's sample with pitch + AHR. */
    class SamplerVoice
    {
    public:
        SamplerVoice() = default;

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            reset();
        }

        void reset() noexcept
        {
            active = false;
            envState = EnvState::Idle;
            envValue = 0.0f;
            position = 0.0;
            pitchRatio = 1.0;
            buffer = nullptr;
        }

        bool isActive() const noexcept { return active; }
        int  getPadIndex() const noexcept { return padIndex; }
        int  getChokeGroup() const noexcept { return chokeGroup; }

        void start (const juce::AudioBuffer<float>* sourceBuffer,
                    double sourceSampleRate,
                    const PadState& state,
                    int slotPadIndex,
                    int slotChokeGroup,
                    float velocity) noexcept
        {
            if (sourceBuffer == nullptr || sourceBuffer->getNumSamples() == 0)
                return;

            buffer       = sourceBuffer;
            padIndex     = slotPadIndex;
            chokeGroup   = state.chokeGroup > 0 ? state.chokeGroup : slotChokeGroup;
            reverse      = state.reverse;

            const float gainLin = juce::Decibels::decibelsToGain (state.gainDb, -90.0f);
            velScale = juce::jlimit (0.05f, 1.0f, velocity);
            const float v = velScale;
            voiceGain = gainLin * (0.35f + 0.65f * v);

            const float pan01 = juce::jlimit (-1.0f, 1.0f, state.pan);
            const float panAng = (pan01 + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            panL = std::cos (panAng);
            panR = std::sin (panAng);

            const double semis = (double) state.pitchSemis + (double) state.fineCents * 0.01;
            const double ratio = std::pow (2.0, semis / 12.0);
            pitchRatio = ratio * (sourceSampleRate / sampleRate);

            const int numSourceSamples = buffer->getNumSamples();
            const float startNorm = juce::jlimit (0.0f, 0.999f, state.startPos);
            const double startSample = startNorm * (double) numSourceSamples;

            position = reverse
                ? (double) numSourceSamples - 1.0
                : startSample;

            attackSamples  = juce::jmax (1.0,  state.attackMs  * 0.001 * sampleRate);
            releaseSamples = juce::jmax (1.0,  state.releaseMs * 0.001 * sampleRate);

            envState = EnvState::Attack;
            envValue = 0.0f;
            active   = true;
        }

        void noteOff() noexcept
        {
            if (envState != EnvState::Idle && envState != EnvState::Release)
                envState = EnvState::Release;
        }

        void choke() noexcept
        {
            // Fast tail-off (4 ms) so it sounds like a real choke
            const double quickRel = juce::jmax (32.0, 0.004 * sampleRate);
            releaseSamples = quickRel;
            envState = EnvState::Release;
        }

        /** Add this voice's contribution into the destination stereo block. */
        void renderAdd (float* destL, float* destR, int numSamples) noexcept
        {
            if (! active || buffer == nullptr) return;

            const int numCh   = buffer->getNumChannels();
            const int numSrc  = buffer->getNumSamples();
            const float* sL   = buffer->getReadPointer (0);
            const float* sR   = numCh > 1 ? buffer->getReadPointer (1) : sL;

            for (int n = 0; n < numSamples; ++n)
            {
                if (! active) break;

                // ---- envelope ----
                if (envState == EnvState::Attack)
                {
                    envValue += 1.0f / (float) attackSamples;
                    if (envValue >= 1.0f) { envValue = 1.0f; envState = EnvState::Hold; }
                }
                else if (envState == EnvState::Release)
                {
                    envValue -= 1.0f / (float) releaseSamples;
                    if (envValue <= 0.0f) { envValue = 0.0f; active = false; envState = EnvState::Idle; }
                }

                // ---- sample read with cubic Hermite ----
                float l = 0.0f, r = 0.0f;

                if (! reverse)
                {
                    if (position >= (double) (numSrc - 2))
                    {
                        envState = EnvState::Release; // gentle natural fade if envelope longer than file
                        if (position >= (double) (numSrc - 1)) { active = false; break; }
                    }
                }
                else
                {
                    if (position <= 1.0)
                    {
                        envState = EnvState::Release;
                        if (position <= 0.0) { active = false; break; }
                    }
                }

                interpolate (sL, sR, numSrc, l, r);

                const float env = envValue * envValue; // perceptually nicer
                const float g   = voiceGain * env;

                destL[n] += l * g * panL;
                destR[n] += r * g * panR;

                position += reverse ? -pitchRatio : pitchRatio;
            }
        }

    private:
        enum class EnvState { Idle, Attack, Hold, Release };

        void interpolate (const float* sL, const float* sR, int numSrc,
                          float& outL, float& outR) const noexcept
        {
            const int   i  = (int) position;
            const float t  = (float) (position - (double) i);

            auto fetch = [numSrc] (const float* s, int idx)
            {
                if (idx < 0 || idx >= numSrc) return 0.0f;
                return s[idx];
            };

            const float xm1L = fetch (sL, i - 1);
            const float x0L  = fetch (sL, i    );
            const float x1L  = fetch (sL, i + 1);
            const float x2L  = fetch (sL, i + 2);
            const float xm1R = fetch (sR, i - 1);
            const float x0R  = fetch (sR, i    );
            const float x1R  = fetch (sR, i + 1);
            const float x2R  = fetch (sR, i + 2);

            // 4-point, 3rd-order Hermite (Laurent de Soras)
            auto hermite = [] (float xm1, float x0, float x1, float x2, float t)
            {
                const float c = (x1 - xm1) * 0.5f;
                const float v = x0 - x1;
                const float w = c + v;
                const float a = w + v + (x2 - x0) * 0.5f;
                const float b = w + a;
                return ((a * t - b) * t + c) * t + x0;
            };

            outL = hermite (xm1L, x0L, x1L, x2L, t);
            outR = hermite (xm1R, x0R, x1R, x2R, t);
        }

        const juce::AudioBuffer<float>* buffer = nullptr;

        double sampleRate    = 44100.0;
        double position      = 0.0;
        double pitchRatio    = 1.0;
        double attackSamples  = 64.0;
        double releaseSamples = 4410.0;

        EnvState envState = EnvState::Idle;
        float envValue    = 0.0f;
        float voiceGain   = 1.0f;
        float velScale    = 1.0f;
        float panL = 0.7071f, panR = 0.7071f;

        int  padIndex   = -1;
        int  chokeGroup = 0;
        bool active     = false;
        bool reverse    = false;
    };

    //==============================================================================
    /** A single pad: one stereo buffer + parameter state + thumbnail peaks. */
    class Pad
    {
    public:
        Pad() = default;

        const juce::AudioBuffer<float>& getBuffer()    const noexcept { return buffer; }
        double                          getSourceSR() const noexcept { return sourceSampleRate; }
        bool                            hasSample()    const noexcept { return buffer.getNumSamples() > 0; }

        const std::array<float, kThumbPoints>& getThumbMin() const noexcept { return thumbMin; }
        const std::array<float, kThumbPoints>& getThumbMax() const noexcept { return thumbMax; }

        PadState&       getState()       noexcept { return state; }
        const PadState& getState() const noexcept { return state; }

        bool loadFromFile (const juce::File& file, juce::AudioFormatManager& fm)
        {
            if (! file.existsAsFile())
                return false;

            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
            if (reader == nullptr)
                return false;

            const int numCh = juce::jmin (2, (int) reader->numChannels);
            const int numSamples = (int) juce::jmin ((juce::int64) (10 * (juce::int64) reader->sampleRate),
                                                     reader->lengthInSamples);

            if (numSamples <= 0 || numCh <= 0)
                return false;

            juce::AudioBuffer<float> tmp (numCh, numSamples);
            if (! reader->read (&tmp, 0, numSamples, 0, true, numCh > 1))
                return false;

            // Promote to stereo if mono
            if (numCh == 1)
            {
                juce::AudioBuffer<float> stereoBuf (2, numSamples);
                stereoBuf.copyFrom (0, 0, tmp, 0, 0, numSamples);
                stereoBuf.copyFrom (1, 0, tmp, 0, 0, numSamples);
                buffer = std::move (stereoBuf);
            }
            else
            {
                buffer = std::move (tmp);
            }

            sourceSampleRate = reader->sampleRate;
            state.filePath    = file.getFullPathName();
            state.displayName = file.getFileNameWithoutExtension();

            buildThumbnail();
            return true;
        }

        void clearSample() noexcept
        {
            buffer.setSize (0, 0);
            sourceSampleRate = 44100.0;
            state.resetSample();
            thumbMin.fill (0.0f);
            thumbMax.fill (0.0f);
        }

    private:
        void buildThumbnail()
        {
            thumbMin.fill (0.0f);
            thumbMax.fill (0.0f);

            const int total = buffer.getNumSamples();
            if (total == 0) return;

            const int numCh = buffer.getNumChannels();
            const float blockSize = (float) total / (float) kThumbPoints;

            for (int p = 0; p < kThumbPoints; ++p)
            {
                const int start = juce::jlimit (0, total - 1, (int) (p * blockSize));
                const int end   = juce::jlimit (start + 1, total,
                                                (int) ((p + 1) * blockSize));
                float lo = 0.0f, hi = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* d = buffer.getReadPointer (ch);
                    for (int i = start; i < end; ++i)
                    {
                        const float v = d[i];
                        if (v < lo) lo = v;
                        if (v > hi) hi = v;
                    }
                }
                thumbMin[p] = lo;
                thumbMax[p] = hi;
            }
        }

        juce::AudioBuffer<float> buffer;
        double                   sourceSampleRate = 44100.0;
        PadState                 state;

        std::array<float, kThumbPoints> thumbMin {};
        std::array<float, kThumbPoints> thumbMax {};
    };

    //==============================================================================
    /** The drum sampling engine: 16 pads + voice pool + global output. */
    class SamplerEngine
    {
    public:
        SamplerEngine()
        {
            formatManager.registerBasicFormats();
        }

        juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

        void prepare (double newSampleRate, int /*samplesPerBlock*/)
        {
            sampleRate = newSampleRate;
            for (auto& v : voices) v.prepare (sampleRate);
        }

        void reset() noexcept
        {
            for (auto& v : voices) v.reset();
        }

        Pad&       getPad (int padIndex)       noexcept { return pads[(size_t) juce::jlimit (0, kNumPads - 1, padIndex)]; }
        const Pad& getPad (int padIndex) const noexcept { return pads[(size_t) juce::jlimit (0, kNumPads - 1, padIndex)]; }

        bool loadSampleIntoPad (int padIndex, const juce::File& file)
        {
            if (! juce::isPositiveAndBelow (padIndex, kNumPads)) return false;
            // Stop any voices using this pad before swapping the buffer
            killVoicesForPad (padIndex);
            return pads[(size_t) padIndex].loadFromFile (file, formatManager);
        }

        void clearPad (int padIndex)
        {
            if (! juce::isPositiveAndBelow (padIndex, kNumPads)) return;
            killVoicesForPad (padIndex);
            pads[(size_t) padIndex].clearSample();
        }

        int  midiNoteForPad (int padIndex) const noexcept
        {
            const auto& s = pads[(size_t) padIndex].getState();
            return s.midiNote >= 0 ? s.midiNote : getDefaultSlot (padIndex).midiNote;
        }

        int  padForMidiNote (int note) const noexcept
        {
            for (int i = 0; i < kNumPads; ++i)
                if (midiNoteForPad (i) == note) return i;
            return -1;
        }

        int  chokeGroupForPad (int padIndex) const noexcept
        {
            const auto& s = pads[(size_t) padIndex].getState();
            const int   d = getDefaultSlot (padIndex).chokeGroup;
            return s.chokeGroup > 0 ? s.chokeGroup : d;
        }

        /** Trigger from audio thread. velocity in [0, 1]. */
        void triggerPad (int padIndex, float velocity)
        {
            if (! juce::isPositiveAndBelow (padIndex, kNumPads)) return;
            auto& pad = pads[(size_t) padIndex];
            if (! pad.hasSample()) return;

            const int chokeG = chokeGroupForPad (padIndex);
            if (chokeG > 0)
            {
                for (auto& v : voices)
                    if (v.isActive() && v.getChokeGroup() == chokeG && v.getPadIndex() != padIndex)
                        v.choke();
            }

            // Find a free voice; if none, steal the quietest active voice
            SamplerVoice* target = nullptr;
            for (auto& v : voices)
                if (! v.isActive()) { target = &v; break; }

            if (target == nullptr)
                target = &voices.front();

            target->start (&pad.getBuffer(),
                           pad.getSourceSR(),
                           pad.getState(),
                           padIndex,
                           getDefaultSlot (padIndex).chokeGroup,
                           juce::jlimit (0.05f, 1.0f, velocity));

            lastPlayedPad.store (padIndex, std::memory_order_release);
            triggerCounter.fetch_add (1, std::memory_order_release);
        }

        void releasePad (int padIndex)
        {
            // For one-shot drum samples we generally let them ring; this is
            // here so that long sustained samples can be cut by note-off.
            for (auto& v : voices)
                if (v.isActive() && v.getPadIndex() == padIndex)
                    v.noteOff();
        }

        /** Render the global mix into the supplied stereo buffer. */
        void renderBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
        {
            if (output.getNumChannels() < 1 || numSamples <= 0) return;

            float* outL = output.getWritePointer (0, startSample);
            float* outR = output.getNumChannels() > 1
                            ? output.getWritePointer (1, startSample)
                            : outL;

            for (auto& v : voices)
                v.renderAdd (outL, outR, numSamples);

            // Master gain + soft limiter
            const float g = juce::Decibels::decibelsToGain (masterGainDb.load());
            for (int n = 0; n < numSamples; ++n)
            {
                float l = outL[n] * g;
                float r = outR[n] * g;
                // gentle soft clip — gives the engine its "luxury" character
                l = std::tanh (l * 0.85f) * 1.18f;
                r = std::tanh (r * 0.85f) * 1.18f;
                outL[n] = l;
                outR[n] = r;
            }
        }

        void  setMasterGainDb (float db) noexcept { masterGainDb.store (db); }
        float getMasterGainDb()    const noexcept { return masterGainDb.load(); }

        int   consumeTriggerEvent (int& padOut) noexcept
        {
            const int now    = triggerCounter.load (std::memory_order_acquire);
            const int delta  = now - lastSeen;
            lastSeen         = now;
            padOut           = lastPlayedPad.load (std::memory_order_relaxed);
            return delta;
        }

    private:
        void killVoicesForPad (int padIndex) noexcept
        {
            for (auto& v : voices)
                if (v.isActive() && v.getPadIndex() == padIndex)
                    v.reset();
        }

        std::array<Pad, kNumPads> pads;
        std::array<SamplerVoice, kMaxVoices> voices;

        juce::AudioFormatManager formatManager;
        double sampleRate = 44100.0;
        std::atomic<float> masterGainDb { 0.0f };

        std::atomic<int> triggerCounter { 0 };
        std::atomic<int> lastPlayedPad  { -1 };
        int              lastSeen       = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEngine)
    };
}
