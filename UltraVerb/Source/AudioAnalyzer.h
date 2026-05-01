#pragma once

#include <JuceHeader.h>
#include "UI/ParticleSystemComponent.h"

class AudioAnalyzer
{
public:
    AudioAnalyzer() : forwardFFT(fftOrder), window(fftSize, dsp::WindowingFunction<float>::hann)
    {
        zeromem(fftData, sizeof(fftData));
        zeromem(fifo, sizeof(fifo));
    }

    void pushNextSampleIntoFifo(float sample)
    {
        if (fifoIndex == fftSize)
        {
            if (!nextFFTBlockReady)
            {
                zeromem(fftData, sizeof(fftData));
                memcpy(fftData, fifo, sizeof(fifo));
                nextFFTBlockReady = true;
            }
            fifoIndex = 0;
        }

        fifo[fifoIndex++] = sample;
    }

    void performFFT(ParticleSystemComponent* particleSystem)
    {
        if (nextFFTBlockReady)
        {
            window.multiplyWithWindowingTable(fftData, fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform(fftData);
            nextFFTBlockReady = false;

            for (int i = 0; i < fftSize / 2; ++i)
            {
                float level = jmap(fftData[i], 0.0f, 10.0f, 0.0f, 1.0f);
                if (level > 0.1f)
                {
                    Point<float> position = { Random::getSystemRandom().nextFloat() * particleSystem->getWidth(), particleSystem->getHeight() * 1.f };
                    Point<float> velocity = { Random::getSystemRandom().nextFloat() * 2.0f - 1.0f, -level * 100.0f }; // Geschwindigkeit nach oben basierend auf Lautstärke
                    float size = level * 2.0f;
                    float lifetime = 1.0f;// Random::getSystemRandom().nextFloat() * 2.0f + 1.0f; // Zufällige Lebensdauer zwischen 1 und 3 Sekunden

                    // Generiere eine zufällige Cyan-Farbe mit Helligkeit basierend auf der Frequenz
                    float hue = 0.5f + Random::getSystemRandom().nextFloat() * 0.1f; // Zwischen 180° (Cyan) und 210°
                    float saturation = 1.0f;
                    float brightness = jmap(float(i), 0.0f, float(fftSize / 2), 0.5f, 4.0f); // Helligkeit basierend auf Frequenz
                    Colour color = Colour::fromHSV(hue, saturation, brightness, 1.0f);

                    particleSystem->addParticle(position, velocity, size, color, lifetime);
                }
            }
        }
    }

private:
    enum
    {
        fftOrder = 11,
        fftSize = 1 << fftOrder
    };

    dsp::FFT forwardFFT;
    dsp::WindowingFunction<float> window;
    float fifo[fftSize];
    float fftData[2 * fftSize];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;
};
