/*
  ==============================================================================

    LuxuryStereoWidener.h
    Premium stereo widening engine: M/S processing, 3-band Linkwitz-Riley
    (24 dB/oct) crossovers with per-band width, bass-mono filter,
    all-pass decorrelation network ("shimmer"), Haas micro-delay,
    rotation matrix, and a lock-free goniometer feed for visualisation.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

class LuxuryStereoWidener
{
public:
    struct Parameters
    {
        float widthPct       = 100.0f;   // 0..200 (%)
        float lowWidthPct    = 100.0f;
        float midWidthPct    = 100.0f;
        float highWidthPct   = 100.0f;

        float xLowHz         = 250.0f;
        float xHighHz        = 3500.0f;

        float bassMonoHz     = 120.0f;
        bool  bassMonoOn     = true;

        float shimmer        = 0.0f;     // 0..1
        float haasMs         = 0.0f;     // -20..+20
        float rotationDeg    = 0.0f;     // -45..+45

        float outputDb       = 0.0f;     // -24..+24
        float mix            = 1.0f;     // 0..1
        bool  bypass         = false;
        bool  monoCheck      = false;
    };

    //==============================================================================
    // Lock-free goniometer ring used by the visualiser
    static constexpr int VisualSize = 2048;

    struct VisualState
    {
        std::array<float, VisualSize> bufL {};
        std::array<float, VisualSize> bufR {};
        std::atomic<int> writeIdx { 0 };

        std::atomic<float> correlation { 1.0f };
        std::atomic<float> peakL { 0.0f };
        std::atomic<float> peakR { 0.0f };
        std::atomic<float> peakM { 0.0f };
        std::atomic<float> peakS { 0.0f };
    };

    LuxuryStereoWidener() = default;

    //==============================================================================
    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;

        // Haas delays — 30 ms head-room each side
        const int haasMaxSamples = (int) std::ceil (0.030 * sr) + 4;
        haasL.resize ((size_t) haasMaxSamples, 0.0f);
        haasR.resize ((size_t) haasMaxSamples, 0.0f);
        haasMaxLen = haasMaxSamples;
        haasWriteIdx = 0;

        // Crossovers
        for (int ch = 0; ch < 2; ++ch)
        {
            xoLow[ch].prepare (sr, params.xLowHz);
            xoHigh[ch].prepare (sr, params.xHighHz);
        }

        // Bass-mono LP on the side signal (single-pole)
        bassMonoLP.reset();

        // Shimmer all-pass chain
        for (int ch = 0; ch < 2; ++ch)
            for (auto& ap : shimmerChain[ch])
                ap.reset();

        configureShimmer();
        updateBassMonoCoeff();

        for (auto& a : visual.bufL) a = 0.0f;
        for (auto& a : visual.bufR) a = 0.0f;
        visual.writeIdx.store (0);
        visual.correlation.store (1.0f);
        visual.peakL.store (0.0f);
        visual.peakR.store (0.0f);
        visual.peakM.store (0.0f);
        visual.peakS.store (0.0f);

        smoothL.reset (sr, 0.005);
        smoothR.reset (sr, 0.005);
        smoothLowW.reset (sr, 0.020);
        smoothMidW.reset (sr, 0.020);
        smoothHighW.reset (sr, 0.020);
        smoothMasterW.reset (sr, 0.020);
        smoothShimmer.reset (sr, 0.030);
        smoothMix.reset (sr, 0.030);
        smoothOutGain.reset (sr, 0.030);
        smoothRotCos.reset (sr, 0.030);
        smoothRotSin.reset (sr, 0.030);

        smoothLowW.setCurrentAndTargetValue (params.lowWidthPct  * 0.01f);
        smoothMidW.setCurrentAndTargetValue (params.midWidthPct  * 0.01f);
        smoothHighW.setCurrentAndTargetValue (params.highWidthPct * 0.01f);
        smoothMasterW.setCurrentAndTargetValue (params.widthPct  * 0.01f);
        smoothShimmer.setCurrentAndTargetValue (params.shimmer);
        smoothMix.setCurrentAndTargetValue (params.mix);
        smoothOutGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputDb));
        const float rad = juce::degreesToRadians (params.rotationDeg);
        smoothRotCos.setCurrentAndTargetValue (std::cos (rad));
        smoothRotSin.setCurrentAndTargetValue (std::sin (rad));
    }

    void reset()
    {
        std::fill (haasL.begin(), haasL.end(), 0.0f);
        std::fill (haasR.begin(), haasR.end(), 0.0f);
        haasWriteIdx = 0;

        for (int ch = 0; ch < 2; ++ch)
        {
            xoLow[ch].reset();
            xoHigh[ch].reset();
            for (auto& ap : shimmerChain[ch]) ap.reset();
        }

        bassMonoLP.reset();

        for (auto& a : visual.bufL) a = 0.0f;
        for (auto& a : visual.bufR) a = 0.0f;
        visual.writeIdx.store (0);
    }

    void setParameters (const Parameters& p)
    {
        const bool xoChanged = (std::abs (p.xLowHz  - params.xLowHz)  > 0.01f)
                            || (std::abs (p.xHighHz - params.xHighHz) > 0.01f);
        const bool bassChanged = std::abs (p.bassMonoHz - params.bassMonoHz) > 0.01f
                              || p.bassMonoOn != params.bassMonoOn;

        params = p;

        if (xoChanged)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                xoLow[ch].prepare (sampleRate, params.xLowHz);
                xoHigh[ch].prepare (sampleRate, params.xHighHz);
            }
        }
        if (bassChanged) updateBassMonoCoeff();

        const float rad = juce::degreesToRadians (params.rotationDeg);
        smoothRotCos.setTargetValue (std::cos (rad));
        smoothRotSin.setTargetValue (std::sin (rad));

        smoothLowW.setTargetValue   (params.lowWidthPct  * 0.01f);
        smoothMidW.setTargetValue   (params.midWidthPct  * 0.01f);
        smoothHighW.setTargetValue  (params.highWidthPct * 0.01f);
        smoothMasterW.setTargetValue(params.widthPct     * 0.01f);
        smoothShimmer.setTargetValue(params.shimmer);
        smoothMix.setTargetValue    (params.mix);
        smoothOutGain.setTargetValue(juce::Decibels::decibelsToGain (params.outputDb));
    }

    const VisualState& getVisualState() const noexcept { return visual; }

    //==============================================================================
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        if (numCh < 2) return;
        const int n = buffer.getNumSamples();

        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);

        // Per-block running stats for visualiser
        double sumL2 = 0.0, sumR2 = 0.0, sumLR = 0.0;
        float pL = 0.0f, pR = 0.0f, pM = 0.0f, pS = 0.0f;

        // Haas read positions
        const float haasSamps = (params.haasMs * 0.001f) * (float) sampleRate;
        const float delayL = haasSamps < 0.0f ? -haasSamps : 0.0f;
        const float delayR = haasSamps > 0.0f ?  haasSamps : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float inL = L[i];
            float inR = R[i];

            float dryL = inL;
            float dryR = inR;

            if (params.bypass)
            {
                accumulateVisual (inL, inR, sumL2, sumR2, sumLR, pL, pR, pM, pS);
                continue;
            }

            // ---------------- Haas micro-delay ----------------
            haasL[(size_t) haasWriteIdx] = inL;
            haasR[(size_t) haasWriteIdx] = inR;
            float haasOutL = readHaas (haasL, delayL);
            float haasOutR = readHaas (haasR, delayR);
            haasWriteIdx = (haasWriteIdx + 1) % haasMaxLen;

            float sigL = haasOutL;
            float sigR = haasOutR;

            // ---------------- Multi-band split (LR4) ----------
            float lowL, midHighL, lowR, midHighR;
            xoLow[0].process (sigL, lowL, midHighL);
            xoLow[1].process (sigR, lowR, midHighR);

            float midL, highL, midR, highR;
            xoHigh[0].process (midHighL, midL, highL);
            xoHigh[1].process (midHighR, midR, highR);

            // ---------------- Per-band M/S widening ------------
            const float wLow  = smoothLowW.getNextValue();
            const float wMid  = smoothMidW.getNextValue();
            const float wHigh = smoothHighW.getNextValue();
            const float wMst  = smoothMasterW.getNextValue();

            auto applyMS = [] (float& l, float& r, float scale)
            {
                const float m = 0.5f * (l + r);
                const float s = 0.5f * (l - r) * scale;
                l = m + s;
                r = m - s;
            };

            applyMS (lowL,  lowR,  wLow  * wMst);
            applyMS (midL,  midR,  wMid  * wMst);
            applyMS (highL, highR, wHigh * wMst);

            // ---------------- Bass-mono on the SIDE of low band
            // (collapses low-frequency side energy below the cutoff for tight bass)
            if (params.bassMonoOn)
            {
                const float mLow    = 0.5f * (lowL + lowR);
                const float sLow    = 0.5f * (lowL - lowR);
                const float sLowLP  = bassMonoLP.processSample (sLow);
                const float sideKept = sLow - sLowLP;
                lowL = mLow + sideKept;
                lowR = mLow - sideKept;
            }

            // ---------------- Recombine ------------------------
            float outL = lowL + midL + highL;
            float outR = lowR + midR + highR;

            // ---------------- Field Rotation (M/S domain) ------
            const float c = smoothRotCos.getNextValue();
            const float s = smoothRotSin.getNextValue();
            {
                float m = 0.5f * (outL + outR);
                float sd = 0.5f * (outL - outR);
                float mR =  c * m + s * sd;
                float sR = -s * m + c * sd;
                outL = mR + sR;
                outR = mR - sR;
            }

            // ---------------- Shimmer (decorrelation) ----------
            const float shim = smoothShimmer.getNextValue();
            if (shim > 0.0001f)
            {
                float decL = outL;
                float decR = outR;
                for (auto& ap : shimmerChain[0]) decL = ap.processOne (decL);
                for (auto& ap : shimmerChain[1]) decR = ap.processOne (decR);

                outL = outL * (1.0f - 0.5f * shim) + decL * (0.5f * shim);
                outR = outR * (1.0f - 0.5f * shim) + decR * (0.5f * shim);
            }

            // ---------------- Mono check / Output --------------
            if (params.monoCheck)
            {
                float mono = 0.5f * (outL + outR);
                outL = mono;
                outR = mono;
            }

            const float gain = smoothOutGain.getNextValue();
            outL *= gain;
            outR *= gain;

            const float mix = smoothMix.getNextValue();
            outL = dryL * (1.0f - mix) + outL * mix;
            outR = dryR * (1.0f - mix) + outR * mix;

            L[i] = outL;
            R[i] = outR;

            accumulateVisual (outL, outR, sumL2, sumR2, sumLR, pL, pR, pM, pS);
        }

        // Update visualiser stats
        if (n > 0)
        {
            const double denom = std::sqrt (std::max (sumL2 * sumR2, 1.0e-12));
            const float corr   = (float) (sumLR / denom);
            visual.correlation.store (juce::jlimit (-1.0f, 1.0f, corr), std::memory_order_relaxed);
            visual.peakL.store (smooth (visual.peakL.load (std::memory_order_relaxed), pL), std::memory_order_relaxed);
            visual.peakR.store (smooth (visual.peakR.load (std::memory_order_relaxed), pR), std::memory_order_relaxed);
            visual.peakM.store (smooth (visual.peakM.load (std::memory_order_relaxed), pM), std::memory_order_relaxed);
            visual.peakS.store (smooth (visual.peakS.load (std::memory_order_relaxed), pS), std::memory_order_relaxed);
        }
    }

private:
    //==============================================================================
    static float smooth (float prev, float now)
    {
        return now > prev ? now : prev * 0.85f + now * 0.15f;
    }

    void accumulateVisual (float l, float r,
                           double& sumL2, double& sumR2, double& sumLR,
                           float& pL, float& pR, float& pM, float& pS)
    {
        // Push to ring
        const int idx = visual.writeIdx.load (std::memory_order_relaxed);
        visual.bufL[(size_t) idx] = l;
        visual.bufR[(size_t) idx] = r;
        visual.writeIdx.store ((idx + 1) % VisualSize, std::memory_order_release);

        sumL2 += (double) l * l;
        sumR2 += (double) r * r;
        sumLR += (double) l * r;

        pL = std::max (pL, std::abs (l));
        pR = std::max (pR, std::abs (r));
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r);
        pM = std::max (pM, std::abs (m));
        pS = std::max (pS, std::abs (s));
    }

    float readHaas (const std::vector<float>& buf, float delaySamples) const
    {
        if (delaySamples <= 0.0f)
        {
            // direct read
            const int idx = (haasWriteIdx - 0 + haasMaxLen) % haasMaxLen;
            return buf[(size_t) idx];
        }
        const float pos = (float) haasWriteIdx - delaySamples;
        int   p0 = (int) std::floor (pos);
        float frac = pos - (float) p0;
        while (p0 < 0) p0 += haasMaxLen;
        p0 %= haasMaxLen;
        const int p1 = (p0 + 1) % haasMaxLen;
        return buf[(size_t) p0] * (1.0f - frac) + buf[(size_t) p1] * frac;
    }

    void updateBassMonoCoeff()
    {
        // Single-pole low-pass; coefficient via the standard exp(-2*pi*f/sr) form
        const double f = juce::jlimit (20.0, 1500.0, (double) params.bassMonoHz);
        const double a = std::exp (-2.0 * juce::MathConstants<double>::pi * f / sampleRate);
        bassMonoLP.alpha = (float) a;
    }

    //==============================================================================
    // 1st-order LP — used for the bass-mono side filter
    struct OnePoleLP
    {
        float alpha = 0.0f;     // exp(-2*pi*fc/sr)
        float z1    = 0.0f;

        void reset() { z1 = 0.0f; }
        float processSample (float x)
        {
            const float y = (1.0f - alpha) * x + alpha * z1;
            z1 = y;
            return y;
        }
    };

    // ---- TPT 2-pole state-variable filter (Vadim Zavalishin) ------------------
    struct SVF
    {
        double g = 0.0, k = 1.4142135f, a1 = 0.0, a2 = 0.0, a3 = 0.0;
        double ic1eq = 0.0, ic2eq = 0.0;

        void setCutoff (double sampleRate, double freq, double Q = 0.7071067811865476)
        {
            g = std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, 0.49 * sampleRate, freq) / sampleRate);
            k = 1.0 / Q;
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        void reset() { ic1eq = ic2eq = 0.0; }

        void process (double v0, double& vlp, double& vhp)
        {
            const double v3 = v0 - ic2eq;
            const double v1 = a1 * ic1eq + a2 * v3;
            const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0 * v1 - ic1eq;
            ic2eq = 2.0 * v2 - ic2eq;
            vlp = v2;
            vhp = v0 - k * v1 - v2;
        }
    };

    // 4th-order Linkwitz-Riley = two cascaded 2nd-order Butterworth
    struct LR4Crossover
    {
        SVF lpA, lpB;
        SVF hpA, hpB;

        void prepare (double sr, double freq)
        {
            lpA.setCutoff (sr, freq);
            lpB.setCutoff (sr, freq);
            hpA.setCutoff (sr, freq);
            hpB.setCutoff (sr, freq);
        }
        void reset() { lpA.reset(); lpB.reset(); hpA.reset(); hpB.reset(); }

        void process (float in, float& lp, float& hp)
        {
            double l1, h1, l2, h2;
            lpA.process ((double) in, l1, h1);
            lpB.process (l1, l2, h2);
            lp = (float) l2;

            double l3, h3, l4, h4;
            hpA.process ((double) in, l3, h3);
            hpB.process (h3, l4, h4);
            hp = (float) h4;
        }
    };

    // First-order all-pass — used in the shimmer decorrelation chain
    struct AllpassFirst
    {
        float a  = 0.0f;
        float z1 = 0.0f;
        void setCoeff (float coeff) { a = coeff; }
        void reset() { z1 = 0.0f; }
        float processOne (float x)
        {
            const float y = -a * x + z1;
            z1 = x + a * y;
            return y;
        }
    };

    void configureShimmer()
    {
        // Different coefficients for L and R = decorrelation
        const float lCoeffs[ShimmerStages] = { 0.42f, 0.71f, 0.83f, 0.55f, 0.38f, 0.79f };
        const float rCoeffs[ShimmerStages] = { 0.39f, 0.74f, 0.81f, 0.58f, 0.36f, 0.77f };

        for (int i = 0; i < ShimmerStages; ++i)
        {
            shimmerChain[0][(size_t) i].setCoeff (lCoeffs[i]);
            shimmerChain[1][(size_t) i].setCoeff (rCoeffs[i]);
        }
    }

    //==============================================================================
    Parameters params;
    double sampleRate = 44100.0;

    // Haas
    std::vector<float> haasL, haasR;
    int haasMaxLen = 0;
    int haasWriteIdx = 0;

    // Crossovers
    LR4Crossover xoLow [2];
    LR4Crossover xoHigh[2];

    // Bass mono LP on side
    OnePoleLP bassMonoLP;

    // Shimmer all-pass chain
    static constexpr int ShimmerStages = 6;
    std::array<AllpassFirst, ShimmerStages> shimmerChain[2];

    // Param smoothers
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothL, smoothR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothLowW, smoothMidW, smoothHighW, smoothMasterW;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothShimmer, smoothMix, smoothOutGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothRotCos, smoothRotSin;

    VisualState visual;

    JUCE_DECLARE_NON_COPYABLE (LuxuryStereoWidener)
};
