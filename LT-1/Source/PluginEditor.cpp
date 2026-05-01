/*
  ==============================================================================

    LT-1 — Luxury Limiter
    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace LuxColours
{
    static const juce::Colour bgTop      = juce::Colour (0xff0e1014);
    static const juce::Colour bgBottom   = juce::Colour (0xff05060a);
    static const juce::Colour panel      = juce::Colour (0xff14171d);
    static const juce::Colour panelEdge  = juce::Colour (0xff2a2f3a);
    static const juce::Colour gold       = juce::Colour (0xffd4a24c);
    static const juce::Colour goldBright = juce::Colour (0xfff4cd7a);
    static const juce::Colour text       = juce::Colour (0xffe8e8e8);
    static const juce::Colour textDim    = juce::Colour (0xff8a8e98);
    static const juce::Colour meterGreen = juce::Colour (0xff5fd17a);
    static const juce::Colour meterAmber = juce::Colour (0xffe6a93b);
    static const juce::Colour meterRed   = juce::Colour (0xffe04848);
    static const juce::Colour grColour   = juce::Colour (0xffd4a24c);
    static const juce::Colour scopeBlue  = juce::Colour (0xff7fb3ff);
}

//==============================================================================
// Helper: draw a thin gold-on-black panel with the ElegantDark feel.
static void drawLuxuryPanel (juce::Graphics& g, juce::Rectangle<float> r,
                             const juce::String& title = {})
{
    juce::ColourGradient grad (LuxColours::panel.brighter (0.04f), r.getX(), r.getY(),
                               LuxColours::panel.darker  (0.20f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);

    g.setColour (LuxColours::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    // hairline gold inset
    g.setColour (LuxColours::gold.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (3.0f), 4.0f, 0.6f);

    if (title.isNotEmpty())
    {
        g.setColour (LuxColours::gold);
        g.setFont (juce::Font (12.0f, juce::Font::plain).withExtraKerningFactor (0.18f));
        g.drawText (title.toUpperCase(), r.reduced (10.0f, 6.0f).removeFromTop (16.0f),
                    juce::Justification::topLeft, false);
    }
}

//==============================================================================
LevelMeter::LevelMeter (std::atomic<float>& src, Style s, juce::Colour accent)
    : source (src), style (s), accentColour (accent)
{
    startTimerHz (30);
}

LevelMeter::~LevelMeter() { stopTimer(); }

void LevelMeter::timerCallback()
{
    const float target = source.load();
    if (target > displayed) displayed = target;
    else                    displayed = 0.85f * displayed + 0.15f * target;

    if (peakSource != nullptr)
        peakDisplay = peakSource->load();

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    // Background trough
    g.setColour (juce::Colour (0xff0a0c10));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (LuxColours::panelEdge);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    const bool isGR = style == Style::gainReduction;

    // Map value to a 0..1 fill amount using a dB scale.
    float v = displayed;
    float fill;
    if (isGR)
    {
        // v already in dB of reduction (0..maybe 24dB)
        fill = juce::jlimit (0.0f, 1.0f, v / 24.0f);
    }
    else
    {
        const float dB = juce::Decibels::gainToDecibels (juce::jmax (v, 1.0e-6f));
        fill = juce::jlimit (0.0f, 1.0f, (dB + 60.0f) / 60.0f); // -60..0 dB
    }

    auto inner = bounds.reduced (3.0f);

    if (isGR)
    {
        // GR fills from the top down — a downward bar in classic limiter style.
        const float h = inner.getHeight() * fill;
        auto fillR = inner.withHeight (h);

        juce::ColourGradient grad (accentColour.brighter (0.4f), fillR.getX(), fillR.getY(),
                                   accentColour.darker (0.3f),  fillR.getX(), fillR.getBottom(),
                                   false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fillR, 2.0f);

        // peak-hold tick
        if (peakSource != nullptr)
        {
            const float peakFill = juce::jlimit (0.0f, 1.0f, peakDisplay / 24.0f);
            const float py = inner.getY() + inner.getHeight() * peakFill;
            g.setColour (accentColour.brighter (0.6f));
            g.drawLine (inner.getX(), py, inner.getRight(), py, 1.5f);
        }
    }
    else
    {
        // Input/output fills bottom up, with green→amber→red gradient.
        const float h = inner.getHeight() * fill;
        auto fillR = inner.withTrimmedTop (inner.getHeight() - h);

        juce::ColourGradient grad (LuxColours::meterRed,   inner.getX(), inner.getY(),
                                   LuxColours::meterGreen, inner.getX(), inner.getBottom(),
                                   false);
        grad.addColour (0.30, LuxColours::meterAmber);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fillR, 2.0f);

        // Specular sheen
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f),
                                                  fillR.getX(), fillR.getY(),
                                                  juce::Colours::white.withAlpha (0.0f),
                                                  fillR.getX(), fillR.getCentreY(), false));
        g.fillRoundedRectangle (fillR.withWidth (fillR.getWidth() * 0.5f), 2.0f);
    }

    // Tick marks (every 6 dB)
    g.setColour (LuxColours::textDim.withAlpha (0.35f));
    for (int dB = 0; dB >= -60; dB -= 6)
    {
        float pos = (dB + 60.0f) / 60.0f;
        float y = isGR
            ? inner.getY() + inner.getHeight() * juce::jlimit (0.0f, 1.0f, (-dB) / 24.0f)
            : inner.getBottom() - inner.getHeight() * pos;
        g.drawHorizontalLine ((int) y, inner.getX(), inner.getRight());
    }
}

//==============================================================================
LimiterScope::LimiterScope (LT1AudioProcessor& p)
    : processor (p),
      inSnap (LT1AudioProcessor::scopeSize, 0.0f),
      outSnap (LT1AudioProcessor::scopeSize, 0.0f),
      gainSnap (LT1AudioProcessor::scopeSize, 1.0f)
{
    startTimerHz (30);
}

LimiterScope::~LimiterScope() { stopTimer(); }

void LimiterScope::timerCallback()
{
    const int writeIdx = processor.scopeWriteIndex.load();
    const int N = LT1AudioProcessor::scopeSize;
    for (int i = 0; i < N; ++i)
    {
        const int src = (writeIdx + i) % N;
        inSnap[i]   = processor.scopeIn  [src].load();
        outSnap[i]  = processor.scopeOut [src].load();
        gainSnap[i] = processor.scopeGain[src].load();
    }
    repaint();
}

void LimiterScope::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    drawLuxuryPanel (g, bounds, "Limiter Activity");

    auto plot = bounds.reduced (12.0f, 26.0f);

    // grid
    g.setColour (LuxColours::panelEdge.withAlpha (0.7f));
    g.drawRect (plot, 1.0f);
    g.setColour (LuxColours::panelEdge.withAlpha (0.35f));
    for (int i = 1; i < 4; ++i)
    {
        const float y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
        g.drawLine (plot.getX(), y, plot.getRight(), y, 0.5f);
    }
    for (int i = 1; i < 8; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 8.0f;
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 0.5f);
    }

    const float midY = plot.getCentreY();
    const float halfH = plot.getHeight() * 0.45f;
    const int N = (int) inSnap.size();

    auto xFor = [&] (int i) { return plot.getX() + plot.getWidth() * (float) i / (float) (N - 1); };

    // 0 dBFS guide
    g.setColour (LuxColours::textDim.withAlpha (0.5f));
    g.drawLine (plot.getX(), midY, plot.getRight(), midY, 0.5f);

    // ====== Input waveform (faded) ======
    {
        juce::Path p;
        for (int i = 0; i < N; ++i)
        {
            const float y = midY - juce::jlimit (-1.0f, 1.0f, inSnap[i]) * halfH;
            if (i == 0) p.startNewSubPath (xFor (i), y);
            else        p.lineTo (xFor (i), y);
        }
        g.setColour (LuxColours::scopeBlue.withAlpha (0.5f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    // ====== Output waveform (clipped to ceiling, bright) ======
    {
        juce::Path p;
        for (int i = 0; i < N; ++i)
        {
            const float y = midY - juce::jlimit (-1.0f, 1.0f, outSnap[i]) * halfH;
            if (i == 0) p.startNewSubPath (xFor (i), y);
            else        p.lineTo (xFor (i), y);
        }
        g.setColour (LuxColours::goldBright);
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }

    // ====== Gain envelope filled area at top ======
    {
        juce::Path filled;
        filled.startNewSubPath (xFor (0), plot.getY());
        for (int i = 0; i < N; ++i)
        {
            const float reductionAmt = juce::jlimit (0.0f, 1.0f, 1.0f - gainSnap[i]); // 0 = no GR
            const float y = plot.getY() + plot.getHeight() * 0.35f * reductionAmt;
            filled.lineTo (xFor (i), y);
        }
        filled.lineTo (plot.getRight(), plot.getY());
        filled.closeSubPath();

        juce::ColourGradient grad (LuxColours::gold.withAlpha (0.35f), plot.getX(), plot.getY(),
                                   LuxColours::gold.withAlpha (0.0f),  plot.getX(), plot.getCentreY(), false);
        g.setGradientFill (grad);
        g.fillPath (filled);

        // outline
        juce::Path outline;
        for (int i = 0; i < N; ++i)
        {
            const float reductionAmt = juce::jlimit (0.0f, 1.0f, 1.0f - gainSnap[i]);
            const float y = plot.getY() + plot.getHeight() * 0.35f * reductionAmt;
            if (i == 0) outline.startNewSubPath (xFor (i), y);
            else        outline.lineTo (xFor (i), y);
        }
        g.setColour (LuxColours::gold);
        g.strokePath (outline, juce::PathStrokeType (1.0f));
    }

    // Legend
    g.setFont (11.0f);
    g.setColour (LuxColours::scopeBlue);
    g.drawText ("Input", plot.removeFromTop (14.0f).removeFromLeft (60.0f), juce::Justification::left);
    g.setColour (LuxColours::goldBright);
    g.drawText ("Output", juce::Rectangle<float> (plot.getX() + 60.0f, plot.getY() - 14.0f, 60.0f, 14.0f),
                juce::Justification::left);
    g.setColour (LuxColours::gold);
    g.drawText ("Gain Reduction", juce::Rectangle<float> (plot.getX() + 130.0f, plot.getY() - 14.0f, 110.0f, 14.0f),
                juce::Justification::left);
}

//==============================================================================
LabeledKnob::LabeledKnob (juce::AudioProcessorValueTreeState& s,
                          const juce::String& paramId,
                          const juce::String& displayName)
    : name (displayName)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
}

void LabeledKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);          // label
    slider.setBounds (r);
}

void LabeledKnob::paint (juce::Graphics& g)
{
    g.setColour (LuxColours::gold);
    g.setFont (juce::Font (12.0f, juce::Font::plain).withExtraKerningFactor (0.15f));
    g.drawText (name.toUpperCase(), getLocalBounds().removeFromTop (16),
                juce::Justification::centred, false);
}

//==============================================================================
PresetBar::PresetBar (LT1AudioProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (saveAsButton);
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (initButton);

    presetBox.setTextWhenNothingSelected ("— Preset —");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && name != processor.getPresetManager().getCurrentPresetName())
            processor.getPresetManager().loadPreset (name);
    };

    prevButton.onClick = [this] { processor.getPresetManager().loadPrevious(); };
    nextButton.onClick = [this] { processor.getPresetManager().loadNext(); };

    saveButton.onClick = [this]
    {
        auto current = processor.getPresetManager().getCurrentPresetName();
        if (current.isEmpty())
        {
            saveAsButton.triggerClick();
            return;
        }
        processor.getPresetManager().savePreset (current);
    };

    saveAsButton.onClick = [this]
    {
        auto* aw = new juce::AlertWindow ("Save Preset", "Enter a name:", juce::AlertWindow::NoIcon);
        aw->addTextEditor ("name", processor.getPresetManager().getCurrentPresetName(), {});
        aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, aw] (int result)
            {
                if (result == 1)
                {
                    auto name = aw->getTextEditorContents ("name").trim();
                    if (name.isNotEmpty())
                        processor.getPresetManager().savePreset (name);
                }
                delete aw;
            }), false);
    };

    deleteButton.onClick = [this]
    {
        auto current = processor.getPresetManager().getCurrentPresetName();
        if (current.isEmpty()) return;

        juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon,
            "Delete Preset",
            "Delete \"" + current + "\"?",
            "Delete", "Cancel", this,
            juce::ModalCallbackFunction::create ([this, current] (int r)
            {
                if (r == 1) processor.getPresetManager().deletePreset (current);
            }));
    };

    initButton.onClick = [this] { processor.getPresetManager().loadInitPreset(); };

    processor.getPresetManager().addChangeListener (this);
    refreshList();
}

PresetBar::~PresetBar()
{
    processor.getPresetManager().removeChangeListener (this);
}

void PresetBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshList();
}

void PresetBar::refreshList()
{
    presetBox.clear (juce::dontSendNotification);
    auto names = processor.getPresetManager().getPresetNames();
    int id = 1;
    for (auto& n : names)
        presetBox.addItem (n, id++);

    auto current = processor.getPresetManager().getCurrentPresetName();
    if (current.isNotEmpty())
        presetBox.setText (current, juce::dontSendNotification);
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced (4);

    initButton  .setBounds (r.removeFromRight (56).reduced (2));
    deleteButton.setBounds (r.removeFromRight (66).reduced (2));
    saveAsButton.setBounds (r.removeFromRight (90).reduced (2));
    saveButton  .setBounds (r.removeFromRight (60).reduced (2));
    nextButton  .setBounds (r.removeFromRight (28).reduced (2));
    prevButton  .setBounds (r.removeFromRight (28).reduced (2));

    r.removeFromRight (8);
    presetBox.setBounds (r.reduced (2));
}

void PresetBar::paint (juce::Graphics& g)
{
    drawLuxuryPanel (g, getLocalBounds().toFloat());
}

//==============================================================================
LT1AudioProcessorEditor::LT1AudioProcessorEditor (LT1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      presetBar (p),
      threshold (p.apvts, "threshold", "Threshold"),
      ceiling   (p.apvts, "ceiling",   "Ceiling"),
      release   (p.apvts, "release",   "Release"),
      knee      (p.apvts, "knee",      "Knee"),
      inGain    (p.apvts, "inGain",    "Input"),
      outGain   (p.apvts, "outGain",   "Output"),
      meterInL  (p.meterInL,  LevelMeter::Style::input,         LuxColours::meterGreen),
      meterInR  (p.meterInR,  LevelMeter::Style::input,         LuxColours::meterGreen),
      meterOutL (p.meterOutL, LevelMeter::Style::output,        LuxColours::meterGreen),
      meterOutR (p.meterOutR, LevelMeter::Style::output,        LuxColours::meterGreen),
      meterGR   (p.meterGR,   LevelMeter::Style::gainReduction, LuxColours::grColour),
      scope     (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (threshold);
    addAndMakeVisible (ceiling);
    addAndMakeVisible (release);
    addAndMakeVisible (knee);
    addAndMakeVisible (inGain);
    addAndMakeVisible (outGain);
    addAndMakeVisible (lookaheadToggle);
    addAndMakeVisible (autoReleaseToggle);
    addAndMakeVisible (stereoLinkToggle);
    addAndMakeVisible (bypassToggle);
    addAndMakeVisible (meterInL);
    addAndMakeVisible (meterInR);
    addAndMakeVisible (meterOutL);
    addAndMakeVisible (meterOutR);
    addAndMakeVisible (meterGR);
    addAndMakeVisible (scope);

    meterGR.setPeakSource (&p.meterGRPeak);

    lookaheadAttach   = std::make_unique<BAttach> (p.apvts, "lookahead",   lookaheadToggle);
    autoReleaseAttach = std::make_unique<BAttach> (p.apvts, "autoRelease", autoReleaseToggle);
    stereoLinkAttach  = std::make_unique<BAttach> (p.apvts, "stereoLink",  stereoLinkToggle);
    bypassAttach      = std::make_unique<BAttach> (p.apvts, "bypass",      bypassToggle);

    setResizable (false, false);
    setSize (940, 560);
}

LT1AudioProcessorEditor::~LT1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void LT1AudioProcessorEditor::paint (juce::Graphics& g)
{
    // ====== Luxury background: dark vignette with a subtle gold seam ======
    juce::ColourGradient bg (LuxColours::bgTop.brighter (0.08f), 0.0f, 0.0f,
                             LuxColours::bgBottom,                0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Brushed-metal noise overlay (very subtle)
    juce::Random rng (0xc0ffee);
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (int i = 0; i < 800; ++i)
        g.fillRect ((float) rng.nextInt (getWidth()), (float) rng.nextInt (getHeight()), 1.0f, 1.0f);

    auto r = getLocalBounds().toFloat();

    // Header band
    auto header = r.removeFromTop (54.0f).reduced (12.0f, 6.0f);
    g.setColour (LuxColours::panel.darker (0.15f));
    g.fillRoundedRectangle (header, 6.0f);
    g.setColour (LuxColours::gold.withAlpha (0.45f));
    g.drawRoundedRectangle (header, 6.0f, 1.0f);

    // Title "LT-1 / Luxury Limiter"
    g.setColour (LuxColours::goldBright);
    g.setFont (juce::Font (28.0f, juce::Font::bold).withExtraKerningFactor (0.04f));
    g.drawText ("LT-1", header.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);

    g.setColour (LuxColours::gold.withAlpha (0.85f));
    g.setFont (juce::Font (12.0f, juce::Font::italic).withExtraKerningFactor (0.25f));
    g.drawText ("LUXURY  LIMITER", header.withTrimmedLeft (76.0f), juce::Justification::centredLeft, false);

    // Subtle gold seam across the top
    g.setColour (LuxColours::gold.withAlpha (0.6f));
    g.drawLine (0.0f, 54.0f, (float) getWidth(), 54.0f, 0.6f);
}

void LT1AudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (54);                         // header

    // ===== Preset bar =====
    presetBar.setBounds (r.removeFromTop (40).reduced (12, 4));

    // ===== Main content =====
    r.reduce (12, 8);

    // Right column: meters
    auto rightCol = r.removeFromRight (220);
    {
        // GR meter takes the top half
        auto grPanel = rightCol.removeFromTop (rightCol.getHeight() / 2).reduced (4);
        auto grLabelArea = grPanel.removeFromTop (16);
        juce::ignoreUnused (grLabelArea);
        meterGR.setBounds (grPanel.reduced (60, 6));

        // I/O meters fill the bottom
        auto ioPanel = rightCol.reduced (4);
        ioPanel.removeFromTop (16);
        auto inPair  = ioPanel.removeFromLeft (ioPanel.getWidth() / 2).reduced (10, 6);
        auto outPair = ioPanel.reduced (10, 6);

        auto inL = inPair.removeFromLeft (inPair.getWidth() / 2).reduced (4, 0);
        auto inR = inPair.reduced (4, 0);
        meterInL.setBounds (inL);
        meterInR.setBounds (inR);

        auto outL = outPair.removeFromLeft (outPair.getWidth() / 2).reduced (4, 0);
        auto outR = outPair.reduced (4, 0);
        meterOutL.setBounds (outL);
        meterOutR.setBounds (outR);
    }

    r.removeFromRight (6);

    // Left column: scope on top, knobs + toggles below
    auto scopeArea = r.removeFromTop (220);
    scope.setBounds (scopeArea.reduced (4));

    auto knobsArea = r.reduced (4);
    auto togglesArea = knobsArea.removeFromBottom (40);

    // 6 knobs in a row
    const int n = 6;
    const int kw = knobsArea.getWidth() / n;
    threshold.setBounds (knobsArea.removeFromLeft (kw).reduced (4));
    ceiling  .setBounds (knobsArea.removeFromLeft (kw).reduced (4));
    release  .setBounds (knobsArea.removeFromLeft (kw).reduced (4));
    knee     .setBounds (knobsArea.removeFromLeft (kw).reduced (4));
    inGain   .setBounds (knobsArea.removeFromLeft (kw).reduced (4));
    outGain  .setBounds (knobsArea.removeFromLeft (kw).reduced (4));

    // Toggles
    const int tw = togglesArea.getWidth() / 4;
    lookaheadToggle  .setBounds (togglesArea.removeFromLeft (tw).reduced (8, 6));
    autoReleaseToggle.setBounds (togglesArea.removeFromLeft (tw).reduced (8, 6));
    stereoLinkToggle .setBounds (togglesArea.removeFromLeft (tw).reduced (8, 6));
    bypassToggle     .setBounds (togglesArea.reduced (8, 6));
}
