/*
  ==============================================================================

    SequencerComponents.h
    SA-1 Luxury Quick Sampler — step sequencer UI.

    StepSequencerPanel: a control bar (mode toggle, sync / hold mode, rate,
    length, swing, clear, randomize) above a 16-track x N-step grid. Cells are
    click-painted; vertical drag (or shift / right drag) sets per-step velocity.
    A glowing playhead column follows the running sequence.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "StepSequencer.h"

namespace SA1
{
    //==============================================================================
    /** The grid of step cells (one row per pad). */
    class StepGrid : public juce::Component
    {
    public:
        explicit StepGrid (SA1AudioProcessor& proc) : processor (proc) {}

        std::function<void()> onEdited;

        void refreshPlayhead()
        {
            const int step    = processor.getSequencer().getCurrentStep();
            const bool running = processor.getSequencer().isRunning();
            if (step != lastStep || running != lastRunning)
            {
                lastStep    = step;
                lastRunning = running;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto& seq = processor.getSequencer();
            const int nSteps = seq.getNumSteps();

            auto area = getLocalBounds().toFloat();
            drawLuxuryPanel (g, area, 10.0f,
                              juce::Colour (0xff141a24),
                              juce::Colour (0xff080c12),
                              juce::Colour (0xff262e3a));

            const auto cells = cellsArea();
            const int  curStep = (lastRunning && lastStep >= 0) ? lastStep : -1;

            // Beat-column shading + playhead column
            const float colW = cells.getWidth() / (float) juce::jmax (1, nSteps);
            for (int s = 0; s < nSteps; ++s)
            {
                auto col = juce::Rectangle<float> (cells.getX() + s * colW, cells.getY(),
                                                   colW, cells.getHeight());
                if ((s / 4) % 2 == 1)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.03f));
                    g.fillRect (col);
                }
                if (s == curStep)
                {
                    g.setColour (juce::Colour (0xffffd29c).withAlpha (0.16f));
                    g.fillRect (col);
                }
            }

            const float rowH = cells.getHeight() / (float) kNumPads;

            for (int p = 0; p < kNumPads; ++p)
            {
                const auto& slot   = getDefaultSlot (p);
                const auto  accent = juce::Colour (slot.colour);
                const bool  loaded = processor.getEngine().getPad (p).hasSample();

                const float rowY = cells.getY() + p * rowH;

                // Row label
                auto label = juce::Rectangle<float> (area.getX() + 6.0f, rowY,
                                                     labelWidth - 8.0f, rowH).reduced (0.0f, 1.0f);
                g.setColour (accent.withAlpha (loaded ? 0.9f : 0.4f));
                g.setFont (juce::Font (juce::FontOptions (juce::jmin (12.0f, rowH - 4.0f)).withStyle ("Bold")));
                g.drawText (slot.shortName, label.toNearestInt(), juce::Justification::centredLeft);

                // Cells
                for (int s = 0; s < nSteps; ++s)
                {
                    auto cell = cellRect (p, s).reduced (1.5f);
                    const bool on  = seq.getStep (p, s);
                    const float vel = seq.getStepVel (p, s);

                    if (on)
                    {
                        const float a = 0.35f + 0.65f * vel;
                        g.setColour (accent.withAlpha (loaded ? a : a * 0.5f));
                        g.fillRoundedRectangle (cell, 3.0f);
                        g.setColour (accent.brighter (0.4f).withAlpha (0.9f));
                        g.drawRoundedRectangle (cell, 3.0f, 1.0f);

                        // velocity tick at top
                        g.setColour (juce::Colours::white.withAlpha (0.25f));
                        const float vh = cell.getHeight() * (1.0f - vel);
                        g.fillRect (cell.getX(), cell.getY() + vh, cell.getWidth(), 1.0f);
                    }
                    else
                    {
                        g.setColour (juce::Colour (0xff1c2330));
                        g.fillRoundedRectangle (cell, 3.0f);
                        g.setColour (juce::Colour (0xff2c3543));
                        g.drawRoundedRectangle (cell, 3.0f, 1.0f);
                    }
                }
            }

            // Playhead head marker line
            if (curStep >= 0 && curStep < nSteps)
            {
                auto col = juce::Rectangle<float> (cells.getX() + curStep * colW, cells.getY(),
                                                   colW, cells.getHeight());
                g.setColour (juce::Colour (0xffffd29c).withAlpha (0.55f));
                g.drawRect (col, 1.5f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            int pad, step;
            if (! hitTest (e.position, pad, step)) return;

            auto& seq = processor.getSequencer();

            if (e.mods.isRightButtonDown() || e.mods.isShiftDown())
            {
                // Velocity edit on this cell
                velPad = pad; velStep = step;
                if (! seq.getStep (pad, step)) seq.setStep (pad, step, true);
            }
            else
            {
                velPad = -1;
                seq.toggleStep (pad, step);
                paintState = seq.getStep (pad, step);   // paint subsequent cells to match
            }
            repaint();
            if (onEdited) onEdited();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            auto& seq = processor.getSequencer();

            if (velPad >= 0)
            {
                seq.setStepVel (velPad, velStep, velFromDrag (e));
                repaint();
                if (onEdited) onEdited();
                return;
            }

            int pad, step;
            if (! hitTest (e.position, pad, step)) return;
            if (seq.getStep (pad, step) != paintState)
            {
                seq.setStep (pad, step, paintState);
                repaint();
                if (onEdited) onEdited();
            }
        }

    private:
        float velFromDrag (const juce::MouseEvent& e)
        {
            // Map vertical position within the anchored cell's row to velocity.
            auto cell = cellRect (velPad, velStep);
            const float t = juce::jlimit (0.0f, 1.0f,
                                          1.0f - (e.position.y - cell.getY()) / juce::jmax (1.0f, cell.getHeight()));
            // Combine with drag for finer control if dragging outside the cell.
            const float byDrag = juce::jlimit (0.05f, 1.0f,
                                               0.5f - (float) e.getDistanceFromDragStartY() * 0.004f);
            return cell.contains (e.position) ? juce::jmax (0.05f, t) : byDrag;
        }

        juce::Rectangle<float> cellsArea() const
        {
            return getLocalBounds().toFloat().reduced (6.0f)
                     .withTrimmedLeft (labelWidth);
        }

        juce::Rectangle<float> cellRect (int pad, int step) const
        {
            const int nSteps = juce::jmax (1, processor.getSequencer().getNumSteps());
            auto cells = cellsArea();
            const float colW = cells.getWidth()  / (float) nSteps;
            const float rowH = cells.getHeight() / (float) kNumPads;
            return { cells.getX() + step * colW, cells.getY() + pad * rowH, colW, rowH };
        }

        bool hitTest (juce::Point<float> pos, int& pad, int& step) const
        {
            const int nSteps = processor.getSequencer().getNumSteps();
            auto cells = cellsArea();
            if (! cells.contains (pos)) return false;

            const float colW = cells.getWidth()  / (float) juce::jmax (1, nSteps);
            const float rowH = cells.getHeight() / (float) kNumPads;

            step = (int) ((pos.x - cells.getX()) / colW);
            pad  = (int) ((pos.y - cells.getY()) / rowH);
            return juce::isPositiveAndBelow (pad, kNumPads)
                && juce::isPositiveAndBelow (step, nSteps);
        }

        SA1AudioProcessor& processor;
        static constexpr float labelWidth = 64.0f;

        int  lastStep    = -1;
        bool lastRunning = false;

        bool paintState  = true;
        int  velPad = -1, velStep = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGrid)
    };

    //==============================================================================
    class StepSequencerPanel : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit StepSequencerPanel (SA1AudioProcessor& proc)
            : processor (proc), grid (proc)
        {
            auto& seq = processor.getSequencer();

            // --- Mode toggle ---
            enableBtn.setButtonText ("SEQ MODE");
            enableBtn.setClickingTogglesState (true);
            enableBtn.setToggleState (seq.isEnabled(), juce::dontSendNotification);
            enableBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff9a4a));
            enableBtn.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff10141d));
            enableBtn.onClick = [this]
            {
                processor.getSequencer().setEnabled (enableBtn.getToggleState());
                repaint();
            };
            addAndMakeVisible (enableBtn);

            // --- Sync mode ---
            syncBox.addItem ("Host Sync",   1);
            syncBox.addItem ("Note Trigger",2);
            syncBox.setSelectedId ((int) seq.getSyncMode() + 1, juce::dontSendNotification);
            syncBox.onChange = [this]
            { processor.getSequencer().setSyncMode ((SeqSync) (syncBox.getSelectedId() - 1)); };
            addAndMakeVisible (syncBox);

            // --- Hold mode ---
            holdBox.addItem ("Gate",  1);
            holdBox.addItem ("Latch", 2);
            holdBox.setSelectedId ((int) seq.getHoldMode() + 1, juce::dontSendNotification);
            holdBox.onChange = [this]
            { processor.getSequencer().setHoldMode ((SeqHold) (holdBox.getSelectedId() - 1)); };
            addAndMakeVisible (holdBox);

            // --- Rate ---
            for (int i = 0; i < numSeqRates(); ++i)
                rateBox.addItem (seqRateOption (i).name, i + 1);
            rateBox.setSelectedId (seq.getRateIndex() + 1, juce::dontSendNotification);
            rateBox.onChange = [this]
            { processor.getSequencer().setRateIndex (rateBox.getSelectedId() - 1); };
            addAndMakeVisible (rateBox);

            // --- Length ---
            length.setSliderStyle (juce::Slider::IncDecButtons);
            length.setRange (1, kSeqMaxSteps, 1);
            length.setValue (seq.getNumSteps(), juce::dontSendNotification);
            length.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
            length.textFromValueFunction = [] (double v) { return juce::String ((int) v) + " st"; };
            length.onValueChange = [this]
            {
                processor.getSequencer().setNumSteps ((int) length.getValue());
                grid.repaint();
            };
            addAndMakeVisible (length);

            // --- Swing ---
            swing.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            swing.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                       juce::MathConstants<float>::pi * 2.75f, true);
            swing.setRange (0.0, 70.0, 1.0);
            swing.setValue (seq.getSwing() * 100.0, juce::dontSendNotification);
            swing.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
            swing.textFromValueFunction = [] (double v) { return juce::String ((int) v) + " %"; };
            swing.onValueChange = [this]
            { processor.getSequencer().setSwing ((float) swing.getValue() * 0.01f); };
            addAndMakeVisible (swing);

            // --- Buttons ---
            clearBtn .setButtonText ("Clear");
            randomBtn.setButtonText ("Random");
            clearBtn .onClick = [this] { processor.getSequencer().clearAll(); grid.repaint(); };
            randomBtn.onClick = [this]
            {
                juce::Random rng;
                processor.getSequencer().randomize (rng);
                grid.repaint();
            };
            addAndMakeVisible (clearBtn);
            addAndMakeVisible (randomBtn);

            addAndMakeVisible (grid);

            startTimerHz (30);
        }

        ~StepSequencerPanel() override { stopTimer(); }

        /** Re-sync the controls after a preset load. */
        void refreshFromSequencer()
        {
            auto& seq = processor.getSequencer();
            enableBtn.setToggleState (seq.isEnabled(), juce::dontSendNotification);
            syncBox.setSelectedId ((int) seq.getSyncMode() + 1, juce::dontSendNotification);
            holdBox.setSelectedId ((int) seq.getHoldMode() + 1, juce::dontSendNotification);
            rateBox.setSelectedId (seq.getRateIndex() + 1, juce::dontSendNotification);
            length.setValue (seq.getNumSteps(), juce::dontSendNotification);
            swing.setValue (seq.getSwing() * 100.0, juce::dontSendNotification);
            grid.repaint();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (1.0f);
            drawLuxuryPanel (g, bounds, 12.0f,
                              juce::Colour (0xff1a212c),
                              juce::Colour (0xff0d1118),
                              juce::Colour (0xff262e3a));

            g.setColour (juce::Colour (0xffe2c8a8));
            g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
            g.drawText ("STEP SEQUENCER",
                        bounds.toNearestInt().reduced (14, 8).removeFromTop (16).removeFromLeft (160),
                        juce::Justification::centredLeft);

            const bool on = processor.getSequencer().isEnabled();
            g.setColour (on ? juce::Colour (0xffff9a4a) : juce::Colour (0xff556581));
            g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
            g.drawText (on ? "MODE ACTIVE \xc2\xb7 notes trigger the sequence"
                           : "MODE OFF \xc2\xb7 notes play pads directly",
                        bounds.toNearestInt().reduced (180, 8).removeFromTop (16),
                        juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 8);

            auto controls = r.removeFromTop (32);
            controls.removeFromTop (4);
            r.removeFromTop (6);

            controls.removeFromLeft (4);
            enableBtn.setBounds (controls.removeFromLeft (96));
            controls.removeFromLeft (10);
            syncBox  .setBounds (controls.removeFromLeft (120));
            controls.removeFromLeft (6);
            holdBox  .setBounds (controls.removeFromLeft (84));
            controls.removeFromLeft (6);
            rateBox  .setBounds (controls.removeFromLeft (84));
            controls.removeFromLeft (6);
            length   .setBounds (controls.removeFromLeft (104));
            controls.removeFromLeft (10);

            // Swing knob on the far right
            auto swingCell = controls.removeFromRight (60);
            swing.setBounds (swingCell);
            controls.removeFromRight (8);
            randomBtn.setBounds (controls.removeFromRight (64));
            controls.removeFromRight (6);
            clearBtn .setBounds (controls.removeFromRight (60));

            grid.setBounds (r);
        }

    private:
        void timerCallback() override { grid.refreshPlayhead(); }

        SA1AudioProcessor& processor;

        juce::TextButton enableBtn, clearBtn, randomBtn;
        juce::ComboBox   syncBox, holdBox, rateBox;
        juce::Slider     length, swing;
        StepGrid         grid;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencerPanel)
    };
}
