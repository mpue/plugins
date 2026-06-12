/*
  ==============================================================================

    OscillatorsPage.h
    Fixed-layout Oscillators tab: three large oscillator panels that fill the
    space, each with a live waveform display + its controls, and a routing/mixer
    panel below. (Master gain now lives on the Mix/EQ tab.)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeUI.h"
#include "GlassStyle.h"
#include "Visualisers.h"
#include "../params/ParameterIDs.h"

namespace pike::gui
{
    //==============================================================================
    class OscPanel : public juce::Component
    {
    public:
        OscPanel (juce::AudioProcessorValueTreeState& state, int n)
            : index (n)
        {
            wave = std::make_unique<WaveformDisplay> (state, pid::oscWave[n], pid::oscPW[n], pid::oscWtPos[n]);
            addAndMakeVisible (*wave);

            const std::vector<CtrlSpec> specs {
                { CtrlType::Combo, pid::oscWave[n],   "Wave" },
                { CtrlType::Knob,  pid::oscOctave[n], "Oct" },
                { CtrlType::Knob,  pid::oscSemi[n],   "Semi" },
                { CtrlType::Knob,  pid::oscFine[n],   "Fine" },
                { CtrlType::Knob,  pid::oscLevel[n],  "Level" },
                { CtrlType::Knob,  pid::oscPW[n],     "PW" },
                { CtrlType::Knob,  pid::oscWtPos[n],  "WT Pos" }
            };
            for (const auto& s : specs)
                addAndMakeVisible (controls.add (new Control (state, s)));
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.5f);
            fillGlassPanel (g, b, 6.0f);

            auto titleBar = b.removeFromTop (22.0f);
            g.setColour (theme::accent);
            g.fillRoundedRectangle (titleBar.getX() + 8.0f, titleBar.getY() + 6.0f, 3.0f, titleBar.getHeight() - 9.0f, 1.5f);
            g.setColour (juce::Colour (0xffe8f4ff));
            g.setFont (hudFont (12.0f));
            g.drawText ("OSC " + juce::String (index + 1), titleBar.withTrimmedLeft (16), juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            area.removeFromTop (18);                    // title

            wave->setBounds (area.removeFromTop (juce::jmin (90, area.getHeight() / 3)));
            area.removeFromTop (6);

            const int cols = 4;
            const int cellW = area.getWidth() / cols;
            const int cellH = juce::jmin (88, area.getHeight() / 2);
            for (int i = 0; i < controls.size(); ++i)
            {
                const int c = i % cols, r = i / cols;
                controls[i]->setBounds (area.getX() + c * cellW, area.getY() + r * cellH, cellW, cellH);
            }
        }

    private:
        int index;
        std::unique_ptr<WaveformDisplay> wave;
        juce::OwnedArray<Control>        controls;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscPanel)
    };

    //==============================================================================
    class OscillatorsPage : public juce::Component
    {
    public:
        explicit OscillatorsPage (juce::AudioProcessorValueTreeState& state)
        {
            for (int n = 0; n < 3; ++n)
            {
                osc[n] = std::make_unique<OscPanel> (state, n);
                addAndMakeVisible (*osc[n]);
            }

            routing = std::make_unique<Group> (state, GroupSpec { "Routing / Mixer", {
                { CtrlType::Toggle, pid::osc2Sync,     "Sync 2" },
                { CtrlType::Toggle, pid::osc3Sync,     "Sync 3" },
                { CtrlType::Knob,   pid::fmAmount,     "FM 3>1" },
                { CtrlType::Knob,   pid::ringModLevel, "Ring 1x2" },
                { CtrlType::Knob,   pid::noiseLevel,   "Noise" } } });
            addAndMakeVisible (*routing);
        }

        void paint (juce::Graphics& g) override
        {
            juce::ColourGradient bg (juce::Colour (0xff1e242e), 0.0f, 0.0f,
                                     juce::Colour (0xff0b0e13), 0.0f, (float) getHeight(), false);
            g.setGradientFill (bg);
            g.fillAll();
            paintPanelShadows (g, *this);
        }

        void resized() override
        {
            const int pad = 8;
            auto area = getLocalBounds().reduced (pad);

            auto bottom = area.removeFromBottom (juce::jmax (118, routing->preferredHeight()));
            routing->setBounds (bottom);
            area.removeFromBottom (pad);

            const int colW = (area.getWidth() - 2 * pad) / 3;
            for (int n = 0; n < 3; ++n)
                osc[n]->setBounds (area.getX() + n * (colW + pad), area.getY(), colW, area.getHeight());
        }

    private:
        std::unique_ptr<OscPanel> osc[3];
        std::unique_ptr<Group>    routing;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscillatorsPage)
    };
}
