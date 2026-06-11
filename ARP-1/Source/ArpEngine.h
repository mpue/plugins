/*
  ==============================================================================

    ArpEngine.h
    ARP-1 Luxury Arpeggiator — host-synced MIDI arpeggiator engine.

    Incoming MIDI chords are captured as a "held set". The engine resolves that
    set into a melodic note order (direction + octave stacking) and clocks it
    out as sample-accurate MIDI note-on / note-off events, shaped by an editable
    step pattern (rest / velocity / ratchet per step), gate length and swing.

    Design mirrors the house style used elsewhere in the repo (e.g. SA-1's
    StepSequencer): real-time-safe fixed arrays, atomics for parameters, plain
    reads/writes for pattern cells (benign cross-thread races), and a small
    snapshot published under a try-lock for the UI to visualise.

    The clock is free-running and locked to the host tempo (so the arp plays
    whenever notes are held, regardless of whether the transport is rolling).
    Step length follows the host BPM; a manual BPM is used as a fallback when
    the host reports none.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

namespace ARP1
{
    static constexpr int kMaxSteps  = 16;   // pattern length cap
    static constexpr int kMaxHeld   = 32;   // simultaneously held input notes
    static constexpr int kMaxOctave = 4;    // octave stack cap
    static constexpr int kMaxOrder  = kMaxHeld * kMaxOctave;
    static constexpr int kMaxVoices = 256;  // scheduled output notes in flight

    //==============================================================================
    enum class Direction
    {
        Up = 0, Down, UpDown, DownUp, Converge, Diverge, AsPlayed, Random, Chord,
        NumDirections
    };

    inline const char* directionName (int i) noexcept
    {
        static const char* names[] =
        {
            "Up", "Down", "Up / Down", "Down / Up", "Converge",
            "Diverge", "As Played", "Random", "Chord"
        };
        return names[juce::jlimit (0, (int) Direction::NumDirections - 1, i)];
    }

    //==============================================================================
    /** A selectable rate, expressed as the length of one step in quarter notes. */
    struct ArpRate { const char* name; double stepLenQ; };

    inline int numRates() noexcept { return 9; }

    inline const ArpRate& rateOption (int i) noexcept
    {
        static const ArpRate rates[] =
        {
            { "1/2",   2.0       },
            { "1/4",   1.0       },
            { "1/4T",  2.0 / 3.0 },
            { "1/8.",  0.75      },
            { "1/8",   0.5       },
            { "1/8T",  1.0 / 3.0 },
            { "1/16",  0.25      },
            { "1/16T", 1.0 / 6.0 },
            { "1/32",  0.125     }
        };
        return rates[juce::jlimit (0, numRates() - 1, i)];
    }

    //==============================================================================
    /** One step of the rhythmic pattern overlaid on the note order. */
    struct ArpStep
    {
        bool  on      = true;
        float vel     = 0.80f;   // 0..1, multiplies the source note velocity
        int   ratchet = 1;       // 1..4 sub-hits within the step
    };

    /** Host transport snapshot. */
    struct ArpHostInfo
    {
        bool   isPlaying = false;
        double bpm       = 120.0;
    };

    /** Lightweight copy of engine state for the UI to draw the arpeggio. */
    struct ArpSnapshot
    {
        int  numHeld       = 0;
        std::array<int, kMaxHeld>  held {};      // base chord, ascending
        int  numOrder      = 0;
        std::array<int, kMaxOrder> order {};     // resolved playback sequence
        int  currentStep   = -1;                 // index into the pattern
        int  currentOrderPos = -1;               // index into order (-1 = none/all)
        bool running       = false;
        int  lowNote       = 60;                 // pitch range actually in play
        int  highNote      = 72;
    };

    //==============================================================================
    class ArpEngine
    {
    public:
        ArpEngine()
        {
            for (auto& s : pattern) s = ArpStep{};
        }

        //--------------------------------------------------------------------
        // Lifecycle

        void prepare (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            reset();
        }

        void reset() noexcept
        {
            numHeld = numPhysical = 0;
            numActive = numPending = 0;
            running = false;
            sustainOn = false;
            phaseQ = 0.0;
            stepCounter = 0;
            absSample = 0;
            seqCounter = 0;
            orderLen = 0;
            currentStep.store (-1, std::memory_order_relaxed);
            currentOrderPos.store (-1, std::memory_order_relaxed);
            runningUI.store (false, std::memory_order_relaxed);
        }

        //--------------------------------------------------------------------
        // Parameters (UI thread writes, audio thread reads)

        void setEnabled (bool e) noexcept { enabled.store (e, std::memory_order_release); }
        bool isEnabled() const noexcept   { return enabled.load (std::memory_order_acquire); }

        void setRateIndex (int i) noexcept { rateIndex.store (juce::jlimit (0, numRates() - 1, i)); }
        int  getRateIndex() const noexcept { return rateIndex.load(); }

        void setOctaves (int n) noexcept { octaves.store (juce::jlimit (1, kMaxOctave, n)); }
        int  getOctaves() const noexcept { return octaves.load(); }

        void setDirection (int d) noexcept { direction.store (juce::jlimit (0, (int) Direction::NumDirections - 1, d)); }
        int  getDirection() const noexcept { return direction.load(); }

        void  setGate (float g) noexcept { gate.store (juce::jlimit (0.05f, 1.0f, g)); }
        float getGate() const noexcept   { return gate.load(); }

        void  setSwing (float s) noexcept { swing.store (juce::jlimit (0.0f, 0.7f, s)); }
        float getSwing() const noexcept   { return swing.load(); }

        void setNumSteps (int n) noexcept { numSteps.store (juce::jlimit (1, kMaxSteps, n)); }
        int  getNumSteps() const noexcept { return numSteps.load(); }

        void setHold (bool h) noexcept { hold.store (h, std::memory_order_release); }
        bool getHold() const noexcept  { return hold.load (std::memory_order_acquire); }

        void   setManualBpm (double b) noexcept { manualBpm.store (juce::jlimit (20.0, 300.0, b)); }
        double getManualBpm() const noexcept    { return manualBpm.load(); }

        //--------------------------------------------------------------------
        // Pattern editing (UI thread, plain reads/writes — benign races)

        const ArpStep& getStep (int i) const noexcept
        {
            static ArpStep dummy;
            if (! juce::isPositiveAndBelow (i, kMaxSteps)) return dummy;
            return pattern[(size_t) i];
        }

        void setStepOn (int i, bool on) noexcept
        {
            if (juce::isPositiveAndBelow (i, kMaxSteps)) pattern[(size_t) i].on = on;
        }

        void setStepVel (int i, float v) noexcept
        {
            if (juce::isPositiveAndBelow (i, kMaxSteps))
                pattern[(size_t) i].vel = juce::jlimit (0.05f, 1.0f, v);
        }

        void setStepRatchet (int i, int r) noexcept
        {
            if (juce::isPositiveAndBelow (i, kMaxSteps))
                pattern[(size_t) i].ratchet = juce::jlimit (1, 4, r);
        }

        void clearPattern() noexcept
        {
            for (auto& s : pattern) s = ArpStep{};
        }

        void resetPatternToDefault() noexcept
        {
            for (auto& s : pattern) s = ArpStep{};
            setNumSteps (8);
        }

        /** Sprinkle rests / accents for instant variation. */
        void randomizePattern (juce::Random& rng) noexcept
        {
            const int n = numSteps.load();
            for (int i = 0; i < kMaxSteps; ++i)
            {
                if (i < n)
                {
                    pattern[(size_t) i].on      = rng.nextFloat() < 0.80f;
                    pattern[(size_t) i].vel     = 0.45f + 0.55f * rng.nextFloat();
                    pattern[(size_t) i].ratchet = rng.nextFloat() < 0.15f ? 2 : 1;
                }
            }
        }

        /** Generate a Euclidean rhythm spread evenly across the steps. */
        void setEuclid (int pulses, int steps) noexcept
        {
            steps  = juce::jlimit (1, kMaxSteps, steps);
            pulses = juce::jlimit (0, steps, pulses);
            setNumSteps (steps);

            int bucket = 0;
            for (int i = 0; i < steps; ++i)
            {
                bucket += pulses;
                const bool hit = bucket >= steps;
                if (hit) bucket -= steps;
                pattern[(size_t) i].on  = hit;
                pattern[(size_t) i].vel = hit ? 0.85f : pattern[(size_t) i].vel;
            }
        }

        //--------------------------------------------------------------------
        // UI feedback

        bool getSnapshot (ArpSnapshot& dst) const noexcept
        {
            const juce::SpinLock::ScopedTryLockType l (snapLock);
            if (! l.isLocked()) return false;
            dst = snap;
            return true;
        }

        bool isRunning() const noexcept { return runningUI.load (std::memory_order_relaxed); }

        //--------------------------------------------------------------------
        /** Process one audio block: read input MIDI, write arpeggiated output. */
        void renderBlock (int numSamples, const ArpHostInfo& host,
                          const juce::MidiBuffer& midiIn, juce::MidiBuffer& midiOut)
        {
            midiOut.clear();

            const bool active = enabled.load (std::memory_order_acquire);

            // ---- Gather input note events; forward everything else verbatim ----
            int numIn = 0;
            for (const auto meta : midiIn)
            {
                const auto m   = meta.getMessage();
                const int  off = juce::jlimit (0, numSamples - 1, meta.samplePosition);

                if (m.isNoteOn())
                {
                    if (numIn < (int) inEvents.size())
                        inEvents[(size_t) numIn++] = { off, EvType::NoteOn,
                                                       m.getNoteNumber(),
                                                       juce::jlimit (0.05f, 1.0f, (float) m.getFloatVelocity()) };
                }
                else if (m.isNoteOff())
                {
                    if (numIn < (int) inEvents.size())
                        inEvents[(size_t) numIn++] = { off, EvType::NoteOff, m.getNoteNumber(), 0.0f };
                }
                else if (m.isSustainPedalOn() || m.isSustainPedalOff())
                {
                    if (numIn < (int) inEvents.size())
                        inEvents[(size_t) numIn++] = { off, m.isSustainPedalOn() ? EvType::SustainOn
                                                                                 : EvType::SustainOff, 0, 0.0f };
                    midiOut.addEvent (m, off);   // pass the pedal through too
                }
                else
                {
                    midiOut.addEvent (m, off);   // CC, pitch-bend, aftertouch …
                }
            }

            // Bypassed: pass notes straight through, flush any arp tails.
            if (! active)
            {
                if (running || numActive > 0)
                    flushAllNotes (midiOut, 0);
                reset();

                for (int i = 0; i < numIn; ++i)
                {
                    const auto& e = inEvents[(size_t) i];
                    if (e.type == EvType::NoteOn)
                        midiOut.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) juce::jlimit (1, 127, (int) std::round (e.vel * 127.0f))), e.offset);
                    else if (e.type == EvType::NoteOff)
                        midiOut.addEvent (juce::MidiMessage::noteOff (1, e.note), e.offset);
                }
                publishSnapshot();
                return;
            }

            // Hold/sustain just turned off: release any latched notes that are
            // no longer physically held, so the arp doesn't run forever.
            if (! effectiveHold() && numHeld > 0)
            {
                const int before = numHeld;
                pruneToPhysical();
                if (numHeld != before)
                {
                    const bool wasRunning = running;
                    rebuildOrder();
                    running = numHeld > 0;
                    if (! running && wasRunning)
                    {
                        flushAllNotes (midiOut, 0);
                        currentStep.store (-1, std::memory_order_relaxed);
                        currentOrderPos.store (-1, std::memory_order_relaxed);
                    }
                }
            }

            // ---- Cache parameters for the block ----
            const double bpm        = host.bpm > 1.0 ? host.bpm : manualBpm.load();
            const double ppqPerSamp = (bpm / 60.0) / sampleRate;
            const double stepLenQ   = rateOption (rateIndex.load()).stepLenQ;
            const double stepSamps  = juce::jmax (1.0, stepLenQ * (60.0 / bpm) * sampleRate);
            const int    nSteps     = numSteps.load();
            const float  sw         = swing.load();
            const float  gateFrac   = gate.load();
            const int    dir        = direction.load();

            int evIdx = 0;
            for (int n = 0; n < numSamples; ++n)
            {
                // ---- apply input events on this sample ----
                while (evIdx < numIn && inEvents[(size_t) evIdx].offset <= n)
                {
                    handleInputEvent (inEvents[(size_t) evIdx], n, midiOut);
                    ++evIdx;
                }

                // ---- release scheduled note-offs that have come due ----
                for (int i = 0; i < numActive; )
                {
                    if (absSample >= activeNotes[(size_t) i].offAbs)
                    {
                        midiOut.addEvent (juce::MidiMessage::noteOff (1, activeNotes[(size_t) i].note), n);
                        activeNotes[(size_t) i] = activeNotes[(size_t) --numActive];
                    }
                    else ++i;
                }

                // ---- fire pending ratchet sub-hits that have come due ----
                for (int i = 0; i < numPending; )
                {
                    if (absSample >= pendingNotes[(size_t) i].onAbs)
                    {
                        emitNoteOn (pendingNotes[(size_t) i].note,
                                    pendingNotes[(size_t) i].vel,
                                    n, pendingNotes[(size_t) i].gateSamps, midiOut);
                        pendingNotes[(size_t) i] = pendingNotes[(size_t) --numPending];
                    }
                    else ++i;
                }

                // ---- advance the clock and fire step boundaries ----
                if (running && orderLen > 0)
                {
                    const double phase = phaseQ;
                    int guard = 0;
                    while (scheduledPhaseQ (stepCounter, stepLenQ, sw) <= phase && guard++ < kMaxSteps + 1)
                    {
                        fireStep (stepCounter, nSteps, dir, gateFrac, stepSamps, n, midiOut);
                        ++stepCounter;
                    }
                    phaseQ += ppqPerSamp;
                }

                ++absSample;
            }

            publishSnapshot();
        }

    private:
        //--------------------------------------------------------------------
        enum class EvType { NoteOn, NoteOff, SustainOn, SustainOff };
        struct InEvent { int offset; EvType type; int note; float vel; };

        struct HeldNote { int note; float vel; long long seq; bool physical; };
        struct ActiveNote  { int note; long long offAbs; };
        struct PendingNote { int note; int vel; long long onAbs; int gateSamps; };

        //--------------------------------------------------------------------
        void handleInputEvent (const InEvent& e, int offset, juce::MidiBuffer& midiOut)
        {
            switch (e.type)
            {
                case EvType::NoteOn:    noteOn  (e.note, e.vel); break;
                case EvType::NoteOff:   noteOff (e.note);        break;
                case EvType::SustainOn:  sustainOn = true;  break;
                case EvType::SustainOff: sustainOn = false; pruneToPhysical(); break;
            }

            const bool wasRunning = running;
            rebuildOrder();
            running = numHeld > 0;

            if (running && ! wasRunning)
            {
                // Gate opens: restart the pattern from the top, fire immediately.
                phaseQ      = 0.0;
                stepCounter = 0;
            }
            else if (! running && wasRunning)
            {
                flushAllNotes (midiOut, offset);
                currentStep.store (-1, std::memory_order_relaxed);
                currentOrderPos.store (-1, std::memory_order_relaxed);
            }
        }

        bool effectiveHold() const noexcept { return hold.load (std::memory_order_acquire) || sustainOn; }

        void noteOn (int note, float vel)
        {
            // A fresh press after everything was released starts a new chord
            // when holding/latching, instead of stacking forever.
            if (effectiveHold() && numPhysical == 0)
                numHeld = 0;

            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i].note == note)
                {
                    held[(size_t) i].vel      = vel;
                    held[(size_t) i].physical = true;
                    ++numPhysical;
                    return;
                }

            if (numHeld < kMaxHeld)
                held[(size_t) numHeld++] = { note, vel, seqCounter++, true };
            ++numPhysical;
        }

        void noteOff (int note)
        {
            for (int i = 0; i < numHeld; ++i)
            {
                if (held[(size_t) i].note == note)
                {
                    if (held[(size_t) i].physical)
                    {
                        held[(size_t) i].physical = false;
                        if (numPhysical > 0) --numPhysical;
                    }
                    if (! effectiveHold())
                        held[(size_t) i] = held[(size_t) --numHeld];
                    return;
                }
            }
        }

        /** When sustain releases, drop any notes no longer physically held. */
        void pruneToPhysical()
        {
            if (hold.load (std::memory_order_acquire)) return;   // still latching
            for (int i = 0; i < numHeld; )
            {
                if (! held[(size_t) i].physical)
                    held[(size_t) i] = held[(size_t) --numHeld];
                else ++i;
            }
        }

        //--------------------------------------------------------------------
        /** Resolve the held set into the melodic playback order. */
        void rebuildOrder()
        {
            const int oct = octaves.load();
            const int dir = direction.load();

            // base notes sorted ascending (with velocities)
            std::array<int, kMaxHeld>   baseNote {};
            std::array<float, kMaxHeld> baseVel  {};
            int nb = 0;
            for (int i = 0; i < numHeld; ++i)
            {
                int n = held[(size_t) i].note;
                float v = held[(size_t) i].vel;
                int j = nb - 1;
                while (j >= 0 && baseNote[(size_t) j] > n)
                {
                    baseNote[(size_t) (j + 1)] = baseNote[(size_t) j];
                    baseVel [(size_t) (j + 1)] = baseVel [(size_t) j];
                    --j;
                }
                baseNote[(size_t) (j + 1)] = n;
                baseVel [(size_t) (j + 1)] = v;
                ++nb;
            }

            // expand octaves (ascending) into E
            std::array<int, kMaxOrder>   eNote {};
            std::array<float, kMaxOrder> eVel  {};
            int eN = 0;

            if (dir == (int) Direction::AsPlayed)
            {
                // play order within each octave, octaves stacked upward
                std::array<int, kMaxHeld>   pNote {};
                std::array<float, kMaxHeld> pVel  {};
                int np = 0;
                for (int i = 0; i < numHeld; ++i)   // already in press order
                {
                    pNote[(size_t) np] = held[(size_t) i].note;
                    pVel [(size_t) np] = held[(size_t) i].vel;
                    ++np;
                }
                for (int o = 0; o < oct; ++o)
                    for (int i = 0; i < np && eN < kMaxOrder; ++i)
                    {
                        eNote[(size_t) eN] = pNote[(size_t) i] + 12 * o;
                        eVel [(size_t) eN] = pVel [(size_t) i];
                        ++eN;
                    }
            }
            else
            {
                for (int o = 0; o < oct; ++o)
                    for (int i = 0; i < nb && eN < kMaxOrder; ++i)
                    {
                        eNote[(size_t) eN] = baseNote[(size_t) i] + 12 * o;
                        eVel [(size_t) eN] = baseVel [(size_t) i];
                        ++eN;
                    }
            }

            // apply the geometric direction to E → order
            orderLen = 0;
            auto push = [&] (int n, float v)
            {
                if (orderLen < kMaxOrder)
                {
                    orderNote[(size_t) orderLen] = n;
                    orderVel [(size_t) orderLen] = v;
                    ++orderLen;
                }
            };

            switch ((Direction) dir)
            {
                case Direction::Down:
                    for (int i = eN - 1; i >= 0; --i) push (eNote[(size_t) i], eVel[(size_t) i]);
                    break;

                case Direction::UpDown:
                    for (int i = 0; i < eN; ++i)       push (eNote[(size_t) i], eVel[(size_t) i]);
                    for (int i = eN - 2; i > 0; --i)   push (eNote[(size_t) i], eVel[(size_t) i]);
                    break;

                case Direction::DownUp:
                    for (int i = eN - 1; i >= 0; --i)  push (eNote[(size_t) i], eVel[(size_t) i]);
                    for (int i = 1; i < eN - 1; ++i)   push (eNote[(size_t) i], eVel[(size_t) i]);
                    break;

                case Direction::Converge:
                {
                    int lo = 0, hi = eN - 1;
                    while (lo <= hi)
                    {
                        push (eNote[(size_t) lo], eVel[(size_t) lo]);
                        if (hi != lo) push (eNote[(size_t) hi], eVel[(size_t) hi]);
                        ++lo; --hi;
                    }
                    break;
                }

                case Direction::Diverge:
                {
                    int mid = (eN - 1) / 2;
                    int lo = mid, hi = mid + 1;
                    while (lo >= 0 || hi < eN)
                    {
                        if (lo >= 0)  push (eNote[(size_t) lo], eVel[(size_t) lo]);
                        if (hi < eN)  push (eNote[(size_t) hi], eVel[(size_t) hi]);
                        --lo; ++hi;
                    }
                    break;
                }

                case Direction::Up:
                case Direction::AsPlayed:
                case Direction::Random:
                case Direction::Chord:
                default:
                    for (int i = 0; i < eN; ++i) push (eNote[(size_t) i], eVel[(size_t) i]);
                    break;
            }
        }

        //--------------------------------------------------------------------
        void fireStep (long long g, int nSteps, int dir, float gateFrac,
                       double stepSamps, int sampleOffset, juce::MidiBuffer& midiOut)
        {
            const int patIdx = (int) (((g % nSteps) + nSteps) % nSteps);
            currentStep.store (patIdx, std::memory_order_relaxed);

            const ArpStep st = pattern[(size_t) patIdx];
            if (! st.on || orderLen == 0)
            {
                currentOrderPos.store (-1, std::memory_order_relaxed);
                return;
            }

            const int   R      = juce::jlimit (1, 4, st.ratchet);
            const double subLen = stepSamps / (double) R;
            const int   gateSamps = juce::jmax (1, (int) (gateFrac * subLen));

            // decide which note(s) this step plays
            int  hitNotes[kMaxOrder];
            int  hitVels [kMaxOrder];
            int  numHits = 0;

            auto velByte = [] (float v01, float stepVel)
            {
                return juce::jlimit (1, 127, (int) std::round (juce::jlimit (0.0f, 1.0f, v01 * stepVel) * 127.0f));
            };

            if (dir == (int) Direction::Chord)
            {
                for (int i = 0; i < orderLen && numHits < kMaxOrder; ++i)
                {
                    hitNotes[numHits] = orderNote[(size_t) i];
                    hitVels [numHits] = velByte (orderVel[(size_t) i], st.vel);
                    ++numHits;
                }
                currentOrderPos.store (-1, std::memory_order_relaxed);
            }
            else
            {
                int idx;
                if (dir == (int) Direction::Random)
                    idx = rng.nextInt (orderLen);
                else
                    idx = (int) (((g % orderLen) + orderLen) % orderLen);

                hitNotes[0] = orderNote[(size_t) idx];
                hitVels [0] = velByte (orderVel[(size_t) idx], st.vel);
                numHits = 1;
                currentOrderPos.store (idx, std::memory_order_relaxed);
            }

            // schedule the ratchet sub-hits
            for (int k = 0; k < R; ++k)
            {
                const long long onAbs = absSample + (long long) std::llround ((double) k * subLen);
                for (int h = 0; h < numHits; ++h)
                {
                    if (k == 0)
                        emitNoteOn (hitNotes[h], hitVels[h], sampleOffset, gateSamps, midiOut);
                    else if (numPending < kMaxVoices)
                        pendingNotes[(size_t) numPending++] = { hitNotes[h], hitVels[h], onAbs, gateSamps };
                }
            }
        }

        void emitNoteOn (int note, int vel, int offset, int gateSamps, juce::MidiBuffer& midiOut)
        {
            // retrigger guard: close any still-open instance of this note first
            for (int i = 0; i < numActive; )
            {
                if (activeNotes[(size_t) i].note == note)
                {
                    midiOut.addEvent (juce::MidiMessage::noteOff (1, note), offset);
                    activeNotes[(size_t) i] = activeNotes[(size_t) --numActive];
                }
                else ++i;
            }

            midiOut.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), offset);
            if (numActive < kMaxVoices)
                activeNotes[(size_t) numActive++] = { note, absSample + gateSamps };
        }

        void flushAllNotes (juce::MidiBuffer& midiOut, int offset)
        {
            for (int i = 0; i < numActive; ++i)
                midiOut.addEvent (juce::MidiMessage::noteOff (1, activeNotes[(size_t) i].note), offset);
            numActive  = 0;
            numPending = 0;
        }

        //--------------------------------------------------------------------
        /** Scheduled musical position of a step boundary, including swing. */
        static double scheduledPhaseQ (long long g, double stepLenQ, float swing) noexcept
        {
            double base = (double) g * stepLenQ;
            if (swing > 0.0f && (((g % 2) + 2) % 2) == 1)
                base += (double) swing * stepLenQ;   // delay the off-beat step
            return base;
        }

        void publishSnapshot()
        {
            const juce::SpinLock::ScopedTryLockType l (snapLock);
            if (! l.isLocked()) return;

            snap.numHeld = 0;
            int lo = 127, hi = 0;

            // held base notes, ascending
            std::array<int, kMaxHeld> tmp {};
            int nt = 0;
            for (int i = 0; i < numHeld; ++i)
            {
                int n = held[(size_t) i].note, j = nt - 1;
                while (j >= 0 && tmp[(size_t) j] > n) { tmp[(size_t) (j + 1)] = tmp[(size_t) j]; --j; }
                tmp[(size_t) (j + 1)] = n; ++nt;
            }
            for (int i = 0; i < nt && i < kMaxHeld; ++i)
                snap.held[(size_t) snap.numHeld++] = tmp[(size_t) i];

            snap.numOrder = juce::jmin (orderLen, kMaxOrder);
            for (int i = 0; i < snap.numOrder; ++i)
            {
                snap.order[(size_t) i] = orderNote[(size_t) i];
                lo = juce::jmin (lo, orderNote[(size_t) i]);
                hi = juce::jmax (hi, orderNote[(size_t) i]);
            }

            if (snap.numOrder == 0) { lo = 60; hi = 72; }

            snap.lowNote         = lo;
            snap.highNote        = juce::jmax (hi, lo + 1);
            snap.currentStep     = currentStep.load (std::memory_order_relaxed);
            snap.currentOrderPos = currentOrderPos.load (std::memory_order_relaxed);
            snap.running         = running;

            runningUI.store (running, std::memory_order_relaxed);
        }

        //--------------------------------------------------------------------
        // Parameters
        std::atomic<bool>  enabled   { true };
        std::atomic<int>   rateIndex { 6 };       // 1/16
        std::atomic<int>   octaves   { 1 };
        std::atomic<int>   direction { (int) Direction::Up };
        std::atomic<float> gate      { 0.70f };
        std::atomic<float> swing     { 0.0f };
        std::atomic<int>   numSteps  { 8 };
        std::atomic<bool>  hold      { false };
        std::atomic<double> manualBpm { 120.0 };

        // Pattern (plain; benign cross-thread races)
        std::array<ArpStep, kMaxSteps> pattern {};

        // Held-note state (audio thread)
        std::array<HeldNote, kMaxHeld> held {};
        int numHeld     = 0;
        int numPhysical = 0;
        bool sustainOn  = false;
        long long seqCounter = 0;

        // Resolved order (audio thread)
        std::array<int, kMaxOrder>   orderNote {};
        std::array<float, kMaxOrder> orderVel  {};
        int orderLen = 0;

        // Output scheduling
        std::array<ActiveNote, kMaxVoices>  activeNotes  {};
        std::array<PendingNote, kMaxVoices> pendingNotes {};
        int numActive = 0, numPending = 0;

        std::array<InEvent, 512> inEvents {};

        // Clock
        double    sampleRate  = 44100.0;
        bool      running     = false;
        double    phaseQ      = 0.0;
        long long stepCounter = 0;
        long long absSample   = 0;

        juce::Random rng;

        // UI feedback
        std::atomic<int>  currentStep     { -1 };
        std::atomic<int>  currentOrderPos { -1 };
        std::atomic<bool> runningUI       { false };
        mutable juce::SpinLock snapLock;
        ArpSnapshot snap;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArpEngine)
    };
}
