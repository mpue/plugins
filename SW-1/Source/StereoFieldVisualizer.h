/*
  ==============================================================================

    StereoFieldVisualizer.h
    A luxurious phosphor-style goniometer with correlation meter, M/S/L/R level
    bars, percentage rings, axis legends, and a stereo-spread arc.
    Draws via a persistent juce::Image so trails decay smoothly like a CRT.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LuxuryStereoWidener.h"

class StereoFieldVisualizer : public juce::Component,
                              private juce::Timer
{
public:
    explicit StereoFieldVisualizer (LuxuryStereoWidener& w)
        : widener (w)
    {
        startTimerHz (45);
        setOpaque (true);
    }

    ~StereoFieldVisualizer() override = default;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // -------- Background panel ----------------------------------------------
        juce::ColourGradient bg (
            juce::Colour (0xff121821), bounds.getCentreX(), bounds.getY(),
            juce::Colour (0xff080b10), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 10.0f);

        g.setColour (juce::Colour (0xff223046));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

        // -------- Compute layout (square goniometer + meters area) ---------------
        const auto inner   = bounds.reduced (12.0f);
        const float metersW = 92.0f;
        const float bottomH = 56.0f;

        auto goniArea = inner;
        goniArea.removeFromRight (metersW + 14.0f);
        goniArea.removeFromBottom (bottomH + 10.0f);

        // square it
        const float side = juce::jmin (goniArea.getWidth(), goniArea.getHeight());
        goniArea = juce::Rectangle<float> (goniArea.getX(),
                                           goniArea.getY() + (goniArea.getHeight() - side) * 0.5f,
                                           side, side);

        drawGoniometer (g, goniArea);

        // -------- Right-side meters ---------------------------------------------
        auto meterArea = juce::Rectangle<float> (
            inner.getRight() - metersW, inner.getY(),
            metersW, inner.getHeight() - bottomH - 10.0f);
        drawMeters (g, meterArea);

        // -------- Bottom: correlation + spread arc ------------------------------
        auto bottom = juce::Rectangle<float> (
            inner.getX(), inner.getBottom() - bottomH,
            inner.getWidth(), bottomH);
        drawCorrelationStrip (g, bottom);
    }

    void resized() override
    {
        // Rebuild the phosphor image
        auto goniSide = juce::jmin (getWidth(), getHeight()) - 24;
        goniSide = juce::jmax (goniSide, 64);
        phosphor = juce::Image (juce::Image::ARGB, goniSide, goniSide, true);
        clearPhosphor();
        lastReadIdx = -1;
    }

private:
    //==============================================================================
    void timerCallback() override
    {
        // Fade phosphor
        if (phosphor.isValid())
        {
            juce::Graphics g (phosphor);
            g.setColour (juce::Colour (0, 0, 0).withAlpha (0.18f));
            g.fillAll();
        }

        // Pull new samples since last read
        const auto& vis = widener.getVisualState();
        const int writeIdx = vis.writeIdx.load (std::memory_order_acquire);
        if (lastReadIdx < 0) lastReadIdx = writeIdx;

        int idx = lastReadIdx;
        const int max = LuxuryStereoWidener::VisualSize;
        const int radius2 = phosphor.isValid() ? (phosphor.getWidth() / 2) : 0;

        if (radius2 > 0)
        {
            juce::Graphics g (phosphor);
            const float cx = (float) radius2;
            const float cy = (float) radius2;
            const float r  = (float) radius2 - 4.0f;
            const float invSqrt2 = 0.7071067811865475f;

            const juce::Colour core   (0xffaad6ff);
            const juce::Colour glowC  (0xff4d9eff);

            int safety = 0;
            while (idx != writeIdx && safety < max)
            {
                const float L = vis.bufL[(size_t) idx];
                const float R = vis.bufR[(size_t) idx];

                // Rotate (L,R) so L points up-left, R points up-right
                const float x = (R - L) * invSqrt2;
                const float y = (L + R) * invSqrt2;

                const float px = cx + x * r;
                const float py = cy - y * r;

                // Soft glow underlay (only every few samples for performance)
                if ((safety & 0x3) == 0)
                {
                    g.setColour (glowC.withAlpha (0.18f));
                    g.fillEllipse (px - 2.5f, py - 2.5f, 5.0f, 5.0f);
                }
                g.setColour (core.withAlpha (0.85f));
                g.fillEllipse (px - 0.7f, py - 0.7f, 1.4f, 1.4f);

                idx = (idx + 1) % max;
                ++safety;
            }
        }

        lastReadIdx = writeIdx;
        repaint();
    }

    void clearPhosphor()
    {
        if (! phosphor.isValid()) return;
        juce::Graphics g (phosphor);
        g.fillAll (juce::Colours::transparentBlack);
    }

    //==============================================================================
    void drawGoniometer (juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Recessed plate
        g.setColour (juce::Colour (0xff05080d));
        g.fillRoundedRectangle (area, 8.0f);
        g.setColour (juce::Colour (0xff1a2535));
        g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

        const float cx = area.getCentreX();
        const float cy = area.getCentreY();
        const float maxR = area.getWidth() * 0.5f - 8.0f;

        // Range rings
        for (int i = 1; i <= 4; ++i)
        {
            const float t = (float) i / 4.0f;
            const float r = maxR * t;
            g.setColour (juce::Colour (0xff1d2a3d).withAlpha (i == 4 ? 0.9f : 0.6f));
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        }

        // Diagonal L / R axes
        const float invSqrt2 = 0.7071067811865475f;
        auto drawAxis = [&] (float dx, float dy)
        {
            g.drawLine (cx - dx * maxR, cy - dy * maxR,
                        cx + dx * maxR, cy + dy * maxR, 1.0f);
        };
        g.setColour (juce::Colour (0xff1f2c3f));
        drawAxis (-invSqrt2, -invSqrt2); // L axis (up-left)
        drawAxis ( invSqrt2, -invSqrt2); // R axis (up-right)

        // Vertical M / horizontal S axes
        g.setColour (juce::Colour (0xff263a55));
        g.drawLine (cx, cy - maxR, cx, cy + maxR, 1.0f);
        g.drawLine (cx - maxR, cy, cx + maxR, cy, 1.0f);

        // Phosphor trails
        if (phosphor.isValid())
        {
            const auto destSize = juce::jmin (area.getWidth(), area.getHeight()) - 16.0f;
            const float dx = cx - destSize * 0.5f;
            const float dy = cy - destSize * 0.5f;
            g.drawImage (phosphor,
                         juce::Rectangle<float> (dx, dy, destSize, destSize),
                         juce::RectanglePlacement::stretchToFit);
        }

        // Outer rim glow
        juce::ColourGradient rim (
            juce::Colour (0x4d4d9eff), cx, cy,
            juce::Colour (0x004d9eff), cx, cy + maxR, true);
        g.setGradientFill (rim);
        g.drawEllipse (cx - maxR, cy - maxR, maxR * 2.0f, maxR * 2.0f, 1.5f);

        // Axis labels
        g.setColour (juce::Colour (0xff7d99c0));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        const float ll = maxR * 0.96f;
        g.drawText ("L",  juce::Rectangle<float> (cx - invSqrt2 * ll - 14, cy - invSqrt2 * ll - 14, 14, 14),
                    juce::Justification::centred);
        g.drawText ("R",  juce::Rectangle<float> (cx + invSqrt2 * ll,      cy - invSqrt2 * ll - 14, 14, 14),
                    juce::Justification::centred);
        g.drawText ("M",  juce::Rectangle<float> (cx - 8, cy - maxR - 14, 16, 14),
                    juce::Justification::centred);
        g.drawText ("S",  juce::Rectangle<float> (cx + maxR + 4, cy - 7, 16, 14),
                    juce::Justification::centred);

        // "GONIOMETER" caption
        g.setColour (juce::Colour (0xff5778a3));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText ("GONIOMETER", area.reduced (10.0f, 6.0f),
                    juce::Justification::topLeft);
    }

    //==============================================================================
    void drawMeters (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (juce::Colour (0xff05080d));
        g.fillRoundedRectangle (area, 8.0f);
        g.setColour (juce::Colour (0xff1a2535));
        g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (juce::Colour (0xff5778a3));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText ("LEVELS", area.reduced (8.0f, 6.0f), juce::Justification::topLeft);

        auto inner = area.reduced (10.0f, 22.0f);
        const float gap = 6.0f;
        const float colW = (inner.getWidth() - gap * 3.0f) / 4.0f;

        const auto& v = widener.getVisualState();
        const float pL = v.peakL.load (std::memory_order_relaxed);
        const float pR = v.peakR.load (std::memory_order_relaxed);
        const float pM = v.peakM.load (std::memory_order_relaxed);
        const float pS = v.peakS.load (std::memory_order_relaxed);

        auto drawMeter = [&] (juce::Rectangle<float> r, float level, juce::Colour col, const juce::String& cap)
        {
            // Track
            g.setColour (juce::Colour (0xff10161f));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colour (0xff1d2a3d));
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

            // Tick marks
            g.setColour (juce::Colour (0xff223247));
            for (int i = 1; i < 8; ++i)
            {
                const float ty = r.getY() + r.getHeight() * (float) i / 8.0f;
                g.drawHorizontalLine ((int) ty, r.getX() + 2.0f, r.getRight() - 2.0f);
            }

            // Fill
            const float lvlDb  = juce::Decibels::gainToDecibels (juce::jmax (level, 1.0e-5f));
            const float norm   = juce::jlimit (0.0f, 1.0f, (lvlDb + 60.0f) / 60.0f);
            const float fillH  = (r.getHeight() - 4.0f) * norm;
            auto fillRect = juce::Rectangle<float> (r.getX() + 2.0f,
                                                    r.getBottom() - 2.0f - fillH,
                                                    r.getWidth() - 4.0f,
                                                    fillH);

            juce::ColourGradient grad (
                juce::Colour (0xffff5566), fillRect.getX(), fillRect.getY(),
                col,                       fillRect.getX(), fillRect.getBottom(), false);
            grad.addColour (0.30, juce::Colour (0xffffd166));
            grad.addColour (0.55, col.brighter (0.2f));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fillRect, 2.0f);

            // Glossy top
            g.setGradientFill (juce::ColourGradient (
                juce::Colour (0x55ffffff), fillRect.getX(), fillRect.getY(),
                juce::Colour (0x00ffffff), fillRect.getX(), fillRect.getY() + fillRect.getHeight() * 0.5f,
                false));
            g.fillRoundedRectangle (fillRect.withHeight (juce::jmin (fillRect.getHeight(), 14.0f)), 2.0f);

            // Caption
            g.setColour (juce::Colour (0xff7d99c0));
            g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
            g.drawText (cap, juce::Rectangle<float> (r.getX() - 2, r.getBottom() + 2, r.getWidth() + 4, 12),
                        juce::Justification::centred);
        };

        auto col = inner;
        col.setHeight (col.getHeight() - 14.0f);

        const juce::Colour lrCol (0xff4d9eff);
        const juce::Colour msCol (0xff8b6dff);

        drawMeter (col.withWidth (colW),                              pL, lrCol, "L");
        drawMeter (col.withWidth (colW).translated (colW + gap, 0.0f), pR, lrCol, "R");
        drawMeter (col.withWidth (colW).translated ((colW + gap) * 2.0f, 0.0f), pM, msCol, "M");
        drawMeter (col.withWidth (colW).translated ((colW + gap) * 3.0f, 0.0f), pS, msCol, "S");
    }

    //==============================================================================
    void drawCorrelationStrip (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (juce::Colour (0xff05080d));
        g.fillRoundedRectangle (area, 8.0f);
        g.setColour (juce::Colour (0xff1a2535));
        g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (juce::Colour (0xff5778a3));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText ("CORRELATION", area.reduced (10.0f, 4.0f), juce::Justification::topLeft);

        auto bar = area.reduced (12.0f, 0.0f);
        bar.removeFromTop (16.0f);
        bar.removeFromBottom (4.0f);
        const float barH = 16.0f;
        bar = juce::Rectangle<float> (bar.getX(), bar.getY(), bar.getWidth(), barH);

        // Background gradient: red (left) -> yellow (mid) -> green (right)
        juce::ColourGradient bg (
            juce::Colour (0xff7a2030), bar.getX(),     bar.getY(),
            juce::Colour (0xff1f7a48), bar.getRight(), bar.getY(), false);
        bg.addColour (0.5, juce::Colour (0xff706018));
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bar, 4.0f);

        g.setColour (juce::Colour (0xff1a2535));
        g.drawRoundedRectangle (bar.reduced (0.5f), 4.0f, 1.0f);

        // Tick marks at -1, -0.5, 0, +0.5, +1
        g.setColour (juce::Colour (0xff10161f));
        for (int i = 0; i <= 4; ++i)
        {
            const float t = (float) i / 4.0f;
            const float x = bar.getX() + bar.getWidth() * t;
            g.drawLine (x, bar.getY() + 2, x, bar.getBottom() - 2, 1.0f);
        }

        // Pointer
        const float corr = widener.getVisualState().correlation.load (std::memory_order_relaxed);
        const float pointerT = juce::jlimit (0.0f, 1.0f, (corr + 1.0f) * 0.5f);
        const float px = bar.getX() + bar.getWidth() * pointerT;

        // Pointer triangle
        juce::Path tri;
        tri.addTriangle (px,        bar.getY() - 6.0f,
                         px - 6.0f, bar.getY() - 1.0f,
                         px + 6.0f, bar.getY() - 1.0f);
        g.setColour (juce::Colour (0xffe6f0ff));
        g.fillPath (tri);

        // Pointer line
        g.setColour (juce::Colour (0xffe6f0ff).withAlpha (0.85f));
        g.drawLine (px, bar.getY(), px, bar.getBottom(), 1.5f);

        // Value text + width % readout
        const float widthPct = juce::jlimit (0.0f, 200.0f, (1.0f - corr) * 100.0f);

        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.setColour (juce::Colour (0xffd2dbe6));
        auto txtArea = area;
        txtArea.removeFromLeft (area.getWidth() - 200.0f);
        txtArea.removeFromTop (3.0f);
        g.drawText (juce::String ("Corr ") + juce::String (corr, 2)
                    + "    Width " + juce::String ((int) widthPct) + "%",
                    txtArea, juce::Justification::topRight);

        // Hints
        g.setColour (juce::Colour (0xff506b8f));
        g.setFont (juce::Font (juce::FontOptions (8.5f)));
        g.drawText ("MONO -1",  juce::Rectangle<float> (bar.getX() - 4, bar.getBottom(),  60, 12),
                    juce::Justification::topLeft);
        g.drawText ("WIDE 0",   juce::Rectangle<float> (bar.getCentreX() - 30, bar.getBottom(), 60, 12),
                    juce::Justification::centredTop);
        g.drawText ("MONO +1",  juce::Rectangle<float> (bar.getRight() - 56, bar.getBottom(), 60, 12),
                    juce::Justification::topRight);
    }

    //==============================================================================
    LuxuryStereoWidener& widener;
    juce::Image phosphor;
    int lastReadIdx = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoFieldVisualizer)
};
