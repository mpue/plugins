/*
  ==============================================================================

    Lfo.h
    Low-frequency oscillator with Sine / Triangle / Saw / Square / Sample&Hold.
    Pure DSP (no JUCE). Two render paths:
      - renderPoly: per-voice, advances its own phase (key-sync friendly).
      - renderMono: reads an externally-supplied shared total phase so all voices
        stay locked (mono mode).
    Sample & Hold is derived from the integer step index via a hash, so it is
    deterministic and identical across voices in mono mode.

  ==============================================================================
*/

#pragma once

#include <cmath>
#include <cstdint>

namespace pike
{
    enum class LfoShape { Sine = 0, Triangle, Saw, Square, SampleHold };

    class Lfo
    {
    public:
        void setSampleRate (double sr) noexcept { sampleRate = sr > 0.0 ? sr : 44100.0; }
        void setShape (LfoShape s)     noexcept { shape = s; }

        /** Key-sync retrigger: restart the per-voice phase. */
        void noteOn (bool keySync) noexcept
        {
            if (keySync)
            {
                phase = 0.0;
                step  = 0;
            }
        }

        /** Per-voice render; advances the internal phase by inc (cycles/sample). */
        float renderPoly (double inc, uint32_t seed) noexcept
        {
            const float v = shapeValue (phase, step, seed);

            phase += inc;
            while (phase >= 1.0) { phase -= 1.0; ++step; }
            while (phase <  0.0) { phase += 1.0; --step; }

            return v;
        }

        /** Mono render from a shared total phase (integer part = S&H step). */
        float renderMono (double totalPhase, uint32_t seed) const noexcept
        {
            const double p  = totalPhase - std::floor (totalPhase);
            const long long st = (long long) std::floor (totalPhase);
            return shapeValue (p, st, seed);
        }

    private:
        static float hash01 (uint64_t x) noexcept
        {
            x = (x ^ (x >> 33)) * 0xff51afd7ed558ccdULL;
            x = (x ^ (x >> 33)) * 0xc4ceb9fe1a85ec53ULL;
            x ^=  x >> 33;
            return (float) ((x >> 11) * (1.0 / 9007199254740992.0));
        }

        float shapeValue (double p, long long stepIndex, uint32_t seed) const noexcept
        {
            switch (shape)
            {
                case LfoShape::Sine:       return (float) std::sin (p * 6.283185307179586);
                case LfoShape::Triangle:   return (float) (1.0 - 4.0 * std::abs (p - 0.5));
                case LfoShape::Saw:        return (float) (2.0 * p - 1.0);
                case LfoShape::Square:     return p < 0.5 ? 1.0f : -1.0f;
                case LfoShape::SampleHold: return hash01 ((uint64_t) stepIndex * 2654435761ull ^ seed) * 2.0f - 1.0f;
            }
            return 0.0f;
        }

        double   sampleRate = 44100.0;
        double   phase      = 0.0;
        long long step      = 0;
        LfoShape shape      = LfoShape::Sine;
    };
}
