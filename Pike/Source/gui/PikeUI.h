/*
  ==============================================================================

    PikeUI.h
    Small data-driven control framework for the editor:
      - Control : one APVTS-bound widget (rotary knob, combo or LED toggle) + label
      - Group   : a titled box arranging its controls in a grid
      - Page    : flows groups left-to-right (wrapping), hosted in a Viewport

    The editor describes each section as data (group title + control specs) and
    these classes build and lay everything out.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GlassStyle.h"

namespace pike::gui
{
    enum class CtrlType { Knob, Combo, Toggle };

    struct CtrlSpec
    {
        CtrlType     type;
        juce::String id;
        juce::String name;
    };

    struct GroupSpec
    {
        juce::String          title;
        std::vector<CtrlSpec> controls;
    };

    namespace layout
    {
        constexpr int cellW   = 100;
        constexpr int cellH   = 84;
        constexpr int titleH  = 22;
        constexpr int pad     = 8;
        constexpr int maxCols = 4;
    }

    /** Draws a soft drop shadow behind each visible child panel. Call from the
        parent's paint() (before children paint) so the shadow shows in the gaps. */
    inline void paintPanelShadows (juce::Graphics& g, juce::Component& parent)
    {
        for (auto* c : parent.getChildren())
        {
            if (c == nullptr || ! c->isVisible())
                continue;

            juce::DropShadow ds (juce::Colours::black.withAlpha (0.55f), 12, { 0, 5 });
            juce::Path p;
            p.addRoundedRectangle (c->getBounds().toFloat().reduced (2.5f), 7.0f);
            ds.drawForPath (g, p);
        }
    }

    //==============================================================================
    class Control : public juce::Component
    {
    public:
        Control (juce::AudioProcessorValueTreeState& state, const CtrlSpec& spec)
            : type (spec.type)
        {
            using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
            using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
            using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

            if (type == CtrlType::Knob)
            {
                slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
                addAndMakeVisible (slider);

                label.setText (spec.name, juce::dontSendNotification);
                label.setJustificationType (juce::Justification::centred);
                label.setInterceptsMouseClicks (false, false);
                addAndMakeVisible (label);

                sliderAtt = std::make_unique<SA> (state, spec.id, slider);
            }
            else if (type == CtrlType::Combo)
            {
                if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (spec.id)))
                    combo.addItemList (cp->choices, 1);
                combo.setJustificationType (juce::Justification::centred);
                addAndMakeVisible (combo);

                label.setText (spec.name, juce::dontSendNotification);
                label.setJustificationType (juce::Justification::centred);
                label.setInterceptsMouseClicks (false, false);
                addAndMakeVisible (label);

                comboAtt = std::make_unique<CA> (state, spec.id, combo);
            }
            else // Toggle
            {
                toggle.setButtonText (spec.name);
                addAndMakeVisible (toggle);
                buttonAtt = std::make_unique<BA> (state, spec.id, toggle);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 3);

            if (type == CtrlType::Knob)
            {
                label.setBounds (r.removeFromTop (15));
                slider.setBounds (r);
            }
            else if (type == CtrlType::Combo)
            {
                label.setBounds (r.removeFromTop (15));
                combo.setBounds (r.removeFromTop (26));
            }
            else
            {
                toggle.setBounds (r.withSizeKeepingCentre (r.getWidth(), 26));
            }
        }

    private:
        CtrlType        type;
        juce::Slider    slider;
        juce::ComboBox  combo;
        juce::ToggleButton toggle;
        juce::Label     label;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   sliderAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   buttonAtt;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Control)
    };

    //==============================================================================
    class Group : public juce::Component
    {
    public:
        Group (juce::AudioProcessorValueTreeState& state, const GroupSpec& spec)
            : title (spec.title)
        {
            for (const auto& c : spec.controls)
            {
                auto* ctrl = controls.add (new Control (state, c));
                addAndMakeVisible (ctrl);
            }
        }

        int columns() const { return juce::jlimit (1, layout::maxCols, controls.size()); }
        int rows()    const { return (controls.size() + columns() - 1) / juce::jmax (1, columns()); }

        int preferredWidth()  const { return columns() * layout::cellW + layout::pad; }
        int preferredHeight() const { return layout::titleH + rows() * layout::cellH + layout::pad; }

        void paint (juce::Graphics& g) override
        {
            const juce::Colour accent { 0xff4d9eff };
            auto b = getLocalBounds().toFloat().reduced (2.5f);

            // Glossy glass panel (the parent draws the drop shadow behind us).
            fillGlassPanel (g, b, 8.0f);

            // Title with accent tab + underline.
            auto titleBar = b.removeFromTop ((float) layout::titleH);
            g.setColour (accent);
            g.fillRoundedRectangle (titleBar.getX() + 8.0f, titleBar.getY() + 6.0f,
                                    3.0f, titleBar.getHeight() - 9.0f, 1.5f);
            g.setFont (juce::Font (13.0f, juce::Font::bold));
            g.drawText (title, titleBar.withTrimmedLeft (16).withTrimmedRight (8),
                        juce::Justification::centredLeft, false);
            g.setColour (accent.withAlpha (0.22f));
            g.drawLine (b.getX() + 10.0f, titleBar.getBottom(), b.getRight() - 10.0f, titleBar.getBottom(), 1.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            area.removeFromTop (layout::titleH);
            const int cols = columns();

            for (int i = 0; i < controls.size(); ++i)
            {
                const int col = i % cols;
                const int row = i / cols;
                controls[i]->setBounds (layout::pad / 2 + col * layout::cellW,
                                        layout::titleH + row * layout::cellH,
                                        layout::cellW, layout::cellH);
            }
        }

    private:
        juce::String title;
        juce::OwnedArray<Control> controls;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Group)
    };

    //==============================================================================
    /** Base for components hosted in a PageViewport: reports content height for
        a given width so the viewport can size and scroll them. */
    struct ScrollPage : public juce::Component
    {
        virtual int layoutForWidth (int width) = 0;
    };

    //==============================================================================
    class Page : public ScrollPage
    {
    public:
        Page (juce::AudioProcessorValueTreeState& state, const std::vector<GroupSpec>& specs)
        {
            for (const auto& gs : specs)
            {
                auto* g = groups.add (new Group (state, gs));
                addAndMakeVisible (g);
            }
        }

        /** Flows groups within the given width; returns the total content height. */
        int layoutForWidth (int width) override
        {
            const int pad = layout::pad;
            int x = pad, y = pad, rowHeight = 0;

            for (auto* g : groups)
            {
                const int gw = g->preferredWidth();
                const int gh = g->preferredHeight();

                if (x > pad && x + gw + pad > width)   // wrap
                {
                    x = pad;
                    y += rowHeight + pad;
                    rowHeight = 0;
                }

                g->setBounds (x, y, gw, gh);
                x += gw + pad;
                rowHeight = juce::jmax (rowHeight, gh);
            }

            return y + rowHeight + pad;
        }

        void paint (juce::Graphics& g) override
        {
            juce::ColourGradient bg (juce::Colour (0xff2a313c), 0.0f, 0.0f,
                                     juce::Colour (0xff141820), 0.0f, (float) getHeight(), false);
            g.setGradientFill (bg);
            g.fillAll();

            paintPanelShadows (g, *this);
        }

        void resized() override
        {
            layoutForWidth (getWidth());
        }

    private:
        juce::OwnedArray<Group> groups;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Page)
    };

    //==============================================================================
    /** A Viewport that sizes its Page to the visible width and content height. */
    class PageViewport : public juce::Viewport
    {
    public:
        explicit PageViewport (ScrollPage* pageToOwn)
        {
            setViewedComponent (pageToOwn, true);   // takes ownership
            setScrollBarsShown (true, false);
        }

        void resized() override
        {
            juce::Viewport::resized();

            if (auto* p = dynamic_cast<ScrollPage*> (getViewedComponent()))
            {
                const int w = getMaximumVisibleWidth();
                const int h = p->layoutForWidth (w);
                p->setSize (w, juce::jmax (h, getHeight()));
            }
        }
    };
}
