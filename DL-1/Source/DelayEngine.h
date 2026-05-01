/*
  ==============================================================================

    DelayEngine.h
    Luxury stereo delay DSP for DL-1.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

class DelayEngine
{
public:
    struct VisualState
    {
        std::atomic<float> inputLevelL  { 0.0f };
        std::atomic<float> inputLevelR  { 0.0f };
        std::atomic<float> wetLevelL    { 0.0f };
        std::atomic<float> wetLevelR    { 0.0f };
        std::atomic<float> delayMsL     { 350.0f };
        std::atomic<float> delayMsR     { 525.0f };
        std::atomic<float> feedback     { 0.45f };
        std::atomic<float> crossfeed    { 0.0f };
        std::atomic<float> modPhase     { 0.0f };
        std::atomic<float> modDepth     { 0.0f };
    };

    DelayEngine() = default;

    void prepare(double newSampleRate, int blockSize)
    {
        sampleRate = newSampleRate;

        const int maxDelaySamples = (int)(sampleRate * 2.6); // up to ~2500 ms + headroom

        for (int ch = 0; ch < 2; ++ch)
        {
            delayLines[ch].setMaximumDelayInSamples(maxDelaySamples);
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)blockSize, 1 };
            delayLines[ch].prepare(spec);
            delayLines[ch].reset();

            lowPass[ch].prepare(spec);
            lowPass[ch].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            lowPass[ch].setResonance(0.5f);

            highPass[ch].prepare(spec);
            highPass[ch].setType(juce::dsp::StateVariableTPTFilterType::highpass);
            highPass[ch].setResonance(0.5f);
        }

        smoothedDelayL .reset(sampleRate, 0.10);
        smoothedDelayR .reset(sampleRate, 0.10);
        smoothedFeedback.reset(sampleRate, 0.02);
        smoothedCross   .reset(sampleRate, 0.02);
        smoothedMix     .reset(sampleRate, 0.02);
        smoothedInGain  .reset(sampleRate, 0.02);
        smoothedOutGain .reset(sampleRate, 0.02);
        smoothedDrive   .reset(sampleRate, 0.02);
        smoothedWidth   .reset(sampleRate, 0.02);
        smoothedDuck    .reset(sampleRate, 0.02);
        smoothedHighCut .reset(sampleRate, 0.05);
        smoothedLowCut  .reset(sampleRate, 0.05);
        smoothedModRate .reset(sampleRate, 0.05);
        smoothedModDepth.reset(sampleRate, 0.05);

        envelopeL = envelopeR = 0.0f;
        modPhase = 0.0f;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            delayLines[ch].reset();
            lowPass[ch].reset();
            highPass[ch].reset();
            feedbackSample[ch] = 0.0f;
        }
        envelopeL = envelopeR = 0.0f;
        modPhase = 0.0f;
    }

    void setParameters(float inGainDb, float outGainDb,
                       float delayLms, float delayRms,
                       float feedback, float crossfeed,
                       float lowCutHz, float highCutHz,
                       float modRateHz, float modDepth01,
                       float drive01, float width01,
                       float ducking01, float mix01)
    {
        smoothedInGain   .setTargetValue(juce::Decibels::decibelsToGain(inGainDb));
        smoothedOutGain  .setTargetValue(juce::Decibels::decibelsToGain(outGainDb));
        smoothedDelayL   .setTargetValue(delayLms * 0.001f * (float)sampleRate);
        smoothedDelayR   .setTargetValue(delayRms * 0.001f * (float)sampleRate);
        smoothedFeedback .setTargetValue(feedback);
        smoothedCross    .setTargetValue(crossfeed);
        smoothedLowCut   .setTargetValue(lowCutHz);
        smoothedHighCut  .setTargetValue(highCutHz);
        smoothedModRate  .setTargetValue(modRateHz);
        smoothedModDepth .setTargetValue(modDepth01);
        smoothedDrive    .setTargetValue(drive01);
        smoothedWidth    .setTargetValue(width01);
        smoothedDuck     .setTargetValue(ducking01);
        smoothedMix      .setTargetValue(mix01);

        // expose to visualizer
        visual.delayMsL.store(delayLms);
        visual.delayMsR.store(delayRms);
        visual.feedback.store(feedback);
        visual.crossfeed.store(crossfeed);
        visual.modDepth.store(modDepth01);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        if (numChannels < 1) return;

        auto* left  = buffer.getWritePointer(0);
        auto* right = (numChannels > 1) ? buffer.getWritePointer(1) : left;

        float peakInL = 0.0f, peakInR = 0.0f;
        float peakWetL = 0.0f, peakWetR = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            const float inGain  = smoothedInGain.getNextValue();
            const float outGain = smoothedOutGain.getNextValue();
            const float fb      = smoothedFeedback.getNextValue();
            const float xfb     = smoothedCross.getNextValue();
            const float mix     = smoothedMix.getNextValue();
            const float drv     = smoothedDrive.getNextValue();
            const float wdth    = smoothedWidth.getNextValue();
            const float duck    = smoothedDuck.getNextValue();
            const float lcHz    = smoothedLowCut.getNextValue();
            const float hcHz    = smoothedHighCut.getNextValue();
            const float mRate   = smoothedModRate.getNextValue();
            const float mDepth  = smoothedModDepth.getNextValue();
            const float dlySL   = smoothedDelayL.getNextValue();
            const float dlySR   = smoothedDelayR.getNextValue();

            // Update filters cutoffs
            lowPass [0].setCutoffFrequency(juce::jlimit(200.0f, 20000.0f, hcHz));
            lowPass [1].setCutoffFrequency(juce::jlimit(200.0f, 20000.0f, hcHz));
            highPass[0].setCutoffFrequency(juce::jlimit(20.0f,  2000.0f,  lcHz));
            highPass[1].setCutoffFrequency(juce::jlimit(20.0f,  2000.0f,  lcHz));

            // LFO modulation
            modPhase += mRate / (float)sampleRate;
            if (modPhase >= 1.0f) modPhase -= 1.0f;
            const float modL = std::sin(modPhase * juce::MathConstants<float>::twoPi);
            const float modR = std::sin((modPhase + 0.25f) * juce::MathConstants<float>::twoPi);
            const float modAmtSamples = mDepth * 0.012f * (float)sampleRate; // up to ~12 ms
            const float modOffsetL = modL * modAmtSamples;
            const float modOffsetR = modR * modAmtSamples;

            const float dryL = left [n];
            const float dryR = right[n];

            // Input gain
            const float inL = dryL * inGain;
            const float inR = dryR * inGain;

            peakInL = juce::jmax(peakInL, std::abs(inL));
            peakInR = juce::jmax(peakInR, std::abs(inR));

            // Envelope follower of input for ducking
            const float rectifiedL = std::abs(inL);
            const float rectifiedR = std::abs(inR);
            const float rectified = juce::jmax(rectifiedL, rectifiedR);
            const float attack  = std::exp(-1.0f / (0.005f * (float)sampleRate));
            const float release = std::exp(-1.0f / (0.250f * (float)sampleRate));
            float envCoef = (rectified > envelopeL) ? attack : release;
            envelopeL = rectified + envCoef * (envelopeL - rectified);

            // Determine actual delay times (with modulation, clamped)
            const float maxSamples = (float)((int)(sampleRate * 2.5));
            const float delayLsamps = juce::jlimit(2.0f, maxSamples, dlySL + modOffsetL);
            const float delayRsamps = juce::jlimit(2.0f, maxSamples, dlySR + modOffsetR);

            // Read delayed taps
            float tapL = delayLines[0].popSample(0, delayLsamps, true);
            float tapR = delayLines[1].popSample(0, delayRsamps, true);

            // Filter taps (in feedback path)
            tapL = lowPass [0].processSample(0, tapL);
            tapR = lowPass [1].processSample(0, tapR);
            tapL = highPass[0].processSample(0, tapL);
            tapR = highPass[1].processSample(0, tapR);

            // Saturation in feedback path (warmth, gentle tube-like)
            if (drv > 0.0001f)
            {
                const float driveAmt = 1.0f + drv * 4.0f;
                tapL = std::tanh(tapL * driveAmt) / std::tanh(driveAmt);
                tapR = std::tanh(tapR * driveAmt) / std::tanh(driveAmt);
            }

            // Build feedback signal (with cross-feed)
            const float fbL = (1.0f - xfb) * tapL + xfb * tapR;
            const float fbR = (1.0f - xfb) * tapR + xfb * tapL;

            // Push input + feedback into delay lines
            delayLines[0].pushSample(0, inL + fbL * fb);
            delayLines[1].pushSample(0, inR + fbR * fb);

            // Wet output is the read tap (already fed back)
            float wetL = tapL;
            float wetR = tapR;

            // Stereo width on wet (mid/side)
            const float mid  = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR);
            wetL = mid + side * wdth;
            wetR = mid - side * wdth;

            // Ducking: attenuate wet by input envelope
            const float duckGain = 1.0f - duck * juce::jlimit(0.0f, 1.0f, envelopeL * 4.0f);
            wetL *= duckGain;
            wetR *= duckGain;

            peakWetL = juce::jmax(peakWetL, std::abs(wetL));
            peakWetR = juce::jmax(peakWetR, std::abs(wetR));

            // Mix dry/wet (equal-power-ish)
            const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
            const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

            float outL = dryL * dryGain + wetL * wetGain;
            float outR = dryR * dryGain + wetR * wetGain;

            // Output gain
            outL *= outGain;
            outR *= outGain;

            left [n] = outL;
            right[n] = outR;
        }

        // smooth visual peak meters
        const float meterSmooth = 0.6f;
        visual.inputLevelL.store(visual.inputLevelL.load() * meterSmooth + peakInL  * (1.0f - meterSmooth));
        visual.inputLevelR.store(visual.inputLevelR.load() * meterSmooth + peakInR  * (1.0f - meterSmooth));
        visual.wetLevelL .store(visual.wetLevelL .load() * meterSmooth + peakWetL * (1.0f - meterSmooth));
        visual.wetLevelR .store(visual.wetLevelR .load() * meterSmooth + peakWetR * (1.0f - meterSmooth));
        visual.modPhase  .store(modPhase);
    }

    VisualState& getVisualState() noexcept { return visual; }

private:
    double sampleRate = 44100.0;

    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>, 2> delayLines;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> lowPass;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> highPass;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayL, smoothedDelayR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFeedback, smoothedCross, smoothedMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedInGain, smoothedOutGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDrive, smoothedWidth, smoothedDuck;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedHighCut, smoothedLowCut;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedModRate, smoothedModDepth;

    float feedbackSample[2] { 0.0f, 0.0f };
    float envelopeL = 0.0f, envelopeR = 0.0f;
    float modPhase  = 0.0f;

    VisualState visual;
};
