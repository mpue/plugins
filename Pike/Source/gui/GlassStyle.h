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
    /** Fills a rounded "glass" panel. sheenAlpha controls the top gloss strength
        (use a lower value for dark plot backgrounds to keep them readable). */
    inline void fillGlassPanel (juce::Graphics& g, juce::Rectangle<float> b,
                                float corner = 8.0f,
                                juce::Colour topCol    = juce::Colour (0xff3a4250),
                                juce::Colour bottomCol = juce::Colour (0xff1b1f28),
                                float sheenAlpha = 0.13f)
    {
        // Body gradient.
        juce::ColourGradient body (topCol,    b.getX(), b.getY(),
                                   bottomCol, b.getX(), b.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (b, corner);

        // Glossy sheen over the upper half (rounded only at the top).
        auto sheen = b.withHeight (b.getHeight() * 0.5f);
        juce::Path sp;
        sp.addRoundedRectangle (sheen.getX(), sheen.getY(), sheen.getWidth(), sheen.getHeight(),
                                corner, corner, true, true, false, false);
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (sheenAlpha), sheen.getX(), sheen.getY(),
                                    juce::Colours::white.withAlpha (0.0f),       sheen.getX(), sheen.getBottom(), false);
        g.setGradientFill (gloss);
        g.fillPath (sp);

        // Bright specular top edge.
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawLine (b.getX() + corner, b.getY() + 1.0f, b.getRight() - corner, b.getY() + 1.0f, 1.2f);

        // Light bevel border + a thin dark line at the bottom for glass thickness.
        g.setColour (juce::Colour (0xff525a6b).withAlpha (0.85f));
        g.drawRoundedRectangle (b, corner, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.drawLine (b.getX() + corner, b.getBottom() - 1.0f, b.getRight() - corner, b.getBottom() - 1.0f, 1.0f);
    }
}
