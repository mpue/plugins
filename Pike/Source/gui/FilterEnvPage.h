/*
  ==============================================================================

    FilterEnvPage.h
    Bespoke "Filter / Env" page: the filter group plus three envelope blocks,
    each pairing an animated ADSR display with its ADSR knobs.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeUI.h"
#include "Visualisers.h"
#include "../params/ParameterIDs.h"

namespace pike::gui
{
    class FilterEnvPage : public ScrollPage
    {
    public:
        FilterEnvPage (juce::AudioProcessorValueTreeState& state, VisualState& vis)
        {
            using K = CtrlSpec;
            auto knob = [] (juce::String id, juce::String name) { return K { CtrlType::Knob, id, name }; };

            filterGroup = std::make_unique<Group> (state, GroupSpec { "Filter", {
                { CtrlType::Combo,  pid::filterType,      "Type" },
                { CtrlType::Combo,  pid::filterSlope,     "Slope" },
                knob (pid::filterCutoff,    "Cutoff"),
                knob (pid::filterResonance, "Reso"),
                knob (pid::filterKeyTrack,  "Key Trk"),
                knob (pid::filterDrive,     "Drive"),
                knob (pid::filterEnvAmount, "Env Amt") } });
            addAndMakeVisible (*filterGroup);

            filterResponse = std::make_unique<FilterResponse> (
                state, pid::filterType, pid::filterSlope, pid::filterCutoff, pid::filterResonance);
            addAndMakeVisible (*filterResponse);

            auto addEnv = [&] (juce::String title, juce::String a, juce::String d,
                               juce::String s, juce::String r)
            {
                auto& block = envBlocks.emplace_back();
                block.display = std::make_unique<EnvelopeDisplay> (state, vis, a, d, s, r, title);
                block.knobs   = std::make_unique<Group> (state, GroupSpec { title, {
                    knob (a, "A"), knob (d, "D"), knob (s, "S"), knob (r, "R") } });
                addAndMakeVisible (*block.display);
                addAndMakeVisible (*block.knobs);
            };

            addEnv ("Amp Envelope",    pid::ampAttack,  pid::ampDecay,  pid::ampSustain,  pid::ampRelease);
            addEnv ("Filter Envelope", pid::filtAttack, pid::filtDecay, pid::filtSustain, pid::filtRelease);
            addEnv ("Aux Envelope",    pid::auxAttack,  pid::auxDecay,  pid::auxSustain,  pid::auxRelease);
        }

        int layoutForWidth (int width) override
        {
            const int pad = 8;
            int y = pad;

            const int fgW = filterGroup->preferredWidth();
            const int fgH = filterGroup->preferredHeight();
            filterGroup->setBounds (pad, y, fgW, fgH);
            filterResponse->setBounds (pad + fgW + pad, y,
                                       juce::jmax (200, width - fgW - 3 * pad), fgH);
            y += fgH + pad;

            for (auto& b : envBlocks)
            {
                const int groupW = b.knobs->preferredWidth();
                const int groupH = b.knobs->preferredHeight();
                const int blockH = juce::jmax (groupH, 116);
                const int dispW  = juce::jmax (180, width - groupW - 3 * pad);

                b.display->setBounds (pad, y, dispW, blockH);
                b.knobs->setBounds (pad + dispW + pad, y, groupW, groupH);
                y += blockH + pad;
            }

            return y + pad;
        }

        void paint (juce::Graphics& g) override
        {
            juce::ColourGradient bg (juce::Colour (0xff2a313c), 0.0f, 0.0f,
                                     juce::Colour (0xff141820), 0.0f, (float) getHeight(), false);
            g.setGradientFill (bg);
            g.fillAll();

            paintPanelShadows (g, *this);
        }

        void resized() override { layoutForWidth (getWidth()); }

    private:
        struct EnvBlock
        {
            std::unique_ptr<EnvelopeDisplay> display;
            std::unique_ptr<Group>           knobs;
        };

        std::unique_ptr<Group>          filterGroup;
        std::unique_ptr<FilterResponse> filterResponse;
        std::vector<EnvBlock>           envBlocks;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterEnvPage)
    };
}
