/*
  ==============================================================================

    FlangerPhaserEngine.h
    Luxury flanger / phaser / hybrid modulation effect.
      - Flanger: through-zero capable modulated comb delay with cubic Hermite
        interpolation, resonant feedback path with HP & soft saturation.
      - Phaser: cascaded analog-modelled all-pass filters (4..12 stages),
        sweeping centre frequency, resonant feedback.
      - Hybrid: blends both engines for thicker, more 3D motion.

    Stereo via quadrature LFOs and a separate per-channel offset for width.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class FlangerPhaserEngine
{
public:
    enum class Mode    { Flanger = 0, Phaser = 1, Hybrid = 2 };
    enum class LfoShape { Sine = 0, Triangle = 1, Drift = 2 };

    static constexpr int  kMaxStages = 12;
    static constexpr int  kMaxChannels = 2;
    static constexpr float kFlangerMinDelayMs  = 0.10f;
    static constexpr float kFlangerMaxDelayMs  = 14.0f;
    static constexpr float kPhaserMinHz        = 80.0f;
    static constexpr float kPhaserMaxHz        = 8000.0f;

    FlangerPhaserEngine() = default;

    void prepare (double newSampleRate, int /*samplesPerBlock*/, int numChans)
    {
        sampleRate    = newSampleRate;
        numChannels   = juce::jlimit (1, kMaxChannels, numChans);

        // 30 ms head-room for the flanger delay (TZF needs a few ms of pre-delay)
        delayBufferSamples = juce::nextPowerOfTwo ((int) (sampleRate * 0.030) + 8);
        delayMask          = delayBufferSamples - 1;

        delayLines.assign ((size_t) numChannels, std::vector<float> ((size_t) delayBufferSamples, 0.0f));
        writePos.assign   ((size_t) numChannels, 0);
        feedbackTail.assign ((size_t) numChannels, 0.0f);
        toneState.assign  ((size_t) numChannels, 0.0f);
        feedbackHpState.assign ((size_t) numChannels, 0.0f);

        for (auto& chain : allpassChain)
            for (auto& s : chain)
                s = 0.0f;

        const double smoothTime = 0.04;
        rateHz      .reset (sampleRate, smoothTime);
        depth       .reset (sampleRate, smoothTime);
        manual      .reset (sampleRate, smoothTime);
        feedback    .reset (sampleRate, smoothTime);
        mix         .reset (sampleRate, smoothTime);
        widthAmt    .reset (sampleRate, smoothTime);
        toneHz      .reset (sampleRate, smoothTime);
        outputGain  .reset (sampleRate, smoothTime);
        modeBlend   .reset (sampleRate, smoothTime);

        lfoPhase = 0.0f;
        driftValue = 0.0f;
        driftTarget = 0.0f;
        driftCounter = 0;
    }

    void reset()
    {
        for (auto& dl : delayLines) std::fill (dl.begin(), dl.end(), 0.0f);
        std::fill (writePos.begin(), writePos.end(), 0);
        std::fill (feedbackTail.begin(), feedbackTail.end(), 0.0f);
        std::fill (toneState.begin(), toneState.end(), 0.0f);
        std::fill (feedbackHpState.begin(), feedbackHpState.end(), 0.0f);

        for (auto& chain : allpassChain)
            for (auto& s : chain)
                s = 0.0f;

        lfoPhase = 0.0f;
        driftValue = 0.0f;
        driftTarget = 0.0f;
        driftCounter = 0;
    }

    // ---- Setters ----
    void setMode (Mode m)            { mode = m; }
    void setLfoShape (LfoShape s)    { lfoShape = s; }
    void setNumStages (int n)        { numStages = juce::jlimit (2, kMaxStages, (n / 2) * 2); }
    void setRate (float hz)          { rateHz.setTargetValue (juce::jlimit (0.01f, 10.0f, hz)); }
    void setDepthPct (float pct)     { depth.setTargetValue (juce::jlimit (0.0f, 1.0f, pct * 0.01f)); }
    void setManualPct (float pct)    { manual.setTargetValue (juce::jlimit (0.0f, 1.0f, pct * 0.01f)); }
    void setFeedbackPct (float pct)  { feedback.setTargetValue (juce::jlimit (-0.95f, 0.95f, pct * 0.01f)); }
    void setMixPct (float pct)       { mix.setTargetValue (juce::jlimit (0.0f, 1.0f, pct * 0.01f)); }
    void setWidthPct (float pct)     { widthAmt.setTargetValue (juce::jlimit (0.0f, 1.0f, pct * 0.01f)); }
    void setToneHz (float hz)        { toneHz.setTargetValue (juce::jlimit (200.0f, 20000.0f, hz)); }
    void setOutputDb (float db)      { outputGain.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, db))); }

    // ---- Visualizer accessors (lock-free) ----
    Mode     getMode()             const noexcept { return mode; }
    LfoShape getLfoShape()         const noexcept { return lfoShape; }
    int      getNumStages()        const noexcept { return numStages; }

    float getLfoPhase()            const noexcept { return phaseAtomic.load (std::memory_order_relaxed); }
    float getRateValue()           const noexcept { return rateValAtomic.load (std::memory_order_relaxed); }
    float getDepthValue()          const noexcept { return depthValAtomic.load (std::memory_order_relaxed); }
    float getManualValue()         const noexcept { return manualValAtomic.load (std::memory_order_relaxed); }
    float getFeedbackValue()       const noexcept { return feedbackValAtomic.load (std::memory_order_relaxed); }
    float getMixValue()            const noexcept { return mixValAtomic.load (std::memory_order_relaxed); }

    // Current modulated state used by the visualizer:
    //   flanger delay time in ms (channel 0 / 1)
    //   phaser sweep centre frequency in Hz (channel 0 / 1)
    float getCurrentFlangerDelayMs (int ch) const noexcept
    {
        return flangerDelayAtomic[juce::jlimit (0, kMaxChannels - 1, ch)].load (std::memory_order_relaxed);
    }
    float getCurrentPhaserHz (int ch) const noexcept
    {
        return phaserHzAtomic[juce::jlimit (0, kMaxChannels - 1, ch)].load (std::memory_order_relaxed);
    }

    double getSampleRate() const noexcept { return sampleRate; }

    // ---- Audio processing ----
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh   = juce::jmin (buffer.getNumChannels(), numChannels);
        const int numSmps = buffer.getNumSamples();
        if (numCh <= 0 || numSmps <= 0) return;

        const float lfoChQuad[kMaxChannels] = { 0.0f, 0.25f }; // quadrature

        for (int n = 0; n < numSmps; ++n)
        {
            // Smooth params
            const float rate     = rateHz.getNextValue();
            const float dep      = depth.getNextValue();
            const float man      = manual.getNextValue();
            const float fb       = feedback.getNextValue();
            const float mxWet    = mix.getNextValue();
            const float widthCur = widthAmt.getNextValue();
            const float tone     = toneHz.getNextValue();
            const float outG     = outputGain.getNextValue();

            // LFO advance
            const float dPhase = rate / (float) sampleRate;
            lfoPhase += dPhase;
            if (lfoPhase >= 1.0f) lfoPhase -= std::floor (lfoPhase);

            // Drift LFO (shape "Drift" = sine + slow random walk for vintage organic motion)
            if (lfoShape == LfoShape::Drift)
            {
                if (--driftCounter <= 0)
                {
                    driftCounter = (int) (sampleRate * 0.18); // new target every ~180 ms
                    driftTarget  = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                }
                driftValue += (driftTarget - driftValue) * 0.0008f;
            }

            // Tone filter coef
            const float toneCoef = std::exp (-juce::MathConstants<float>::twoPi
                                             * tone / (float) sampleRate);

            // HP coef inside feedback path (~120 Hz) – stops DC accumulation
            const float fbHpCoef = std::exp (-juce::MathConstants<float>::twoPi
                                             * 120.0f / (float) sampleRate);

            // Mode blend coefficients: flanger weight, phaser weight (sum = 1 in non-hybrid)
            float wFlanger, wPhaser;
            switch (mode)
            {
                case Mode::Flanger: wFlanger = 1.0f; wPhaser = 0.0f; break;
                case Mode::Phaser:  wFlanger = 0.0f; wPhaser = 1.0f; break;
                default:            wFlanger = 0.7f; wPhaser = 0.7f; break;
            }

            float wet[kMaxChannels] = { 0.0f, 0.0f };
            float dry[kMaxChannels] = { 0.0f, 0.0f };

            for (int c = 0; c < numCh; ++c)
            {
                // Per-channel LFO offset (quadrature scaled by width)
                const float chPh = lfoPhase + lfoChQuad[c] * widthCur;
                const float lfo  = computeLfo (chPh - std::floor (chPh));
                const float lfoBipolar = lfo;                 // -1 .. +1

                auto* data = buffer.getWritePointer (c);
                const float in = data[n];
                dry[c] = in;

                float wetCh = 0.0f;

                // ===== Flanger path =====
                if (wFlanger > 0.0f)
                {
                    // Manual sets centre delay; depth modulates around it.
                    const float baseMs = juce::jmap (man, 0.0f, 1.0f,
                                                      kFlangerMinDelayMs, kFlangerMaxDelayMs);
                    const float swing  = (kFlangerMaxDelayMs - kFlangerMinDelayMs) * 0.45f * dep;
                    const float dMs    = juce::jlimit (kFlangerMinDelayMs * 0.5f,
                                                       kFlangerMaxDelayMs,
                                                       baseMs + lfoBipolar * swing);
                    const float dSmp   = juce::jmax (1.0f, dMs * 0.001f * (float) sampleRate);

                    // Read with cubic Hermite interpolation for smooth, glassy modulation
                    auto& dl = delayLines[(size_t) c];
                    const int wp = writePos[(size_t) c];

                    const float readPosF = (float) wp - dSmp;
                    int   i1 = (int) std::floor (readPosF);
                    float frac = readPosF - (float) i1;
                    int   i0 = ((i1 - 1) % delayBufferSamples + delayBufferSamples) & delayMask;
                    int   i1m = ((i1)     % delayBufferSamples + delayBufferSamples) & delayMask;
                    int   i2  = ((i1 + 1) % delayBufferSamples + delayBufferSamples) & delayMask;
                    int   i3  = ((i1 + 2) % delayBufferSamples + delayBufferSamples) & delayMask;

                    const float y0 = dl[(size_t) i0];
                    const float y1 = dl[(size_t) i1m];
                    const float y2 = dl[(size_t) i2];
                    const float y3 = dl[(size_t) i3];
                    const float flangerOut = hermite (frac, y0, y1, y2, y3);

                    // Feedback path: HP-filter and gently saturate the tail before re-injecting
                    float fbSig = feedbackTail[(size_t) c];
                    // One-pole HP via subtracting LP state
                    feedbackHpState[(size_t) c] = fbHpCoef * feedbackHpState[(size_t) c]
                                                + (1.0f - fbHpCoef) * fbSig;
                    fbSig = fbSig - feedbackHpState[(size_t) c];
                    fbSig = std::tanh (fbSig * 1.2f) * 0.85f; // luxurious soft clip

                    // Write into delay line
                    dl[(size_t) wp] = in + fbSig * fb;
                    writePos[(size_t) c] = (wp + 1) & delayMask;

                    wetCh += flangerOut * wFlanger;

                    // Publish for the visualizer (channel 0 only is fine; ch 1 stored separately below)
                    flangerDelayAtomic[c].store (dMs, std::memory_order_relaxed);
                }
                else
                {
                    // Even in pure-phaser mode keep the delay line live (TZF look-ahead) but silent
                    auto& dl = delayLines[(size_t) c];
                    const int wp = writePos[(size_t) c];
                    dl[(size_t) wp] = in;
                    writePos[(size_t) c] = (wp + 1) & delayMask;
                }

                // ===== Phaser path =====
                if (wPhaser > 0.0f)
                {
                    // Manual sets centre frequency (log scale), depth modulates around it
                    const float baseHz = std::exp (juce::jmap (man, 0.0f, 1.0f,
                                                               std::log (kPhaserMinHz),
                                                               std::log (kPhaserMaxHz)));
                    // ±2 octaves at full depth
                    const float octaves = lfoBipolar * dep * 2.0f;
                    const float fHz     = juce::jlimit (40.0f, 12000.0f,
                                                        baseHz * std::pow (2.0f, octaves));

                    // First-order all-pass: y = -x + a*xPrev + a*y... we use the canonical form:
                    //   y[n] = a*x[n] + xPrev - a*yPrev,  a = (tan(piF/Fs) - 1)/(tan(piF/Fs) + 1)
                    const float piFoverFs = juce::MathConstants<float>::pi * fHz / (float) sampleRate;
                    const float t   = std::tan (piFoverFs);
                    const float a   = (t - 1.0f) / (t + 1.0f);

                    // Run the chain, with feedback from previous block's output
                    float ph = in + feedbackTail[(size_t) c] * fb * (wPhaser > 0.5f ? 1.0f : 0.6f);

                    auto& chain = allpassChain[c];
                    for (int s = 0; s < numStages; ++s)
                    {
                        // chain[s] holds previous y; we use a transposed direct-form single state
                        const float xn = ph;
                        const float yn = a * xn + chain[(size_t) s];
                        chain[(size_t) s] = xn - a * yn; // state = x[n] - a*y[n]
                        ph = yn;
                    }

                    wetCh += ph * wPhaser;

                    phaserHzAtomic[c].store (fHz, std::memory_order_relaxed);
                }

                // Tone filter (1-pole LP) keeps the high end smooth
                float& z = toneState[(size_t) c];
                z = toneCoef * z + (1.0f - toneCoef) * wetCh;
                wetCh = z;

                feedbackTail[(size_t) c] = wetCh;
                wet[c] = wetCh;
            }

            // Mono input: mirror to stereo
            if (numCh == 1)
            {
                wet[1] = wet[0];
                dry[1] = dry[0];
            }

            // Stereo width post-process (mid/side)
            const float wetSum = 0.5f * (wet[0] + wet[1]);
            const float wetDif = 0.5f * (wet[0] - wet[1]);
            const float w = widthCur;
            wet[0] = wetSum + wetDif * (0.5f + 0.5f * w);
            wet[1] = wetSum - wetDif * (0.5f + 0.5f * w);

            const float dryGain = 1.0f - mxWet;
            const float wetGain = mxWet;

            for (int c = 0; c < numCh; ++c)
            {
                float y = (dry[c] * dryGain + wet[c] * wetGain) * outG;
                buffer.getWritePointer (c)[n] = y;
            }
        }

        // Publish smoothed values for the GUI / visualizer
        phaseAtomic       .store (lfoPhase,                        std::memory_order_relaxed);
        rateValAtomic     .store (rateHz.getCurrentValue(),        std::memory_order_relaxed);
        depthValAtomic    .store (depth.getCurrentValue(),         std::memory_order_relaxed);
        manualValAtomic   .store (manual.getCurrentValue(),        std::memory_order_relaxed);
        feedbackValAtomic .store (feedback.getCurrentValue(),      std::memory_order_relaxed);
        mixValAtomic      .store (mix.getCurrentValue(),           std::memory_order_relaxed);
    }

