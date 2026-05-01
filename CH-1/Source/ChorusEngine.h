/*
  ==============================================================================

    ChorusEngine.h
    Multi-voice modulated chorus with feedback, stereo width and tone.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class ChorusEngine
{
public:
    static constexpr int kMaxVoices = 4;

    ChorusEngine() = default;

    void prepare (double newSampleRate, int /*samplesPerBlock*/, int numChans)
    {
        sampleRate    = newSampleRate;
        numChannels   = juce::jmax (1, numChans);

        // 60 ms max delay (more than enough for chorus modulation)
        delayBufferSamples = juce::nextPowerOfTwo ((int) (sampleRate * 0.060) + 8);
        delayMask          = delayBufferSamples - 1;

        delayLines.assign ((size_t) numChannels, std::vector<float> ((size_t) delayBufferSamples, 0.0f));
        writePos.assign  ((size_t) numChannels, 0);
        toneState.assign ((size_t) numChannels, 0.0f);
        feedbackTail.assign ((size_t) numChannels, 0.0f);

        const double smoothTime = 0.04; // 40 ms — silky parameter changes
        rateHz       .reset (sampleRate, smoothTime);
        depthMs      .reset (sampleRate, smoothTime);
        baseDelayMs  .reset (sampleRate, smoothTime);
        feedback     .reset (sampleRate, smoothTime);
        mix          .reset (sampleRate, smoothTime);
        widthAmt     .reset (sampleRate, smoothTime);
        toneHz       .reset (sampleRate, smoothTime);
        outputGain   .reset (sampleRate, smoothTime);

        for (int v = 0; v < kMaxVoices; ++v)
            lfoPhase[v] = (float) v / (float) kMaxVoices;
    }

    void reset()
    {
        for (auto& dl : delayLines) std::fill (dl.begin(), dl.end(), 0.0f);
        std::fill (writePos.begin(),     writePos.end(),     0);
        std::fill (toneState.begin(),    toneState.end(),    0.0f);
        std::fill (feedbackTail.begin(), feedbackTail.end(), 0.0f);
        for (int v = 0; v < kMaxVoices; ++v) lfoPhase[v] = (float) v / (float) kMaxVoices;
    }

    // ----- Parameter setters (target values; smoothing handles the ramp) -----
    void setRate          (float hz)  { rateHz     .setTargetValue (juce::jlimit (0.01f, 12.0f,  hz)); }
    void setDepthMs       (float ms)  { depthMs    .setTargetValue (juce::jlimit (0.0f,  15.0f,  ms)); }
    void setBaseDelayMs   (float ms)  { baseDelayMs.setTargetValue (juce::jlimit (1.0f,  30.0f,  ms)); }
    void setFeedback      (float pct) { feedback   .setTargetValue (juce::jlimit (-0.95f, 0.95f, pct * 0.01f)); }
    void setMix           (float pct) { mix        .setTargetValue (juce::jlimit (0.0f,   1.0f,  pct * 0.01f)); }
    void setWidth         (float pct) { widthAmt   .setTargetValue (juce::jlimit (0.0f,   1.0f,  pct * 0.01f)); }
    void setToneHz        (float hz)  { toneHz     .setTargetValue (juce::jlimit (200.0f, 20000.0f, hz)); }
    void setOutputDb      (float db)  { outputGain .setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, db))); }
    void setNumVoices     (int n)     { numVoices = juce::jlimit (1, kMaxVoices, n); }

    // ----- For visualizer (lock-free reads) -----
    float getLfoPhase (int voice)   const noexcept { return phaseAtomic[voice].load (std::memory_order_relaxed); }
    float getCurrentDelayMs (int v) const noexcept { return delayAtomic[v].load (std::memory_order_relaxed); }
    float getRateValue()            const noexcept { return rateValAtomic.load   (std::memory_order_relaxed); }
    float getDepthValue()           const noexcept { return depthValAtomic.load  (std::memory_order_relaxed); }
    int   getNumVoices()            const noexcept { return numVoices; }

    // ----- Audio processing -----
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh   = juce::jmin (buffer.getNumChannels(), numChannels);
        const int numSmps = buffer.getNumSamples();
        if (numCh <= 0 || numSmps <= 0) return;

        // Per-voice phase offset (spread across the period)
        const float voicePhaseOffset[kMaxVoices] = { 0.0f, 0.25f, 0.5f, 0.75f };

        for (int n = 0; n < numSmps; ++n)
        {
            const float rate     = rateHz.getNextValue();
            const float depth    = depthMs.getNextValue();
            const float baseMs   = baseDelayMs.getNextValue();
            const float fb       = feedback.getNextValue();
            const float mxWet    = mix.getNextValue();
            const float widthCur = widthAmt.getNextValue();
            const float tone     = toneHz.getNextValue();
            const float outG     = outputGain.getNextValue();

            // Advance the master LFO phase (one full cycle per 1/rate seconds)
            const float dPhase = rate / (float) sampleRate;

            // Compute per-voice modulated delay times in samples
            float voiceDelaySamples[kMaxVoices];
            float voiceLfo        [kMaxVoices];
            for (int v = 0; v < kMaxVoices; ++v)
            {
                float ph = lfoPhase[v];
                ph += dPhase;
                if (ph >= 1.0f) ph -= 1.0f;
                lfoPhase[v] = ph;

                const float twoPi = juce::MathConstants<float>::twoPi;
                voiceLfo[v]      = std::sin (twoPi * ph);
                const float dMs  = baseMs + voiceLfo[v] * depth;
                voiceDelaySamples[v] = juce::jmax (1.0f, dMs * 0.001f * (float) sampleRate);
            }

            // Tone-filter coefficient (one-pole low-pass)
            const float toneCoef = std::exp (-juce::MathConstants<float>::twoPi
                                             * tone / (float) sampleRate);

            // For each input channel, build the wet signal as a sum of voice taps,
            // panned to L/R according to voice index and the width parameter.
            float wetOut[2] = { 0.0f, 0.0f };
            float dryOut[2] = { 0.0f, 0.0f };

            for (int c = 0; c < numCh; ++c)
            {
                auto* data = buffer.getWritePointer (c);
                const float in = data[n];
                dryOut[c] = in;

                // Push into delay line: input + a small amount of feedback from the wet tail
                auto& dl = delayLines[(size_t) c];
                int   wp = writePos[(size_t) c];
                dl[(size_t) wp] = in + feedbackTail[(size_t) c] * fb;

                // Sum voice taps
                float wetCh = 0.0f;
                int   used  = 0;

                for (int v = 0; v < numVoices; ++v)
                {
                    // Phase offset between voices, slightly different per channel for stereo width.
                    // The channel offset is scaled by widthAmt — width=0 means mono modulation,
                    // width=1 means voices are nearly anti-phase between L and R.
                    const float chOffset = (c == 0 ? -0.25f : 0.25f) * widthCur;
                    float ph = lfoPhase[v] + voicePhaseOffset[v] + chOffset;
                    ph -= std::floor (ph);

                    const float lfo = std::sin (juce::MathConstants<float>::twoPi * ph);
                    const float dMs = baseMs + lfo * depth;
                    const float dSmp = juce::jmax (1.0f, dMs * 0.001f * (float) sampleRate);

                    wetCh += readDelayInterpolated ((int) c, dSmp);
                    ++used;
                }

                if (used > 0) wetCh /= (float) used;

                // Soft tone shaping — keep modulation warm
                float& z = toneState[(size_t) c];
                z = toneCoef * z + (1.0f - toneCoef) * wetCh;
                wetCh = z;

                feedbackTail[(size_t) c] = wetCh;
                wetOut[c] = wetCh;

                writePos[(size_t) c] = (wp + 1) & delayMask;
            }

            // For mono inputs, mirror the wet to both ears so we still get a stereo image.
            if (numCh == 1)
            {
                wetOut[1] = wetOut[0];
                dryOut[1] = dryOut[0];
            }

            // Stereo width: blend wet between mono-sum and L/R
            const float wetSum = 0.5f * (wetOut[0] + wetOut[1]);
            const float wetDif = 0.5f * (wetOut[0] - wetOut[1]);
            const float w      = widthCur;
            wetOut[0] = wetSum + wetDif * w;
            wetOut[1] = wetSum - wetDif * w;

            // Final dry/wet mix and output gain
            const float dryGain = 1.0f - mxWet;
            const float wetGain = mxWet;

            for (int c = 0; c < numCh; ++c)
            {
                const float y = (dryOut[c] * dryGain + wetOut[c] * wetGain) * outG;
                buffer.getWritePointer (c)[n] = y;
            }

            (void) voiceDelaySamples; (void) voiceLfo; // kept for clarity
        }

        // Publish current state for the visualizer (lock-free, cheap)
        for (int v = 0; v < kMaxVoices; ++v)
        {
            phaseAtomic[v].store (lfoPhase[v], std::memory_order_relaxed);
            const float lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase[v]);
            const float dMs = baseDelayMs.getCurrentValue() + lfo * depthMs.getCurrentValue();
            delayAtomic[v].store (dMs, std::memory_order_relaxed);
        }
        rateValAtomic .store (rateHz.getCurrentValue(),  std::memory_order_relaxed);
        depthValAtomic.store (depthMs.getCurrentValue(), std::memory_order_relaxed);
    }

