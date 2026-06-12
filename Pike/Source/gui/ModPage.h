/*
  ==============================================================================

    ModPage.h
    Fixed-layout Mod tab: two LFO groups on top and a compact 16-slot modulation
    matrix below (2 columns x 8 rows, one tight row per slot) so nothing scrolls.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeUI.h"
#include "GlassStyle.h"
#include "../params/ParameterIDs.h"
#include "../params/ModMatrixDefs.h"

namespace pike::gui
{
    //==============================================================================
    class ModMatrixPanel : public juce::Component
    {
    public:
        explicit ModMatrixPanel (juce::AudioProcessorValueTreeState& state)
        {
            const auto sources = mod::sourceNames();
            const auto dests   = mod::destNames();

            for (int s = 0; s < mod::numSlots; ++s)
            {
                auto& row = rows[(size_t) s];

                row.src = std::make_unique<juce::ComboBox>();
                row.src->addItemList (sources, 1);
                row.src->setJustificationType (juce::Justification::centredLeft);
                row.src->getProperties().set ("pikePid", pid::modSourceId (s));
                addAndMakeVisible (*row.src);
                row.srcAtt = std::make_unique<ComboAtt> (state, pid::modSourceId (s), *row.src);

                row.dst = std::make_unique<juce::ComboBox>();
                row.dst->addItemList (dests, 1);
                row.dst->setJustificationType (juce::Justification::centredLeft);
                row.dst->getProperties().set ("pikePid", pid::modDestId (s));
                addAndMakeVisible (*row.dst);
                row.dstAtt = std::make_unique<ComboAtt> (state, pid::modDestId (s), *row.dst);

                row.depth = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                            juce::Slider::NoTextBox);
                row.depth->setPopupDisplayEnabled (true, true, this);
                row.depth->getProperties().set ("pikePid", pid::modDepthId (s));
                addAndMakeVisible (*row.depth);
                row.depthAtt = std::make_unique<SliderAtt> (state, pid::modDepthId (s), *row.depth);
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.5f);
            fillGlassPanel (g, b, 5.0f);

            g.setColour (theme::accent);
            g.setFont (hudFont (12.0f));
            g.drawText ("MODULATION MATRIX", b.reduced (12, 5).removeFromTop (15),
                        juce::Justification::topLeft, false);

            // Column headers aligned to the control columns.
            g.setColour (juce::Colour (0xff8a93a6));
            g.setFont (juce::Font (9.5f, juce::Font::bold));
            for (int c = 0; c < 2; ++c)
            {
                const auto src   = cellBounds (c, 0, Cell::src);
                const auto dst   = cellBounds (c, 0, Cell::dst);
                const auto depth = cellBounds (c, 0, Cell::depth);
                const int  hy    = headerTop();
                g.drawText ("SOURCE",      src.getX(),   hy, src.getWidth(),   11, juce::Justification::bottomLeft, false);
                g.drawText ("DESTINATION", dst.getX(),   hy, dst.getWidth(),   11, juce::Justification::bottomLeft, false);
                g.drawText ("DPT",         depth.getX(), hy, depth.getWidth(), 11, juce::Justification::bottomLeft, false);
            }
        }

        void resized() override
        {
            for (int s = 0; s < mod::numSlots; ++s)
            {
                const int c = s / rowsPerCol;
                const int r = s % rowsPerCol;
                auto& row = rows[(size_t) s];
                row.src->setBounds   (cellBounds (c, r, Cell::src));
                row.dst->setBounds   (cellBounds (c, r, Cell::dst));
                row.depth->setBounds (cellBounds (c, r, Cell::depth));
            }
        }

    private:
        using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

        static constexpr int rowsPerCol = 8;
        static constexpr int rowH    = 26;
        static constexpr int gap     = 6;
        static constexpr int depthW  = 22;
        static constexpr int colGap  = 16;
        static constexpr int marginX = 12;

        enum class Cell { src, dst, depth };

        int headerTop() const { return 24; }
        int rowsTop()   const { return 38; }

        int columnWidth() const
        {
            return (getWidth() - 2 * marginX - colGap) / 2;
        }

        juce::Rectangle<int> cellBounds (int c, int r, Cell which) const
        {
            const int colW = columnWidth();
            const int colX = marginX + c * (colW + colGap);
            const int y    = rowsTop() + r * rowH;

            const int comboW = (colW - depthW - 3 * gap) / 2;
            const int srcX   = colX;
            const int dstX   = srcX + comboW + gap;
            const int depthX = dstX + comboW + gap;

            const juce::Rectangle<int> rowR (colX, y + 2, colW, rowH - 4);

            switch (which)
            {
                case Cell::src:   return { srcX, rowR.getY(), comboW, rowR.getHeight() };
                case Cell::dst:   return { dstX, rowR.getY(), comboW, rowR.getHeight() };
                case Cell::depth: return juce::Rectangle<int> (depthX, y + 1, depthW, rowH - 2);
            }
            return {};
        }

        struct Row
        {
            std::unique_ptr<juce::ComboBox> src, dst;
            std::unique_ptr<juce::Slider>   depth;
            std::unique_ptr<ComboAtt>       srcAtt, dstAtt;
            std::unique_ptr<SliderAtt>      depthAtt;
        };
        std::array<Row, mod::numSlots> rows;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModMatrixPanel)
    };

    //==============================================================================
    class ModPage : public juce::Component
    {
    public:
        ModPage (juce::AudioProcessorValueTreeState& state)
        {
            for (int n = 0; n < 2; ++n)
            {
                std::vector<CtrlSpec> ctrls {
                    { CtrlType::Combo,  pid::lfoShape[n],   "Shape" },
                    { CtrlType::Toggle, pid::lfoSync[n],    "Sync" },
                    { CtrlType::Knob,   pid::lfoRate[n],    "Rate" },
                    { CtrlType::Combo,  pid::lfoDiv[n],     "Div" },
                    { CtrlType::Toggle, pid::lfoKeySync[n], "Key Sync" },
                    { CtrlType::Toggle, pid::lfoMono[n],    "Mono" },
                    { CtrlType::Knob,   pid::lfoFade[n],    "Fade" }
                };
                lfo[n] = std::make_unique<Group> (state, GroupSpec { "LFO " + juce::String (n + 1), ctrls });
                addAndMakeVisible (*lfo[n]);
            }

            matrix = std::make_unique<ModMatrixPanel> (state);
            addAndMakeVisible (*matrix);
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

            const int lfoH = lfo[0]->preferredHeight();
            const int lfoW = (area.getWidth() - pad) / 2;
            lfo[0]->setBounds (area.getX(),             area.getY(), lfoW, lfoH);
            lfo[1]->setBounds (area.getX() + lfoW + pad, area.getY(), lfoW, lfoH);

            auto matrixArea = area.withTrimmedTop (lfoH + pad);
            matrix->setBounds (matrixArea);
        }

    private:
        std::unique_ptr<Group>          lfo[2];
        std::unique_ptr<ModMatrixPanel> matrix;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModPage)
    };
}
