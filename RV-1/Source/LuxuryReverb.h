/*
  ==============================================================================

    LuxuryReverb.h
    A lush, musical reverb based on a modulated 8-line FDN with Schroeder
    allpass diffusion, per-line damping, pre-delay, tone shaping, and stereo
    width control. Designed to sound rich and three-dimensional rather than
    metallic or cheap.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class LuxuryReverb
{
public:
    enum Character { Hall = 0, Plate, Room, Chamber, Cathedral, Spring, NumCharacters };

    struct Parameters
    {
        float size       = 0.7f;     // 0..1   delay-time scaler
        float decay      = 0.6f;     // 0..1   maps logarithmically to T60
        float damping    = 0.4f;     // 0..1   high-frequency absorption in tail
        float predelayMs = 20.0f;    // 0..250 ms
        float modulation = 0.3f;     // 0..1   chorus-like motion in tail
        float diffusion  = 0.72f;    // 0..1   allpass coefficient
        float width      = 1.0f;     // 0..1   stereo width
        float mix        = 0.3f;     // 0..1   dry/wet
        float lowCutHz   = 80.0f;    // 20..1000
        float highCutHz  = 9000.0f;  // 500..20000
        int   character  = Hall;
    };

    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;

        const auto spec = juce::dsp::ProcessSpec {
            newSampleRate, (juce::uint32) maxBlockSize, 1
        };

        const int preMaxSamples = (int) (0.30 * newSampleRate) + 4;
        for (auto& d : preDelay) {
            d.setMaximumDelayInSamples (preMaxSamples);
            d.prepare (spec);
            d.reset();
        }

        const int apMaxSamples = 4096;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kAP; ++i)
            {
                ap[ch][i].setMaximumDelayInSamples (apMaxSamples);
                ap[ch][i].prepare (spec);
                ap[ch][i].reset();
            }

        const int fdnMaxSamples = (int) (0.40 * newSampleRate) + 4;
        for (int i = 0; i < kFdn; ++i)
        {
            fdn[i].setMaximumDelayInSamples (fdnMaxSamples);
            fdn[i].prepare (spec);
            fdn[i].reset();
            damper[i] = 0.0f;
            lfoPhase[i] = (float) i / (float) kFdn;
            lfoFreq[i]  = 0.13f + 0.071f * (float) i;
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            highCutFilter[ch].reset();
            lowCutFilter[ch].reset();
        }

        applyParameters();
    }

    void reset()
    {
        for (auto& d : preDelay) d.reset();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kAP; ++i)
                ap[ch][i].reset();
        for (int i = 0; i < kFdn; ++i)
        {
            fdn[i].reset();
            damper[i] = 0;
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            highCutFilter[ch].reset();
            lowCutFilter[ch].reset();
        }
        wetEnergyL = wetEnergyR = 0.0f;
    }

    void setParameters (const Parameters& p)
    {
        params = p;
        applyParameters();
    }

    const Parameters& getParameters() const { return params; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numChannels < 1 || n <= 0)
            return;

        auto* L = buffer.getWritePointer (0);
        auto* R = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        const float wetGain = std::sin (params.mix * juce::MathConstants<float>::halfPi);
        const float dryGain = std::cos (params.mix * juce::MathConstants<float>::halfPi);

        const float diff = juce::jlimit (0.0f, 0.85f, params.diffusion * 0.85f);

        for (int s = 0; s < n; ++s)
        {
            const float inL = L[s];
            const float inR = (R != nullptr) ? R[s] : inL;

            preDelay[0].pushSample (0, inL);
            preDelay[1].pushSample (0, inR);
            float pl = preDelay[0].popSample (0, (float) predelaySamples, true);
            float pr = preDelay[1].popSample (0, (float) predelaySamples, true);

            pl = highCutFilter[0].processSample (pl);
            pr = highCutFilter[1].processSample (pr);

            float dL = pl;
            float dR = pr;
            for (int i = 0; i < kAP; ++i)
            {
                dL = processAP (0, i, dL, apDelaysL[i], diff);
                dR = processAP (1, i, dR, apDelaysR[i], diff);
            }

            float out[kFdn];
            for (int i = 0; i < kFdn; ++i)
            {
                lfoPhase[i] += lfoFreq[i] / (float) sampleRate;
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                const float mod = std::sin (lfoPhase[i] * juce::MathConstants<float>::twoPi);

                float delaySamps = juce::jmax (2.0f, fdnDelays[i] + mod * modDepthSamples);
                if (delaySamps > (float) (fdn[i].getMaximumDelayInSamples() - 2))
                    delaySamps = (float) (fdn[i].getMaximumDelayInSamples() - 2);

                float v = fdn[i].popSample (0, delaySamps, true);

                damper[i] = dampingCoeff * damper[i] + (1.0f - dampingCoeff) * v;
                out[i] = damper[i];
            }

            float wL = (out[0] - out[3] + out[4] - out[7]) * 0.5f;
            float wR = (out[1] - out[2] + out[5] - out[6]) * 0.5f;

            const float mid  = 0.5f * (wL + wR);
            const float side = 0.5f * (wL - wR) * params.width;
            wL = mid + side;
            wR = mid - side;

            wL = lowCutFilter[0].processSample (wL);
            wR = lowCutFilter[1].processSample (wR);

            float v[kFdn];
            for (int i = 0; i < kFdn; ++i) v[i] = out[i];
            fwht8 (v);

            const float inj = 0.5f;
            v[0] += dL * inj;
            v[1] += dR * inj;
            v[2] += dL * (inj * 0.7f);
            v[3] -= dR * (inj * 0.7f);
            v[4] += dL * (inj * 0.5f);
            v[5] -= dR * (inj * 0.5f);
            v[6] += dL * (inj * 0.3f);
            v[7] += dR * (inj * 0.3f);

            for (int i = 0; i < kFdn; ++i)
                fdn[i].pushSample (0, v[i] * fdnGains[i]);

            const float outL = inL * dryGain + wL * wetGain;
            const float outR = inR * dryGain + wR * wetGain;
            L[s] = outL;
            if (R != nullptr) R[s] = outR;

            const float aL = std::abs (wL);
            const float aR = std::abs (wR);
            wetEnergyL = energyCoeff * wetEnergyL + (1.0f - energyCoeff) * aL;
            wetEnergyR = energyCoeff * wetEnergyR + (1.0f - energyCoeff) * aR;

            inputEnergy = energyCoeff * inputEnergy
                        + (1.0f - energyCoeff) * 0.5f * (std::abs (inL) + std::abs (inR));
        }
    }

    float getWetEnergyL() const noexcept   { return wetEnergyL; }
    float getWetEnergyR() const noexcept   { return wetEnergyR; }
    float getInputEnergy() const noexcept  { return inputEnergy; }

    float getApproxT60() const noexcept    { return currentT60; }
    float getEffectiveSize() const noexcept { return params.size; }

private:
    static constexpr int kFdn = 8;
    static constexpr int kAP  = 4;

    Parameters params;
    double sampleRate = 44100.0;

    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> preDelay;
    int predelaySamples = 0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> ap[2][kAP];
    int apDelaysL[kAP] = { 142, 379, 107, 277 };
    int apDelaysR[kAP] = { 149, 397, 113, 281 };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> fdn[kFdn];
    float fdnDelays[kFdn] {};
    float fdnGains[kFdn]  {};

    float damper[kFdn] {};
    float dampingCoeff = 0.5f;

    float lfoPhase[kFdn] {};
    float lfoFreq[kFdn]  {};
    float modDepthSamples = 0.0f;

    juce::dsp::IIR::Filter<float> highCutFilter[2];
    juce::dsp::IIR::Filter<float> lowCutFilter[2];

    float wetEnergyL = 0.0f;
    float wetEnergyR = 0.0f;
    float inputEnergy = 0.0f;
    static constexpr float energyCoeff = 0.998f;

    float currentT60 = 1.0f;

    inline float processAP (int ch, int i, float x, int delay, float g) noexcept
    {
        auto& d = ap[ch][i];
        const float z = d.popSample (0, (float) delay, true);
        const float v = x + g * z;
        d.pushSample (0, v);
        return -g * v + z;
    }

    static inline void fwht8 (float* a) noexcept
    {
        float t;
        // h = 1
        t = a[0] + a[1]; a[1] = a[0] - a[1]; a[0] = t;
        t = a[2] + a[3]; a[3] = a[2] - a[3]; a[2] = t;
        t = a[4] + a[5]; a[5] = a[4] - a[5]; a[4] = t;
        t = a[6] + a[7]; a[7] = a[6] - a[7]; a[6] = t;
        // h = 2
        t = a[0] + a[2]; a[2] = a[0] - a[2]; a[0] = t;
        t = a[1] + a[3]; a[3] = a[1] - a[3]; a[1] = t;
        t = a[4] + a[6]; a[6] = a[4] - a[6]; a[4] = t;
        t = a[5] + a[7]; a[7] = a[5] - a[7]; a[5] = t;
        // h = 4
        t = a[0] + a[4]; a[4] = a[0] - a[4]; a[0] = t;
        t = a[1] + a[5]; a[5] = a[1] - a[5]; a[1] = t;
        t = a[2] + a[6]; a[6] = a[2] - a[6]; a[2] = t;
        t = a[3] + a[7]; a[7] = a[3] - a[7]; a[3] = t;

        const float norm = 0.35355339059327373f; // 1/sqrt(8)
        for (int i = 0; i < 8; ++i) a[i] *= norm;
    }

    void applyParameters()
    {
        if (sampleRate <= 0) return;

        predelaySamples = juce::jlimit (0,
            (int) (0.30 * sampleRate),
            (int) (params.predelayMs * 0.001f * (float) sampleRate));

        // Per-character base delay templates (milliseconds, mostly primes for natural diffusion)
        static const float halls[kFdn]    = { 47.f, 53.f,  67.f,  71.f,  83.f,  89.f, 101.f, 109.f };
        static const float plates[kFdn]   = { 13.f, 17.f,  19.f,  23.f,  29.f,  31.f,  37.f,  41.f };
        static const float rooms[kFdn]    = { 23.f, 29.f,  31.f,  37.f,  41.f,  43.f,  47.f,  53.f };
        static const float chambers[kFdn] = { 31.f, 37.f,  41.f,  43.f,  47.f,  53.f,  59.f,  61.f };
        static const float caths[kFdn]    = { 71.f, 79.f,  89.f,  97.f, 113.f, 127.f, 139.f, 149.f };
        static const float springs[kFdn]  = {  5.f,  7.f,  11.f,  13.f,  17.f,  19.f,  23.f,  29.f };

        const float* base = halls;
        switch (params.character)
        {
            case Hall:      base = halls;    break;
            case Plate:     base = plates;   break;
            case Room:      base = rooms;    break;
            case Chamber:   base = chambers; break;
            case Cathedral: base = caths;    break;
            case Spring:    base = springs;  break;
        }

        const float sizeFactor = 0.45f + 1.55f * params.size; // 0.45..2.0

        // T60 logarithmic mapping: ~0.30 s at decay=0, ~20 s at decay=1
        const float t60 = std::pow (10.0f, 0.5f + 1.8f * params.decay) * 0.1f;
        currentT60 = juce::jmax (0.05f, t60);

        for (int i = 0; i < kFdn; ++i)
        {
            float ms = base[i] * sizeFactor;
            float samps = ms * 0.001f * (float) sampleRate;
            const float maxAllowed = (float) (fdn[i].getMaximumDelayInSamples() - 8);
            samps = juce::jlimit (8.0f, maxAllowed, samps);
            fdnDelays[i] = samps;

            const float g = std::pow (10.0f,
                -3.0f * (samps / (float) sampleRate) / currentT60);
            fdnGains[i] = juce::jlimit (0.0f, 0.999f, g);
        }

        dampingCoeff = juce::jlimit (0.0f, 0.97f, params.damping * 0.93f);

        modDepthSamples = params.modulation * 0.0035f * (float) sampleRate;
        if (params.character == Spring)
            modDepthSamples *= 1.8f;

        const float hi = juce::jlimit (500.0f, 20000.0f, params.highCutHz);
        const float lo = juce::jlimit (20.0f, 1000.0f, params.lowCutHz);
        auto coeffsHi = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, hi);
        auto coeffsLo = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, lo);
        for (int ch = 0; ch < 2; ++ch)
        {
            *highCutFilter[ch].coefficients = *coeffsHi;
            *lowCutFilter[ch].coefficients = *coeffsLo;
        }
    }
};
