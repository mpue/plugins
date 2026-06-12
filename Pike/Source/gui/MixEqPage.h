/*
  ==============================================================================

    MixEqPage.h
    Mix / EQ tab: master gain + an 8-band parametric EQ (ported from Lupo) with a
    draggable frequency-response display and per-band gain/freq/Q knobs.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeUI.h"
#include "GlassStyle.h"
#include "Visualisers.h"
#include "../params/ParameterIDs.h"
#include "../dsp/fx/ParametricEQ.h"

namespace pike::gui
{
    //==============================================================================
    /** Frequency response of the 8-band EQ, computed from the parameters, with
        draggable band nodes (horizontal = freq, vertical = gain). */
    class EqDisplay : public juce::Component, private juce::Timer
    {
    public:
        explicit EqDisplay (juce::AudioProcessorValueTreeState& s) : state (s)
        {
            for (int b = 0; b < numBands; ++b)
            {
                freqParam[b] = state.getParameter (pid::eqFreq[b]);
                gainParam[b] = state.getParameter (pid::eqGain[b]);
            }
            startTimerHz (30);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (1.0f);
            fillGlassPanel (g, b, 5.0f, juce::Colour (0xff222833), juce::Colour (0xff090c11), 0.08f);

            plot = b.reduced (10.0f, 10.0f);

            // Grid: 0 dB line + decade verticals.
            g.setColour (col::line.withAlpha (0.5f));
            g.drawHorizontalLine ((int) dbToY (0.0f), plot.getX(), plot.getRight());
            g.setColour (col::line.withAlpha (0.22f));
            for (double f : { 100.0, 1000.0, 10000.0 })
                g.drawVerticalLine ((int) freqToX (f), plot.getY(), plot.getBottom());

            // Response curve.
            juce::Path curve;
            const int w = juce::jmax (2, (int) plot.getWidth());
            for (int x = 0; x <= w; ++x)
            {
                const double f  = xToFreqNorm ((float) x / w);
                const float  db = magnitudeDb (f);
                const float  px = plot.getX() + (float) x;
                const float  py = dbToY (db);
                if (x == 0) curve.startNewSubPath (px, py);
                else        curve.lineTo (px, py);
            }
            juce::Path fill = curve;
            fill.lineTo (plot.getRight(), dbToY (0.0f));
            fill.lineTo (plot.getX(),     dbToY (0.0f));
            fill.closeSubPath();
            g.setColour (col::accent.withAlpha (0.18f));
            g.fillPath (fill);
            g.setColour (col::accent);
            g.strokePath (curve, juce::PathStrokeType (1.8f));

            // Band nodes.
            for (int i = 0; i < numBands; ++i)
            {
                const float x = freqToX (getF (i));
                const float y = dbToY (getG (i));
                const bool  hot = (i == hoverBand || i == dragBand);
                g.setColour (col::accent.withAlpha (hot ? 0.35f : 0.18f));
                g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
                g.setColour (juce::Colours::white);
                g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragBand = nearestBand (e.position);
            if (dragBand >= 0)
            {
                if (freqParam[dragBand]) freqParam[dragBand]->beginChangeGesture();
                if (gainParam[dragBand]) gainParam[dragBand]->beginChangeGesture();
                mouseDrag (e);
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragBand < 0) return;
            setF (dragBand, xToFreqNorm (juce::jlimit (0.0f, 1.0f, (e.position.x - plot.getX()) / plot.getWidth())));
            setG (dragBand, yToDb (e.position.y));
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragBand >= 0)
            {
                if (freqParam[dragBand]) freqParam[dragBand]->endChangeGesture();
                if (gainParam[dragBand]) gainParam[dragBand]->endChangeGesture();
            }
            dragBand = -1;
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int h = nearestBand (e.position);
            if (h != hoverBand) { hoverBand = h; repaint(); }
        }

    private:
        static constexpr int numBands = pike::ParametricEQ::numBands;
        static constexpr double fMin = 20.0, fMax = 20000.0;
        static constexpr float  dbMax = 24.0f, dbMin = -24.0f;

        float getF (int b) const { auto* p = state.getRawParameterValue (pid::eqFreq[b]); return p ? p->load() : 1000.0f; }
        float getG (int b) const { auto* p = state.getRawParameterValue (pid::eqGain[b]); return p ? p->load() : 0.0f; }
        float getQ (int b) const { auto* p = state.getRawParameterValue (pid::eqQ[b]);    return p ? p->load() : 1.0f; }

        void setF (int b, float hz) { if (freqParam[b]) freqParam[b]->setValueNotifyingHost (freqParam[b]->convertTo0to1 (hz)); }
        void setG (int b, float db) { if (gainParam[b]) gainParam[b]->setValueNotifyingHost (gainParam[b]->convertTo0to1 (juce::jlimit (dbMin, dbMax, db))); }

        static double xToFreqNorm (float n) { return fMin * std::pow (fMax / fMin, (double) juce::jlimit (0.0f, 1.0f, n)); }
        float freqToX (double f) const { return plot.getX() + (float) (std::log (f / fMin) / std::log (fMax / fMin)) * plot.getWidth(); }
        float dbToY  (float db) const  { return plot.getBottom() - (juce::jlimit (dbMin, dbMax, db) - dbMin) / (dbMax - dbMin) * plot.getHeight(); }
        float yToDb  (float y) const   { return dbMin + (plot.getBottom() - y) / plot.getHeight() * (dbMax - dbMin); }

        int nearestBand (juce::Point<float> p) const
        {
            int best = -1; float bestD = 26.0f;
            for (int i = 0; i < numBands; ++i)
            {
                const float d = std::abs (freqToX (getF (i)) - p.x);
                if (d < bestD) { bestD = d; best = i; }
            }
            return best;
        }

        float magnitudeDb (double freq) const
        {
            double mag = 1.0;
            for (int i = 0; i < numBands; ++i)
            {
                using IIR = juce::dsp::IIR::Coefficients<float>;
                juce::ReferenceCountedObjectPtr<IIR> coeff;
                const float f = getF (i), q = juce::jlimit (0.1f, 10.0f, getQ (i)), lin = juce::Decibels::decibelsToGain (getG (i));
                if (i == 0)            coeff = IIR::makeLowShelf  (44100.0, f, q, lin);
                else if (i == numBands - 1) coeff = IIR::makeHighShelf (44100.0, f, q, lin);
                else                   coeff = IIR::makePeakFilter (44100.0, f, q, lin);
                if (coeff != nullptr)  mag *= coeff->getMagnitudeForFrequency (freq, 44100.0);
            }
            return (float) juce::Decibels::gainToDecibels (mag);
        }

        void timerCallback() override
        {
            float s = 0.0f;
            for (int b = 0; b < numBands; ++b) s += getF (b) * 0.001f + getG (b) + getQ (b);
            if (std::abs (s - lastSum) > 1.0e-5f) { lastSum = s; repaint(); }
        }

        juce::AudioProcessorValueTreeState& state;
        juce::RangedAudioParameter* freqParam[numBands] {};
        juce::RangedAudioParameter* gainParam[numBands] {};
        juce::Rectangle<float> plot;
        int   dragBand = -1, hoverBand = -1;
        float lastSum = -1.0e9f;
    };

    //==============================================================================
    /** One EQ band column: glass panel with a band title + Gain/Freq/Q knobs. */
    class EqBandPanel : public juce::Component
    {
    public:
        EqBandPanel (juce::AudioProcessorValueTreeState& state, int band)
            : title (band == 0 ? "LOW" : band == 7 ? "HIGH" : "BAND " + juce::String (band + 1))
        {
            gain = std::make_unique<Control> (state, CtrlSpec { CtrlType::Knob, pid::eqGain[band], "Gain" });
            freq = std::make_unique<Control> (state, CtrlSpec { CtrlType::Knob, pid::eqFreq[band], "Freq" });
            q    = std::make_unique<Control> (state, CtrlSpec { CtrlType::Knob, pid::eqQ[band],    "Q" });
            for (auto* c : { gain.get(), freq.get(), q.get() })
                addAndMakeVisible (*c);
        }

        static constexpr int titleH = 18;

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.5f);
            fillGlassPanel (g, b, 5.0f);

            auto titleBar = b.removeFromTop ((float) titleH);
            g.setColour (juce::Colour (0xffe8f4ff));
            g.setFont (hudFont (10.0f));
            g.drawText (title, titleBar, juce::Justification::centred, false);
            juce::ColourGradient underline (theme::accent.withAlpha (0.0f),  b.getX() + 8.0f, 0.0f,
                                            theme::accent.withAlpha (0.45f), b.getCentreX(), 0.0f, false);
            underline.addColour (1.0, theme::accent.withAlpha (0.0f));
            g.setGradientFill (underline);
            g.fillRect (b.getX() + 8.0f, titleBar.getBottom(), b.getWidth() - 16.0f, 1.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (4);
            r.removeFromTop (titleH);
            const int rh = r.getHeight() / 3;
            gain->setBounds (r.removeFromTop (rh));
            freq->setBounds (r.removeFromTop (rh));
            q->setBounds (r);
        }

    private:
        juce::String title;
        std::unique_ptr<Control> gain, freq, q;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqBandPanel)
    };

    //==============================================================================
    class MixEqPage : public juce::Component
    {
    public:
        explicit MixEqPage (juce::AudioProcessorValueTreeState& state)
        {
            master = std::make_unique<Group> (state, GroupSpec { "Master", {
                { CtrlType::Knob,   pid::masterGain, "Gain" },
                { CtrlType::Toggle, pid::eqOn,       "EQ On" } } });
            addAndMakeVisible (*master);

            display = std::make_unique<EqDisplay> (state);
            addAndMakeVisible (*display);

            for (int b = 0; b < 8; ++b)
                addAndMakeVisible (bands.add (new EqBandPanel (state, b)));
        }

        void paint (juce::Graphics& g) override
        {
            juce::ColourGradient bg (juce::Colour (0xff1e242e), 0.0f, 0.0f,
                                     juce::Colour (0xff0b0e13), 0.0f, (float) getHeight(), false);
            g.setGradientFill (bg);
            g.fillAll();
            paintPanelShadows (g, *this);

            // Subtle "EQ" caption above the band grid.
            g.setColour (theme::accent);
            g.setFont (hudFont (11.0f));
            g.drawText ("8-BAND PARAMETRIC EQ", bandTop.translated (4, -16).withHeight (14),
                        juce::Justification::bottomLeft, false);
        }

        void resized() override
        {
            const int pad = 8;
            auto area = getLocalBounds().reduced (pad);

            // Top row: Master group (left) + response display (rest).
            auto top = area.removeFromTop (juce::jmax (150, master->preferredHeight()));
            master->setBounds (top.removeFromLeft (master->preferredWidth()));
            top.removeFromLeft (pad);
            display->setBounds (top);

            area.removeFromTop (pad + 14);
            bandTop = area;

            // 8 band panels side by side (each: title + Gain / Freq / Q).
            const int cols = 8;
            const int cw = area.getWidth() / cols;
            const int h  = juce::jmin (EqBandPanel::titleH + 8 + 3 * 84, area.getHeight());
            for (int b = 0; b < cols; ++b)
                bands[b]->setBounds (area.getX() + b * cw, area.getY(), cw, h);
        }

    private:
        std::unique_ptr<Group>       master;
        std::unique_ptr<EqDisplay>   display;
        juce::OwnedArray<EqBandPanel> bands;
        juce::Rectangle<int>         bandTop;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixEqPage)
    };
}
