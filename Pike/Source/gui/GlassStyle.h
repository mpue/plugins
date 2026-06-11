/*
  ==============================================================================

    GlassStyle.h
    Shared "glass" panel painter used across the editor for a glossy, frosted
    look: tinted vertical gradient body, a specular sheen over the upper half,
    a bright top light-line and a light/dark bevel border.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace pike::gui
{
    /** Fills a rounded "polished dark smoked glass" panel: a deep dark tinted
        body, ONE thin sharp polished highlight along the very top edge, and a
        crisp bevel border (dark outer / subtle light inner). highlightAlpha
        controls the polish strength. */
    inline void fillGlassPanel (juce::Graphics& g, juce::Rectangle<float> b,
                                float corner = 5.0f,
                                juce::Colour topCol    = juce::Colour (0xff2b313d),
                                juce::Colour bottomCol = juce::Colour (0xff0d1117),
                                float highlightAlpha = 0.20f)
    {
        // Deep smoked-glass body gradient (dark throughout).
        juce::ColourGradient body (topCol,    b.getX(), b.getY(),
                                   bottomCol, b.getX(), b.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (b, corner);

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

        // Bevel border: dark outer line + subtle light inner line.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (b, corner, 1.2f);
        g.setColour (juce::Colour (0xff464e5e).withAlpha (0.8f));
        g.drawRoundedRectangle (b.reduced (1.2f), juce::jmax (1.0f, corner - 1.0f), 1.0f);
    }
}
