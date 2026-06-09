/*
  ==============================================================================

    Filter.h
    Multimode state-variable filter (Cytomic / Zavalishin TPT topology). Stable
    and clean up to self-oscillation. LP/BP/HP selectable, 12 or 24 dB/oct.

    For 24 dB/oct two stages are cascaded, but only the first carries the
    user resonance; the second is a flat Butterworth stage. This keeps the
    resonant peak musical instead of squaring the Q.

    The resonance (bandpass) integrator state is softly saturated, so high
    resonance / self-oscillation settles to a clean, bounded amplitude (analog-
    style) while staying linear at normal signal levels.

    Pure DSP, no JUCE, unit-testable.

  ==============================================================================
*/

#pragma once

#include <cmath>
#include <algorithm>

namespace pike
{
    enum class FilterType { LowPass = 0, BandPass, HighPass };

    class StateVariableFilter
    {
    public:
        void setSampleRate (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            updateCoefficients();
        }

        void reset() noexcept
        {
            for (auto& s : stages)
                s = Stage{};
        }

        void setType   (FilterType t) noexcept { type = t; }
        void setSlope24 (bool b)      noexcept { slope24 = b; }

        void setCutoff (float cutoffHz) noexcept
        {
            const double maxHz = sampleRate * 0.49;
            cutoff = std::clamp ((double) cutoffHz, 10.0, maxHz);
            updateCoefficients();
        }

        /** Resonance in [0,1]; exponential Q taper, true self-oscillation at 1. */
        void setResonance (float resonance01) noexcept
        {
            const double r = std::clamp ((double) resonance01, 0.0, 1.0);
            if (r >= 0.999)
            {
                kRes = 0.0;                       // poles on the unit circle: self-oscillates
            }
            else
            {
                const double q = 0.5 * std::pow (200.0, r);   // 0.5 .. ~100
                kRes = 1.0 / q;
            }
            updateCoefficients();
        }

        float process (float input) noexcept
        {
            double out = processStage (stages[0], (double) input, kRes, resCoeffs, true);
            if (slope24)
                out = processStage (stages[1], out, kBw, bwCoeffs, false);
            return (float) out;
        }

    private:
        struct Stage   { double ic1eq = 0.0, ic2eq = 0.0; };
        struct Coeffs  { double a1 = 0.0, a2 = 0.0, a3 = 0.0; };

        // Linear up to |x|=1, soft beyond -> bounds resonance/self-oscillation.
        static double softClip (double x) noexcept
        {
            if (x >  1.0) return  1.0 + std::tanh (x - 1.0);
            if (x < -1.0) return -1.0 - std::tanh (-x - 1.0);
            return x;
        }

        void updateCoefficients() noexcept
        {
            constexpr double pi = 3.14159265358979323846;
            g = std::tan (pi * cutoff / sampleRate);
            resCoeffs = makeCoeffs (kRes);
            bwCoeffs  = makeCoeffs (kBw);
        }

        Coeffs makeCoeffs (double k) const noexcept
        {
            Coeffs c;
            c.a1 = 1.0 / (1.0 + g * (g + k));
            c.a2 = g * c.a1;
            c.a3 = g * c.a2;
            return c;
        }

        double processStage (Stage& s, double v0, double k, const Coeffs& c, bool saturate) noexcept
        {
            const double v3 = v0 - s.ic2eq;
            const double v1 = c.a1 * s.ic1eq + c.a2 * v3;
            const double v2 = s.ic2eq + c.a2 * s.ic1eq + c.a3 * v3;

            s.ic1eq = saturate ? softClip (2.0 * v1 - s.ic1eq) : (2.0 * v1 - s.ic1eq);
            s.ic2eq = 2.0 * v2 - s.ic2eq;

            switch (type)
            {
                case FilterType::LowPass:  return v2;
                case FilterType::BandPass: return v1;
                case FilterType::HighPass: return v0 - k * v1 - v2;
            }
            return v2;
        }

        double sampleRate = 44100.0;
        double cutoff     = 20000.0;
        double kRes       = 1.0 / 0.707;          // damping from resonance (stage 0)
        const  double kBw = 1.41421356237;        // Butterworth damping (stage 1)
        double g          = 0.0;

        Coeffs resCoeffs, bwCoeffs;

        FilterType type    = FilterType::LowPass;
        bool       slope24 = false;
        Stage      stages[2];
    };
}
