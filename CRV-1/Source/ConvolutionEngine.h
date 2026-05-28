/*
  ==============================================================================

    ConvolutionEngine.h
    The DSP heart of CRV-1. Wraps juce::dsp::Convolution and adds the
    surrounding processing chain needed to make a convolution reverb sound
    luxurious: pre-delay, low/high cut, subtle tail modulation (chorus-like
    motion that prevents the convolution from sounding "static"), stereo
    width, and dry/wet crossfading. Tail meters drive the visualizer.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class ConvolutionEngine
{
public:
    struct Parameters
    {
        float predelayMs = 20.0f;     // 0..500
        float lowCutHz   = 80.0f;     // 20..1000
        float highCutHz  = 9000.0f;   // 500..20000
        float modulation = 0.25f;     // 0..1
        float width      = 1.0f;      // 0..1.5
        float mix        = 0.35f;     // 0..1
        float outputDb   = 0.0f;      // -24..+24 dB
    };

    void prepare (double sampleRate, int maxBlockSize)
    {
        currentSampleRate = sampleRate;
        currentBlock      = maxBlockSize;

        juce::dsp::ProcessSpec specStereo {
            sampleRate, (juce::uint32) maxBlockSize, 2
        };
        juce::dsp::ProcessSpec specMono {
            sampleRate, (juce::uint32) maxBlockSize, 1
        };

        convolution.reset();
        convolution.prepare (specStereo);

        const int preMaxSamples = (int) (0.55 * sampleRate) + 16;
        for (auto& d : preDelay)
        {
            d.setMaximumDelayInSamples (preMaxSamples);
            d.prepare (specMono);
            d.reset();
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            lowCut[ch].reset();
            highCut[ch].reset();

            // Modulation delay lines for tail motion
            modLine[ch].setMaximumDelayInSamples ((int) (0.02 * sampleRate) + 16);
            modLine[ch].prepare (specMono);
            modLine[ch].reset();
            modPhase[ch] = (ch == 0) ? 0.0f : 0.25f;
        }

        applyParameters();
        irLoaded.store (false);
    }

    void reset()
    {
        convolution.reset();
        for (auto& d : preDelay)  d.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            lowCut[ch].reset();
            highCut[ch].reset();
            modLine[ch].reset();
        }
        wetEnergyL = wetEnergyR = inputEnergy = 0.0f;
    }

    void setParameters (const Parameters& p)
    {
        params = p;
        applyParameters();
    }

    const Parameters& getParameters() const noexcept { return params; }

    // Hands the convolver a new impulse response. Safe to call from the
    // message thread; the convolution module copies and partitions internally.
    void loadImpulseResponse (juce::AudioBuffer<float>&& ir, double irSampleRate)
    {
        // Take ownership of the buffer's memory so JUCE can stream it in.
        // Convolution::loadImpulseResponse expects a moved AudioBuffer.
        if (ir.getNumSamples() < 16) return;

        convolution.loadImpulseResponse (
            std::move (ir),
            irSampleRate,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::yes);

        irLoaded.store (true);
    }

    bool isIRLoaded() const noexcept { return irLoaded.load(); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numChannels < 1 || n <= 0) return;

        auto* L = buffer.getWritePointer (0);
        auto* R = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        // ---- 1) Cache dry signal & feed energy meter ----
        if (dryBuffer.getNumChannels() != 2 || dryBuffer.getNumSamples() < n)
            dryBuffer.setSize (2, juce::jmax (n, currentBlock), false, false, true);

        float inE = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = L[i];
            const float b = R != nullptr ? R[i] : a;
            dryBuffer.setSample (0, i, a);
            dryBuffer.setSample (1, i, b);
            inE += 0.5f * (std::abs (a) + std::abs (b));
        }
        inE /= juce::jmax (1, n);
        inputEnergy = energyCoeff * inputEnergy + (1.0f - energyCoeff) * inE;

        // ---- 2) Wet path: pre-delay -> convolution -> mod -> filters -> width ----
        if (wetBuffer.getNumChannels() != 2 || wetBuffer.getNumSamples() < n)
            wetBuffer.setSize (2, juce::jmax (n, currentBlock), false, false, true);

        // Pre-delay (per channel)
        for (int i = 0; i < n; ++i)
        {
            preDelay[0].pushSample (0, L[i]);
            const float inR = R != nullptr ? R[i] : L[i];
            preDelay[1].pushSample (0, inR);

            wetBuffer.setSample (0, i, preDelay[0].popSample (0, (float) predelaySamples, true));
            wetBuffer.setSample (1, i, preDelay[1].popSample (0, (float) predelaySamples, true));
        }

        // Convolution (stereo)
        if (irLoaded.load())
        {
            juce::dsp::AudioBlock<float> wetBlock (wetBuffer.getArrayOfWritePointers(),
                                                   2, 0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (wetBlock);
            convolution.process (ctx);
        }

        // Tail motion: subtle modulated delay on wet, gives the impulse a
        // sense of motion and prevents the dreaded "static cloud" feeling.
        const float modDepth = juce::jlimit (0.0f, 1.0f, params.modulation) * 0.0035f
                             * (float) currentSampleRate;
        const float modFreqL = 0.27f;
        const float modFreqR = 0.31f;
        const float invSR    = 1.0f / (float) currentSampleRate;

        auto* wL = wetBuffer.getWritePointer (0);
        auto* wR = wetBuffer.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            modPhase[0] += modFreqL * invSR;
            modPhase[1] += modFreqR * invSR;
            if (modPhase[0] >= 1.0f) modPhase[0] -= 1.0f;
            if (modPhase[1] >= 1.0f) modPhase[1] -= 1.0f;

            const float dL = 4.0f + modDepth * (0.5f + 0.5f * std::sin (modPhase[0] * juce::MathConstants<float>::twoPi));
            const float dR = 4.0f + modDepth * (0.5f + 0.5f * std::sin (modPhase[1] * juce::MathConstants<float>::twoPi));

            modLine[0].pushSample (0, wL[i]);
            modLine[1].pushSample (0, wR[i]);
            wL[i] = modLine[0].popSample (0, dL, true);
            wR[i] = modLine[1].popSample (0, dR, true);
        }

        // Filters: high-pass (lowCut) and low-pass (highCut)
        for (int i = 0; i < n; ++i)
        {
            wL[i] = lowCut[0].processSample (wL[i]);
            wR[i] = lowCut[1].processSample (wR[i]);
            wL[i] = highCut[0].processSample (wL[i]);
            wR[i] = highCut[1].processSample (wR[i]);
        }

        // Stereo width
        const float widthAmount = juce::jlimit (0.0f, 1.5f, params.width);
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (wL[i] + wR[i]);
            const float s = 0.5f * (wL[i] - wR[i]) * widthAmount;
            wL[i] = m + s;
            wR[i] = m - s;
        }

        // ---- 3) Mix dry + wet (equal-power) & output gain ----
        const float mix = juce::jlimit (0.0f, 1.0f, params.mix);
        const float wetGain = std::sin (mix * juce::MathConstants<float>::halfPi);
        const float dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);
        const float outGain = juce::Decibels::decibelsToGain (params.outputDb);

        for (int i = 0; i < n; ++i)
        {
            const float dL = dryBuffer.getSample (0, i);
            const float dR = dryBuffer.getSample (1, i);

            const float oL = (dL * dryGain + wL[i] * wetGain) * outGain;
            const float oR = (dR * dryGain + wR[i] * wetGain) * outGain;

            L[i] = oL;
            if (R != nullptr) R[i] = oR;

            // Wet energy for visualization
            wetEnergyL = energyCoeff * wetEnergyL + (1.0f - energyCoeff) * std::abs (wL[i]);
            wetEnergyR = energyCoeff * wetEnergyR + (1.0f - energyCoeff) * std::abs (wR[i]);
        }
    }

    // Meters
    float getWetEnergyL() const noexcept   { return wetEnergyL; }
    float getWetEnergyR() const noexcept   { return wetEnergyR; }
    float getInputEnergy() const noexcept  { return inputEnergy; }

private:
    Parameters params;
    double currentSampleRate = 44100.0;
    int    currentBlock      = 512;
    int    predelaySamples   = 0;

    // Convolver from juce::dsp – partitioned, zero-latency by default
    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 256 } };

    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> preDelay;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> modLine;
    float modPhase[2] { 0.0f, 0.25f };

    juce::dsp::IIR::Filter<float> lowCut[2];   // high-pass at lowCutHz
    juce::dsp::IIR::Filter<float> highCut[2];  // low-pass at highCutHz

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;

    std::atomic<bool> irLoaded { false };

    float wetEnergyL = 0.0f;
    float wetEnergyR = 0.0f;
    float inputEnergy = 0.0f;
    static constexpr float energyCoeff = 0.997f;

    void applyParameters()
    {
        if (currentSampleRate <= 0) return;

        predelaySamples = juce::jlimit (0,
            (int) (0.55 * currentSampleRate),
            (int) (params.predelayMs * 0.001f * (float) currentSampleRate));

        const float lo = juce::jlimit (20.0f,  1000.0f, params.lowCutHz);
        const float hi = juce::jlimit (500.0f, 20000.0f, params.highCutHz);

        auto coLow  = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, lo);
        auto coHigh = juce::dsp::IIR::Coefficients<float>::makeLowPass  (currentSampleRate, hi);

        for (int ch = 0; ch < 2; ++ch)
        {
            *lowCut[ch].coefficients  = *coLow;
            *highCut[ch].coefficients = *coHigh;
        }
    }
};
