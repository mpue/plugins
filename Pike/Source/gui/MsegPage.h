/*
  ==============================================================================

    MsegPage.h
    Editor tab for the multi-segment envelopes: a selector strip (one button
    per existing MSEG plus [+] to add more, up to mseg::maxMsegs), the MsegEditor
    canvas for the selected MSEG and its time-base parameter group.

    All four parameter groups are constructed up front and merely toggled
    visible — the editor's MIDI-Learn walk runs once at construction and would
    miss lazily-created controls.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeUI.h"
#include "MsegEditor.h"
#include "VisualState.h"
#include "../params/MsegStore.h"
#include "../params/ParameterIDs.h"

namespace pike::gui
{
    class MsegPage : public ScrollPage,
                     private juce::ValueTree::Listener
    {
    public:
        MsegPage (juce::AudioProcessorValueTreeState& state, MsegStore& msegStore, VisualState& vs)
            : apvts (state), store (msegStore), visualState (vs)
        {
            for (int n = 0; n < mseg::maxMsegs; ++n)
            {
                auto* b = selectButtons.add (new juce::TextButton ("MSEG " + juce::String (n + 1)));
                b->setClickingTogglesState (false);
                b->onClick = [this, n] { select (n); };
                addChildComponent (b);

                GroupSpec spec { "MSEG " + juce::String (n + 1) + " Time",
                                 { { CtrlType::Toggle, pid::msegSync[n], "Sync" },
                                   { CtrlType::Knob,   pid::msegRate[n], "Time" },
                                   { CtrlType::Combo,  pid::msegDiv[n],  "Length" },
                                   { CtrlType::Toggle, pid::msegLoop[n], "Loop" } } };
                auto* g = paramGroups.add (new Group (state, spec));
                addChildComponent (g);
            }

            addButton.onClick = [this]
            {
                if (store.addMseg().isValid())
                    select (store.activeCount() - 1);
            };
            addChildComponent (addButton);

            addAndMakeVisible (editor);

            apvts.state.addListener (this);
            refresh();
        }

        ~MsegPage() override { apvts.state.removeListener (this); }

        //======================================================================
        int layoutForWidth (int width) override
        {
            const int pad = layout::pad;
            const int stripH = 26;

            // Selector strip.
            int x = pad;
            for (int n = 0; n < selectButtons.size(); ++n)
            {
                if (selectButtons[n]->isVisible())
                {
                    selectButtons[n]->setBounds (x, pad, 84, stripH);
                    x += 84 + 4;
                }
            }
            if (addButton.isVisible())
                addButton.setBounds (x, pad, stripH, stripH);

            // Canvas left, parameter group right.
            const int top = pad + stripH + pad;
            const int groupW = paramGroups.isEmpty() ? 0 : paramGroups[0]->preferredWidth();
            const int groupH = paramGroups.isEmpty() ? 0 : paramGroups[0]->preferredHeight();
            const int canvasW = juce::jmax (300, width - groupW - 3 * pad);
            const int canvasH = 340;

            editor.setBounds (pad, top, canvasW, canvasH);
            for (auto* g : paramGroups)
                g->setBounds (pad * 2 + canvasW, top, groupW, groupH);

            return top + canvasH + pad;
        }

        void paint (juce::Graphics& g) override
        {
            juce::ColourGradient bg (juce::Colour (0xff1e242e), 0.0f, 0.0f,
                                     juce::Colour (0xff0b0e13), 0.0f, (float) getHeight(), false);
            g.setGradientFill (bg);
            g.fillAll();
            drawHoloGrid (g, getLocalBounds().toFloat());
            paintPanelShadows (g, *this);
        }

        void resized() override { layoutForWidth (getWidth()); }

    private:
        //======================================================================
        void select (int index)
        {
            selected = juce::jlimit (0, juce::jmax (0, store.activeCount() - 1), index);

            for (int n = 0; n < selectButtons.size(); ++n)
            {
                selectButtons[n]->setToggleState (n == selected, juce::dontSendNotification);
                selectButtons[n]->setColour (juce::TextButton::buttonColourId,
                                             n == selected ? theme::accent.withAlpha (0.35f)
                                                           : juce::Colour (0xff1a212b));
            }
            for (int n = 0; n < paramGroups.size(); ++n)
                paramGroups[n]->setVisible (n == selected);

            editor.setTree (store.getMsegTree (selected));
            editor.setPlayheadSource (&visualState, selected);
            repaint();
        }

        /** Syncs the strip and the canvas binding with the store's contents. */
        void refresh()
        {
            const int count = store.activeCount();
            for (int n = 0; n < selectButtons.size(); ++n)
                selectButtons[n]->setVisible (n < count);
            addButton.setVisible (count < mseg::maxMsegs);

            select (selected);
            layoutForWidth (getWidth());
        }

        void refreshAsync()
        {
            juce::Component::SafePointer<MsegPage> self (this);
            juce::MessageManager::callAsync ([self]
            {
                if (self != nullptr)
                    self->refresh();
            });
        }

        //======================================================================
        // ValueTree::Listener on apvts.state: track MSEG add/remove and state
        // reloads (replaceState fires valueTreeRedirected). Edits can arrive
        // off the message thread (setStateInformation), hence callAsync.
        void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override
        {
            if (parent.hasType (MsegStore::msegsType))
                refreshAsync();
        }

        void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override
        {
            if (parent.hasType (MsegStore::msegsType))
                refreshAsync();
        }

        void valueTreeRedirected (juce::ValueTree&) override { refreshAsync(); }

        juce::AudioProcessorValueTreeState& apvts;
        MsegStore& store;
        VisualState& visualState;

        juce::OwnedArray<juce::TextButton> selectButtons;
        juce::TextButton addButton { "+" };
        juce::OwnedArray<Group> paramGroups;
        MsegEditor editor;
        int selected = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MsegPage)
    };
}
