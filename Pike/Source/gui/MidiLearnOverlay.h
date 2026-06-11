/*
  ==============================================================================

    MidiLearnOverlay.h
    A transparent, mouse-through overlay drawn on top of the whole editor. It
    polls the MidiLearnManager and draws, over each visible parameter widget:
      - a blinking accent outline while that parameter is the learn target,
      - a small "CC n" badge once a CC is mapped to it.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../midi/MidiLearnManager.h"

namespace pike::gui
{
    class MidiLearnOverlay : public juce::Component, private juce::Timer
    {
    public:
        MidiLearnOverlay (MidiLearnManager& m, juce::AudioProcessorValueTreeState& s,
                          const std::vector<juce::Component::SafePointer<juce::Component>>& w)
            : mlm (m), state (s), widgets (w)
        {
            setInterceptsMouseClicks (false, false);
            startTimerHz (20);
        }

        void paint (juce::Graphics& g) override
        {
            const juce::Colour accent { 0xff4d9eff };
            const float blink = 0.35f + 0.35f * std::sin (phase);

            for (auto& wp : widgets)
            {
                auto* w = wp.getComponent();
                if (w == nullptr || ! w->isShowing())
                    continue;

                const auto pid = w->getProperties().getWithDefault ("pikePid", {}).toString();
                auto* param = state.getParameter (pid);
                if (param == nullptr)
                    continue;

                const int  idx   = param->getParameterIndex();
                const int  cc    = mlm.ccForParam (idx);
                const bool armed = mlm.armedParam() == idx;
                if (! armed && cc < 0)
                    continue;

                auto b = getLocalArea (w, w->getLocalBounds()).toFloat();

                if (armed)
                {
                    g.setColour (accent.withAlpha (blink));
                    g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f);
                }

                if (cc >= 0)
                {
                    auto badge = juce::Rectangle<float> (0.0f, 0.0f, 24.0f, 12.0f)
                                     .withRightX (b.getRight()).withY (b.getY() - 4.0f);
                    g.setColour (accent);
                    g.fillRoundedRectangle (badge, 3.0f);
                    g.setColour (juce::Colours::white);
                    g.setFont (juce::Font (8.5f, juce::Font::bold));
                    g.drawText ("CC" + juce::String (cc), badge, juce::Justification::centred, false);
                }
            }
        }

    private:
        void timerCallback() override
        {
            phase += 0.32f;
            if (phase > juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
            repaint();
        }

        MidiLearnManager&                   mlm;
        juce::AudioProcessorValueTreeState& state;
        const std::vector<juce::Component::SafePointer<juce::Component>>& widgets;
        float phase = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnOverlay)
    };
}
