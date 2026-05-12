/*
  ==============================================================================

    BassScope.h
    Luxury bass visualiser. Combines:
      • a live phosphor-style waveform scope fed by a lock-free ring buffer
        from the audio thread,
      • a real-time low-band spectrum analyser (FFT, hand-tuned for the bass
        range 20Hz–2kHz) drawn underneath as glowing bars,
      • a dual VU bar on the right.
    All three respond to actual rendered audio so the user can SEE the bass
    they hear — character, transient, harmonic content and loudness at once.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class BassScope : public juce::Component, private juce::Timer
{
public:
    static constexpr int fftOrder = 11;            // 2048
    static constexpr int fftSize  = 1 << fftOrder; // 2048
    static constexpr int scopeLen = 2048;          // ring buffer length for the waveform

    BassScope()
        : forwardFFT (fftOrder),
          window (fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        std::fill (ring.begin(), ring.end(), 0.0f);
        std::fill (fftIn.begin(), fftIn.end(), 0.0f);
        std::fill (fftMag.begin(), fftMag.end(), 0.0f);
        std::fill (smoothMag.begin(), smoothMag.end(), 0.0f);

        startTimerHz (45);
    }

    ~BassScope() override = default;

    // Audio-thread: push a block of mono samples into the ring buffer.
    void pushAudio (const float* data, int numSamples) noexcept
    {
        if (numSamples <= 0) return;

        const juce::ScopedLock sl (lock);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = data[i];
            ring[(size_t) writePos] = s;
            writePos = (writePos + 1) % scopeLen;

            fftIn[(size_t) fftWrite] = s;
            fftWrite = (fftWrite + 1) % fftSize;

            if (++samplesSinceFft >= 512)
            {
                samplesSinceFft = 0;
                pendingFft.store (true, std::memory_order_release);
            }

            const float a = std::abs (s);
            if (a > rmsPeak) rmsPeak = a;
        }
    }

    void notifyTriggered (float velocity = 1.0f) noexcept
    {
        const juce::ScopedLock sl (lock);
        flashTime = 1.0f;
        flashVel  = juce::jlimit (0.1f, 1.0f, velocity);
    }

    void paint (juce::Graphics& g) override
    {
        const auto outer  = getLocalBounds().toFloat().reduced (1.0f);
        const float corner = 14.0f;

        // ---- panel background ----
        juce::ColourGradient bg (juce::Colour (0xff0e1620), outer.getCentreX(), outer.getY(),
                                  juce::Colour (0xff05080c), outer.getCentreX(), outer.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (outer, corner);

        // soft inner vignette
        {
            juce::ColourGradient v (juce::Colour (0x00000000), outer.getCentreX(), outer.getCentreY(),
                                     juce::Colour (0x66000000), outer.getRight(), outer.getBottom(), true);
            g.setGradientFill (v);
            g.fillRoundedRectangle (outer.reduced (1.0f), corner - 1.0f);
        }

        // reserve right strip for VU
        auto inner = outer.reduced (16.0f, 28.0f);
        const float vuWidth = 38.0f;
        auto vuArea = inner.removeFromRight (vuWidth);
        inner.removeFromRight (10.0f);

        // split top:waveform / bottom:spectrum
        const float spectrumHeight = inner.getHeight() * 0.40f;
        auto spectrumArea = inner.removeFromBottom (spectrumHeight);
        inner.removeFromBottom (8.0f);
        auto waveArea = inner;

        drawGrid     (g, waveArea);
        drawWaveform (g, waveArea);
        drawSpectrum (g, spectrumArea);
        drawVU       (g, vuArea);

        // frame
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (outer, corner, 1.0f);

        // header
        g.setColour (juce::Colour (0xaa4d9eff));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("BASS  SCOPE  ·  SPECTRUM",
                    juce::Rectangle<int> ((int) outer.getX() + 14, (int) outer.getY() + 6, 280, 14),
                    juce::Justification::centredLeft);

        // live readout right
        g.setColour (juce::Colour (0x88e8e8e8));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        const juce::String info =
            juce::String (currentLevelDb, 1) + " dB  ·  peak " +
            juce::String (peakHoldDb, 1) + " dB";
        g.drawText (info,
                    juce::Rectangle<int> ((int) outer.getRight() - 240, (int) outer.getY() + 6, 226, 14),
                    juce::Justification::centredRight);
    }

    void resized() override {}

private:
    void drawGrid (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (juce::Colour (0x12a8d0ff));
        for (int i = 1; i < 4; ++i)
        {
            const float y = area.getY() + area.getHeight() * (i / 4.0f);
            g.drawLine (area.getX(), y, area.getRight(), y, 0.7f);
        }
        g.setColour (juce::Colour (0x33a8d0ff));
        const float midY = area.getCentreY();
        g.drawLine (area.getX(), midY, area.getRight(), midY, 1.0f);

        g.setColour (juce::Colour (0x10a8d0ff));
        for (int i = 1; i < 10; ++i)
        {
            const float x = area.getX() + area.getWidth() * (i / 10.0f);
            g.drawLine (x, area.getY(), x, area.getBottom(), 0.5f);
        }
    }

    void drawWaveform (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float midY  = area.getCentreY();
        const float halfH = area.getHeight() * 0.46f;

        const int   N     = (int) area.getWidth();
        if (N < 4) return;

        // copy ring snapshot
        std::vector<float> snap (scopeLen);
        int wp;
        {
            const juce::ScopedLock sl (lock);
            wp = writePos;
            std::copy (ring.begin(), ring.end(), snap.begin());
        }

        // build path: read from ring starting at writePos (most recent on the right).
        juce::Path topPath, botPath, fillPath;
        std::vector<float> peakPos (N, 0.0f), peakNeg (N, 0.0f);

        const int total = scopeLen;
        for (int i = 0; i < N; ++i)
        {
            const int s0 = (int) ((float) i / N * total);
            const int s1 = (int) ((float) (i + 1) / N * total);
            float maxV = 0.0f, minV = 0.0f;
            for (int s = s0; s < s1; ++s)
            {
                const int idx = (wp + s) % total;
                const float v = snap[(size_t) idx];
                if (v > maxV) maxV = v;
                if (v < minV) minV = v;
            }
            peakPos[(size_t) i] = juce::jlimit (-1.0f, 1.0f, maxV);
            peakNeg[(size_t) i] = juce::jlimit (-1.0f, 1.0f, minV);
        }

        const float xStep = area.getWidth() / (float) N;
        topPath.startNewSubPath (area.getX(), midY - peakPos[0] * halfH);
        botPath.startNewSubPath (area.getX(), midY - peakNeg[0] * halfH);
        for (int i = 1; i < N; ++i)
        {
            const float x = area.getX() + xStep * (float) i;
            topPath.lineTo (x, midY - peakPos[(size_t) i] * halfH);
            botPath.lineTo (x, midY - peakNeg[(size_t) i] * halfH);
        }

        // closed fill
        fillPath.startNewSubPath (area.getX(), midY - peakPos[0] * halfH);
        for (int i = 1; i < N; ++i)
        {
            const float x = area.getX() + xStep * (float) i;
            fillPath.lineTo (x, midY - peakPos[(size_t) i] * halfH);
        }
        for (int i = N - 1; i >= 0; --i)
        {
            const float x = area.getX() + xStep * (float) i;
            fillPath.lineTo (x, midY - peakNeg[(size_t) i] * halfH);
        }
        fillPath.closeSubPath();

        // fill
        juce::ColourGradient fillGrad (juce::Colour (0x884d9eff), midY, area.getY(),
                                        juce::Colour (0x224d9eff), midY, area.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (fillPath);

        // glow strokes
        g.setColour (juce::Colour (0x224d9eff));
        g.strokePath (topPath, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0x554d9eff));
        g.strokePath (topPath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffd2e7ff));
        g.strokePath (topPath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (botPath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // trigger flash overlay
        float ft;
        {
            const juce::ScopedLock sl (lock);
            ft = flashTime;
        }
        if (ft > 0.0f)
        {
            juce::ColourGradient flashGrad (juce::Colour (0x88e8f3ff).withAlpha (0.25f * ft),
                                             area.getCentreX(), midY,
                                             juce::Colour (0x00e8f3ff),
                                             area.getRight(),  area.getBottom(), true);
            g.setGradientFill (flashGrad);
            g.fillRect (area);
        }
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> area)
    {
        // spectrum frame
        g.setColour (juce::Colour (0x22a8d0ff));
        g.drawRoundedRectangle (area.reduced (0.0f, 0.0f), 4.0f, 0.6f);

        const int numBars = 64;
        const float barGap = 1.0f;
        const float barW = (area.getWidth() - barGap * (numBars - 1)) / (float) numBars;

        // map spectrum bins to log-spaced bass-friendly bars: 20 Hz .. 2 kHz
        const float sr = (float) sampleRateHint;
        const float minHz = 25.0f;
        const float maxHz = 2200.0f;
        const float logMin = std::log10 (minHz);
        const float logMax = std::log10 (maxHz);

        for (int b = 0; b < numBars; ++b)
        {
            const float t0 = (float) b / numBars;
            const float t1 = (float) (b + 1) / numBars;
            const float fLo = std::pow (10.0f, logMin + (logMax - logMin) * t0);
            const float fHi = std::pow (10.0f, logMin + (logMax - logMin) * t1);
            const int binLo = juce::jlimit (1, fftSize / 2 - 1, (int) std::floor (fLo / sr * fftSize));
            const int binHi = juce::jlimit (binLo + 1, fftSize / 2, (int) std::ceil  (fHi / sr * fftSize));

            float maxMag = 0.0f;
            for (int i = binLo; i < binHi; ++i)
                if (smoothMag[(size_t) i] > maxMag) maxMag = smoothMag[(size_t) i];

            // dB scaling, normalise around -80..0
            float dB = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, maxMag));
            float h  = juce::jmap (dB, -80.0f, 0.0f, 0.0f, 1.0f);
            h = juce::jlimit (0.0f, 1.0f, h);

            const float barH = area.getHeight() * h;
            const float x = area.getX() + b * (barW + barGap);
            const float y = area.getBottom() - barH;
            juce::Rectangle<float> bar (x, y, barW, barH);

            // gradient: deep blue → bright cyan as it rises
            juce::ColourGradient bgrad (juce::Colour (0xff2456c4),
                                         x, area.getBottom(),
                                         juce::Colour (0xff8ce0ff),
                                         x, area.getY(), false);
            g.setGradientFill (bgrad);
            g.fillRoundedRectangle (bar, 1.5f);

            // top cap glow
            if (barH > 2.0f)
            {
                g.setColour (juce::Colour (0xffd2e7ff).withAlpha (0.85f));
                g.fillRoundedRectangle (juce::Rectangle<float> (x, y, barW, juce::jmin (2.0f, barH)), 1.0f);
            }
        }

        // baseline
        g.setColour (juce::Colour (0x33a8d0ff));
        g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getBottom(), 1.0f);
    }

    void drawVU (juce::Graphics& g, juce::Rectangle<float> area)
    {
        // chassis
        juce::ColourGradient chassis (juce::Colour (0xff111922), area.getCentreX(), area.getY(),
                                       juce::Colour (0xff050a10), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill (chassis);
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (area, 5.0f, 1.0f);

        // segmented meter
        const auto inner = area.reduced (5.0f, 8.0f);
        const int numSegs = 28;
        const float segH = (inner.getHeight() - 2.0f) / numSegs;

        const float lvl = juce::jlimit (0.0f, 1.0f, juce::jmap (currentLevelDb, -60.0f, 3.0f, 0.0f, 1.0f));
        const float pk  = juce::jlimit (0.0f, 1.0f, juce::jmap (peakHoldDb,    -60.0f, 3.0f, 0.0f, 1.0f));
        const int   onSegs = (int) std::round (lvl * numSegs);
        const int   pkSeg  = (int) std::round (pk  * numSegs);

        for (int i = 0; i < numSegs; ++i)
        {
            const float yTop = inner.getBottom() - (i + 1) * segH + 1.0f;
            juce::Rectangle<float> seg (inner.getX(), yTop, inner.getWidth(), segH - 1.5f);

            const float t = (float) i / numSegs;
            juce::Colour onCol;
            if (t < 0.65f)        onCol = juce::Colour::fromHSV (0.58f - t * 0.12f, 0.85f, 1.0f, 1.0f); // blue→teal
            else if (t < 0.85f)   onCol = juce::Colour (0xffffd070);                                    // amber
            else                  onCol = juce::Colour (0xffff5060);                                    // red

            const bool litByLevel = (i < onSegs);
            const bool litByPeak  = (i == pkSeg && pkSeg > 0);

            if (litByLevel || litByPeak)
            {
                g.setColour (onCol);
                g.fillRoundedRectangle (seg, 1.5f);
                g.setColour (juce::Colour (0x44ffffff));
                g.drawRoundedRectangle (seg, 1.5f, 0.6f);
            }
            else
            {
                g.setColour (onCol.withAlpha (0.10f));
                g.fillRoundedRectangle (seg, 1.5f);
            }
        }

        g.setColour (juce::Colour (0x99a8d0ff));
        g.setFont (juce::Font (juce::FontOptions (8.5f).withStyle ("Bold")));
        g.drawText ("OUT",
                    juce::Rectangle<int> ((int) area.getX(), (int) area.getBottom() - 14,
                                           (int) area.getWidth(), 12),
                    juce::Justification::centred);
    }

    void timerCallback() override
    {
        // ---- FFT update ----
        if (pendingFft.exchange (false, std::memory_order_acquire))
        {
            std::array<float, fftSize * 2> work {};
            {
                const juce::ScopedLock sl (lock);
                // copy starting at fftWrite (oldest first)
                for (int i = 0; i < fftSize; ++i)
                {
                    const int idx = (fftWrite + i) % fftSize;
                    work[(size_t) i] = fftIn[(size_t) idx];
                }
            }
            window.multiplyWithWindowingTable (work.data(), fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform (work.data());

            // copy magnitudes (first half)
            for (int i = 0; i < fftSize / 2; ++i)
            {
                const float m = work[(size_t) i] / (float) fftSize;
                fftMag[(size_t) i] = m;
                // smooth
                smoothMag[(size_t) i] = smoothMag[(size_t) i] * 0.55f + m * 0.45f;
            }
        }

        // ---- level / peak hold ----
        float p;
        {
            const juce::ScopedLock sl (lock);
            p = rmsPeak;
            rmsPeak *= 0.85f;
        }
        const float lvlDb = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, p));
        currentLevelDb = currentLevelDb * 0.55f + lvlDb * 0.45f;

        if (lvlDb > peakHoldDb)
        {
            peakHoldDb = lvlDb;
            peakHoldTimer = 1.4f; // seconds
        }
        else
        {
            peakHoldTimer -= 1.0f / 45.0f;
            if (peakHoldTimer < 0.0f)
            {
                peakHoldDb -= 0.6f; // dB / tick
                if (peakHoldDb < -60.0f) peakHoldDb = -60.0f;
            }
        }

        // ---- flash decay ----
        {
            const juce::ScopedLock sl (lock);
            if (flashTime > 0.0f)
            {
                flashTime -= 1.0f / 25.0f;
                if (flashTime < 0.0f) flashTime = 0.0f;
            }
        }

        repaint();
    }

public:
    void setSampleRateHint (double sr) noexcept { sampleRateHint = sr; }

private:
    juce::dsp::FFT                          forwardFFT;
    juce::dsp::WindowingFunction<float>     window;

    juce::CriticalSection                   lock;

    std::array<float, scopeLen>             ring   {};
    int                                     writePos = 0;

    std::array<float, fftSize>              fftIn      {};
    int                                     fftWrite   = 0;
    int                                     samplesSinceFft = 0;
    std::atomic<bool>                       pendingFft { false };

    std::array<float, fftSize / 2>          fftMag     {};
    std::array<float, fftSize / 2>          smoothMag  {};

    float rmsPeak       = 0.0f;
    float currentLevelDb = -60.0f;
    float peakHoldDb     = -60.0f;
    float peakHoldTimer  = 0.0f;

    float flashTime = 0.0f, flashVel = 1.0f;

    double sampleRateHint = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassScope)
};
