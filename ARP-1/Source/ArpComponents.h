/*
  ==============================================================================

    ArpComponents.h
    ARP-1 Luxury Arpeggiator — UI components.

      * ArpVisualizer — an animated piano-roll view of the resolved arpeggio:
        each pattern step is drawn as a glowing note block at its pitch, joined
        by a melodic contour line, with a live playhead and held-chord bands.
      * StepLane      — the editable rhythmic pattern (velocity / rest / ratchet).
      * ControlPanel  — the global controls (rate, direction, octaves, gate,
        swing, steps, hold, plus generative helpers).

    All components read the live ArpEngine snapshot and drive engine setters
    directly. Drawing relies on the shared ElegantDarkLookAndFeel set on the
    editor (rotary knobs use the image strip from BinaryData).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ArpEngine.h"

namespace ARP1
{
    // Shared palette
    namespace col
    {
        inline juce::Colour accent()    { return juce::Colour (0xff4d9eff); }
        inline juce::Colour accentDim() { return juce::Colour (0xff2e5c8a); }
        inline juce::Colour panel()     { return juce::Colour (0xff161b24); }
        inline juce::Colour panelHi()   { return juce::Colour (0xff1f2632); }
        inline juce::Colour stroke()    { return juce::Colour (0xff2c3543); }
        inline juce::Colour text()      { return juce::Colour (0xffe8edf5); }
        inline juce::Colour textDim()   { return juce::Colour (0xff8294ad); }
        inline juce::Colour gold()      { return juce::Colour (0xffe8c89a); }
    }

    inline void paintPanel (juce::Graphics& g, juce::Rectangle<float> b, float radius = 10.0f)
    {
        juce::ColourGradient bg (col::panelHi(), b.getCentreX(), b.getY(),
                                  col::panel(), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (b, radius);
        g.setColour (col::stroke());
        g.drawRoundedRectangle (b, radius, 1.0f);
    }

    //==============================================================================
    /** Animated piano-roll visualisation of the live arpeggio. */
    class ArpVisualizer : public juce::Component,
                          private juce::Timer
    {
    public:
        explicit ArpVisualizer (ArpEngine& e) : engine (e) { startTimerHz (60); }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            paintPanel (g, bounds.reduced (1.0f), 12.0f);

            auto area = bounds.reduced (14.0f);

            // ---- pitch range ----
            const int dispLow  = juce::jmax (0,   snap.lowNote  - 2);
            const int dispHigh = juce::jmin (127, juce::jmax (snap.highNote + 2, dispLow + 12));
            const float span   = (float) juce::jmax (1, dispHigh - dispLow);

            auto pitchToY = [&] (float note)
            {
                const float t = (note - (float) dispLow) / span;
                return area.getBottom() - t * area.getHeight();
            };

            // ---- octave bands & C grid lines ----
            for (int n = dispLow; n <= dispHigh; ++n)
            {
                if (n % 12 == 0)   // C
                {
                    const float y = pitchToY ((float) n);
                    g.setColour (juce::Colour (0x14ffffff));
                    g.fillRect (area.getX(), y - 0.5f, area.getWidth(), 1.0f);
                    g.setColour (col::textDim().withAlpha (0.5f));
                    g.setFont (juce::Font (juce::FontOptions (10.0f)));
                    g.drawText ("C" + juce::String (n / 12 - 1),
                                juce::Rectangle<float> (area.getX() + 2.0f, y - 12.0f, 26.0f, 12.0f),
                                juce::Justification::centredLeft);
                }
            }

            // ---- held-chord pitch bands (the input notes) ----
            for (int i = 0; i < snap.numHeld; ++i)
            {
                const float y = pitchToY ((float) snap.held[(size_t) i] + 0.5f);
                const float h = juce::jmax (3.0f, area.getHeight() / span);
                g.setColour (col::gold().withAlpha (0.10f));
                g.fillRect (area.getX(), y - h, area.getWidth(), h);
            }

            const int nSteps   = engine.getNumSteps();
            const int orderLen = snap.numOrder;
            const int dir      = engine.getDirection();
            const float colW   = area.getWidth() / (float) juce::jmax (1, nSteps);

            // ---- empty state hint ----
            if (orderLen == 0)
            {
                g.setColour (col::textDim().withAlpha (0.7f));
                g.setFont (juce::Font (juce::FontOptions (16.0f)));
                g.drawText ("Play a chord to arpeggiate",
                            area, juce::Justification::centred);
                drawPlayhead (g, area, colW, nSteps);
                return;
            }

            const bool isChord  = (dir == (int) Direction::Chord);
            const bool isRandom = (dir == (int) Direction::Random);

            // ---- contour line through the per-step notes ----
            if (! isChord && ! isRandom)
            {
                juce::Path contour;
                bool started = false;
                for (int s = 0; s < nSteps; ++s)
                {
                    const int idx  = ((s % orderLen) + orderLen) % orderLen;
                    const float cx = area.getX() + (s + 0.5f) * colW;
                    const float cy = pitchToY ((float) snap.order[(size_t) idx] + 0.5f);
                    if (! started) { contour.startNewSubPath (cx, cy); started = true; }
                    else            contour.lineTo (cx, cy);
                }
                g.setColour (col::accent().withAlpha (0.35f));
                g.strokePath (contour, juce::PathStrokeType (1.5f));
            }

            // ---- per-step note blocks ----
            for (int s = 0; s < nSteps; ++s)
            {
                const auto& st = engine.getStep (s);
                const float cx = area.getX() + s * colW;
                const bool  isCurrent = (s == snap.currentStep) && snap.running;

                if (isChord)
                {
                    for (int i = 0; i < orderLen; ++i)
                        drawNote (g, cx, colW, pitchToY ((float) snap.order[(size_t) i] + 0.5f),
                                  span, area, st.on, isCurrent, st.vel, st.ratchet);
                }
                else if (isRandom)
                {
                    // faint candidate dots, live note highlighted at the playhead
                    for (int i = 0; i < orderLen; ++i)
                    {
                        const float cy = pitchToY ((float) snap.order[(size_t) i] + 0.5f);
                        const bool live = isCurrent && (i == snap.currentOrderPos);
                        if (live)
                            drawNote (g, cx, colW, cy, span, area, st.on, true, st.vel, st.ratchet);
                        else
                        {
                            g.setColour (col::accentDim().withAlpha (st.on ? 0.35f : 0.12f));
                            g.fillEllipse (cx + colW * 0.5f - 2.0f, cy - 2.0f, 4.0f, 4.0f);
                        }
                    }
                }
                else
                {
                    const int idx = ((s % orderLen) + orderLen) % orderLen;
                    drawNote (g, cx, colW, pitchToY ((float) snap.order[(size_t) idx] + 0.5f),
                              span, area, st.on, isCurrent, st.vel, st.ratchet);
                }
            }

            drawPlayhead (g, area, colW, nSteps);
        }

    private:
        void drawNote (juce::Graphics& g, float colX, float colW, float cy,
                       float span, juce::Rectangle<float> area,
                       bool on, bool current, float vel, int ratchet)
        {
            const float h = juce::jlimit (6.0f, 22.0f, area.getHeight() / span * 0.9f);
            const float pad = colW * 0.14f;
            juce::Rectangle<float> r (colX + pad, cy - h * 0.5f, colW - pad * 2.0f, h);

            if (! on)
            {
                g.setColour (col::stroke().withAlpha (0.5f));
                g.drawRoundedRectangle (r, 3.0f, 1.0f);
                return;
            }

            const float bright = 0.35f + 0.65f * vel;
            juce::Colour base = col::accent().withMultipliedBrightness (current ? 1.0f : 0.85f);

            if (current)
            {
                const float pulse = 0.5f + 0.5f * std::sin (animPhase * 6.2831853f);
                g.setColour (col::accent().withAlpha (0.30f + 0.25f * pulse));
                g.fillRoundedRectangle (r.expanded (5.0f + 3.0f * pulse), 6.0f);
            }

            juce::ColourGradient grad (base.brighter (0.25f), r.getX(), r.getY(),
                                        base.darker (0.25f),  r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.setOpacity (bright);
            g.fillRoundedRectangle (r, 3.0f);
            g.setOpacity (1.0f);

            // specular top
            g.setColour (juce::Colours::white.withAlpha (current ? 0.45f : 0.18f));
            g.drawLine (r.getX() + 2.0f, r.getY() + 1.0f, r.getRight() - 2.0f, r.getY() + 1.0f, 1.0f);

            // ratchet subdivisions
            if (ratchet > 1)
            {
                g.setColour (juce::Colour (0xff0d1320).withAlpha (0.8f));
                for (int k = 1; k < ratchet; ++k)
                {
                    const float x = r.getX() + r.getWidth() * (float) k / (float) ratchet;
                    g.drawLine (x, r.getY() + 1.0f, x, r.getBottom() - 1.0f, 1.0f);
                }
            }
        }

        void drawPlayhead (juce::Graphics& g, juce::Rectangle<float> area, float colW, int nSteps)
        {
            if (! snap.running || ! juce::isPositiveAndBelow (snap.currentStep, nSteps))
                return;
            const float x = area.getX() + snap.currentStep * colW;
            juce::ColourGradient ph (col::accent().withAlpha (0.22f), x + colW * 0.5f, area.getY(),
                                      col::accent().withAlpha (0.0f),  x + colW * 0.5f, area.getBottom(), false);
            g.setGradientFill (ph);
            g.fillRect (x, area.getY(), colW, area.getHeight());
            g.setColour (col::accent().withAlpha (0.7f));
            g.fillRect (x + colW * 0.5f - 0.5f, area.getY(), 1.0f, area.getHeight());
        }

        void timerCallback() override
        {
            animPhase += 0.016f;
            if (animPhase > 1.0f) animPhase -= 1.0f;
            engine.getSnapshot (snap);
            repaint();
        }

        ArpEngine&  engine;
        ArpSnapshot snap;
        float       animPhase = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArpVisualizer)
    };

    //==============================================================================
    /** Editable rhythmic step pattern: velocity bars, rest toggle, ratchet. */
    class StepLane : public juce::Component,
                     private juce::Timer
    {
    public:
        explicit StepLane (ArpEngine& e) : engine (e)
        {
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            startTimerHz (45);
        }

        std::function<void()> onChanged;

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            paintPanel (g, bounds.reduced (1.0f), 10.0f);

            auto area = bounds.reduced (10.0f);
            const int n = engine.getNumSteps();
            const float colW = getLocalBounds().toFloat().reduced (10.0f).getWidth() / (float) juce::jmax (1, kMaxSteps);

            const int liveStep = engine.isRunning() ? liveCurrentStep : -1;

            for (int i = 0; i < kMaxSteps; ++i)
            {
                juce::Rectangle<float> cell (area.getX() + i * colW, area.getY(), colW, area.getHeight());
                auto inner = cell.reduced (2.0f, 0.0f);

                const bool inRange = i < n;
                const auto& st = engine.getStep (i);

                // cell background
                g.setColour (inRange ? juce::Colour (0xff10151e) : juce::Colour (0xff0c0f15));
                g.fillRoundedRectangle (inner, 4.0f);

                if (i == liveStep && inRange)
                {
                    g.setColour (col::accent().withAlpha (0.18f));
                    g.fillRoundedRectangle (inner, 4.0f);
                }

                if (inRange)
                {
                    const float full = inner.getHeight() - 4.0f;
                    if (st.on)
                    {
                        const float h = juce::jmax (3.0f, full * st.vel);
                        juce::Rectangle<float> bar (inner.getX() + 2.0f, inner.getBottom() - 2.0f - h,
                                                    inner.getWidth() - 4.0f, h);
                        const bool live = (i == liveStep);
                        juce::Colour c = live ? col::accent() : col::accent().withMultipliedBrightness (0.8f);
                        juce::ColourGradient grad (c.brighter (0.2f), bar.getX(), bar.getY(),
                                                    c.darker (0.3f), bar.getX(), bar.getBottom(), false);
                        g.setGradientFill (grad);
                        g.fillRoundedRectangle (bar, 3.0f);
                        g.setColour (juce::Colours::white.withAlpha (0.25f));
                        g.drawLine (bar.getX() + 1.0f, bar.getY() + 1.0f, bar.getRight() - 1.0f, bar.getY() + 1.0f, 1.0f);

                        // ratchet ticks across the top of the cell
                        if (st.ratchet > 1)
                        {
                            g.setColour (col::gold().withAlpha (0.9f));
                            for (int k = 0; k < st.ratchet; ++k)
                            {
                                const float seg = (inner.getWidth() - 4.0f) / (float) st.ratchet;
                                const float x = inner.getX() + 2.0f + k * seg;
                                g.fillRect (x, inner.getY() + 2.0f, seg - 1.5f, 2.5f);
                            }
                        }
                    }
                    else
                    {
                        // rest marker
                        g.setColour (col::stroke());
                        g.drawRoundedRectangle (inner.reduced (3.0f), 3.0f, 1.0f);
                    }

                    // step number
                    g.setColour (col::textDim().withAlpha (0.55f));
                    g.setFont (juce::Font (juce::FontOptions (9.0f)));
                    g.drawText (juce::String (i + 1),
                                juce::Rectangle<float> (inner.getX(), inner.getBottom() - 12.0f, inner.getWidth(), 11.0f),
                                juce::Justification::centred);
                }
            }

            // hint
            g.setColour (col::textDim().withAlpha (0.5f));
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.drawText ("drag: velocity   ·   right-click: rest   ·   wheel: ratchet",
                        getLocalBounds().removeFromTop (16).reduced (12, 2),
                        juce::Justification::centredRight);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = stepAt (e.x);
            if (i < 0) return;

            if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
            {
                engine.setStepOn (i, ! engine.getStep (i).on);
                notify();
            }
            else
            {
                setVelocityFromY (i, e.y);
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.mods.isRightButtonDown()) return;
            const int i = stepAt (e.x);
            if (i >= 0) setVelocityFromY (i, e.y);
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int i = stepAt (e.x);
            if (i < 0) return;
            const int dir = w.deltaY > 0 ? 1 : -1;
            engine.setStepRatchet (i, juce::jlimit (1, 4, engine.getStep (i).ratchet + dir));
            engine.setStepOn (i, true);
            notify();
        }

        void refresh() { repaint(); }

    private:
        int stepAt (int px) const
        {
            auto area = getLocalBounds().toFloat().reduced (10.0f);
            const float colW = area.getWidth() / (float) kMaxSteps;
            const int i = (int) ((px - area.getX()) / colW);
            return (i >= 0 && i < engine.getNumSteps()) ? i : -1;
        }

        void setVelocityFromY (int i, int py)
        {
            auto area = getLocalBounds().toFloat().reduced (10.0f);
            const float t = juce::jlimit (0.0f, 1.0f, 1.0f - (py - area.getY()) / area.getHeight());
            engine.setStepVel (i, juce::jmax (0.05f, t));
            engine.setStepOn (i, true);
            notify();
        }

        void notify() { repaint(); if (onChanged) onChanged(); }

        void timerCallback() override
        {
            const int s = engine.isRunning() ? readCurrentStep() : -1;
            if (s != liveCurrentStep) { liveCurrentStep = s; repaint(); }
        }

        int readCurrentStep()
        {
            ArpSnapshot s;
            if (engine.getSnapshot (s)) lastSnap = s;
            return lastSnap.currentStep;
        }

        ArpEngine&  engine;
        ArpSnapshot lastSnap;
        int         liveCurrentStep = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepLane)
    };

    //==============================================================================
    /** Global controls. */
    class ControlPanel : public juce::Component
    {
    public:
        explicit ControlPanel (ArpEngine& e) : engine (e)
        {
            // ---- ARP on / hold toggles ----
            setupToggle (onToggle,   "ARP",  engine.isEnabled());
            setupToggle (holdToggle, "HOLD", engine.getHold());
            onToggle.onClick   = [this] { engine.setEnabled (onToggle.getToggleState()); changed(); };
            holdToggle.onClick = [this] { engine.setHold (holdToggle.getToggleState());  changed(); };

            // ---- combos ----
            addAndMakeVisible (rateCombo);
            for (int i = 0; i < numRates(); ++i) rateCombo.addItem (rateOption (i).name, i + 1);
            rateCombo.setSelectedId (engine.getRateIndex() + 1, juce::dontSendNotification);
            rateCombo.onChange = [this] { engine.setRateIndex (rateCombo.getSelectedId() - 1); changed(); };
            addLabel (rateLabel, "RATE");

            addAndMakeVisible (dirCombo);
            for (int i = 0; i < (int) Direction::NumDirections; ++i) dirCombo.addItem (directionName (i), i + 1);
            dirCombo.setSelectedId (engine.getDirection() + 1, juce::dontSendNotification);
            dirCombo.onChange = [this] { engine.setDirection (dirCombo.getSelectedId() - 1); changed(); };
            addLabel (dirLabel, "DIRECTION");

            // ---- knobs ----
            setupKnob (octKnob,   1, kMaxOctave, 1, engine.getOctaves(),       "OCTAVES");
            setupKnob (stepsKnob, 1, kMaxSteps,  1, engine.getNumSteps(),      "STEPS");
            setupKnob (gateKnob,  5, 100,        1, (int) std::round (engine.getGate() * 100.0f), "GATE %");
            setupKnob (swingKnob, 0, 70,         1, (int) std::round (engine.getSwing() * 100.0f), "SWING %");

            octKnob.onValueChange   = [this] { engine.setOctaves ((int) octKnob.getValue());   changed(); };
            stepsKnob.onValueChange = [this] { engine.setNumSteps ((int) stepsKnob.getValue()); patternChanged(); };
            gateKnob.onValueChange  = [this] { engine.setGate ((float) gateKnob.getValue() / 100.0f);  changed(); };
            swingKnob.onValueChange = [this] { engine.setSwing ((float) swingKnob.getValue() / 100.0f); changed(); };

            // ---- generative helpers ----
            setupKnob (pulsesKnob, 1, kMaxSteps, 1, 8, "PULSES");

            euclidBtn.setButtonText ("Euclid");
            randomBtn.setButtonText ("Random");
            clearBtn .setButtonText ("Clear");
            addAndMakeVisible (euclidBtn);
            addAndMakeVisible (randomBtn);
            addAndMakeVisible (clearBtn);

            euclidBtn.onClick = [this] { engine.setEuclid ((int) pulsesKnob.getValue(), engine.getNumSteps());
                                         stepsKnob.setValue (engine.getNumSteps(), juce::dontSendNotification);
                                         patternChanged(); };
            randomBtn.onClick = [this] { engine.randomizePattern (rng); patternChanged(); };
            clearBtn .onClick = [this] { engine.clearPattern(); patternChanged(); };
        }

        std::function<void()> onChanged;
        std::function<void()> onPatternChanged;

        void refreshFromEngine()
        {
            onToggle.setToggleState (engine.isEnabled(), juce::dontSendNotification);
            holdToggle.setToggleState (engine.getHold(), juce::dontSendNotification);
            rateCombo.setSelectedId (engine.getRateIndex() + 1, juce::dontSendNotification);
            dirCombo.setSelectedId (engine.getDirection() + 1, juce::dontSendNotification);
            octKnob.setValue (engine.getOctaves(), juce::dontSendNotification);
            stepsKnob.setValue (engine.getNumSteps(), juce::dontSendNotification);
            gateKnob.setValue (engine.getGate() * 100.0f, juce::dontSendNotification);
            swingKnob.setValue (engine.getSwing() * 100.0f, juce::dontSendNotification);
        }

        void paint (juce::Graphics& g) override
        {
            paintPanel (g, getLocalBounds().toFloat().reduced (1.0f), 10.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (14, 10);

            // top row: toggles (left) + generative helpers (right)
            auto top = area.removeFromTop (30);
            onToggle.setBounds   (top.removeFromLeft (70));
            top.removeFromLeft (6);
            holdToggle.setBounds (top.removeFromLeft (78));

            auto gen = top;
            clearBtn .setBounds (gen.removeFromRight (64));
            gen.removeFromRight (6);
            randomBtn.setBounds (gen.removeFromRight (70));
            gen.removeFromRight (6);
            euclidBtn.setBounds (gen.removeFromRight (64));

            area.removeFromTop (10);

            // combos row
            auto combos = area.removeFromTop (54);
            auto rateCell = combos.removeFromLeft (130);
            placeCombo (rateCell, rateLabel, rateCombo);
            combos.removeFromLeft (12);
            auto dirCell = combos.removeFromLeft (180);
            placeCombo (dirCell, dirLabel, dirCombo);

            area.removeFromTop (8);

            // knob row
            const int knobW = area.getWidth() / 5;
            placeKnob (area.removeFromLeft (knobW), octKnob,   octLabel);
            placeKnob (area.removeFromLeft (knobW), stepsKnob, stepsLabel);
            placeKnob (area.removeFromLeft (knobW), gateKnob,  gateLabel);
            placeKnob (area.removeFromLeft (knobW), swingKnob, swingLabel);
            placeKnob (area,                        pulsesKnob, pulsesLabel);
        }

    private:
        void changed() { if (onChanged) onChanged(); }
        void patternChanged() { if (onPatternChanged) onPatternChanged(); else if (onChanged) onChanged(); }

        void setupToggle (juce::ToggleButton& t, const juce::String& text, bool state)
        {
            t.setButtonText (text);
            t.setToggleState (state, juce::dontSendNotification);
            addAndMakeVisible (t);
        }

        void addLabel (juce::Label& l, const juce::String& text)
        {
            l.setText (text, juce::dontSendNotification);
            l.setJustificationType (juce::Justification::centred);
            l.setFont (juce::Font (juce::FontOptions (10.5f)));
            l.setColour (juce::Label::textColourId, col::textDim());
            addAndMakeVisible (l);
        }

        void setupKnob (juce::Slider& s, int lo, int hi, int step, int value, const juce::String& labelText)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setRange ((double) lo, (double) hi, (double) step);
            s.setValue (value, juce::dontSendNotification);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
            addAndMakeVisible (s);

            // pair a label using the slider's name field via a sibling label
            auto* l = new juce::Label();
            knobLabels.add (l);
            l->setText (labelText, juce::dontSendNotification);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::Font (juce::FontOptions (10.5f)));
            l->setColour (juce::Label::textColourId, col::textDim());
            addAndMakeVisible (l);
            knobToLabel[&s] = l;
        }

        void placeKnob (juce::Rectangle<int> cell, juce::Slider& s, juce::Label& /*unused*/)
        {
            cell.reduce (4, 0);
            if (auto* l = knobToLabel[&s])
                l->setBounds (cell.removeFromTop (14));
            s.setBounds (cell);
        }

        void placeCombo (juce::Rectangle<int> cell, juce::Label& label, juce::ComboBox& combo)
        {
            label.setBounds (cell.removeFromTop (14));
            combo.setBounds (cell.removeFromTop (28));
        }

        ArpEngine& engine;

        juce::ToggleButton onToggle, holdToggle;
        juce::ComboBox     rateCombo, dirCombo;
        juce::Label        rateLabel, dirLabel;
        juce::Slider       octKnob, stepsKnob, gateKnob, swingKnob, pulsesKnob;
        juce::Label        octLabel, stepsLabel, gateLabel, swingLabel, pulsesLabel;
        juce::TextButton   euclidBtn, randomBtn, clearBtn;

        juce::OwnedArray<juce::Label>             knobLabels;
        std::map<juce::Slider*, juce::Label*>     knobToLabel;
        juce::Random rng;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlPanel)
    };
}
