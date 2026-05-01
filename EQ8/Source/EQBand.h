/*
  ==============================================================================

    EQBand.h
    Created: Parametric EQ Band Filter
    Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <complex>

class EQBand
{
public:
    enum FilterType
    {
        LowShelf,
        Peak,
        HighShelf
    };

    EQBand()
    {
        reset();
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        this->sampleRate = sampleRate;
        updateCoefficients();
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i)
        {
            x1[i] = x2[i] = 0.0f;
            y1[i] = y2[i] = 0.0f;
        }
    }

    void setType(FilterType newType)
    {
        type = newType;
        updateCoefficients();
    }

    void setFrequency(float freq)
    {
        frequency = juce::jlimit(20.0f, 20000.0f, freq);
        updateCoefficients();
    }

    void setGain(float gainDb)
    {
        gain = juce::jlimit(-24.0f, 24.0f, gainDb);
        updateCoefficients();
    }

    void setQ(float qValue)
    {
        q = juce::jlimit(0.1f, 10.0f, qValue);
        updateCoefficients();
    }

    void setEnabled(bool shouldBeEnabled)
    {
        enabled = shouldBeEnabled;
    }

    bool isEnabled() const { return enabled; }
    float getFrequency() const { return frequency; }
    float getGain() const { return gain; }
    float getQ() const { return q; }
    FilterType getType() const { return type; }

    void processSample(float& leftSample, float& rightSample)
    {
        if (!enabled)
            return;

        leftSample = processSingleSample(leftSample, 0);
        rightSample = processSingleSample(rightSample, 1);
    }

    std::complex<float> getFrequencyResponse(float freq) const
    {
        if (!enabled)
            return std::complex<float>(1.0f, 0.0f);

        float omega = juce::MathConstants<float>::twoPi * freq / static_cast<float>(sampleRate);
        std::complex<float> z = std::exp(std::complex<float>(0.0f, omega));
        std::complex<float> z2 = z * z;

        std::complex<float> numerator = b0 + b1 / z + b2 / z2;
        std::complex<float> denominator = 1.0f + a1 / z + a2 / z2;

        return numerator / denominator;
    }

private:
    float processSingleSample(float input, int channel)
    {
        float output = b0 * input + b1 * x1[channel] + b2 * x2[channel]
                                  - a1 * y1[channel] - a2 * y2[channel];

        x2[channel] = x1[channel];
        x1[channel] = input;
        y2[channel] = y1[channel];
        y1[channel] = output;

        return output;
    }

    void updateCoefficients()
    {
        if (sampleRate <= 0.0)
            return;

        float omega = juce::MathConstants<float>::twoPi * frequency / static_cast<float>(sampleRate);
        float sinOmega = std::sin(omega);
        float cosOmega = std::cos(omega);
        float alpha = sinOmega / (2.0f * q);
        float A = std::pow(10.0f, gain / 40.0f);

        switch (type)
        {
            case LowShelf:
            {
                float beta = std::sqrt(A) / q;
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega + beta * sinOmega);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosOmega);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega - beta * sinOmega);
                float a0 = (A + 1.0f) + (A - 1.0f) * cosOmega + beta * sinOmega;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosOmega);
                a2 = (A + 1.0f) + (A - 1.0f) * cosOmega - beta * sinOmega;

                b0 /= a0;
                b1 /= a0;
                b2 /= a0;
                a1 /= a0;
                a2 /= a0;
                break;
            }

            case Peak:
            {
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cosOmega;
                b2 = 1.0f - alpha * A;
                float a0 = 1.0f + alpha / A;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha / A;

                b0 /= a0;
                b1 /= a0;
                b2 /= a0;
                a1 /= a0;
                a2 /= a0;
                break;
            }

            case HighShelf:
            {
                float beta = std::sqrt(A) / q;
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega + beta * sinOmega);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosOmega);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega - beta * sinOmega);
                float a0 = (A + 1.0f) - (A - 1.0f) * cosOmega + beta * sinOmega;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosOmega);
                a2 = (A + 1.0f) - (A - 1.0f) * cosOmega - beta * sinOmega;

                b0 /= a0;
                b1 /= a0;
                b2 /= a0;
                a1 /= a0;
                a2 /= a0;
                break;
            }
        }
    }

    FilterType type = Peak;
    float frequency = 1000.0f;
    float gain = 0.0f;
    float q = 0.707f;
    bool enabled = true;
    double sampleRate = 44100.0;

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float x1[2] = {0.0f}, x2[2] = {0.0f};
    float y1[2] = {0.0f}, y2[2] = {0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQBand)
};
