/*
  ==============================================================================

    Distortion.h
    Stateless waveshaping distortion: soft (tanh), hard clip, wavefolder.
    Pure DSP, no JUCE.

  ==============================================================================
*/

#pragma once

#include <cmath>
#include <algorithm>

namespace pike
{
    enum class DistortionType { Soft = 0, Hard, Fold };

    class Distortion
    {
    public:
        void setParams (DistortionType t, float drive01, float mix01) noexcept
        {
            type    = t;
            preGain = 1.0f + drive01 * 24.0f;             // up to ~25x
            makeup  = 1.0f / (1.0f + drive01 * 3.0f);     // tame the level back down
            mix     = std::clamp (mix01, 0.0f, 1.0f);
        }

        float processSample (float x) const noexcept
        {
            const float g = x * preGain;
            float d = 0.0f;

            switch (type)
            {
                case DistortionType::Soft: d = std::tanh (g);                 break;
                case DistortionType::Hard: d = std::clamp (g, -1.0f, 1.0f);   break;
                case DistortionType::Fold: d = fold (g);                      break;
            }

            d *= makeup;
            return x + mix * (d - x);
        }

    private:
        // Period-4 triangle wavefolder, bounded to [-1, 1], no loops.
        static float fold (float x) noexcept
        {
            x = std::fmod (x + 1.0f, 4.0f);
            if (x < 0.0f) x += 4.0f;
            x -= 1.0f;
            if (x > 1.0f) x = 2.0f - x;
            return x;
        }

        DistortionType type    = DistortionType::Soft;
        float          preGain = 1.0f;
        float          makeup  = 1.0f;
        float          mix     = 0.0f;
    };
}
