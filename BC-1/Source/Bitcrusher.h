/*
  ==============================================================================

    Bitcrusher.h
    Luxury bitcrusher DSP – oversampled, fractional bit depth, fractional
    sample-rate reduction with sample-and-hold, soft saturation, TPDF dither,
    and a post-crush tilt EQ for taming harshness. Built so that even
    aggressive settings remain musical instead of harsh.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class LuxuryBitcrusher
{
public:
    LuxuryBitcrusher() = default;

    void prepare (double sampleRate, int blockSize, int numChannels)
    {
        baseSR   = sampleRate;
        channels = juce::jmax (1, numChannels);

        // 4x oversampling – polyphase IIR halfband, max quality, integer latency
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) channels, 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, true);
        oversampler->initProcessing ((size_t) blockSize);
        oversampledSR = sampleRate * (double) oversampler->getOversamplingFactor();

        srHold .assign ((size_t) channels, 0.0f);
        srPhase.assign ((size_t) channels, 1.0f);

        juce::dsp::ProcessSpec spec { sampleRate,
                                      (juce::uint32) blockSize,
                                      (juce::uint32) channels };
        toneLow .prepare (spec);
        toneHigh.prepare (spec);
        toneLow .reset();
        toneHigh.reset();
        currentTone = std::numeric_limits<float>::lowest();
        updateToneCoefficients (0.0f);

        const double smoothMs = 0.025; // 25 ms parameter ramp
        driveSm .reset (oversampledSR, smoothMs);
        bitsSm  .reset (oversampledSR, smoothMs);
        rateSm  .reset (oversampledSR, smoothMs);
        ditherSm.reset (oversampledSR, smoothMs);
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        std::fill (srHold .begin(), srHold .end(), 0.0f);
        std::fill (srPhase.begin(), srPhase.end(), 1.0f);

        toneLow .reset();
        toneHigh.reset();
    }

    void setParameters (float driveDb,
                        float bits,
                        float rateHz,
                        float dither,
                        float tone)
    {
        driveSm .setTargetValue (juce::Decibels::decibelsToGain (driveDb));
        bitsSm  .setTargetValue (juce::jlimit (1.0f, 16.0f, bits));
        rateSm  .setTargetValue (juce::jmax (50.0f, rateHz));
        ditherSm.setTargetValue (juce::jlimit (0.0f, 1.0f, dither));

        if (std::abs (tone - currentTone) > 1.0e-4f)
        {
            currentTone = tone;
            updateToneCoefficients (tone);
        }
    }

    int getLatencyInSamples() const noexcept
    {
        return oversampler != nullptr ? (int) oversampler->getLatencyInSamples() : 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (oversampler == nullptr)
            return;

        juce::dsp::AudioBlock<float> block (buffer);
        auto osBlock = oversampler->processSamplesUp (block);

        const int numCh      = (int) osBlock.getNumChannels();
        const int numSamples = (int) osBlock.getNumSamples();

        for (int n = 0; n < numSamples; ++n)
        {
            const float drive  = driveSm .getNextValue();
            const float bits   = bitsSm  .getNextValue();
            const float rateHz = rateSm  .getNextValue();
            const float dither = ditherSm.getNextValue();

            // continuous quantisation step from fractional bit depth
            const float levels = std::pow (2.0f, bits) - 1.0f;
            const float qStep  = 2.0f / juce::jmax (1.0f, levels);

            const float phaseInc = (float) (rateHz / oversampledSR);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* data = osBlock.getChannelPointer ((size_t) ch);
                float x = data[n];

                // soft saturation – tanh shape, gentle warmth, no hard edges
                x = std::tanh (x * drive * 0.85f) * 1.05f;

                // TPDF dither (sum of two uniform noises) scaled to step size
                if (dither > 0.0f)
                {
                    const float r = (rng.nextFloat() - rng.nextFloat()) * qStep * 0.5f;
                    x += r * dither;
                }

                // mid-tread quantisation
                x = std::round (x / qStep) * qStep;
                x = juce::jlimit (-1.5f, 1.5f, x);

                // fractional sample-and-hold
                float& phase = srPhase[(size_t) ch];
                phase += phaseInc;
                if (phase >= 1.0f)
                {
                    phase  -= std::floor (phase);
                    srHold[(size_t) ch] = x;
                }

                data[n] = srHold[(size_t) ch];
            }
        }

        oversampler->processSamplesDown (block);

        // tilt EQ runs at base rate, post-crush, to tame harsh content musically
        juce::dsp::AudioBlock<float> baseBlock (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (baseBlock);
        toneLow .process (ctx);
        toneHigh.process (ctx);
    }

private:
    void updateToneCoefficients (float tone)
    {
        // -1..+1 → ±9 dB tilt around 1 kHz (low-shelf opposite to high-shelf)
        const float gainDb = juce::jlimit (-9.0f, 9.0f, tone * 9.0f);
        const float lowG   = juce::Decibels::decibelsToGain (-gainDb);
        const float highG  = juce::Decibels::decibelsToGain ( gainDb);

        *toneLow .state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  (baseSR, 350.0f,  0.7f, lowG);
        *toneHigh.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (baseSR, 3500.0f, 0.7f, highG);
    }

    double baseSR        = 44100.0;
    double oversampledSR = 176400.0;
    int    channels      = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using IIRCoefs  = juce::dsp::IIR::Coefficients<float>;
    juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoefs> toneLow, toneHigh;
    float currentTone = 0.0f;

    std::vector<float> srHold, srPhase;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSm, bitsSm, rateSm, ditherSm;
    juce::Random rng;
};
