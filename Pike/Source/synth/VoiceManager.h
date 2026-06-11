/*
  ==============================================================================

    VoiceManager.h
    Note dispatch for all voice modes. Drives PikeVoice instances directly (the
    processor renders the synth with an empty MIDI buffer), which keeps mono,
    legato and unison logic in one place instead of fighting juce::Synthesiser's
    allocator.

      - Poly:   each note allocates `unison` voices (detuned, last-note steal).
      - Mono:   one note sounds (last-note priority); new notes retrigger.
      - Legato: like mono, but overlapping notes glide without retriggering.
      - Unison: `unison` detuned voices per sounded note; glide is per-voice.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeVoice.h"

namespace pike
{
    class VoiceManager
    {
    public:
        enum Mode { Poly = 0, Mono, Legato };

        /** Collects the PikeVoice instances owned by the synthesiser. */
        void prepare (juce::Synthesiser& synth)
        {
            voices.clear();
            for (int i = 0; i < synth.getNumVoices(); ++i)
                if (auto* v = dynamic_cast<PikeVoice*> (synth.getVoice (i)))
                    voices.push_back (v);

            voiceNote.assign (voices.size(), -1);
            voiceAge.assign (voices.size(), 0);
            reset();
        }

        void reset() noexcept
        {
            stackCount = 0;
            monoCount  = 0;
            ageCounter = 0;
            for (auto& n : voiceNote) n = -1;
        }

        void setConfig (int newMode, int newUnison, float newDetuneCents) noexcept
        {
            const int uni = juce::jlimit (1, (int) voices.size(), newUnison);
            if (newMode != mode || uni != unison)
            {
                allNotesOff();
                mode   = newMode;
                unison = uni;
            }
            detuneCents = newDetuneCents;
        }

        void allNotesOff() noexcept
        {
            for (auto* v : voices)
                v->stopNote (0.0f, true);
            for (auto& n : voiceNote) n = -1;
            stackCount = 0;
            monoCount  = 0;
        }

        //======================================================================
        void noteOn (int note, float vel) noexcept
        {
            if (mode == Poly)
            {
                for (int u = 0; u < unison; ++u)
                    startVoice (findVoice(), note, vel, u);
            }
            else
            {
                pushStack (note, vel);

                if (stackCount == 1)
                {
                    monoCount = 0;
                    for (int u = 0; u < unison; ++u)
                    {
                        const int idx = findVoice();
                        monoVoices[monoCount++] = idx;
                        startVoice (idx, note, vel, u);
                    }
                }
                else
                {
                    retopMono (note, vel);
                }
            }
        }

        void noteOff (int note) noexcept
        {
            if (mode == Poly)
            {
                for (size_t i = 0; i < voices.size(); ++i)
                {
                    if (voiceNote[i] == note)
                    {
                        voices[i]->stopNote (0.0f, true);
                        voiceNote[i] = -1;
                    }
                }
            }
            else
            {
                removeFromStack (note);

                if (stackCount == 0)
                {
                    for (int i = 0; i < monoCount; ++i)
                        voices[(size_t) monoVoices[i]]->stopNote (0.0f, true);
                    monoCount = 0;
                }
                else
                {
                    retopMono (stack[stackCount - 1].note, stack[stackCount - 1].vel);
                }
            }
        }

    private:
        struct Held { int note; float vel; };

        void startVoice (int idx, int note, float vel, int unisonIndex) noexcept
        {
            if (idx < 0)
                return;

            auto* v = voices[(size_t) idx];
            v->setUnison (unisonOffset (unisonIndex), unisonGain(), unisonPan (unisonIndex));
            v->startNote (note, vel, nullptr, 0);
            voiceNote[(size_t) idx] = note;
            voiceAge[(size_t) idx]  = ++ageCounter;
        }

        // Update the currently-sounding mono/unison voices to a new top note.
        void retopMono (int note, float vel) noexcept
        {
            for (int i = 0; i < monoCount; ++i)
            {
                auto* v = voices[(size_t) monoVoices[i]];
                if (mode == Legato)
                {
                    v->retune (note);
                }
                else // Mono: retrigger
                {
                    v->setUnison (unisonOffset (i), unisonGain(), unisonPan (i));
                    v->startNote (note, vel, nullptr, 0);
                }
                voiceNote[(size_t) monoVoices[i]] = note;
            }
        }

        int findVoice() noexcept
        {
            // Prefer a silent voice; otherwise steal the oldest.
            int best = -1;
            for (size_t i = 0; i < voices.size(); ++i)
            {
                if (! voices[i]->isSounding())
                    return (int) i;
            }
            uint64_t oldest = UINT64_MAX;
            for (size_t i = 0; i < voices.size(); ++i)
            {
                if (voiceAge[i] < oldest) { oldest = voiceAge[i]; best = (int) i; }
            }
            return best;
        }

        float unisonOffset (int u) const noexcept
        {
            if (unison <= 1)
                return 0.0f;
            return detuneCents * ((float) u / (float) (unison - 1) - 0.5f);
        }

        float unisonGain() const noexcept
        {
            return 1.0f / std::sqrt ((float) juce::jmax (1, unison));
        }

        // Spreads unison voices across the stereo field (0..1); single voice is centred.
        float unisonPan (int u) const noexcept
        {
            if (unison <= 1)
                return 0.5f;
            constexpr float spread = 0.9f;
            return 0.5f + ((float) u / (float) (unison - 1) - 0.5f) * spread;
        }

        void pushStack (int note, float vel) noexcept
        {
            for (int i = 0; i < stackCount; ++i)
                if (stack[i].note == note) { stack[i].vel = vel; return; }
            if (stackCount < maxStack)
                stack[stackCount++] = { note, vel };
        }

        void removeFromStack (int note) noexcept
        {
            for (int i = 0; i < stackCount; ++i)
            {
                if (stack[i].note == note)
                {
                    for (int j = i; j < stackCount - 1; ++j)
                        stack[j] = stack[j + 1];
                    --stackCount;
                    return;
                }
            }
        }

        static constexpr int maxStack = 32;
        static constexpr int maxUnison = 16;

        std::vector<PikeVoice*> voices;
        std::vector<int>        voiceNote;
        std::vector<uint64_t>   voiceAge;

        int      mode = Poly;
        int      unison = 1;
        float    detuneCents = 0.0f;
        uint64_t ageCounter = 0;

        Held stack[maxStack] {};
        int  stackCount = 0;
        int  monoVoices[maxUnison] {};
        int  monoCount = 0;
    };
}
