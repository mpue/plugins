/*
  ==============================================================================

    SpectrumAnalyzer.h
    Created: Real-time FFT Spectrum Analyzer
    Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer() : fftOrder(11), fftSize(1 << fftOrder)
    {
        fft = std::make_unique<juce::dsp::FFT>(fftOrder);
        fftData.resize(fftSize * 2, 0.0f);
        scopeData.resize(fftSize / 2, 0.0f);
    }

    void prepare(double sampleRate)
    {
        this->sampleRate = sampleRate;
    }

    void pushSample(float sample)
    {
        if (fifoIndex < fftSize)
        {
            fftData[fifoIndex++] = sample;

            if (fifoIndex == fftSize)
            {
                performFFT();
                fifoIndex = 0;
            }
        }
    }

    float getLevel(int binIndex) const
    {
        if (binIndex >= 0 && binIndex < scopeData.size())
            return scopeData[binIndex];
        return 0.0f;
    }

    int getNumBins() const
    {
        return static_cast<int>(scopeData.size());
    }

    float binToFrequency(int bin) const
    {
        return static_cast<float>(bin * sampleRate / fftSize);
    }

    int frequencyToBin(float frequency) const
    {
        return static_cast<int>(frequency * fftSize / sampleRate);
    }

    const std::vector<float>& getScopeData() const
    {
        return scopeData;
    }

private:
    void performFFT()
    {
        std::copy(fftData.begin(), fftData.begin() + fftSize, fftData.begin() + fftSize);

        for (int i = 0; i < fftSize; ++i)
        {
            float window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (fftSize - 1));
            fftData[i] *= window;
        }

        fft->performFrequencyOnlyForwardTransform(fftData.data());

        const float minDb = -100.0f;
        const float maxDb = 0.0f;

        for (int i = 0; i < scopeData.size(); ++i)
        {
            float level = fftData[i];
            level = juce::jlimit(0.0f, 1.0f, level);
            float db = level > 0.0f ? juce::Decibels::gainToDecibels(level) : minDb;
            db = juce::jlimit(minDb, maxDb, db);

            float normalised = juce::jmap(db, minDb, maxDb, 0.0f, 1.0f);

            scopeData[i] = scopeData[i] * 0.7f + normalised * 0.3f;
        }
    }

    const int fftOrder;
    std::unique_ptr<juce::dsp::FFT> fft;
    const int fftSize;
    std::vector<float> fftData;
    std::vector<float> scopeData;
    int fifoIndex = 0;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
