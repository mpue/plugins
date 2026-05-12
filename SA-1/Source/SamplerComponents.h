/*
  ==============================================================================

    SamplerComponents.h
    SA-1 Luxury Quick Sampler — UI components.

    DrumPadComponent: a single luxurious drum pad with mini waveform,
    name label, LED, drag-and-drop target, click-to-audition and
    right-click context menu.

    PadGrid: 4x4 layout of DrumPadComponents.

    BigWaveformDisplay: focused view of the selected pad's sample with
    start marker, peak rendering, gradient fill and a glowing playhead
    that pulses when the pad triggers.

    PadParameterPanel: knob bank for the selected pad
    (Volume, Pan, Pitch, Fine, Attack, Release, Start, Reverse, Choke).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SamplerEngine.h"
#include "PluginProcessor.h"

namespace SA1
{
    //==============================================================================
    /** Small helper: draws a rounded gloss panel used for all UI surfaces. */
    inline void drawLuxuryPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner,
                                 juce::Colour top, juce::Colour bottom, juce::Colour outline)
    {
        juce::ColourGradient bg (top,    r.getCentreX(), r.getY(),
                                  bottom, r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, corner);

        g.setColour (outline);
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);

        // Top sheen
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawLine (r.getX() + corner, r.getY() + 1.5f,
                    r.getRight() - corner, r.getY() + 1.5f, 1.0f);
    }

    inline void drawWaveformPath (juce::Graphics& g,
                                  juce::Rectangle<float> area,
                                  const std::array<float, kThumbPoints>& mins,
                                  const std::array<float, kThumbPoints>& maxs,
                                  juce::Colour fillCol,
                                  juce::Colour edgeCol)
    {
        const float midY  = area.getCentreY();
        const float halfH = area.getHeight() * 0.5f;

        juce::Path top;
        juce::Path bot;
        const float w = area.getWidth();

        top.startNewSubPath (area.getX(), midY);
        for (int i = 0; i < kThumbPoints; ++i)
        {
            const float x = area.getX() + (float) i / (float) (kThumbPoints - 1) * w;
            const float y = midY - juce::jlimit (-1.0f, 1.0f, maxs[(size_t) i]) * halfH;
            top.lineTo (x, y);
        }
        top.lineTo (area.getRight(), midY);

        bot.startNewSubPath (area.getX(), midY);
        for (int i = 0; i < kThumbPoints; ++i)
        {
            const float x = area.getX() + (float) i / (float) (kThumbPoints - 1) * w;
            const float y = midY - juce::jlimit (-1.0f, 1.0f, mins[(size_t) i]) * halfH;
            bot.lineTo (x, y);
        }
        bot.lineTo (area.getRight(), midY);

        juce::Path filled = top;
        filled.addPath (bot);

        juce::ColourGradient g1 (fillCol.withAlpha (0.85f),  area.getCentreX(), area.getY(),
                                  fillCol.withAlpha (0.15f), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill (g1);
        g.fillPath (filled);

        g.setColour (edgeCol);
        g.strokePath (top, juce::PathStrokeType (1.2f));
        g.strokePath (bot, juce::PathStrokeType (1.2f));
    }

    //==============================================================================
    class DrumPadComponent : public juce::Component,
                             public juce::FileDragAndDropTarget
    {
    public:
        DrumPadComponent (SA1AudioProcessor& proc, int pad)
            : processor (proc), padIndex (pad) {}

        void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
        bool isSelected() const noexcept { return selected; }

        void notifyTriggered (float velocity = 1.0f)
        {
            triggerPulse = juce::jlimit (0.0f, 1.0f, velocity);
            startTimerHz (45);
            repaint();
        }

        std::function<void(int)> onPadClicked;
        std::function<void(int)> onPadDoubleClicked;
        std::function<void(int)> onSampleAssigned;
        std::function<void(int)> onPadCleared;

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced (3.0f);
            const float corner = 10.0f;

            const auto& slot = getDefaultSlot (padIndex);
            const auto accent = juce::Colour (slot.colour);

            const auto& pad = processor.getEngine().getPad (padIndex);
            const bool  has = pad.hasSample();

            // Body
            const juce::Colour top    = has ? juce::Colour (0xff232a35) : juce::Colour (0xff1a1f28);
            const juce::Colour bottom = has ? juce::Colour (0xff10141c) : juce::Colour (0xff0c0f15);
            drawLuxuryPanel (g, bounds, corner, top, bottom, juce::Colour (0xff343d4b));

            // Selection ring
            if (selected)
            {
                g.setColour (accent.withAlpha (0.85f));
                g.drawRoundedRectangle (bounds.reduced (1.0f), corner, 2.0f);
                g.setColour (accent.withAlpha (0.18f));
                g.drawRoundedRectangle (bounds.expanded (2.0f), corner + 2.0f, 2.0f);
            }
            else
            {
                g.setColour (accent.withAlpha (has ? 0.35f : 0.18f));
                g.drawRoundedRectangle (bounds.reduced (1.0f), corner, 1.0f);
            }

            // Trigger glow
            if (triggerPulse > 0.001f)
            {
                juce::ColourGradient pulse (accent.withAlpha (0.45f * triggerPulse),
                                            bounds.getCentreX(), bounds.getCentreY(),
                                            accent.withAlpha (0.0f),
                                            bounds.getRight(), bounds.getBottom(), true);
                g.setGradientFill (pulse);
                g.fillRoundedRectangle (bounds, corner);
            }

            // Drop highlight
            if (dropHover)
            {
                g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.55f));
                g.drawRoundedRectangle (bounds.reduced (1.0f), corner, 2.0f);
            }

            // Pad number top-left
            g.setColour (juce::Colour (0xff67738a));
            g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
            g.drawText (juce::String (padIndex + 1).paddedLeft ('0', 2),
                        bounds.toNearestInt().reduced (8, 6).removeFromTop (12).removeFromLeft (24),
                        juce::Justification::topLeft);

            // MIDI note top-right
            const int note = processor.getEngine().midiNoteForPad (padIndex);
            g.setColour (juce::Colour (0xff67738a));
            g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                        bounds.toNearestInt().reduced (8, 6).removeFromTop (12).removeFromRight (40),
                        juce::Justification::topRight);

            // Slot name
            g.setColour (accent.withAlpha (has ? 1.0f : 0.55f));
            g.setFont (juce::Font (juce::FontOptions (15.0f).withStyle ("Bold")));
            auto labelArea = bounds.toNearestInt();
            labelArea.removeFromTop (18);
            labelArea.removeFromBottom (50);
            g.drawText (slot.shortName, labelArea, juce::Justification::centred);

            // Mini waveform area
            auto wfArea = bounds.reduced (8.0f, 8.0f);
            wfArea.removeFromTop (40.0f);
            wfArea.removeFromBottom (16.0f);

            if (has)
            {
                drawWaveformPath (g, wfArea,
                                   pad.getThumbMin(), pad.getThumbMax(),
                                   accent, accent.withAlpha (0.95f));
            }
            else
            {
                g.setColour (juce::Colour (0xff394251));
                g.drawLine (wfArea.getX(), wfArea.getCentreY(),
                            wfArea.getRight(), wfArea.getCentreY(), 1.0f);

                g.setColour (juce::Colour (0xff596780));
                g.setFont (juce::Font (juce::FontOptions (10.0f)));
                g.drawText ("drop sample",
                            wfArea.toNearestInt(),
                            juce::Justification::centred);
            }

            // Bottom strip with sample name
            auto nameArea = bounds.toNearestInt().removeFromBottom (16).reduced (6, 0);
            const auto name = pad.getState().displayName;
            if (name.isNotEmpty())
            {
                g.setColour (juce::Colour (0xffc8d3e6));
                g.setFont (juce::Font (juce::FontOptions (10.0f)));
                g.drawFittedText (name, nameArea, juce::Justification::centred, 1);
            }

            // LED
            const float ledSize = 6.0f;
            juce::Rectangle<float> led (bounds.getX() + 8.0f, bounds.getBottom() - 12.0f,
                                         ledSize, ledSize);
            const float ledIntensity = juce::jmax (triggerPulse, has ? 0.25f : 0.05f);
            g.setColour (accent.withAlpha (juce::jlimit (0.1f, 1.0f, ledIntensity)));
            g.fillEllipse (led);
            if (triggerPulse > 0.05f)
            {
                g.setColour (accent.withAlpha (0.35f * triggerPulse));
                g.fillEllipse (led.expanded (2.0f * triggerPulse));
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                showContextMenu();
                return;
            }
            if (onPadClicked) onPadClicked (padIndex);
            const float vel = juce::jlimit (0.2f, 1.0f,
                              1.0f - (float) e.position.y / (float) getHeight() * 0.6f);
            processor.requestAudition (padIndex, vel);
            notifyTriggered (vel);
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (onPadDoubleClicked) onPadDoubleClicked (padIndex);
            chooseSampleFile();
        }

        // ---- file drag & drop ----
        bool isInterestedInFileDrag (const juce::StringArray& files) override
        {
            for (auto& f : files)
                if (isAudioFile (f)) return true;
            return false;
        }

        void fileDragEnter (const juce::StringArray&, int, int) override
        {
            dropHover = true; repaint();
        }

        void fileDragExit (const juce::StringArray&) override
        {
            dropHover = false; repaint();
        }

        void filesDropped (const juce::StringArray& files, int, int) override
        {
            dropHover = false;
            for (auto& f : files)
            {
                if (isAudioFile (f))
                {
                    if (processor.getEngine().loadSampleIntoPad (padIndex, juce::File (f)))
                    {
                        if (onSampleAssigned) onSampleAssigned (padIndex);
                    }
                    break;
                }
            }
            repaint();
        }

        int getPadIndex() const noexcept { return padIndex; }

    private:
        void timerCallback() {}  // not used; we drive pulse from outside via repaints

        static bool isAudioFile (const juce::String& f)
        {
            const auto e = juce::File (f).getFileExtension().toLowerCase();
            return e == ".wav" || e == ".aif" || e == ".aiff"
                || e == ".flac" || e == ".ogg" || e == ".mp3";
        }

        void startTimerHz (int hz)
        {
            // simple decay animation using a juce::Timer-derived helper
            class DecayTimer : public juce::Timer
            {
            public:
                DecayTimer (DrumPadComponent& o) : owner (o) {}
                void timerCallback() override
                {
                    owner.triggerPulse *= 0.85f;
                    if (owner.triggerPulse < 0.01f) { owner.triggerPulse = 0.0f; stopTimer(); }
                    owner.repaint();
                }
                DrumPadComponent& owner;
            };
            if (decayTimer == nullptr)
                decayTimer = std::make_unique<DecayTimer> (*this);
            decayTimer->startTimerHz (hz);
        }

        void chooseSampleFile()
        {
            chooser = std::make_unique<juce::FileChooser> (
                "Choose audio sample for " + juce::String (getDefaultSlot (padIndex).longName),
                juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (! file.existsAsFile()) return;
                    if (processor.getEngine().loadSampleIntoPad (padIndex, file))
                    {
                        if (onSampleAssigned) onSampleAssigned (padIndex);
                        repaint();
                    }
                });
        }

        void showContextMenu()
        {
            juce::PopupMenu m;
            m.addItem (1, "Load sample...");
            m.addItem (2, "Clear pad", processor.getEngine().getPad (padIndex).hasSample());
            m.addSeparator();
            m.addItem (3, "Reveal in Finder", processor.getEngine().getPad (padIndex).hasSample());

            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (*this),
                [this] (int r)
                {
                    if (r == 1) chooseSampleFile();
                    else if (r == 2)
                    {
                        processor.getEngine().clearPad (padIndex);
                        if (onPadCleared) onPadCleared (padIndex);
                        repaint();
                    }
                    else if (r == 3)
                    {
                        const auto p = processor.getEngine().getPad (padIndex).getState().filePath;
                        juce::File f (p);
                        if (f.existsAsFile())
                            f.revealToUser();
                    }
                });
        }

        SA1AudioProcessor& processor;
        const int          padIndex;
        bool               selected     = false;
        bool               dropHover    = false;
        float              triggerPulse = 0.0f;

        std::unique_ptr<juce::FileChooser> chooser;
        std::unique_ptr<juce::Timer>       decayTimer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumPadComponent)
    };

    //==============================================================================
    class PadGrid : public juce::Component
    {
    public:
        explicit PadGrid (SA1AudioProcessor& proc) : processor (proc)
        {
            for (int i = 0; i < kNumPads; ++i)
            {
                auto p = std::make_unique<DrumPadComponent> (proc, i);
                p->onPadClicked       = [this] (int idx) { selectPad (idx); };
                p->onPadDoubleClicked = [this] (int idx) { selectPad (idx); };
                p->onSampleAssigned   = [this] (int idx) { selectPad (idx); if (onChanged) onChanged(); };
                p->onPadCleared       = [this] (int)     { if (onChanged) onChanged(); };
                addAndMakeVisible (*p);
                pads.add (p.release());
            }
            selectPad (0, false);
        }

        DrumPadComponent& getPadComponent (int idx) noexcept
        {
            return *pads[juce::jlimit (0, kNumPads - 1, idx)];
        }

        void selectPad (int padIndex, bool notify = true)
        {
            padIndex = juce::jlimit (0, kNumPads - 1, padIndex);
            for (int i = 0; i < kNumPads; ++i)
                pads[i]->setSelected (i == padIndex);
            selectedPad = padIndex;
            if (notify && onSelectedPadChanged) onSelectedPadChanged (padIndex);
        }

        int getSelectedPad() const noexcept { return selectedPad; }

        void resized() override
        {
            const int rows = 4, cols = 4;
            const int gap  = 6;
            auto r = getLocalBounds().reduced (4);

            const int cellW = (r.getWidth()  - gap * (cols - 1)) / cols;
            const int cellH = (r.getHeight() - gap * (rows - 1)) / rows;

            for (int i = 0; i < kNumPads; ++i)
            {
                const int row = i / cols;
                const int col = i % cols;
                const int x = r.getX() + col * (cellW + gap);
                const int y = r.getY() + row * (cellH + gap);
                pads[i]->setBounds (x, y, cellW, cellH);
            }
        }

        std::function<void(int)> onSelectedPadChanged;
        std::function<void()>    onChanged;

    private:
        SA1AudioProcessor& processor;
        juce::OwnedArray<DrumPadComponent> pads;
        int selectedPad = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadGrid)
    };

    //==============================================================================
    /** The big focused waveform display for the selected pad. */
    class BigWaveformDisplay : public juce::Component
    {
    public:
        explicit BigWaveformDisplay (SA1AudioProcessor& proc) : processor (proc) {}

        void setSelectedPad (int padIdx) { selectedPad = padIdx; repaint(); }

        void notifyTriggered (int padIdx, float velocity)
        {
            if (padIdx != selectedPad) return;
            playheadActive   = true;
            playheadProgress = 0.0f;
            playheadVel      = juce::jlimit (0.1f, 1.0f, velocity);
            ringPulse        = 1.0f;
            if (! isTimerRunning())
                animTimer = std::make_unique<AnimTimer> (*this);
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            const float corner = 14.0f;

            // Body
            drawLuxuryPanel (g, bounds.reduced (1.0f), corner,
                              juce::Colour (0xff121822),
                              juce::Colour (0xff05080d),
                              juce::Colour (0xff262e3a));

            // Subtle vignette
            {
                juce::ColourGradient v (juce::Colour (0x00000000),
                                         bounds.getCentreX(), bounds.getCentreY(),
                                         juce::Colour (0x66000000),
                                         bounds.getRight(), bounds.getBottom(), true);
                g.setGradientFill (v);
                g.fillRoundedRectangle (bounds.reduced (1.0f), corner);
            }

            const auto& slot = getDefaultSlot (selectedPad);
            const auto accent = juce::Colour (slot.colour);
            const auto& pad   = processor.getEngine().getPad (selectedPad);

            auto inner = bounds.reduced (18.0f, 22.0f);
            auto header = inner.removeFromTop (28.0f);
            inner.removeFromTop (8.0f);
            auto footer = inner.removeFromBottom (22.0f);

            // Header text
            g.setColour (accent);
            g.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
            g.drawText (slot.longName, header.toNearestInt(), juce::Justification::centredLeft);

            const auto fileName = pad.getState().displayName;
            g.setColour (juce::Colour (0xffd0d8e8));
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText (fileName.isNotEmpty() ? fileName : juce::String ("— no sample loaded —"),
                        header.toNearestInt(), juce::Justification::centredRight);

            // Grid
            g.setColour (juce::Colour (0x14ffffff));
            for (int i = 1; i < 8; ++i)
            {
                const float x = inner.getX() + inner.getWidth() * (float) i / 8.0f;
                g.drawLine (x, inner.getY(), x, inner.getBottom(), 1.0f);
            }
            g.drawLine (inner.getX(), inner.getCentreY(),
                        inner.getRight(), inner.getCentreY(), 1.0f);

            if (pad.hasSample())
            {
                drawWaveformPath (g, inner, pad.getThumbMin(), pad.getThumbMax(),
                                   accent, accent.brighter (0.4f));

                // Start position marker
                const float startX = inner.getX() + inner.getWidth() * juce::jlimit (0.0f, 1.0f, pad.getState().startPos);
                g.setColour (juce::Colour (0xfff7c873));
                g.drawLine (startX, inner.getY(), startX, inner.getBottom(), 1.5f);
                g.fillRect (juce::Rectangle<float> (startX - 4.0f, inner.getY(), 8.0f, 4.0f));

                // Reverse hint arrow
                if (pad.getState().reverse)
                {
                    g.setColour (juce::Colour (0x99ff7080));
                    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
                    g.drawText ("REV", inner.toNearestInt().reduced (10).removeFromTop (16),
                                juce::Justification::topRight);
                }

                // Playhead
                if (playheadActive)
                {
                    const float headX = juce::jmap (playheadProgress, 0.0f, 1.0f,
                                                     inner.getX(), inner.getRight());
                    juce::Colour glow = accent.withAlpha (0.5f * playheadVel);
                    g.setColour (glow);
                    g.fillRect (juce::Rectangle<float> (headX - 6.0f, inner.getY(),
                                                         12.0f, inner.getHeight()).toFloat());
                    g.setColour (juce::Colours::white.withAlpha (0.85f * playheadVel));
                    g.drawLine (headX, inner.getY(), headX, inner.getBottom(), 1.5f);
                }

                // Ring pulse around the perimeter
                if (ringPulse > 0.001f)
                {
                    g.setColour (accent.withAlpha (0.30f * ringPulse));
                    g.drawRoundedRectangle (bounds.reduced (3.0f), corner, 3.0f);
                }
            }
            else
            {
                // Empty state — invite the user
                g.setColour (juce::Colour (0xff556581));
                g.setFont (juce::Font (juce::FontOptions (16.0f)));
                g.drawText ("Drop an audio file here, double-click the pad,",
                            inner.toNearestInt().withTrimmedBottom (inner.getHeight()/2),
                            juce::Justification::centredBottom);
                g.drawText ("or right-click to load a sample.",
                            inner.toNearestInt().withTrimmedTop (inner.getHeight()/2),
                            juce::Justification::centredTop);
            }

            // Footer info
            g.setColour (juce::Colour (0xff8a96b0));
            g.setFont (juce::Font (juce::FontOptions (11.0f)));

            juce::String stats;
            if (pad.hasSample())
            {
                const double seconds = pad.getBuffer().getNumSamples() / juce::jmax (1.0, pad.getSourceSR());
                stats << "Duration: " << juce::String (seconds, 2) << " s   "
                      << "Samples: "  << pad.getBuffer().getNumSamples() << "   "
                      << "Rate: "     << juce::String ((int) pad.getSourceSR()) << " Hz   "
                      << "Channels: " << pad.getBuffer().getNumChannels();
            }
            g.drawText (stats, footer.toNearestInt(), juce::Justification::centredLeft);

            const int note = processor.getEngine().midiNoteForPad (selectedPad);
            g.drawText ("MIDI: " + juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                        footer.toNearestInt(), juce::Justification::centredRight);
        }

        bool isTimerRunning() const noexcept { return animTimer != nullptr && animTimer->isTimerRunning(); }

    private:
        struct AnimTimer : public juce::Timer
        {
            AnimTimer (BigWaveformDisplay& o) : owner (o) { startTimerHz (60); }
            void timerCallback() override
            {
                owner.playheadProgress += 0.012f;
                owner.ringPulse        *= 0.92f;
                if (owner.playheadProgress >= 1.0f)
                {
                    owner.playheadActive   = false;
                    owner.playheadProgress = 0.0f;
                }
                if (! owner.playheadActive && owner.ringPulse < 0.02f)
                    stopTimer();
                owner.repaint();
            }
            BigWaveformDisplay& owner;
        };

        SA1AudioProcessor& processor;
        int   selectedPad      = 0;
        bool  playheadActive   = false;
        float playheadProgress = 0.0f;
        float playheadVel      = 1.0f;
        float ringPulse        = 0.0f;

        std::unique_ptr<AnimTimer> animTimer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BigWaveformDisplay)
    };

    //==============================================================================
    /** Knobs and switches for the currently selected pad. */
    class PadParameterPanel : public juce::Component,
                              private juce::Slider::Listener,
                              private juce::Button::Listener
    {
    public:
        explicit PadParameterPanel (SA1AudioProcessor& proc) : processor (proc)
        {
            const std::vector<std::pair<juce::Slider*, juce::Label*>> rotaries =
            {
                { &gain,    &gainLbl    },
                { &pan,     &panLbl     },
                { &pitch,   &pitchLbl   },
                { &fine,    &fineLbl    },
                { &attack,  &attackLbl  },
                { &release, &releaseLbl },
                { &start,   &startLbl   }
            };

            for (auto& kv : rotaries)
                configureKnob (*kv.first, *kv.second);

            gain   .setRange (-60.0,  12.0, 0.1);  gain   .setValue (0.0);
            pan    .setRange ( -1.0,   1.0, 0.01); pan    .setValue (0.0);
            pitch  .setRange (-24.0,  24.0, 1.0);  pitch  .setValue (0.0);
            fine   .setRange (-100.0,100.0, 1.0);  fine   .setValue (0.0);
            attack .setRange (   0.0, 500.0,0.1);  attack .setValue (1.0);
            release.setRange (   5.0,4000.0,1.0);  release.setValue (200.0);
            release.setSkewFactor (0.4);
            attack .setSkewFactor (0.5);
            start  .setRange (   0.0,  0.99,0.001);start  .setValue (0.0);

            gainLbl   .setText ("VOLUME",   juce::dontSendNotification);
            panLbl    .setText ("PAN",      juce::dontSendNotification);
            pitchLbl  .setText ("PITCH",    juce::dontSendNotification);
            fineLbl   .setText ("FINE",     juce::dontSendNotification);
            attackLbl .setText ("ATTACK",   juce::dontSendNotification);
            releaseLbl.setText ("RELEASE",  juce::dontSendNotification);
            startLbl  .setText ("START",    juce::dontSendNotification);

            gain   .textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
            pan    .textFromValueFunction = [] (double v)
            {
                if (std::abs (v) < 0.01) return juce::String ("C");
                return (v < 0 ? juce::String ("L") : juce::String ("R"))
                     + juce::String ((int) std::round (std::abs (v) * 100.0));
            };
            pitch  .textFromValueFunction = [] (double v) { return juce::String ((int) v) + " st"; };
            fine   .textFromValueFunction = [] (double v) { return juce::String ((int) v) + " ct"; };
            attack .textFromValueFunction = [] (double v) { return juce::String ((int) v) + " ms"; };
            release.textFromValueFunction = [] (double v) { return juce::String ((int) v) + " ms"; };
            start  .textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v * 100.0)) + " %"; };

            for (auto* s : { &gain, &pan, &pitch, &fine, &attack, &release, &start })
                s->addListener (this);

            // Reverse + choke + load buttons
            reverseBtn.setButtonText ("REVERSE");
            chokeBtn  .setButtonText ("CHOKE");
            loadBtn   .setButtonText ("Load Sample");
            clearBtn  .setButtonText ("Clear");

            reverseBtn.setClickingTogglesState (true);
            chokeBtn  .setClickingTogglesState (true);

            for (auto* b : { (juce::Button*) &reverseBtn, (juce::Button*) &chokeBtn,
                             (juce::Button*) &loadBtn,    (juce::Button*) &clearBtn })
            {
                b->addListener (this);
                addAndMakeVisible (*b);
            }

            for (auto* k : { &gain, &pan, &pitch, &fine, &attack, &release, &start })
                addAndMakeVisible (*k);
            for (auto* l : { &gainLbl, &panLbl, &pitchLbl, &fineLbl,
                             &attackLbl, &releaseLbl, &startLbl })
                addAndMakeVisible (*l);

            // Master gain at the bottom
            configureKnob (master, masterLbl);
            master.setRange (-24.0, 12.0, 0.1);
            master.setValue (0.0);
            master.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
            masterLbl.setText ("MASTER", juce::dontSendNotification);
            master.addListener (this);
            addAndMakeVisible (master);
            addAndMakeVisible (masterLbl);
        }

        void setSelectedPad (int idx)
        {
            selectedPad = idx;
            refreshFromPad();
            repaint();
        }

        void refreshFromPad()
        {
            suppress = true;
            const auto& s = processor.getEngine().getPad (selectedPad).getState();
            gain   .setValue (s.gainDb,     juce::dontSendNotification);
            pan    .setValue (s.pan,        juce::dontSendNotification);
            pitch  .setValue (s.pitchSemis, juce::dontSendNotification);
            fine   .setValue (s.fineCents,  juce::dontSendNotification);
            attack .setValue (s.attackMs,   juce::dontSendNotification);
            release.setValue (s.releaseMs,  juce::dontSendNotification);
            start  .setValue (s.startPos,   juce::dontSendNotification);
            reverseBtn.setToggleState (s.reverse, juce::dontSendNotification);
            chokeBtn  .setToggleState (processor.getEngine().chokeGroupForPad (selectedPad) > 0,
                                       juce::dontSendNotification);
            master.setValue (processor.getEngine().getMasterGainDb(), juce::dontSendNotification);
            suppress = false;
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (1.0f);
            drawLuxuryPanel (g, bounds, 12.0f,
                              juce::Colour (0xff1a212c),
                              juce::Colour (0xff0d1118),
                              juce::Colour (0xff262e3a));

            // Header label
            const auto& slot = getDefaultSlot (selectedPad);
            g.setColour (juce::Colour (0xffe2c8a8));
            g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
            g.drawText ("PAD " + juce::String (selectedPad + 1).paddedLeft ('0', 2)
                        + "  \xc2\xb7  " + juce::String (slot.longName),
                        bounds.toNearestInt().reduced (14, 8).removeFromTop (16),
                        juce::Justification::centredLeft);

            g.setColour (juce::Colour (0x33ffa860));
            g.drawLine (bounds.getX() + 14.0f, bounds.getY() + 32.0f,
                        bounds.getRight() - 14.0f, bounds.getY() + 32.0f, 1.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 36);

            const int knobW = r.getWidth() / 4;
            const int knobH = 84;

            auto laneA = r.removeFromTop (knobH);
            r.removeFromTop (4);
            auto laneB = r.removeFromTop (knobH);
            r.removeFromTop (4);
            auto laneC = r.removeFromTop (knobH);
            r.removeFromTop (8);
            auto buttons = r.removeFromTop (32);
            r.removeFromTop (8);
            auto laneD = r.removeFromTop (knobH);

            auto layout = [knobW] (juce::Rectangle<int> lane,
                                    std::initializer_list<std::pair<juce::Slider*, juce::Label*>> items)
            {
                int i = 0;
                for (auto& kv : items)
                {
                    auto cell = juce::Rectangle<int> (lane.getX() + i * knobW, lane.getY(),
                                                       knobW, lane.getHeight());
                    auto label = cell.removeFromTop (16);
                    kv.second->setBounds (label);
                    kv.first ->setBounds (cell.reduced (4, 0));
                    ++i;
                }
            };

            layout (laneA, { { &gain, &gainLbl }, { &pan, &panLbl }, { &pitch, &pitchLbl }, { &fine, &fineLbl } });
            layout (laneB, { { &attack, &attackLbl }, { &release, &releaseLbl }, { &start, &startLbl } });

            // laneC shows nothing for now (could host extra controls later)
            juce::ignoreUnused (laneC);

            // Buttons row
            const int btnW = (buttons.getWidth() - 12) / 4;
            reverseBtn.setBounds (buttons.removeFromLeft (btnW));
            buttons.removeFromLeft (4);
            chokeBtn  .setBounds (buttons.removeFromLeft (btnW));
            buttons.removeFromLeft (4);
            loadBtn   .setBounds (buttons.removeFromLeft (btnW));
            buttons.removeFromLeft (4);
            clearBtn  .setBounds (buttons.removeFromLeft (btnW));

            // Master
            auto masterCell = laneD.removeFromRight (knobW);
            auto masterLabelArea = masterCell.removeFromTop (16);
            masterLbl.setBounds (masterLabelArea);
            master.setBounds (masterCell.reduced (4, 0));
        }

    private:
        void sliderValueChanged (juce::Slider* s) override
        {
            if (suppress) return;
            auto& padState = processor.getEngine().getPad (selectedPad).getState();
            if (s == &gain)    padState.gainDb     = (float) gain   .getValue();
            if (s == &pan)     padState.pan        = (float) pan    .getValue();
            if (s == &pitch)   padState.pitchSemis = (float) pitch  .getValue();
            if (s == &fine)    padState.fineCents  = (float) fine   .getValue();
            if (s == &attack)  padState.attackMs   = (float) attack .getValue();
            if (s == &release) padState.releaseMs  = (float) release.getValue();
            if (s == &start)   padState.startPos   = (float) start  .getValue();
            if (s == &master)  processor.getEngine().setMasterGainDb ((float) master.getValue());
        }

        void buttonClicked (juce::Button* b) override
        {
            if (suppress) return;

            if (b == &reverseBtn)
            {
                processor.getEngine().getPad (selectedPad).getState().reverse
                    = reverseBtn.getToggleState();
            }
            else if (b == &chokeBtn)
            {
                // Toggle: 0 = off, 9 = "user choke" group separate from default groups
                processor.getEngine().getPad (selectedPad).getState().chokeGroup
                    = chokeBtn.getToggleState() ? 9 : 0;
            }
            else if (b == &loadBtn)
            {
                openFileChooser();
            }
            else if (b == &clearBtn)
            {
                processor.getEngine().clearPad (selectedPad);
                refreshFromPad();
                repaint();
                if (onPadStateChanged) onPadStateChanged();
            }
        }

        void openFileChooser()
        {
            chooser = std::make_unique<juce::FileChooser> (
                "Choose audio sample for "
                  + juce::String (getDefaultSlot (selectedPad).longName),
                juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (! file.existsAsFile()) return;
                    if (processor.getEngine().loadSampleIntoPad (selectedPad, file))
                    {
                        refreshFromPad();
                        if (onPadStateChanged) onPadStateChanged();
                    }
                });
        }

        void configureKnob (juce::Slider& s, juce::Label& lbl)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
            s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x00000000));
            s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffffe6c2));
            s.setVelocityBasedMode (true);
            s.setMouseDragSensitivity (180);
            lbl.setJustificationType (juce::Justification::centred);
            lbl.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
            lbl.setColour (juce::Label::textColourId, juce::Colour (0xffd0a878));
            lbl.setInterceptsMouseClicks (false, false);
        }

        SA1AudioProcessor& processor;
        int                selectedPad = 0;
        bool               suppress    = false;

        juce::Slider gain, pan, pitch, fine, attack, release, start, master;
        juce::Label  gainLbl, panLbl, pitchLbl, fineLbl, attackLbl, releaseLbl, startLbl, masterLbl;

        juce::TextButton reverseBtn, chokeBtn, loadBtn, clearBtn;

        std::unique_ptr<juce::FileChooser> chooser;

    public:
        std::function<void()> onPadStateChanged;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadParameterPanel)
    };
}
