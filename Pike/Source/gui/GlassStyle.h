/*
  ==============================================================================

    GlassStyle.h
    Shared "sci-fi glass" panel painter used across the editor: tinted vertical
    gradient body, a specular sheen, subtle holographic scanlines, a bright top
    light-line, a light/dark bevel border and HUD-style corner brackets.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace pike::gui
{
    /** Central theme colours — the sci-fi neon palette. */
    namespace theme
    {
        const juce::Colour accent      { 0xffff9425 };   // neon orange (HUD primary)
        const juce::Colour accentWarm  { 0xffff9d3c };   // amber telemetry (secondary)
        const juce::Colour bgDeep      { 0xff0b0e13 };
        const juce::Colour panelTop    { 0xff222b36 };
        const juce::Colour panelBottom { 0xff0a0f15 };
        const juce::Colour gridLine    { 0x0dff9425 };   // very faint accent for holo grids
        const juce::Colour textDim     { 0xff7e93a8 };
    }

    /** HUD-style corner brackets: short L-shaped strokes in each corner of the
        panel — the signature sci-fi framing element. */
    inline void drawCornerBrackets (juce::Graphics& g, juce::Rectangle<float> b,
                                    juce::Colour col, float len = 9.0f, float thickness = 1.5f)
    {
        g.setColour (col);
        const float x0 = b.getX(),     y0 = b.getY();
        const float x1 = b.getRight(), y1 = b.getBottom();

        // top-left
        g.fillRect (juce::Rectangle<float> (x0, y0, len, thickness));
        g.fillRect (juce::Rectangle<float> (x0, y0, thickness, len));
        // top-right
        g.fillRect (juce::Rectangle<float> (x1 - len, y0, len, thickness));
        g.fillRect (juce::Rectangle<float> (x1 - thickness, y0, thickness, len));
        // bottom-left
        g.fillRect (juce::Rectangle<float> (x0, y1 - thickness, len, thickness));
        g.fillRect (juce::Rectangle<float> (x0, y1 - len, thickness, len));
        // bottom-right
        g.fillRect (juce::Rectangle<float> (x1 - len, y1 - thickness, len, thickness));
        g.fillRect (juce::Rectangle<float> (x1 - thickness, y1 - len, thickness, len));
    }

    /** Faint holographic grid (vertical + horizontal hairlines) for backgrounds. */
    inline void drawHoloGrid (juce::Graphics& g, juce::Rectangle<float> area,
                              float spacing = 26.0f, juce::Colour col = theme::gridLine)
    {
        g.setColour (col);
        for (float x = area.getX(); x < area.getRight(); x += spacing)
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        for (float y = area.getY(); y < area.getBottom(); y += spacing)
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    }

    /** Fills a rounded "polished dark smoked glass" panel: a deep dark tinted
        body, ONE thin sharp polished highlight along the very top edge, subtle
        scanlines, a crisp bevel border and cyan HUD corner brackets.
        highlightAlpha controls the polish strength. */
    inline void fillGlassPanel (juce::Graphics& g, juce::Rectangle<float> b,
                                float corner = 5.0f,
                                juce::Colour topCol    = theme::panelTop,
                                juce::Colour bottomCol = theme::panelBottom,
                                float highlightAlpha = 0.20f)
    {
        // Deep smoked-glass body gradient (dark throughout).
        juce::ColourGradient body (topCol,    b.getX(), b.getY(),
                                   bottomCol, b.getX(), b.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (b, corner);

        // Subtle holographic scanlines across the panel body.
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (b, corner);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colours::white.withAlpha (0.022f));
            for (float y = b.getY() + 3.0f; y < b.getBottom(); y += 3.0f)
                g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
        }

        // Thin polished highlight strip along the very top (a few px only).
        const float hlH = juce::jmin (4.0f, b.getHeight() * 0.12f);
        auto hl = b.withHeight (hlH);
        juce::Path hp;
        hp.addRoundedRectangle (hl.getX(), hl.getY(), hl.getWidth(), hl.getHeight(),
                                corner, corner, true, true, false, false);
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (highlightAlpha), hl.getX(), hl.getY(),
                                    juce::Colours::white.withAlpha (0.0f),           hl.getX(), hl.getBottom(), false);
        g.setGradientFill (gloss);
        g.fillPath (hp);

        // Crisp specular top line (the polished glint).
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawLine (b.getX() + corner, b.getY() + 1.0f, b.getRight() - corner, b.getY() + 1.0f, 1.0f);

        // Bevel border: dark outer line + a faintly cyan-lit inner line that
        // ties the panel edge to the surrounding glow.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (b, corner, 1.2f);
        g.setColour (theme::accent.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (1.2f), juce::jmax (1.0f, corner - 1.0f), 1.0f);

        // HUD corner brackets.
        drawCornerBrackets (g, b.reduced (0.5f), theme::accent.withAlpha (0.75f));
    }

    /** Condensed uppercase HUD font with letterspacing for titles/labels. */
    inline juce::Font hudFont (float height, bool bold = true)
    {
        auto f = juce::Font (height, bold ? juce::Font::bold : juce::Font::plain);
        return f.withExtraKerningFactor (0.10f);
    }
}
