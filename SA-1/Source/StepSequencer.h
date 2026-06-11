/*
  ==============================================================================

    StepSequencer.h
    SA-1 Luxury Quick Sampler — host-synced step sequencer.

    A 16-track (one per pad) x up-to-16-step grid drum sequencer. When the
    sampler is switched into "sequencer mode", incoming MIDI notes no longer
    trigger pads directly — instead they trigger / gate the sequence, which
    then plays back locked to the host clock.

    Two sync flavours:
      * HostLock   — step boundaries are locked to the host PPQ grid, so the
                     pattern stays bar-aligned to the host transport. Holding
                     a note opens the gate; releasing closes it.
      * NoteTrigger — a note restarts the pattern from step 0 and it advances
                     at the host tempo (works even when the transport is
                     stopped). Great for phrase / fill triggering.

    Two hold flavours:
      * Gate  — the sequence runs while at least one note is held.
      * Latch — a note toggles the sequence on / off.

    Step firing is sample-accurate: the sequencer reports (offset, pad,
    velocity) fire events that the processor interleaves with its sub-block
    rendering, exactly like live MIDI note-ons.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SamplerEngine.h"

namespace SA1
{
    static constexpr int kSeqMaxSteps = 16;

    enum class SeqSync { HostLock = 0, NoteTrigger = 1 };
    enum class SeqHold { Gate = 0, Latch = 1 };

    //==============================================================================
    /** A selectable step rate, expressed as the length of one step in quarter notes. */
    struct SeqRate { const char* name; double stepLenQ; };

    inline int numSeqRates() noexcept { return 6; }

    inline const SeqRate& seqRateOption (int i) noexcept
    {
        static const SeqRate rates[] =
        {
            { "1/4",    1.0        },
            { "1/8",    0.5        },
            { "1/8T",   1.0 / 3.0  },
            { "1/16",   0.25       },
            { "1/16T",  1.0 / 6.0  },
            { "1/32",   0.125      }
        };
        return rates[juce::jlimit (0, numSeqRates() - 1, i)];
    }

    //==============================================================================
    /** One cell of the pattern. */
    struct SeqStep
    {
        bool  on  = false;
        float vel = 0.8f;
    };

    /** Host transport snapshot needed to lock the sequencer to the clock. */
    struct SeqHostInfo
    {
        bool   isPlaying       = false;
        double bpm             = 120.0;
        double ppqAtBlockStart = 0.0;
    };

    /** A note on/off relevant to the sequencer, at a sample offset within the block. */
    struct SeqNoteEvent
    {
        int  offset = 0;
        bool isOn   = false;
    };

    //==============================================================================
    class StepSequencer
    {
    public:
        StepSequencer() = default;

        //--------------------------------------------------------------------
        // Lifecycle

        void prepare (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            resetPlayback();
        }

        /** Reset only the playback position / gate — keeps the pattern. */
        void resetPlayback() noexcept
        {
            running        = false;
            heldNotes      = 0;
            phaseQ         = 0.0;
            nextGlobalStep = 0;
            currentStep.store (-1, std::memory_order_relaxed);
            runningUI  .store (false, std::memory_order_relaxed);
        }

        //--------------------------------------------------------------------
        // Mode / parameters (written by the UI thread, read on the audio thread)

        void setEnabled (bool e) noexcept { enabled.store (e, std::memory_order_release); }
        bool isEnabled() const noexcept   { return enabled.load (std::memory_order_acquire); }

        void    setSyncMode (SeqSync m) noexcept { syncMode.store ((int) m); }
        SeqSync getSyncMode() const noexcept     { return (SeqSync) syncMode.load(); }

        void    setHoldMode (SeqHold m) noexcept { holdMode.store ((int) m); }
        SeqHold getHoldMode() const noexcept     { return (SeqHold) holdMode.load(); }

        void setRateIndex (int i) noexcept { rateIndex.store (juce::jlimit (0, numSeqRates() - 1, i)); }
        int  getRateIndex() const noexcept { return rateIndex.load(); }

        void setNumSteps (int n) noexcept { numSteps.store (juce::jlimit (1, kSeqMaxSteps, n)); }
        int  getNumSteps() const noexcept { return numSteps.load(); }

        void  setSwing (float s) noexcept { swing.store (juce::jlimit (0.0f, 0.7f, s)); }
        float getSwing() const noexcept   { return swing.load(); }

        //--------------------------------------------------------------------
        // Pattern editing (UI thread). Plain reads/writes — benign races with
        // the audio thread, matching the house style used for PadState.

        bool getStep (int pad, int step) const noexcept
        {
            if (! valid (pad, step)) return false;
            return pattern[(size_t) pad][(size_t) step].on;
        }

        void setStep (int pad, int step, bool on) noexcept
        {
            if (! valid (pad, step)) return;
            pattern[(size_t) pad][(size_t) step].on = on;
        }

        void toggleStep (int pad, int step) noexcept
        {
            if (! valid (pad, step)) return;
            auto& s = pattern[(size_t) pad][(size_t) step];
            s.on = ! s.on;
        }

        float getStepVel (int pad, int step) const noexcept
        {
            if (! valid (pad, step)) return 0.0f;
            return pattern[(size_t) pad][(size_t) step].vel;
        }

        void setStepVel (int pad, int step, float vel) noexcept
        {
            if (! valid (pad, step)) return;
            pattern[(size_t) pad][(size_t) step].vel = juce::jlimit (0.05f, 1.0f, vel);
        }

        void clearPad (int pad) noexcept
        {
            if (! juce::isPositiveAndBelow (pad, kNumPads)) return;
            for (auto& s : pattern[(size_t) pad]) s = SeqStep{};
        }

        void clearAll() noexcept
        {
            for (auto& row : pattern)
                for (auto& s : row) s = SeqStep{};
        }

        /** Quick generative helper: sprinkle hits onto the loaded pads. */
        void randomize (juce::Random& rng, float density = 0.35f) noexcept
        {
            const int nSteps = numSteps.load();
            for (int p = 0; p < kNumPads; ++p)
                for (int s = 0; s < nSteps; ++s)
                {
                    const bool hit = rng.nextFloat() < density;
                    pattern[(size_t) p][(size_t) s].on  = hit;
                    pattern[(size_t) p][(size_t) s].vel = hit ? 0.6f + 0.4f * rng.nextFloat() : 0.8f;
                }
        }

        //--------------------------------------------------------------------
        // UI feedback (audio thread writes, UI reads)

        int  getCurrentStep() const noexcept { return currentStep.load (std::memory_order_relaxed); }
        bool isRunning()      const noexcept { return runningUI .load (std::memory_order_relaxed); }

        //--------------------------------------------------------------------
        /** Advance the sequencer over one audio block.

            @param numSamples  block length
            @param host        host transport snapshot
            @param notes       sequencer-relevant note events, sorted by offset
            @param numNotes    number of note events
            @param fire        callback(int sampleOffset, int padIndex, float velocity)
        */
        template <typename FireFn>
        void renderBlock (int numSamples, const SeqHostInfo& host,
                          const SeqNoteEvent* notes, int numNotes,
                          FireFn&& fire)
        {
            if (! enabled.load (std::memory_order_acquire))
            {
                if (running) resetPlayback();
                return;
            }

            const int    nSteps      = numSteps.load();
            const double stepLenQ    = seqRateOption (rateIndex.load()).stepLenQ;
            const double bpm         = host.bpm > 1.0 ? host.bpm : 120.0;
            const double ppqPerSamp  = (bpm / 60.0) / sampleRate;
            const SeqSync sync       = (SeqSync) syncMode.load();
            const SeqHold hold       = (SeqHold) holdMode.load();
            const float   sw         = swing.load();
            const bool    hostLock   = (sync == SeqSync::HostLock) && host.isPlaying;

            // Re-sync the step counter after a host transport relocate / loop.
            if (hostLock && running)
            {
                const long long expected = (long long) std::floor (host.ppqAtBlockStart / stepLenQ);
                if (expected < nextGlobalStep - 1 || expected > nextGlobalStep + nSteps)
                    nextGlobalStep = expected;
            }

            int noteIdx = 0;
            for (int n = 0; n < numSamples; ++n)
            {
                // ---- apply note events landing on this sample ----
                while (noteIdx < numNotes && notes[noteIdx].offset <= n)
                {
                    const bool isOn       = notes[noteIdx].isOn;
                    const bool wasRunning = running;

                    if (hold == SeqHold::Gate)
                    {
                        if (isOn)               ++heldNotes;
                        else if (heldNotes > 0) --heldNotes;
                        running = heldNotes > 0;
                    }
                    else // Latch
                    {
                        if (isOn) running = ! running;
                    }

                    if (running && ! wasRunning)
                        openGate (n, ppqPerSamp, host, hostLock, stepLenQ);
                    else if (! running && wasRunning)
                        currentStep.store (-1, std::memory_order_relaxed);

                    ++noteIdx;
                }

                if (! running)
                    continue;

                // ---- current musical phase, in quarter notes ----
                double phase;
                if (hostLock)
                {
                    phase = host.ppqAtBlockStart + (double) n * ppqPerSamp;
                }
                else
                {
                    phase   = phaseQ;
                    phaseQ += ppqPerSamp;
                }

                // ---- fire every step boundary we've reached ----
                int guard = 0;
                while (scheduledPhaseQ (nextGlobalStep, stepLenQ, sw) <= phase && guard++ < kSeqMaxSteps)
                {
                    const int pat = (int) (((nextGlobalStep % nSteps) + nSteps) % nSteps);
                    for (int p = 0; p < kNumPads; ++p)
                    {
                        const auto& cell = pattern[(size_t) p][(size_t) pat];
                        if (cell.on)
                            fire (n, p, cell.vel);
                    }
                    currentStep.store (pat, std::memory_order_relaxed);
                    ++nextGlobalStep;
                }
            }

            runningUI.store (running, std::memory_order_relaxed);
        }

    private:
        static bool valid (int pad, int step) noexcept
        {
            return juce::isPositiveAndBelow (pad, kNumPads)
                && juce::isPositiveAndBelow (step, kSeqMaxSteps);
        }

        /** Scheduled musical position of a step boundary, including swing. */
        static double scheduledPhaseQ (long long g, double stepLenQ, float swing) noexcept
        {
            double base = (double) g * stepLenQ;
            if (swing > 0.0f)
            {
                const long long inPair = ((g % 2) + 2) % 2;   // 0 = on-beat, 1 = off-beat
                if (inPair == 1)
                    base += (double) swing * stepLenQ;        // delay the off-beat step
            }
            return base;
        }

        void openGate (int n, double ppqPerSamp, const SeqHostInfo& host,
                       bool hostLock, double stepLenQ) noexcept
        {
            if (hostLock)
            {
                // Latch onto the host grid: fire the step we're currently inside.
                const double phase = host.ppqAtBlockStart + (double) n * ppqPerSamp;
                nextGlobalStep = (long long) std::floor (phase / stepLenQ);
            }
            else
            {
                // Restart the pattern from step 0 at the host tempo.
                phaseQ         = 0.0;
                nextGlobalStep = 0;
            }
        }

        // ---- pattern data (plain; benign cross-thread races) ----
        std::array<std::array<SeqStep, kSeqMaxSteps>, kNumPads> pattern {};

        // ---- parameters ----
        std::atomic<bool> enabled  { false };
        std::atomic<int>  syncMode { (int) SeqSync::HostLock };
        std::atomic<int>  holdMode { (int) SeqHold::Gate };
        std::atomic<int>  rateIndex{ 3 };          // 1/16 by default
        std::atomic<int>  numSteps { 16 };
        std::atomic<float> swing   { 0.0f };

        // ---- runtime (audio thread) ----
        double    sampleRate     = 44100.0;
        bool      running        = false;
        int       heldNotes      = 0;
        double    phaseQ         = 0.0;            // free-running accumulator (note-trigger / stopped)
        long long nextGlobalStep = 0;

        // ---- UI feedback ----
        std::atomic<int>  currentStep { -1 };
        std::atomic<bool> runningUI   { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencer)
    };
}
