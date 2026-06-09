/*
  ==============================================================================

    Oscillator.h
    Minimal phase-accumulator oscillator. Phase 2 ships a clean sine; Phase 3
    replaces/extends this with PolyBLEP analogue waveforms and wavetables.
    Pure DSP: no JUCE GUI dependencies, unit-testable.

  ==============================================================================
*/

#pragma once

#include <cmath>

namespace pike
{
    class Oscillator
    {
    public:
        void setSampleRate (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            updateIncrement();
        }

        void setFrequency (float newFrequencyHz) noexcept
        {
            frequency = newFrequencyHz;
            updateIncrement();
        }

        /** Resets the phase to zero (e.g. on note-on for a clean attack). */
        void resetPhase() noexcept { phase = 0.0; }

        /** Advances one sample and returns the oscillator output in [-1, 1]. */
        float processSample() noexcept
        {
            constexpr double twoPi = 6.283185307179586476925286766559;
            const auto value = std::sin (phase * twoPi);

            phase += increment;
            if (phase >= 1.0)
                phase -= 1.0;

            return static_cast<float> (value);
        }

    private:
        void updateIncrement() noexcept
        {
            increment = sampleRate > 0.0 ? (double) frequency / sampleRate : 0.0;
        }

        double sampleRate = 44100.0;
        float  frequency  = 440.0f;
        double phase      = 0.0;
        double increment  = 0.0;
    };
}
