/*
  ==============================================================================

    LuxuryPitchShifter.h
    A high-quality, musical pitch shifter built around overlapping
    Hann-windowed grains with cubic-Hermite interpolation, smooth
    pitch ramping, per-channel detune for rich stereo width, a
    shimmer-style feedback path, and gentle tone shaping for a
    luxurious, never-cheap result.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

class LuxuryPitchShifter
{
public:
    enum Character
    {
        Smooth = 0,    // medium grains, balanced
        Tight,         // shorter grains, snappier, tighter on transients
        Wide,          // longer grains + more detune
        Shimmer,       // typical octave-up shimmer voicing
        Crystal,       // long grains + bright tilt + more feedback
        NumCharacters
    };

    struct Parameters
    {
        float pitchSemitones = 0.0f;     // -24..+24
        float fineCents      = 0.0f;     // -50..+50
        float mix            = 1.0f;     // 0..1   dry/wet (equal-power)
        float feedback       = 0.0f;     // 0..0.85 shimmer
        float formant        = 0.0f;     // 0..1   tilt-EQ formant approximation
        float width          = 0.5f;     // 0..1   stereo detune
        float drive          = 0.0f;     // 0..1   subtle saturation
        float lowCutHz       = 30.0f;
        float highCutHz      = 18000.0f;
        int   character      = Smooth;
        float quality        = 0.6f;     // 0..1   grain length
    };

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;

        // Buffer ~ 0.5s, rounded up to power of two for masking.
        const int desired = (int) (newSampleRate * 0.5);
        bufSize = juce::nextPowerOfTwo (juce::jmax (16384, desired));
        bufMask = bufSize - 1;

        for (int ch = 0; ch < 2; ++ch)
        {
            buffer[ch].assign ((size_t) bufSize, 0.0f);
            writePos[ch] = 0;
            phase[ch]    = (ch == 0 ? 0.25f : 0.75f); // staggered start to keep stereo decorrelated
            ratioSmoothed[ch] = 1.0f;
        }
        feedback[0] = feedback[1] = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
        {
            lowCut[ch].reset();
            highCut[ch].reset();
            tilt[ch].reset();
            postLow[ch].reset();
        }

        applyParameters();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (buffer[ch].begin(), buffer[ch].end(), 0.0f);
            writePos[ch] = 0;
            phase[ch] = (ch == 0 ? 0.25f : 0.75f);
            ratioSmoothed[ch] = 1.0f;
            lowCut[ch].reset();
            highCut[ch].reset();
            tilt[ch].reset();
            postLow[ch].reset();
        }
        feedback[0] = feedback[1] = 0.0f;
        inputEnergy.store (0.0f);
        wetEnergyL.store (0.0f);
        wetEnergyR.store (0.0f);
        currentSemiSmoothed.store (0.0f);
    }

    void setParameters (const Parameters& p)
    {
        params = p;
        applyParameters();
    }

    Parameters getParameters() const noexcept { return params; }

    // Visualizer helpers
    float getInputEnergy()           const noexcept { return inputEnergy.load(); }
    float getWetEnergyL()            const noexcept { return wetEnergyL.load(); }
    float getWetEnergyR()            const noexcept { return wetEnergyR.load(); }
    float getCurrentSemitones()      const noexcept { return currentSemiSmoothed.load(); }
    float getCurrentPitchRatio()     const noexcept { return std::pow (2.0f, currentSemiSmoothed.load() / 12.0f); }
    float getGrainSizeMs()           const noexcept { return 1000.0f * (float) grainSize / (float) sampleRate; }

    void process (juce::AudioBuffer<float>& bus)
    {
        const int numCh = juce::jmin (2, bus.getNumChannels());
        const int numSm = bus.getNumSamples();
        if (numCh == 0 || numSm == 0) return;

        float in_e = 0.0f, w_eL = 0.0f, w_eR = 0.0f;

        const float wetMix  = std::sin (params.mix * juce::MathConstants<float>::halfPi);
        const float dryMix  = std::cos (params.mix * juce::MathConstants<float>::halfPi);
        const float fbAmt   = juce::jlimit (0.0f, 0.85f, params.feedback);
        const float driveK  = 1.0f + params.drive * 4.0f;
        const float driveCmp = 1.0f / std::tanh (driveK);

        const float targetSemi = params.pitchSemitones + params.fineCents * 0.01f;

        for (int n = 0; n < numSm; ++n)
        {
            const float dryL = bus.getReadPointer (0)[n];
            const float dryR = numCh > 1 ? bus.getReadPointer (1)[n] : dryL;

            // Pre-tone shaping (low/high cut on dry path before pitching)
            float fL = lowCut[0].processSample  (dryL);
            fL      = highCut[0].processSample (fL);
            float fR = lowCut[1].processSample  (dryR);
            fR      = highCut[1].processSample (fR);

            // Add feedback (one-sample delayed, post pitch + filter)
            const float pitchInL = fL + feedback[0];
            const float pitchInR = fR + feedback[1];

            // Pitch shift
            float wetL = pitchTick (0, pitchInL);
            float wetR = pitchTick (1, pitchInR);

            // Gentle tilt EQ to suggest formant compensation
            wetL = tilt[0].processSample (wetL);
            wetR = tilt[1].processSample (wetR);

            // Optional soft saturation for subtle warmth
            if (params.drive > 0.001f)
            {
                wetL = std::tanh (driveK * wetL) * driveCmp;
                wetR = std::tanh (driveK * wetR) * driveCmp;
            }

            // Tame brightness inside feedback loop so shimmer stays musical
            const float fbL = postLow[0].processSample (wetL);
            const float fbR = postLow[1].processSample (wetR);
            feedback[0] = juce::jlimit (-1.5f, 1.5f, fbL * fbAmt);
            feedback[1] = juce::jlimit (-1.5f, 1.5f, fbR * fbAmt);

            // Equal-power dry/wet blend
            const float outL = dryL * dryMix + wetL * wetMix;
            const float outR = dryR * dryMix + wetR * wetMix;

            bus.getWritePointer (0)[n] = outL;
            if (numCh > 1) bus.getWritePointer (1)[n] = outR;

            in_e += 0.5f * (dryL * dryL + dryR * dryR);
            w_eL += wetL * wetL;
            w_eR += wetR * wetR;
        }

        // Smooth the visualised pitch readout
        const float currentSemi = currentSemiSmoothed.load();
        const float smoothed = currentSemi + 0.05f * (targetSemi - currentSemi);
        currentSemiSmoothed.store (smoothed);

        const float inv = 1.0f / (float) juce::jmax (1, numSm);
        inputEnergy.store (std::sqrt (in_e * inv));
        wetEnergyL .store (std::sqrt (w_eL * inv));
        wetEnergyR .store (std::sqrt (w_eR * inv));
    }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefs  = juce::dsp::IIR::Coefficients<float>;

    double sampleRate = 44100.0;
    int    bufSize    = 0;
    int    bufMask    = 0;

    std::array<std::vector<float>, 2> buffer;
    std::array<int,   2> writePos      { 0, 0 };
    std::array<float, 2> phase         { 0.25f, 0.75f };
    std::array<float, 2> ratioSmoothed { 1.0f, 1.0f };

    Parameters params;
    float pitchRatioL = 1.0f, pitchRatioR = 1.0f;
    int   grainSize   = 4096;

    float feedback[2] { 0.0f, 0.0f };

    Filter lowCut[2], highCut[2], tilt[2], postLow[2];

    std::atomic<float> inputEnergy { 0.0f };
    std::atomic<float> wetEnergyL  { 0.0f };
    std::atomic<float> wetEnergyR  { 0.0f };
    std::atomic<float> currentSemiSmoothed { 0.0f };

    void applyParameters()
    {
        const float total = params.pitchSemitones + params.fineCents * 0.01f;
        const float baseRatio = std::pow (2.0f, total / 12.0f);

        // Stereo detune for luxurious width — gets more pronounced in Wide/Crystal modes
        float detuneCents = params.width * 9.0f;
        if (params.character == Wide)    detuneCents *= 1.4f;
        if (params.character == Crystal) detuneCents *= 1.2f;
        pitchRatioL = baseRatio * std::pow (2.0f, -detuneCents / 1200.0f);
        pitchRatioR = baseRatio * std::pow (2.0f,  detuneCents / 1200.0f);

        // Grain size by character (in ms)
        float grainMs = juce::jlimit (40.0f, 140.0f, 50.0f + params.quality * 70.0f);
        switch (params.character)
        {
            case Tight:   grainMs *= 0.55f; break;
            case Wide:    grainMs *= 1.10f; break;
            case Shimmer: grainMs *= 1.00f; break;
            case Crystal: grainMs *= 1.30f; break;
            case Smooth:
            default:                       break;
        }
        grainSize = (int) std::round (sampleRate * grainMs / 1000.0);
        grainSize = juce::jlimit (256, bufSize / 2, grainSize);

        // Pre filters
        const float lo = juce::jlimit (20.0f, 1000.0f, params.lowCutHz);
        const float hi = juce::jlimit (2000.0f, 20000.0f, juce::jmin ((float) sampleRate * 0.45f, params.highCutHz));
        const auto loC = Coefs::makeHighPass (sampleRate, lo);
        const auto hiC = Coefs::makeLowPass  (sampleRate, hi);

        // Tilt: a high-shelf that tilts opposite to pitch shift to suggest formants.
        // Up-shifts get darker; down-shifts get brighter. The amount is scaled by 'formant'.
        const float tiltDb  = -juce::jlimit (-1.0f, 1.0f, total / 12.0f) * 5.0f * params.formant;
        const float tiltGain = juce::Decibels::decibelsToGain (tiltDb);
        const auto  tC = Coefs::makeHighShelf (sampleRate, 1500.0f, 0.7071f, tiltGain);

        // Feedback path low-pass — keeps shimmer musical
        float fbCutoff = 6500.0f;
        if (params.character == Crystal) fbCutoff = 9000.0f;
        if (params.character == Tight)   fbCutoff = 5000.0f;
        const auto fbC = Coefs::makeLowPass (sampleRate, juce::jmin ((float) sampleRate * 0.45f, fbCutoff));

        for (int ch = 0; ch < 2; ++ch)
        {
            *lowCut[ch] .coefficients = *loC;
            *highCut[ch].coefficients = *hiC;
            *tilt[ch]   .coefficients = *tC;
            *postLow[ch].coefficients = *fbC;
        }
    }

    inline float pitchTick (int ch, float in)
    {
        // Write input into circular buffer
        buffer[ch][(size_t) writePos[ch]] = in;
        writePos[ch] = (writePos[ch] + 1) & bufMask;

        // Smooth pitch ratio per channel (avoids zipper noise)
        const float target = (ch == 0 ? pitchRatioL : pitchRatioR);
        ratioSmoothed[ch] += 0.0015f * (target - ratioSmoothed[ch]);
        const float r = ratioSmoothed[ch];

        // Phase increment: dDelay = 1 - r per sample, normalised by grain length
        phase[ch] += (1.0f - r) / (float) grainSize;
        while (phase[ch] >= 1.0f) phase[ch] -= 1.0f;
        while (phase[ch] <  0.0f) phase[ch] += 1.0f;

        const float p1 = phase[ch];
        const float p2 = (p1 < 0.5f) ? p1 + 0.5f : p1 - 0.5f;

        const float d1 = p1 * (float) grainSize;
        const float d2 = p2 * (float) grainSize;

        const float r1 = readSample (ch, d1);
        const float r2 = readSample (ch, d2);

        // Hann windows. sin^2(πp) for both phases sums to unity.
        const float w1 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p1);
        const float w2 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p2);

        return w1 * r1 + w2 * r2;
    }

    inline float readSample (int ch, float delay)
    {
        // Position behind write head
        const float fpos = (float) writePos[ch] - delay - 1.0f;
        const int   ifloor = (int) std::floor (fpos);
        const float frac   = fpos - (float) ifloor;

        const int i0  = ((ifloor % bufSize) + bufSize) & bufMask;
        const int im1 = (i0 - 1 + bufSize) & bufMask;
        const int ip1 = (i0 + 1) & bufMask;
        const int ip2 = (i0 + 2) & bufMask;

        const float xm1 = buffer[ch][(size_t) im1];
        const float x0  = buffer[ch][(size_t) i0];
        const float xp1 = buffer[ch][(size_t) ip1];
        const float xp2 = buffer[ch][(size_t) ip2];

        // Catmull-Rom / cubic Hermite interpolation
        const float c0 = x0;
        const float c1 = 0.5f * (xp1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * xp1 - 0.5f * xp2;
        const float c3 = 0.5f * (xp2 - xm1) + 1.5f * (x0 - xp1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
};
