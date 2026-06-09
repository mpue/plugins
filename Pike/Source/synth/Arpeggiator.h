/*
  ==============================================================================

    Arpeggiator.h
    MIDI-level arpeggiator. Consumes incoming note on/offs into a held-note set
    and emits a sequence of note on/offs (sample-accurate within the block).
    Modes: Up / Down / UpDown / Random / As-Played. Tempo-synced rate, gate,
    octave range and latch. Non-note messages pass through unchanged.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace pike
{
    class Arpeggiator
    {
    public:
        struct Params
        {
            bool  enabled = false;
            int   mode    = 0;     // Up, Down, UpDown, Random, AsPlayed
            int   rateDiv = 1;     // index into rate divisions
            float gate    = 0.5f;  // 0..1 of step length
            int   octaves = 1;     // 1..4
            bool  latch   = false;
        };

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            generated.ensureSize (256);
            reset();
        }

        void reset() noexcept
        {
            heldCount = 0;
            physicalDown = 0;
            currentStep = 0;
            stepCountdown = 0;
            gateCountdown = 0;
            activeNote = -1;
            gateOn = false;
            wasEnabled = false;
        }

        /** Transforms midi in place: held-set update + generated arp events. */
        void process (juce::MidiBuffer& midi, const Params& p, double bpm, int numSamples)
        {
            if (! p.enabled)
            {
                if (wasEnabled)
                    reset();
                return;   // pass MIDI through untouched
            }

            // --- update held set + collect pass-through (non-note) messages ---
            generated.clear();
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOn())
                    addNote (m.getNoteNumber(), m.getFloatVelocity(), p.latch);
                else if (m.isNoteOff())
                    removeNote (m.getNoteNumber(), p.latch);
                else
                    generated.addEvent (m, meta.samplePosition);
            }

            // Latch released: stop the drone when no keys are physically down.
            if (! p.latch && physicalDown == 0)
                heldCount = 0;

            // --- step scheduler ---
            const double stepSamples = computeStepSamples (bpm, p.rateDiv);
            const int    stepLen     = juce::jmax (1, (int) std::lround (stepSamples));
            const int    gateLen     = juce::jmax (1, (int) std::lround (stepSamples * juce::jlimit (0.05f, 1.0f, p.gate)));

            for (int i = 0; i < numSamples; ++i)
            {
                if (gateOn && --gateCountdown <= 0)
                {
                    noteOff (i);
                }

                if (--stepCountdown <= 0)
                {
                    stepCountdown += stepLen;

                    if (activeNote >= 0)        // ensure previous note ends
                        noteOff (i);

                    if (heldCount > 0)
                    {
                        Note n = pickNote (p);
                        if (n.note >= 0)
                        {
                            generated.addEvent (juce::MidiMessage::noteOn (1, n.note, n.vel), i);
                            activeNote    = n.note;
                            gateCountdown = gateLen;
                            gateOn        = true;
                        }
                    }
                }
            }

            midi.swapWith (generated);
            wasEnabled = true;
        }

    private:
        struct Note { int note; float vel; };

        void noteOff (int samplePos) noexcept
        {
            if (activeNote >= 0)
                generated.addEvent (juce::MidiMessage::noteOff (1, activeNote), samplePos);
            activeNote = -1;
            gateOn = false;
        }

        void addNote (int note, float vel, bool latch) noexcept
        {
            if (latch && physicalDown == 0)
                heldCount = 0;          // new chord begins

            ++physicalDown;

            for (int i = 0; i < heldCount; ++i)
                if (held[i].note == note) { held[i].vel = vel; return; }

            if (heldCount < maxHeld)
                held[heldCount++] = { note, vel };
        }

        void removeNote (int note, bool latch) noexcept
        {
            if (physicalDown > 0)
                --physicalDown;

            if (latch)
                return;                 // keep latched notes sounding

            for (int i = 0; i < heldCount; ++i)
            {
                if (held[i].note == note)
                {
                    for (int j = i; j < heldCount - 1; ++j)
                        held[j] = held[j + 1];
                    --heldCount;
                    return;
                }
            }
        }

        Note pickNote (const Params& p) noexcept
        {
            buildSequence (p);
            if (seqLen == 0)
                return { -1, 0.0f };

            int idx;
            if (p.mode == 3)            // Random
            {
                rng = rng * 1664525u + 1013904223u;
                idx = (int) ((rng >> 8) % (uint32_t) seqLen);
            }
            else
            {
                idx = currentStep % seqLen;
            }

            currentStep = (currentStep + 1) % seqLen;
            return seq[idx];
        }

        void buildSequence (const Params& p) noexcept
        {
            seqLen = 0;
            if (heldCount == 0)
                return;

            const int octs = juce::jlimit (1, 4, p.octaves);

            // Ascending-by-pitch copy of the held set.
            Note asc[maxHeld];
            for (int i = 0; i < heldCount; ++i) asc[i] = held[i];
            for (int a = 0; a < heldCount - 1; ++a)
                for (int b = 0; b < heldCount - 1 - a; ++b)
                    if (asc[b].note > asc[b + 1].note) std::swap (asc[b], asc[b + 1]);

            // Descending copy.
            Note desc[maxHeld];
            for (int i = 0; i < heldCount; ++i) desc[i] = asc[heldCount - 1 - i];

            // Pushes a base list across octaves; octAsc=false runs octaves high->low.
            auto push = [&] (const Note* base, bool octAsc)
            {
                for (int o = 0; o < octs && seqLen < maxSeq; ++o)
                {
                    const int oct = octAsc ? o : (octs - 1 - o);
                    for (int i = 0; i < heldCount && seqLen < maxSeq; ++i)
                        seq[seqLen++] = { juce::jlimit (0, 127, base[i].note + 12 * oct), base[i].vel };
                }
            };

            switch (p.mode)
            {
                case 0: push (asc,  true);  break;            // Up
                case 1: push (desc, false); break;            // Down (notes & octaves descending)
                case 3: push (asc,  true);  break;            // Random (over ascending set)
                case 4: push (held, true);  break;            // As-played
                case 2:                                       // UpDown
                {
                    push (asc, true);
                    const int upLen = seqLen;
                    for (int i = upLen - 2; i >= 1 && seqLen < maxSeq; --i)
                        seq[seqLen++] = seq[i];
                    break;
                }
                default: push (asc, true); break;
            }
        }

        double computeStepSamples (double bpm, int rateDiv) const noexcept
        {
            // steps per beat for "1/4","1/8","1/8T","1/16","1/16T","1/32"
            static constexpr double stepsPerBeat[] = { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0 };
            const int idx = juce::jlimit (0, (int) std::size (stepsPerBeat) - 1, rateDiv);
            const double beatsPerStep = 1.0 / stepsPerBeat[idx];
            return beatsPerStep * 60.0 / (bpm > 0.0 ? bpm : 120.0) * sampleRate;
        }

        static constexpr int maxHeld = 16;
        static constexpr int maxSeq  = maxHeld * 4 * 2;

        juce::MidiBuffer generated;
        double sampleRate = 44100.0;

        Note  held[maxHeld] {};
        int   heldCount = 0;
        int   physicalDown = 0;

        Note  seq[maxSeq] {};
        int   seqLen = 0;

        int   currentStep   = 0;
        int   stepCountdown = 0;
        int   gateCountdown = 0;
        int   activeNote    = -1;
        bool  gateOn        = false;
        bool  wasEnabled    = false;
        uint32_t rng        = 0x12345678u;
    };
}
