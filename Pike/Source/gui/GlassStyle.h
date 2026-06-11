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
    /** Fills a rounded "glass" panel with a hard, high-contrast look: strong
        tinted gradient, a defined top reflection with a crisp cutoff line, a
        bright specular edge and a bevel border (dark outer / light inner).
        sheenAlpha controls the reflection strength (lower for dark plot panels). */
    inline void fillGlassPanel (juce::Graphics& g, juce::Rectangle<float> b,
                                float corner = 5.0f,
                                juce::Colour topCol    = juce::Colour (0xff464f62),
                                juce::Colour bottomCol = juce::Colour (0xff12151d),
                                float sheenAlpha = 0.24f)
    {
        // Body gradient (wide tonal range for contrast).
        juce::ColourGradient body (topCol,    b.getX(), b.getY(),
                                   bottomCol, b.getX(), b.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (b, corner);

        // Hard reflection: top ~46%, only mildly fading, ending in a crisp line.
        const float sheenH = b.getHeight() * 0.46f;
        auto sheen = b.withHeight (sheenH);
        juce::Path sp;
        sp.addRoundedRectangle (sheen.getX(), sheen.getY(), sheen.getWidth(), sheen.getHeight(),
                                corner, corner, true, true, false, false);
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (sheenAlpha),        sheen.getX(), sheen.getY(),
                                    juce::Colours::white.withAlpha (sheenAlpha * 0.45f), sheen.getX(), sheen.getBottom(), false);
        g.setGradientFill (gloss);
        g.fillPath (sp);

        // Crisp reflection cutoff line where the gloss ends.
        g.setColour (juce::Colours::white.withAlpha (sheenAlpha * 0.5f));
        g.drawLine (b.getX() + corner, sheen.getBottom(), b.getRight() - corner, sheen.getBottom(), 1.0f);

        // Bright specular top edge.
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawLine (b.getX() + corner, b.getY() + 1.0f, b.getRight() - corner, b.getY() + 1.0f, 1.4f);

        // Bevel border: dark outer line + bright inner line = hard glass edge.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (b, corner, 1.2f);
        g.setColour (juce::Colour (0xff6f7a90).withAlpha (0.9f));
        g.drawRoundedRectangle (b.reduced (1.2f), juce::jmax (1.0f, corner - 1.0f), 1.0f);

        // Dark bottom line for glass thickness.
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine (b.getX() + corner, b.getBottom() - 1.5f, b.getRight() - corner, b.getBottom() - 1.5f, 1.0f);
    }
}