private:
    inline float computeLfo (float ph01) noexcept
    {
        const float twoPi = juce::MathConstants<float>::twoPi;
        switch (lfoShape)
        {
            case LfoShape::Triangle:
            {
                const float t = ph01 < 0.5f ? ph01 * 2.0f : (1.0f - ph01) * 2.0f;
                return t * 2.0f - 1.0f;
            }
            case LfoShape::Drift:
            {
                const float s = std::sin (twoPi * ph01);
                // Mix sine with slow random drift for an organic, hand-played feel
                return juce::jlimit (-1.0f, 1.0f, s * 0.85f + driftValue * 0.5f);
            }
            case LfoShape::Sine:
            default:
                return std::sin (twoPi * ph01);
        }
    }

    static inline float hermite (float frac, float ym1, float y0, float y1, float y2) noexcept
    {
        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    double sampleRate = 44100.0;
    int    numChannels = 2;

    Mode     mode      = Mode::Flanger;
    LfoShape lfoShape  = LfoShape::Sine;
    int      numStages = 6;

    // Flanger delay buffers
    std::vector<std::vector<float>> delayLines;
    std::vector<int>                writePos;
    std::vector<float>              feedbackTail;
    std::vector<float>              toneState;
    std::vector<float>              feedbackHpState;
    int delayBufferSamples = 0;
    int delayMask          = 0;

    // Phaser all-pass states (per channel x stages)
    std::array<std::array<float, kMaxStages>, kMaxChannels> allpassChain {};

    // LFO state
    float lfoPhase    = 0.0f;
    float driftValue  = 0.0f;
    float driftTarget = 0.0f;
    int   driftCounter = 0;

    // Smoothed parameters
    juce::SmoothedValue<float> rateHz     { 0.4f };
    juce::SmoothedValue<float> depth      { 0.6f };
    juce::SmoothedValue<float> manual     { 0.45f };
    juce::SmoothedValue<float> feedback   { 0.3f };
    juce::SmoothedValue<float> mix        { 0.5f };
    juce::SmoothedValue<float> widthAmt   { 0.7f };
    juce::SmoothedValue<float> toneHz     { 9000.0f };
    juce::SmoothedValue<float> outputGain { 1.0f };
    juce::SmoothedValue<float> modeBlend  { 0.0f };

    // Atomics for visualizer
    std::atomic<float> phaseAtomic        { 0.0f };
    std::atomic<float> rateValAtomic      { 0.4f };
    std::atomic<float> depthValAtomic     { 0.6f };
    std::atomic<float> manualValAtomic    { 0.45f };
    std::atomic<float> feedbackValAtomic  { 0.3f };
    std::atomic<float> mixValAtomic       { 0.5f };
    std::array<std::atomic<float>, kMaxChannels> flangerDelayAtomic {};
    std::array<std::atomic<float>, kMaxChannels> phaserHzAtomic     {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangerPhaserEngine)
};
