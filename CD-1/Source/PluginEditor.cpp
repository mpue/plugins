/*
  ==============================================================================

    PluginEditor.cpp
    CD-1 Cinematic Drums

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Per-drum accent colour palette (matches DrumScope mood: cool blue → warm
    // amber as we move from sub to crack).
    juce::Colour drumAccent (int idx)
    {
        switch (idx)
        {
            case cd1::Boom:  return juce::Colour (0xff4d9eff); // cinematic blue
            case cd1::Hit:   return juce::Colour (0xff7be4ff); // cyan
            case cd1::Crack: return juce::Colour (0xffffaa55); // warm amber
            case cd1::Sub:   return juce::Colour (0xffb486ff); // violet
            default:         return juce::Colour (0xff8aa6c8);
        }
    }
}

//==============================================================================
CD1AudioProcessorEditor::CD1AudioProcessorEditor (CD1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetBar (p.presetManager)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (scope);

    scope.onPadClicked = [this] (int idx, float vel)
    {
        audioProcessor.requestAudition (idx, vel);
    };

    // ---- per-drum strips ----
    const char* prefixes[] = { "boom", "hit", "crack", "sub" };
    for (int d = 0; d < cd1::NumDrums; ++d)
    {
        auto& s = drumStrips[d];
        s.name   = cd1::drumName (d);
        s.accent = drumAccent (d);

        configureKnob (s.tune,  "TUNE",  s.accent);
        configureKnob (s.decay, "DECAY", s.accent);
        configureKnob (s.level, "LEVEL", s.accent);
        configureKnob (s.pan,   "PAN",   s.accent);

        s.auditionBtn.setButtonText ("PLAY");
        s.auditionBtn.onClick = [this, d] { audioProcessor.requestAudition (d, 0.95f); };
        addAndMakeVisible (s.auditionBtn);

        const juce::String pfx (prefixes[d]);
        tuneAtt [d] = std::make_unique<SAtt> (audioProcessor.apvts, pfx + "Tune",  s.tune .slider);
        decayAtt[d] = std::make_unique<SAtt> (audioProcessor.apvts, pfx + "Decay", s.decay.slider);
        levelAtt[d] = std::make_unique<SAtt> (audioProcessor.apvts, pfx + "Level", s.level.slider);
        panAtt  [d] = std::make_unique<SAtt> (audioProcessor.apvts, pfx + "Pan",   s.pan  .slider);
    }

    // ---- master macros ----
    configureKnob (depth,  "DEPTH",  juce::Colour (0xff4d9eff));
    configureKnob (impact, "IMPACT", juce::Colour (0xffffaa55));
    configureKnob (air,    "AIR",    juce::Colour (0xff7be4ff));
    configureKnob (drive,  "DRIVE",  juce::Colour (0xffff7755));
    configureKnob (width,  "WIDTH",  juce::Colour (0xff7be4ff));
    configureKnob (size,   "SIZE",   juce::Colour (0xffb486ff));
    configureKnob (tone,   "TONE",   juce::Colour (0xff8aa6c8));
    configureKnob (output, "OUTPUT", juce::Colour (0xffe8f3ff));

    auto& apvts = audioProcessor.apvts;
    depthAtt  = std::make_unique<SAtt> (apvts, "depth",  depth .slider);
    impactAtt = std::make_unique<SAtt> (apvts, "impact", impact.slider);
    airAtt    = std::make_unique<SAtt> (apvts, "air",    air   .slider);
    driveAtt  = std::make_unique<SAtt> (apvts, "drive",  drive .slider);
    widthAtt  = std::make_unique<SAtt> (apvts, "width",  width .slider);
    sizeAtt   = std::make_unique<SAtt> (apvts, "size",   size  .slider);
    toneAtt   = std::make_unique<SAtt> (apvts, "tone",   tone  .slider);
    outputAtt = std::make_unique<SAtt> (apvts, "output", output.slider);

    // ---- reverb internals ----
    configureKnob (rvSize, "RV SIZE", juce::Colour (0xffb486ff));
    configureKnob (rvDamp, "DAMPING", juce::Colour (0xffb486ff));
    configureKnob (rvLow,  "LOW CUT", juce::Colour (0xffb486ff));

    rvSizeAtt = std::make_unique<SAtt> (apvts, "rvSize", rvSize.slider);
    rvDampAtt = std::make_unique<SAtt> (apvts, "rvDamp", rvDamp.slider);
    rvLowAtt  = std::make_unique<SAtt> (apvts, "rvLow",  rvLow .slider);

    displayBufferL.assign (1024, 0.0f);
    displayBufferR.assign (1024, 0.0f);

    setSize (1080, 760);
    startTimerHz (45);
}

CD1AudioProcessorEditor::~CD1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void CD1AudioProcessorEditor::configureKnob (KnobControl& k, const juce::String& titleText,
                                              juce::Colour accent)
{
    addAndMakeVisible (k.title);
    k.title.setText (titleText, juce::dontSendNotification);
    k.title.setJustificationType (juce::Justification::centred);
    k.title.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    k.title.setColour (juce::Label::textColourId, accent.withAlpha (0.9f));
    k.title.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k.slider);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f,
                                   true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour (0x00000000));
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x00000000));
    k.slider.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffd6e6ff));
    k.slider.setColour (juce::Slider::rotarySliderFillColourId,  accent);
    k.slider.setColour (juce::Slider::thumbColourId,             accent);
    k.slider.setVelocityBasedMode (true);
    k.slider.setVelocityModeParameters (0.7, 1, 0.09, false);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setDoubleClickReturnValue (false, 0.0);
}

void CD1AudioProcessorEditor::layoutKnob (KnobControl& k, juce::Rectangle<int> slot)
{
    auto title = slot.removeFromTop (16);
    k.title.setBounds (title);
    k.slider.setBounds (slot);
}

//==============================================================================
void CD1AudioProcessorEditor::timerCallback()
{
    // pull triggers and forward to scope
    CD1AudioProcessor::TriggerEvent ev;
    while (audioProcessor.consumeTriggerEvent (ev))
        scope.notifyTrigger (ev.drumIdx, ev.velocity);

    // pull audio for waveform
    int got = 0;
    audioProcessor.copyDisplayAudio (displayBufferL.data(), displayBufferR.data(),
                                      got, (int) displayBufferL.size());
    if (got > 0)
        scope.pushAudio (displayBufferL.data(), displayBufferR.data(), got);

    // master peak
    float pkL = 0.0f, pkR = 0.0f;
    audioProcessor.getMasterPeak (pkL, pkR);
    scope.setMasterPeak (pkL, pkR);
}

//==============================================================================
void CD1AudioProcessorEditor::drawSectionPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                                 const juce::String& title,
                                                 juce::Colour accent) const
{
    juce::ColourGradient pbg (juce::Colour (0xff1a212c), r.getCentreX(), (float) r.getY(),
                               juce::Colour (0xff0d1118), r.getCentreX(), (float) r.getBottom(), false);
    g.setGradientFill (pbg);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff262e3a));
    g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);

    g.setColour (accent.withAlpha (0.18f));
    g.drawLine ((float) r.getX() + 14.0f, (float) r.getY() + 1.0f,
                (float) r.getRight() - 14.0f, (float) r.getY() + 1.0f, 1.0f);

    g.setColour (accent.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (title,
                juce::Rectangle<int> (r.getX() + 14, r.getY() + 6, 240, 14),
                juce::Justification::centredLeft);
}

void CD1AudioProcessorEditor::drawHeader (juce::Graphics& g, juce::Rectangle<int> header)
{
    juce::ColourGradient hbg (juce::Colour (0xff1d2533), header.getCentreX(), (float) header.getY(),
                               juce::Colour (0xff10141d), header.getCentreX(), (float) header.getBottom(), false);
    g.setGradientFill (hbg);
    g.fillRoundedRectangle (header.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff262e3a));
    g.drawRoundedRectangle (header.toFloat(), 8.0f, 1.0f);

    auto title = header.reduced (16, 6);

    g.setColour (juce::Colour (0xff8fc7ff));
    g.setFont (juce::Font (juce::FontOptions (24.0f).withStyle ("Bold")));
    g.drawText ("CD-1",
                juce::Rectangle<int> (title.getX(), title.getY(), 80, 28),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff4d9eff));
    g.fillRect (juce::Rectangle<int> (title.getX() + 78, title.getY() + 6, 1, 22));

    g.setColour (juce::Colour (0xffb6c8e2));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("CINEMATIC",
                juce::Rectangle<int> (title.getX() + 90, title.getY() + 4, 80, 12),
                juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff8aa6c8));
    g.drawText ("DRUMS",
                juce::Rectangle<int> (title.getX() + 90, title.getY() + 16, 80, 12),
                juce::Justification::centredLeft);
}

void CD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // ---- main background ----
    juce::ColourGradient bg (juce::Colour (0xff141a23), 0.0f, 0.0f,
                              juce::Colour (0xff080b10), 0.0f, (float) bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    {
        juce::ColourGradient sheen (juce::Colour (0x14a8c8ff), bounds.getWidth() * 0.5f, -120.0f,
                                     juce::Colour (0x00000000), bounds.getWidth() * 0.5f, 280.0f, true);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    // ---- header ----
    auto header = bounds.withHeight (62).reduced (12, 8);
    drawHeader (g, header);

    // ---- panels ----
    if (! drumPanelBounds.isEmpty())
        drawSectionPanel (g, drumPanelBounds,  "DRUM KIT  ·  TUNE  ·  DECAY  ·  LEVEL  ·  PAN",
                          juce::Colour (0xff8fc7ff));

    if (! macroPanelBounds.isEmpty())
        drawSectionPanel (g, macroPanelBounds, "MASTER  ·  CINEMATIC MACROS",
                          juce::Colour (0xffffaa55));

    if (! reverbPanelBounds.isEmpty())
        drawSectionPanel (g, reverbPanelBounds, "ROOM  ·  CINEMATIC REVERB",
                          juce::Colour (0xffb486ff));

    // ---- per-drum sub-panels (drawn over the drum kit panel) ----
    if (! drumPanelBounds.isEmpty())
    {
        for (int d = 0; d < cd1::NumDrums; ++d)
        {
            const auto& s = drumStrips[d];
            const auto r = s.bounds;
            if (r.isEmpty()) continue;

            // gradient header band per strip
            auto hb = juce::Rectangle<float> (r.toFloat()).withHeight (22.0f);
            juce::ColourGradient hbg (s.accent.withAlpha (0.18f), hb.getCentreX(), hb.getY(),
                                       s.accent.withAlpha (0.0f), hb.getCentreX(), hb.getBottom(), false);
            g.setGradientFill (hbg);
            g.fillRoundedRectangle (hb, 6.0f);

            g.setColour (s.accent.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
            g.drawText (s.name,
                        juce::Rectangle<int> (r.getX(), r.getY() + 2, r.getWidth(), 20),
                        juce::Justification::centred);

            // separator line between drums
            if (d < cd1::NumDrums - 1)
            {
                g.setColour (juce::Colour (0xff262e3a));
                g.drawLine ((float) r.getRight() + 2.0f, (float) r.getY() + 24.0f,
                            (float) r.getRight() + 2.0f, (float) r.getBottom() - 6.0f, 0.6f);
            }
        }
    }
}

void CD1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ---- header ----
    auto header = bounds.removeFromTop (62).reduced (12, 8);
    {
        auto h = header;
        h.removeFromLeft (190);
        presetBar.setBounds (h.reduced (4, 0));
    }

    bounds.removeFromTop (10);

    // ---- scope (visualizer) ----
    auto scopeArea = bounds.removeFromTop (270).reduced (12, 0);
    scope.setBounds (scopeArea);

    bounds.removeFromTop (12);

    // ---- bottom area: drum kit panel (left) + master macros + reverb (right) ----
    auto bottom = bounds.reduced (12, 0);
    bottom.removeFromBottom (12);

    const int gap     = 10;
    const int totalW  = bottom.getWidth();
    const int drumW   = (int) (totalW * 0.62f);
    auto drumArea     = bottom.removeFromLeft (drumW);
    bottom.removeFromLeft (gap);
    auto rightCol     = bottom;

    drumPanelBounds = drumArea;

    // ---- drum strips inside drum panel ----
    auto innerDrum = drumArea.reduced (12, 24);
    innerDrum.removeFromTop (4);

    const int numDrums = cd1::NumDrums;
    const int stripGap = 6;
    const int stripW   = (innerDrum.getWidth() - stripGap * (numDrums - 1)) / numDrums;

    for (int d = 0; d < numDrums; ++d)
    {
        auto& s = drumStrips[d];
        auto stripBounds = juce::Rectangle<int> (innerDrum.getX() + (stripW + stripGap) * d,
                                                  innerDrum.getY(),
                                                  stripW,
                                                  innerDrum.getHeight());
        s.bounds = stripBounds;

        auto inside = stripBounds.reduced (4, 0);
        inside.removeFromTop (26); // sub-panel header

        // PLAY button at top
        auto playRow = inside.removeFromTop (26).reduced (8, 2);
        s.auditionBtn.setBounds (playRow);
        inside.removeFromTop (4);

        // 4 knobs in a 2x2 grid
        const int rowH    = inside.getHeight() / 2;
        auto rowTop       = inside.removeFromTop (rowH);
        auto rowBot       = inside;
        const int knobW   = rowTop.getWidth() / 2;

        layoutKnob (s.tune,  juce::Rectangle<int> (rowTop.getX(),               rowTop.getY(), knobW, rowH).reduced (2));
        layoutKnob (s.decay, juce::Rectangle<int> (rowTop.getX() + knobW,       rowTop.getY(), knobW, rowH).reduced (2));
        layoutKnob (s.level, juce::Rectangle<int> (rowBot.getX(),               rowBot.getY(), knobW, rowH).reduced (2));
        layoutKnob (s.pan,   juce::Rectangle<int> (rowBot.getX() + knobW,       rowBot.getY(), knobW, rowH).reduced (2));
    }

    // ---- right column: macros (top) + reverb (bottom) ----
    const int macroH = (int) (rightCol.getHeight() * 0.62f);
    auto macroArea   = rightCol.removeFromTop (macroH);
    rightCol.removeFromTop (gap);
    auto reverbArea  = rightCol;

    macroPanelBounds  = macroArea;
    reverbPanelBounds = reverbArea;

    // ---- master macros: 4×2 grid ----
    {
        auto inner = macroArea.reduced (12, 24);
        inner.removeFromTop (4);

        const int rows = 2, cols = 4;
        const int rowH = inner.getHeight() / rows;
        const int colW = inner.getWidth()  / cols;

        KnobControl* macros[2][4] = {
            { &depth,  &impact, &air,    &drive  },
            { &width,  &size,   &tone,   &output }
        };

        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                auto slot = juce::Rectangle<int> (inner.getX() + c * colW,
                                                    inner.getY() + r * rowH,
                                                    colW, rowH).reduced (4);
                layoutKnob (*macros[r][c], slot);
            }
    }

    // ---- reverb: 3 knobs in a row ----
    {
        auto inner = reverbArea.reduced (12, 24);
        inner.removeFromTop (2);

        const int colW = inner.getWidth() / 3;
        layoutKnob (rvSize, juce::Rectangle<int> (inner.getX() + 0 * colW, inner.getY(), colW, inner.getHeight()).reduced (4));
        layoutKnob (rvDamp, juce::Rectangle<int> (inner.getX() + 1 * colW, inner.getY(), colW, inner.getHeight()).reduced (4));
        layoutKnob (rvLow,  juce::Rectangle<int> (inner.getX() + 2 * colW, inner.getY(), colW, inner.getHeight()).reduced (4));
    }
}