private:
    inline float readDelayInterpolated (int channel, float delaySamples) const noexcept
    {
        const auto& dl = delayLines[(size_t) channel];
        const int   wp = writePos[(size_t) channel];

        const float readPosF = (float) wp - delaySamples;
        int   i0 = (int) std::floor (readPosF);
        float frac = readPosF - (float) i0;
        i0 = ((i0 % delayBufferSamples) + delayBufferSamples) & delayMask;
        const int i1 = (i0 + 1) & delayMask;

        // Linear interpolation is more than enough for chorus-rate modulation
        return dl[(size_t) i0] + frac * (dl[(size_t) i1] - dl[(size_t) i0]);
    }

    double sampleRate = 44100.0;
    int    numChannels = 2;

    std::vector<std::vector<float>> delayLines;
    std::vector<int>                writePos;
    std::vector<float>              toneState;
    std::vector<float>              feedbackTail;
    int delayBufferSamples = 0;
    int delayMask          = 0;

    std::array<float, kMaxVoices> lfoPhase {};
    int numVoices = 4;

    juce::SmoothedValue<float> rateHz       { 0.5f };
    juce::SmoothedValue<float> depthMs      { 4.0f };
    juce::SmoothedValue<float> baseDelayMs  { 9.0f };
    juce::SmoothedValue<float> feedback     { 0.0f };
    juce::SmoothedValue<float> mix          { 0.5f };
    juce::SmoothedValue<float> widthAmt     { 0.7f };
    juce::SmoothedValue<float> toneHz       { 8000.0f };
    juce::SmoothedValue<float> outputGain   { 1.0f };

    std::array<std::atomic<float>, kMaxVoices> phaseAtomic {};
    std::array<std::atomic<float>, kMaxVoices> delayAtomic {};
    std::atomic<float> rateValAtomic  { 0.5f };
    std::atomic<float> depthValAtomic { 4.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusEngine)
};
