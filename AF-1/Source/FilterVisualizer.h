/*
  ==============================================================================

    FilterVisualizer.h
    Animated, log-frequency response curve with modulation glow.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class FilterVisualizer  : public juce::Component,
                          private juce::Timer
{
public:
    explicit FilterVisualizer (AF1AudioProcessor& proc) : processor (proc)
    {
        setOpaque (true);
        startTimerHz (30);
    }

    ~FilterVisualizer() override = default;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        // ── Luxurious dark background with subtle radial gradient
        {
            juce::ColourGradient bg (juce::Colour (0xff1f242c), bounds.getCentreX(), bounds.getCentreY(),
                                     juce::Colour (0xff0f1216), bounds.getRight(),   bounds.getBottom(), true);
            g.setGradientFill (bg);
            g.fillRect (bounds);
        }

        // ── Inner panel
        const auto plotArea = bounds.reduced (10.0f);

        g.setColour (juce::Colour (0xff0a0c10));
        g.fillRoundedRectangle (plotArea, 8.0f);

        drawGrid (g, plotArea);

        // ── Compute curve points
        const int   numPts = juce::jmax (64, (int) plotArea.getWidth());
        juce::Path curve;
        juce::Path fillPath;

        const float minHz   = 20.0f;
        const float maxHz   = 20000.0f;
        const float logMin  = std::log10 (minHz);
        const float logMax  = std::log10 (maxHz);

        const float cutoff  = processor.getDisplayCutoffHz();
        const float reso    = processor.getDisplayResonance();
        const int   type    = processor.getFilterTypeIndex();
        const int   slope   = processor.getSlopeIndex();
        const float Q       = juce::jmap (reso, 0.0f, 1.0f, 0.7071f, 14.0f);

        const float topY    = plotArea.getY() + 8.0f;
        const float botY    = plotArea.getBottom() - 8.0f;
        const float zeroY   = juce::jmap (0.0f, -36.0f, 18.0f, botY, topY);

        for (int i = 0; i < numPts; ++i)
        {
            const float t  = (float) i / (float) (numPts - 1);
            const float lf = juce::jmap (t, 0.0f, 1.0f, logMin, logMax);
            const float hz = std::pow (10.0f, lf);

            float magDb = magnitudeDb (hz, cutoff, Q, type, slope);
            magDb = juce::jlimit (-36.0f, 18.0f, magDb);

            const float x = plotArea.getX() + t * plotArea.getWidth();
            const float y = juce::jmap (magDb, -36.0f, 18.0f, botY, topY);

            if (i == 0)
            {
                curve.startNewSubPath (x, y);
                fillPath.startNewSubPath (x, botY);
                fillPath.lineTo (x, y);
            }
            else
            {
                curve.lineTo (x, y);
                fillPath.lineTo (x, y);
            }

            if (i == numPts - 1)
                fillPath.lineTo (x, botY);
        }
        fillPath.closeSubPath();

        // ── Fill under curve (deep blue glow)
        {
            juce::ColourGradient fillGrad (juce::Colour (0x884d9eff), plotArea.getCentreX(), topY,
                                           juce::Colour (0x114d9eff), plotArea.getCentreX(), botY, false);
            g.setGradientFill (fillGrad);
            g.fillPath (fillPath);
        }

        // ── Outer glow stroke
        g.setColour (juce::Colour (0x664d9eff));
        g.strokePath (curve, juce::PathStrokeType (4.0f));

        // ── Crisp curve stroke
        g.setColour (juce::Colour (0xffd0e7ff));
        g.strokePath (curve, juce::PathStrokeType (1.6f));

        // ── Cutoff marker
        const float cutT = (std::log10 (juce::jlimit (minHz, maxHz, cutoff)) - logMin) / (logMax - logMin);
        const float cutX = plotArea.getX() + cutT * plotArea.getWidth();

        g.setColour (juce::Colour (0xaa4d9eff));
        g.drawLine (cutX, plotArea.getY(), cutX, plotArea.getBottom(), 1.0f);

        // Cutoff dot
        const float cutY = juce::jmap (juce::jlimit (-36.0f, 18.0f, magnitudeDb (cutoff, cutoff, Q, type, slope)),
                                       -36.0f, 18.0f, botY, topY);
        g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.4f));
        g.fillEllipse (cutX - 9.0f, cutY - 9.0f, 18.0f, 18.0f);
        g.setColour (juce::Colour (0xffe8f4ff));
        g.fillEllipse (cutX - 4.0f, cutY - 4.0f, 8.0f, 8.0f);

        // ── Cutoff readout
        g.setColour (juce::Colour (0xffaab8c8));
        g.setFont (juce::Font (12.0f, juce::Font::plain));
        juce::String hzText = (cutoff >= 1000.0f)
            ? juce::String (cutoff / 1000.0f, 2) + " kHz"
            : juce::String (cutoff, 0) + " Hz";
        g.drawText (hzText, (int) plotArea.getRight() - 90, (int) plotArea.getY() + 6,
                    80, 18, juce::Justification::centredRight);

        // ── 0 dB reference line
        g.setColour (juce::Colour (0xff334050).withAlpha (0.6f));
        g.drawLine (plotArea.getX(), zeroY, plotArea.getRight(), zeroY, 1.0f);

        // ── Frame
        g.setColour (juce::Colour (0xff2a323c));
        g.drawRoundedRectangle (plotArea, 8.0f, 1.0f);

        // ── Activity badges (Env / LFO)
        drawActivity (g, plotArea);
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        repaint();
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float minHz  = 20.0f;
        const float maxHz  = 20000.0f;
        const float logMin = std::log10 (minHz);
        const float logMax = std::log10 (maxHz);

        // Frequency vertical lines + labels
        const float majors[]  = { 100.0f, 1000.0f, 10000.0f };
        const float minors[]  = { 30.0f, 50.0f, 70.0f, 200.0f, 300.0f, 500.0f, 700.0f,
                                  2000.0f, 3000.0f, 5000.0f, 7000.0f, 15000.0f };

        g.setColour (juce::Colour (0xff1c2128));
        for (float f : minors)
        {
            const float t = (std::log10 (f) - logMin) / (logMax - logMin);
            const float x = r.getX() + t * r.getWidth();
            g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
        }

        g.setColour (juce::Colour (0xff242c36));
        g.setFont (juce::Font (10.0f));
        for (float f : majors)
        {
            const float t = (std::log10 (f) - logMin) / (logMax - logMin);
            const float x = r.getX() + t * r.getWidth();
            g.setColour (juce::Colour (0xff2a323c));
            g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);

            g.setColour (juce::Colour (0xff667084));
            juce::String label = (f >= 1000.0f) ? juce::String (f / 1000.0f, 0) + "k" : juce::String (f, 0);
            g.drawText (label, (int) (x - 14), (int) r.getBottom() - 14, 28, 12, juce::Justification::centred);
        }

        // dB horizontal lines (labels: -24, -12, 0, +12)
        const int dBs[] = { -24, -12, 0, 12 };
        for (int dB : dBs)
        {
            const float y = juce::jmap ((float) dB, -36.0f, 18.0f, r.getBottom() - 8.0f, r.getY() + 8.0f);
            g.setColour (juce::Colour (0xff1c2128));
            g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
            g.setColour (juce::Colour (0xff667084));
            g.drawText (juce::String (dB), (int) r.getX() + 4, (int) y - 8, 30, 12, juce::Justification::centredLeft);
        }
    }

    void drawActivity (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float env = processor.getEnvelopeLevel();
        const float lfo = processor.getLfoValue();

        const float padX = 8.0f, padY = 8.0f;
        const float w    = 70.0f, h = 8.0f;

        // ENV bar (top-left)
        const float envX = r.getX() + padX;
        const float envY = r.getY() + padY;
        g.setColour (juce::Colour (0xff1c2128));
        g.fillRoundedRectangle (envX, envY, w, h, 4.0f);
        g.setColour (juce::Colour (0xff4d9eff));
        g.fillRoundedRectangle (envX, envY, w * juce::jlimit (0.0f, 1.0f, env), h, 4.0f);
        g.setColour (juce::Colour (0xff667084));
        g.setFont (juce::Font (10.0f));
        g.drawText ("ENV", (int) envX, (int) envY - 12, 40, 10, juce::Justification::centredLeft);

        // LFO bar (below ENV) - bipolar
        const float lfoX = envX;
        const float lfoY = envY + h + 16.0f;
        g.setColour (juce::Colour (0xff1c2128));
        g.fillRoundedRectangle (lfoX, lfoY, w, h, 4.0f);
        const float midX = lfoX + w * 0.5f;
        const float val  = juce::jlimit (-1.0f, 1.0f, lfo);
        const float barW = std::abs (val) * (w * 0.5f);
        g.setColour (juce::Colour (0xff4d9eff));
        if (val >= 0.0f) g.fillRoundedRectangle (midX, lfoY, barW, h, 4.0f);
        else             g.fillRoundedRectangle (midX - barW, lfoY, barW, h, 4.0f);
        g.setColour (juce::Colour (0xff667084));
        g.drawText ("LFO", (int) lfoX, (int) lfoY - 12, 40, 10, juce::Justification::centredLeft);
    }

    // Approximate analog magnitude responses (Hz, in dB) of the active filter.
    // s = j*(f/fc), Q controls peak height.  For 24 dB cascade we double it.
    float magnitudeDb (float f, float fc, float Q, int type, int slope) const noexcept
    {
        const float w     = juce::jmax (1.0f, f) / juce::jmax (1.0f, fc);
        const float w2    = w * w;

        // 2nd-order canonical bi-quad magnitudes
        // |H_LP|^2 = 1 / ((1 - w^2)^2 + (w/Q)^2)
        // |H_HP|^2 = w^4 / same
        // |H_BP|^2 = (w/Q)^2 / same
        // |H_Notch|^2 = (1 - w^2)^2 / same
        const float oneMinusW2sq = (1.0f - w2) * (1.0f - w2);
        const float wOverQsq     = (w / juce::jmax (0.0001f, Q)) * (w / juce::jmax (0.0001f, Q));
        const float den          = oneMinusW2sq + wOverQsq;

        float magSq = 1.0f;
        switch (type)
        {
            case 0: magSq = 1.0f / juce::jmax (1.0e-12f, den); break;             // LP
            case 1: magSq = wOverQsq / juce::jmax (1.0e-12f, den); break;          // BP
            case 2: magSq = (w2 * w2) / juce::jmax (1.0e-12f, den); break;         // HP
            case 3: magSq = oneMinusW2sq / juce::jmax (1.0e-12f, den); break;      // Notch
        }

        float dB = 10.0f * std::log10 (juce::jmax (1.0e-12f, magSq));
        if (slope == 1) dB *= 2.0f; // 24 dB cascade ≈ doubled magnitude (in dB)
        return dB;
    }

    AF1AudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterVisualizer)
};
