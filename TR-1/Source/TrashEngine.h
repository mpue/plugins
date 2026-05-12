/*
  ==============================================================================

    TrashEngine.h
    A luxurious multi-stage destruction processor for TR-1.
    Six character modes drive a fully oversampled saturation chain followed by
    bit-crushing, a resonant character filter, noise texture, motion-LFO and
    a low-frequency body restoration network. Designed to sound huge, lush and
    musical at any setting — never cheap.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

class TrashEngine
{
public:
    enum Character
    {
        Tube = 0,
        Tape,
        Fuzz,
        Crush,
        Telephone,
        Radio,
        Mangler,
        VintageAmp,
        NumCharacters
    };

    struct Parameters
    {
        float drive       = 0.45f;   // 0..1   master destruction amount
        float crunch      = 0.0f;    // 0..1   bit + sample-rate reduction
        float tone        = 0.5f;    // 0..1   tilt EQ (0=dark, 0.5=flat, 1=bright)
        float body        = 0.5f;    // 0..1   low-end restoration after drive
        float texture     = 0.0f;    // 0..1   noise / grit amount
        float motion      = 0.0f;    // 0..1   filter LFO depth
        float motionRate  = 0.6f;    // 0..1   LFO speed (mapped 0.05..20 Hz)
        float age         = 0.25f;   // 0..1   speaker / cabinet darkening
        float width       = 1.0f;    // 0..1   stereo width
        float mix         = 1.0f;    // 0..1   dry/wet
        float outputDb    = 0.0f;    // -24..+12 dB output trim
        int   character   = Tube;    // mode
    };

    TrashEngine() = default;

    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        maxBlock = juce::jmax (1, maxBlockSize);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, kOsFactorLog2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);
        oversampler->initProcessing ((size_t) maxBlock);
        oversampler->reset();
        osRate = newSampleRate * (1 << kOsFactorLog2);

        juce::dsp::ProcessSpec specBase {
            newSampleRate, (juce::uint32) maxBlock, 2
        };
        juce::dsp::ProcessSpec specOs {
            osRate, (juce::uint32) (maxBlock * (1 << kOsFactorLog2)), 2
        };

        for (int ch = 0; ch < 2; ++ch)
        {
            preLow[ch].reset();      preLow[ch].prepare (specBase);
            preHigh[ch].reset();     preHigh[ch].prepare (specBase);

            // Inside the oversampled domain
            shaperHP[ch].reset();    shaperHP[ch].prepare (specOs);
            shaperLP[ch].reset();    shaperLP[ch].prepare (specOs);

            charFilter[ch].reset();  charFilter[ch].prepare (specBase);
            bodyFilter[ch].reset();  bodyFilter[ch].prepare (specBase);
            ageFilter[ch].reset();   ageFilter[ch].prepare (specBase);
            tiltLow[ch].reset();     tiltLow[ch].prepare (specBase);
            tiltHigh[ch].reset();    tiltHigh[ch].prepare (specBase);

            crushHold[ch] = 0.0f;
            crushPhase[ch] = 0.0f;
            envFollow[ch] = 0.0f;
        }

        lfoPhase = 0.0f;
        ringPhaseL = ringPhaseR = 0.0f;
        rng.setSeedRandomly();

        applyParameters();
    }

    void reset()
    {
        if (oversampler != nullptr) oversampler->reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            preLow[ch].reset();   preHigh[ch].reset();
            shaperHP[ch].reset(); shaperLP[ch].reset();
            charFilter[ch].reset(); bodyFilter[ch].reset();
            ageFilter[ch].reset();
            tiltLow[ch].reset();  tiltHigh[ch].reset();
            crushHold[ch] = 0.0f; crushPhase[ch] = 0.0f;
            envFollow[ch] = 0.0f;
        }
        lfoPhase = 0.0f;
        inMeterL = inMeterR = outMeterL = outMeterR = 0.0f;
        damageMeter = 0.0f;
    }

    void setParameters (const Parameters& p)
    {
        params = p;
        applyParameters();
    }

    const Parameters& getParameters() const noexcept { return params; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numChannels < 1 || n <= 0 || oversampler == nullptr) return;

        const float wetGain = std::sin (params.mix * juce::MathConstants<float>::halfPi);
        const float dryGain = std::cos (params.mix * juce::MathConstants<float>::halfPi);
        const float outGain = juce::Decibels::decibelsToGain (params.outputDb);

        auto* L = buffer.getWritePointer (0);
        auto* R = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        // Stash dry copy for the wet/dry mix.
        juce::AudioBuffer<float> dry (numChannels, n);
        for (int ch = 0; ch < numChannels; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, n);

        // Pre-stage tilt + tone shaping at base rate
        for (int s = 0; s < n; ++s)
        {
            float inL = L[s];
            float inR = R != nullptr ? R[s] : inL;

            inMeterL = 0.995f * inMeterL + 0.005f * std::abs (inL);
            inMeterR = 0.995f * inMeterR + 0.005f * std::abs (inR);

            // Tilt EQ: tone < 0.5 dampens highs, > 0.5 dampens lows
            float tL = tiltLow[0].processSample (inL);
            float tR = tiltLow[1].processSample (inR);
            float hL = tiltHigh[0].processSample (inL);
            float hR = tiltHigh[1].processSample (inR);
            const float tilt = (params.tone - 0.5f) * 2.0f;       // -1..+1
            const float lowGain  = juce::jlimit (0.0f, 1.5f, 1.0f - tilt * 0.6f);
            const float highGain = juce::jlimit (0.0f, 1.5f, 1.0f + tilt * 0.6f);
            inL = tL * lowGain + (inL - tL) * highGain;
            inR = tR * lowGain + (inR - tR) * highGain;
            juce::ignoreUnused (hL, hR);

            // Pre input shaping (gentle highpass to keep DC out of drive stage)
            inL = preHigh[0].processSample (inL);
            inR = preHigh[1].processSample (inR);

            L[s] = inL * inputDriveGain;
            if (R != nullptr) R[s] = inR * inputDriveGain;
        }

        // Oversampled non-linear stage
        juce::dsp::AudioBlock<float> baseBlock (buffer);
        auto osBlock = oversampler->processSamplesUp (baseBlock);
        const int osN = (int) osBlock.getNumSamples();

        auto* osL = osBlock.getChannelPointer (0);
        auto* osR = osBlock.getNumChannels() > 1 ? osBlock.getChannelPointer (1) : osL;

        for (int s = 0; s < osN; ++s)
        {
            float xL = osL[s];
            float xR = osR[s];

            // High-pass before non-linearity to suppress DC build-up
            xL = shaperHP[0].processSample (xL);
            xR = shaperHP[1].processSample (xR);

            // Character drive
            xL = driveSample (xL, 0);
            xR = driveSample (xR, 1);

            // Tame the brittle highs created by the saturator
            xL = shaperLP[0].processSample (xL);
            xR = shaperLP[1].processSample (xR);

            osL[s] = xL;
            osR[s] = xR;
        }

        oversampler->processSamplesDown (baseBlock);

        // Post stage: crunch, character filter, motion, body, age, noise, width, output
        const float lfoInc = lfoFreq / (float) sampleRate;
        const float ringInc = ringFreq / (float) sampleRate;

        for (int s = 0; s < n; ++s)
        {
            float wL = L[s];
            float wR = R != nullptr ? R[s] : wL;

            // ── Bit / SR crunch ────────────────────────────────────────────
            if (crushAmount > 0.001f)
            {
                const float bits = juce::jmap (crushAmount, 0.0f, 1.0f, 16.0f, 4.0f);
                const float steps = std::pow (2.0f, bits) * 0.5f;
                const float srRatio = juce::jmap (crushAmount, 0.0f, 1.0f, 1.0f, 0.06f);

                crushPhase[0] += srRatio;
                if (crushPhase[0] >= 1.0f)
                {
                    crushPhase[0] -= 1.0f;
                    crushHold[0] = std::round (wL * steps) / steps;
                }
                crushPhase[1] += srRatio;
                if (crushPhase[1] >= 1.0f)
                {
                    crushPhase[1] -= 1.0f;
                    crushHold[1] = std::round (wR * steps) / steps;
                }

                wL = juce::jmap (crushAmount, 0.0f, 1.0f, wL, crushHold[0]);
                wR = juce::jmap (crushAmount, 0.0f, 1.0f, wR, crushHold[1]);
            }

            // ── Motion LFO ────────────────────────────────────────────────
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            const float lfo = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);

            // ── Character resonant filter ─────────────────────────────────
            if (filterEnabled)
            {
                const float modCutoff = baseCutoffHz
                    * std::pow (2.0f, lfo * params.motion * 1.5f);
                const float clamped = juce::jlimit (40.0f, (float) sampleRate * 0.45f, modCutoff);

                if (std::abs (clamped - currentCutoff) > 0.5f)
                {
                    currentCutoff = clamped;
                    auto coeffs = makeCharacterCoeffs (currentCutoff, currentResonance);
                    *charFilter[0].coefficients = *coeffs;
                    *charFilter[1].coefficients = *coeffs;
                }

                wL = charFilter[0].processSample (wL);
                wR = charFilter[1].processSample (wR);
            }

            // ── Noise / texture (gated by envelope) ───────────────────────
            if (params.texture > 0.001f)
            {
                envFollow[0] = envCoeff * envFollow[0] + (1.0f - envCoeff) * std::abs (wL);
                envFollow[1] = envCoeff * envFollow[1] + (1.0f - envCoeff) * std::abs (wR);

                const float gate = juce::jmin (1.0f, (envFollow[0] + envFollow[1]) * 8.0f);
                const float noiseAmp = params.texture * 0.20f * gate;
                const float nL = (rng.nextFloat() * 2.0f - 1.0f) * noiseAmp;
                const float nR = (rng.nextFloat() * 2.0f - 1.0f) * noiseAmp;

                // High-pass the noise so it sits as "air" not rumble
                wL += nL - 0.92f * lastNoiseL;
                wR += nR - 0.92f * lastNoiseR;
                lastNoiseL = nL;
                lastNoiseR = nR;
            }

            // Radio mode: subtle ring modulation for AM character
            if (params.character == Radio && params.drive > 0.15f)
            {
                ringPhaseL += ringInc;
                if (ringPhaseL >= 1.0f) ringPhaseL -= 1.0f;
                ringPhaseR += ringInc * 1.001f;
                if (ringPhaseR >= 1.0f) ringPhaseR -= 1.0f;
                const float depth = juce::jlimit (0.0f, 0.35f, params.drive * 0.45f - 0.05f);
                wL *= 1.0f - depth + depth * std::sin (ringPhaseL * juce::MathConstants<float>::twoPi);
                wR *= 1.0f - depth + depth * std::sin (ringPhaseR * juce::MathConstants<float>::twoPi);
            }

            // ── Body restoration (low-shelf bump) ─────────────────────────
            wL = bodyFilter[0].processSample (wL);
            wR = bodyFilter[1].processSample (wR);

            // ── Age / cabinet darkening (low-pass) ────────────────────────
            wL = ageFilter[0].processSample (wL);
            wR = ageFilter[1].processSample (wR);

            // ── Stereo width (M/S) ────────────────────────────────────────
            const float mid  = 0.5f * (wL + wR);
            const float side = 0.5f * (wL - wR) * (params.width * 1.5f);
            wL = mid + side;
            wR = mid - side;

            // ── Output trim ───────────────────────────────────────────────
            wL *= outGain;
            wR *= outGain;

            // ── Soft safety limit ─────────────────────────────────────────
            wL = std::tanh (wL * 0.7f) * 1.4286f;
            wR = std::tanh (wR * 0.7f) * 1.4286f;

            // ── Wet/dry mix with original ────────────────────────────────
            const float dL = dry.getReadPointer (0)[s];
            const float dR = numChannels > 1 ? dry.getReadPointer (1)[s] : dL;
            const float outL = dL * dryGain + wL * wetGain;
            const float outR = dR * dryGain + wR * wetGain;

            L[s] = outL;
            if (R != nullptr) R[s] = outR;

            outMeterL = 0.995f * outMeterL + 0.005f * std::abs (outL);
            outMeterR = 0.995f * outMeterR + 0.005f * std::abs (outR);

            const float damage = juce::jlimit (0.0f, 1.0f,
                std::abs (outL - dL) + std::abs (outR - dR));
            damageMeter = 0.998f * damageMeter + 0.002f * damage;
        }
    }

    // ── Realtime telemetry for the visualizer ────────────────────────────
    float getInputLevelL()  const noexcept { return inMeterL; }
    float getInputLevelR()  const noexcept { return inMeterR; }
    float getOutputLevelL() const noexcept { return outMeterL; }
    float getOutputLevelR() const noexcept { return outMeterR; }
    float getDamageMeter()  const noexcept { return damageMeter; }
    float getLfoPhase()     const noexcept { return lfoPhase; }
    float getCurrentCutoff() const noexcept { return currentCutoff; }

    // Sampling the shaper transfer curve for the UI
    float sampleTransfer (float x) const noexcept
    {
        TrashEngine* mut = const_cast<TrashEngine*> (this);
        return mut->driveSamplePure (x);
    }

private:
    static constexpr int kOsFactorLog2 = 2; // 4x oversampling

    Parameters params;

    double sampleRate = 44100.0;
    double osRate     = 176400.0;
    int    maxBlock   = 512;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::dsp::IIR::Filter<float> preLow[2], preHigh[2];
    juce::dsp::IIR::Filter<float> shaperHP[2], shaperLP[2];
    juce::dsp::IIR::Filter<float> charFilter[2];
    juce::dsp::IIR::Filter<float> bodyFilter[2];
    juce::dsp::IIR::Filter<float> ageFilter[2];
    juce::dsp::IIR::Filter<float> tiltLow[2], tiltHigh[2];

    float crushHold[2]   {};
    float crushPhase[2]  {};
    float crushAmount    = 0.0f;

    float envFollow[2]   {};
    float lastNoiseL = 0.0f, lastNoiseR = 0.0f;
    static constexpr float envCoeff = 0.997f;

    juce::Random rng;

    float lfoPhase = 0.0f;
    float lfoFreq  = 0.5f;
    float ringPhaseL = 0.0f, ringPhaseR = 0.0f;
    float ringFreq = 60.0f;

    float baseCutoffHz   = 1500.0f;
    float currentCutoff  = 1500.0f;
    float currentResonance = 0.7f;
    bool  filterEnabled  = true;

    float inputDriveGain = 1.0f;
    float makeupGain     = 1.0f;
    float biasOffset     = 0.0f;

    // Telemetry
    float inMeterL = 0.0f, inMeterR = 0.0f;
    float outMeterL = 0.0f, outMeterR = 0.0f;
    float damageMeter = 0.0f;

    juce::dsp::IIR::Coefficients<float>::Ptr makeCharacterCoeffs (float cutoff, float q) const
    {
        switch (params.character)
        {
            case Telephone:
                return juce::dsp::IIR::Coefficients<float>::makeBandPass ((double) sampleRate, cutoff, q);
            case Radio:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter ((double) sampleRate, cutoff, q, 1.7f);
            case Tape:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass ((double) sampleRate, cutoff, q);
            case Crush:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass ((double) sampleRate, cutoff, q);
            case Mangler:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass ((double) sampleRate, cutoff, juce::jmax (q, 1.6f));
            case Fuzz:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter ((double) sampleRate, cutoff, q, 1.4f);
            case VintageAmp:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter ((double) sampleRate, cutoff, q, 1.5f);
            case Tube:
            default:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass ((double) sampleRate, cutoff, q);
        }
    }

    inline float driveSample (float x, int /*channel*/) noexcept
    {
        return driveSamplePure (x);
    }

    // Pure non-linearity (no filter state) — used for both the audio path
    // and the UI transfer curve.
    inline float driveSamplePure (float x) noexcept
    {
        const float drv = juce::jlimit (0.0f, 1.0f, params.drive);
        switch (params.character)
        {
            case Tube:
            {
                const float k = 1.0f + drv * 18.0f;
                const float bias = 0.10f * drv;
                const float y = std::tanh ((x + bias) * k) - std::tanh (bias * k);
                return y * (0.85f + 0.15f * (1.0f - drv)) * makeupGain;
            }
            case Tape:
            {
                const float k = 1.0f + drv * 8.0f;
                const float xk = x * k;
                const float y = xk / std::sqrt (1.0f + xk * xk * 0.85f);
                return y * 0.95f * makeupGain;
            }
            case Fuzz:
            {
                const float k = 1.0f + drv * 50.0f;
                const float bias = 0.20f * drv;
                float y = (x + bias) * k;
                y = juce::jlimit (-1.0f, 1.0f, y);
                y = y - bias * k * 0.5f;
                return std::tanh (y * 0.9f) * 0.85f * makeupGain;
            }
            case Crush:
            {
                const float k = 1.0f + drv * 6.0f;
                const float xk = x * k;
                const float y = std::tanh (xk * 1.4f);
                return y * 0.9f * makeupGain;
            }
            case Telephone:
            {
                const float k = 1.0f + drv * 22.0f;
                const float xk = x * k;
                const float y = xk / (1.0f + std::abs (xk));
                return y * 0.95f * makeupGain;
            }
            case Radio:
            {
                const float k = 1.0f + drv * 14.0f;
                const float xk = x * k;
                const float y = std::tanh (xk) - 0.30f * std::tanh (xk * 0.5f);
                return y * 0.95f * makeupGain;
            }
            case Mangler:
            {
                // Sin-based wave folder. Drive multiplies the angular frequency,
                // producing odd harmonics that bloom into chaos but stay bounded.
                const float k = 1.0f + drv * 9.0f;
                const float xk = x * k;
                const float folded = std::sin (xk * 1.2f);
                return folded * (0.55f + 0.35f * (1.0f - drv)) * makeupGain;
            }
            case VintageAmp:
            default:
            {
                const float k = 1.0f + drv * 15.0f;
                const float bias = 0.18f * drv;
                const float y = std::tanh ((x + bias) * k) - std::tanh (bias * k);
                const float secondHarm = 0.10f * drv * (y * y) * (x >= 0.0f ? 1.0f : -1.0f);
                return (y + secondHarm) * 0.95f * makeupGain;
            }
        }
    }

    void applyParameters()
    {
        if (sampleRate <= 0) return;

        params.character = juce::jlimit (0, (int) NumCharacters - 1, params.character);

        // Pre-DC blocker (gentle 30 Hz HPF on the way in)
        auto coeffsHP = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 30.0f);
        for (int ch = 0; ch < 2; ++ch)
            *preHigh[ch].coefficients = *coeffsHP;

        // Tilt EQ split frequency
        auto coeffsTilt = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 700.0f);
        for (int ch = 0; ch < 2; ++ch)
            *tiltLow[ch].coefficients = *coeffsTilt;

        // Drive amount → input gain trim
        const float drvDb = juce::jmap (params.drive, 0.0f, 1.0f, 0.0f, 22.0f);
        inputDriveGain = juce::Decibels::decibelsToGain (drvDb);

        // Per-character makeup compensation so output level stays musical
        const float drvSq = params.drive * params.drive;
        switch (params.character)
        {
            case Tube:       makeupGain = 1.0f - drvSq * 0.40f; break;
            case Tape:       makeupGain = 1.0f - drvSq * 0.20f; break;
            case Fuzz:       makeupGain = 1.0f - drvSq * 0.55f; break;
            case Crush:      makeupGain = 1.0f - drvSq * 0.30f; break;
            case Telephone:  makeupGain = 1.0f - drvSq * 0.35f; break;
            case Radio:      makeupGain = 1.0f - drvSq * 0.40f; break;
            case Mangler:    makeupGain = 1.0f - drvSq * 0.55f; break;
            case VintageAmp: makeupGain = 1.0f - drvSq * 0.40f; break;
            default:         makeupGain = 1.0f;                 break;
        }
        makeupGain = juce::jlimit (0.30f, 1.5f, makeupGain);

        // Oversampled-domain shaping filters (DC blocker before, brilliance tame after)
        auto osHP = juce::dsp::IIR::Coefficients<float>::makeHighPass (osRate, 25.0f);
        auto osLP = juce::dsp::IIR::Coefficients<float>::makeLowPass (osRate,
            juce::jmin (osRate * 0.45, 18000.0));
        for (int ch = 0; ch < 2; ++ch)
        {
            *shaperHP[ch].coefficients = *osHP;
            *shaperLP[ch].coefficients = *osLP;
        }

        // Character resonant filter — base cutoff varies per mode
        switch (params.character)
        {
            case Telephone: baseCutoffHz = 1300.0f; currentResonance = 1.6f; filterEnabled = true; break;
            case Radio:     baseCutoffHz = 2400.0f; currentResonance = 1.2f; filterEnabled = true; break;
            case Crush:     baseCutoffHz = juce::jmap (1.0f - params.crunch, 0.0f, 1.0f, 1200.0f, 8000.0f); currentResonance = 0.7f; filterEnabled = true; break;
            case Tape:      baseCutoffHz = 9000.0f; currentResonance = 0.6f; filterEnabled = true; break;
            case Mangler:   baseCutoffHz = 2200.0f; currentResonance = 1.8f; filterEnabled = true; break;
            case Fuzz:      baseCutoffHz = 3200.0f; currentResonance = 1.3f; filterEnabled = true; break;
            case VintageAmp:baseCutoffHz = 3500.0f; currentResonance = 1.4f; filterEnabled = true; break;
            case Tube:
            default:        baseCutoffHz = 7000.0f; currentResonance = 0.8f; filterEnabled = true; break;
        }
        currentCutoff = baseCutoffHz;
        auto coeffsC = makeCharacterCoeffs (currentCutoff, currentResonance);
        for (int ch = 0; ch < 2; ++ch)
            *charFilter[ch].coefficients = *coeffsC;

        // Crunch
        crushAmount = juce::jlimit (0.0f, 1.0f, params.crunch);

        // Motion LFO rate: 0.05..20 Hz exponential
        lfoFreq = std::pow (10.0f, juce::jmap (params.motionRate, 0.0f, 1.0f, -1.30f, 1.30f));

        // Body restoration: low-shelf bump centered at 120 Hz, scaled by drive too
        const float bodyDb = juce::jmap (params.body, 0.0f, 1.0f, -3.0f, 7.5f)
                           + params.drive * 1.5f;
        auto coeffsBody = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 120.0f, 0.7f, juce::Decibels::decibelsToGain (bodyDb));
        for (int ch = 0; ch < 2; ++ch)
            *bodyFilter[ch].coefficients = *coeffsBody;

        // Age: fixed-Q lowpass that closes from ~18kHz → ~3.5kHz
        const float ageHz = juce::jmap (params.age, 0.0f, 1.0f, 18000.0f, 3500.0f);
        auto coeffsAge = juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sampleRate, juce::jmin ((double) ageHz, sampleRate * 0.45), 0.5f);
        for (int ch = 0; ch < 2; ++ch)
            *ageFilter[ch].coefficients = *coeffsAge;

        // Ring-mod carrier for Radio mode
        ringFreq = juce::jmap (params.drive, 0.0f, 1.0f, 30.0f, 80.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrashEngine)
};
