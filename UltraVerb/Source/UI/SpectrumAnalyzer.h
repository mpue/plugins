#ifndef SPECTRUMANALYZER_H
#define SPECTRUMANALYZER_H

#include <JuceHeader.h>

class SpectrumAnalyzer : public juce::Component,
	private juce::Timer
{
public:
	SpectrumAnalyzer() : fft(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann)
	{
		setOpaque(false);
		startTimerHz(30);
		setSize(800, 600);
	}

	void setFFTData(const float* data)
	{	
		for (int i = 0; i < fftSize; ++i)
			fftData[i] = data[i];

		for (int i = 0; i < scopeSize; ++i)
		{
			int fftDataIndex = juce::jmap<int>(i, 0, scopeSize - 1, 0, fftSize / 2);
			float level = fftData[fftDataIndex];
			level = juce::Decibels::gainToDecibels(level, -80.0f);
			float mappedLevel = juce::jmap<float>(level, -80.0f, 0.0f, 0.0f, 1.0f);

			// Apply smoothing
			scopeData[i] = smoothingFactor * scopeData[i] + (1.0f - smoothingFactor) * mappedLevel;
		}

	}

	void paint(juce::Graphics& g) override
	{
		g.setColour(juce::Colours::white);
		auto width = getLocalBounds().getWidth();
		auto height = getLocalBounds().getHeight();

		float barWidth = ((float)width / (float)scopeSize) - 1;

		for (int i = 0; i < scopeSize; ++i)
		{
			auto x = juce::jmap<int>(i, 0, scopeSize - 1, 0, width);
			auto y = juce::jmap<float>(scopeData[i], 0.0f, 1.0f, static_cast<float>(height), 0.0f);

			g.fillRect((float)x, (float)height, barWidth, (float)y);


			// g.drawLine((float)x, (float)height, (float)x, y);
		}
	}

private:
	void timerCallback() override
	{
		repaint();
	}

	enum
	{
		fftOrder = 10,
		fftSize = 1 << fftOrder,
		scopeSize = 32
	};

	juce::dsp::FFT fft;
	juce::dsp::WindowingFunction<float> window;
	float fftData[2 * fftSize] = { 0 };
	float scopeData[scopeSize] = { 0 };
	float smoothingFactor = 0.6;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
#endif