/*
  ==============================================================================

    Noise.h
    White-noise generator. Pure DSP, realtime-safe (xorshift PRNG, no locks/alloc).

  ==============================================================================
*/

#pragma once

#include <cstdint>

namespace pike
{
    class Noise
    {
    public:
        /** Seed with a per-voice value so voices are decorrelated. */
        void seed (uint32_t s) noexcept { state = s != 0 ? s : 0x9e3779b9u; }

        /** Returns white noise in roughly [-1, 1]. */
        float processSample() noexcept
        {
            // xorshift32
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            // map to [-1, 1)
            return (float) ((double) state / 2147483648.0 - 1.0);
        }

    private:
        uint32_t state = 0x9e3779b9u;
    };
}
