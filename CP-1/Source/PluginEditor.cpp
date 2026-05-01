/*
  ==============================================================================
    CP-1 Compressor — Editor implementation
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kEditorWidth  = 880;
    constexpr int kEditorHeight = 480;

    constexpr int kHeaderHeight = 56;
    constexpr int kKnobRowHeight = 200;
    constexpr int kPad = 14;

    const juce::Colour kAccent      { 0xff4d9eff };
    const juce::Colour kAccentDim   { 0xaa4d9eff };
    const juce::Colour kBgTop       { 0xff222630 };
    const juce::Colour kBgBottom    { 0xff141519 };
    const juce::Colour kPanel       { 0xff1d1f24 };
    const juce::Colour kSeparator   { 0xff2a2d33 };
    const juce::Colour kTextFaded   { 0xff8f95a0 };
    const juce::Colour kTextBright  { 0xffe8e8e8 };
}

//==============================================================================
//  GR meter
//==============================================================================
void CP1AudioProcessorEditor::GRMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = 4.0f;

    // Background trough
    g.setColour (juce::Colour (0xff0d0f12));
    g.fillRoundedRectangle (bounds, corner);

    // Frame
    g.setColour (kSeparator);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    auto inner = bounds.reduced (3.0f);

    // Tick marks every 3 dB
    g.setColour (juce::Colour (0xff404040));
    for (int dB = 0; dB <= (int) maxDb; dB += 3)
    {
        const float t = (float) dB / maxDb;
        const float y = inner.getY() + inner.getHeight() * t;
        const float w = (dB % 6 == 0) ? 7.0f : 4.0f;
        g.drawLine (inner.getX(),     y, inner.getX() + w, y, 1.0f);
        g.drawLine (inner.getRight() - w, y, inner.getRight(), y, 1.0f);
    }

    // GR fill (top down, more reduction = longer bar)
    const float gr = juce::jlimit (0.0f, maxDb, grDb);
    if (gr > 0.001f)
    {
        const float t = gr / maxDb;
        const float h = inner.getHeight() * t;
        auto fill = juce::Rectangle<float> (inner.getX() + 5.0f, inner.getY(),
                                            inner.getWidth() - 10.0f, h);

        // Gradient from bright accent at bottom to warm orange when heavy reduction
        juce::ColourGradient grad (kAccent, fill.getCentreX(), fill.getBottom(),
                                    juce::Colour (0xffff7a3d), fill.getCentreX(), fill.getY(),
                                    false);
        grad.addColour (0.55, juce::Colour (0xffffd24d));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);

        // Subtle highlight
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (fill.withWidth (fill.getWidth() * 0.35f), 2.0f);
    }

    // dB readout at bottom
    g.setColour (kTextBright);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText (juce::String (-grDb, 1) + " dB",
                getLocalBounds().removeFromBottom (18),
                juce::Justification::centred);
}

//==============================================================================
//  Level meter (vertical)
//==============================================================================
void CP1AudioProcessorEditor::LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = 4.0f;

    g.setColour (juce::Colour (0xff0d0f12));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (kSeparator);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    auto inner = bounds.reduced (3.0f);

    const float lvl = juce::jlimit (minDb, maxDb, levelDb);
    const float t   = (lvl - minDb) / (maxDb - minDb);

    if (t > 0.0f)
    {
        const float h = inner.getHeight() * t;
        auto fill = juce::Rectangle<float> (inner.getX() + 3.0f,
                                            inner.getBottom() - h,
                                            inner.getWidth() - 6.0f,
                                            h);

        // Gradient: blue (low) → green-ish → yellow → red (clip)
        juce::ColourGradient grad (juce::Colour (0xff4d9eff), fill.getCentreX(), fill.getBottom(),
                                    juce::Colour (0xffff5050), fill.getCentreX(), inner.getY(),
                                    false);
        grad.addColour (0.55, juce::Colour (0xff8eff7e));
        grad.addColour (0.80, juce::Colour (0xffffd24d));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);
    }

    // 0 dB tick reference
    {
        const float zeroT = (0.0f - minDb) / (maxDb - minDb);
        const float y = inner.getBottom() - inner.getHeight() * zeroT;
        g.setColour (juce::Colour (0xffff7a3d).withAlpha (0.65f));
        g.drawLine (inner.getX(), y, inner.getRight(), y, 1.0f);
    }
}

//==============================================================================
//  Editor
//==============================================================================
CP1AudioProcessorEditor::CP1AudioProcessorEditor (CP1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    // ---------- Header ----------
    titleLabel.setText ("CP-1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font ("Arial", 28.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, kTextBright);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Compressor", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font ("Arial", 16.0f, juce::Font::plain));
    subtitleLabel.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (subtitleLabel);

    bypassBtn.setButtonText ("BYPASS");
    bypassBtn.setClickingTogglesState (true);
    addAndMakeVisible (bypassBtn);

    // ---------- Preset bar ----------
    presetPrevBtn.setButtonText ("<");
    presetPrevBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff1c2438));
    presetPrevBtn.setColour (juce::TextButton::textColourOffId, kTextBright);
    presetPrevBtn.onClick = [this] {
        proc.presetManager.loadPreviousPreset();
        updatePresetBox();
    };
    addAndMakeVisible (presetPrevBtn);

    presetNextBtn.setButtonText (">");
    presetNextBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff1c2438));
    presetNextBtn.setColour (juce::TextButton::textColourOffId, kTextBright);
    presetNextBtn.onClick = [this] {
        proc.presetManager.loadNextPreset();
        updatePresetBox();
    };
    addAndMakeVisible (presetNextBtn);

    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1c2438));
    presetBox.setColour (juce::ComboBox::textColourId,       kTextBright);
    presetBox.setColour (juce::ComboBox::outlineColourId,    kSeparator);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this] {
        int id = presetBox.getSelectedId();
        if (id > 0)
        {
            proc.presetManager.loadPreset (id - 1);
            updatePresetBox();
        }
    };
    addAndMakeVisible (presetBox);

    presetSaveBtn.setButtonText ("SAVE");
    presetSaveBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff1c2438));
    presetSaveBtn.setColour (juce::TextButton::textColourOffId, kAccent);
    presetSaveBtn.onClick = [this] {
        auto currentName = proc.presetManager.getPresetName (proc.presetManager.getCurrentPresetIndex());
        if (proc.presetManager.isFactoryPreset (proc.presetManager.getCurrentPresetIndex()))
            currentName = "My Preset";

        auto* dlg = new juce::AlertWindow ("Save Preset", "Enter preset name:",
                                            juce::MessageBoxIconType::NoIcon, this);
        dlg->addTextEditor ("name", currentName);
        dlg->addButton ("Save",   1);
        dlg->addButton ("Cancel", 0);
        dlg->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, dlg] (int result) {
                if (result == 1)
                {
                    auto name = dlg->getTextEditorContents ("name");
                    if (name.isNotEmpty())
                    {
                        proc.presetManager.saveUserPreset (name);
                        populatePresetBox();
                        updatePresetBox();
                    }
                }
            }), true);
    };
    addAndMakeVisible (presetSaveBtn);

    presetDeleteBtn.setButtonText ("DEL");
    presetDeleteBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff1c2438));
    presetDeleteBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff7a3d));
    presetDeleteBtn.onClick = [this] {
        int idx = proc.presetManager.getCurrentPresetIndex();
        if (! proc.presetManager.isFactoryPreset (idx))
        {
            proc.presetManager.deleteUserPreset (idx);
            populatePresetBox();
            updatePresetBox();
        }
    };
    addAndMakeVisible (presetDeleteBtn);

    populatePresetBox();
    updatePresetBox();

    // ---------- Knobs ----------
    styleRotary (thresholdKnob);
    styleRotary (ratioKnob);
    styleRotary (attackKnob);
    styleRotary (releaseKnob);
    styleRotary (kneeKnob);
    styleRotary (makeupKnob);
    styleRotary (mixKnob);
    styleRotary (scHpfKnob);

    addAndMakeVisible (thresholdKnob);
    addAndMakeVisible (ratioKnob);
    addAndMakeVisible (attackKnob);
    addAndMakeVisible (releaseKnob);
    addAndMakeVisible (kneeKnob);
    addAndMakeVisible (makeupKnob);
    addAndMakeVisible (mixKnob);
    addAndMakeVisible (scHpfKnob);

    styleCaption (thresholdLbl, "THRESHOLD");
    styleCaption (ratioLbl,     "RATIO");
    styleCaption (attackLbl,    "ATTACK");
    styleCaption (releaseLbl,   "RELEASE");
    styleCaption (kneeLbl,      "KNEE");
    styleCaption (makeupLbl,    "MAKEUP");
    styleCaption (mixLbl,       "MIX");
    styleCaption (scHpfLbl,     "SC HPF");

    addAndMakeVisible (thresholdLbl);
    addAndMakeVisible (ratioLbl);
    addAndMakeVisible (attackLbl);
    addAndMakeVisible (releaseLbl);
    addAndMakeVisible (kneeLbl);
    addAndMakeVisible (makeupLbl);
    addAndMakeVisible (mixLbl);
    addAndMakeVisible (scHpfLbl);

    // Tooltips for comfort
    thresholdKnob.setTooltip ("Threshold — Pegel ab dem komprimiert wird");
    ratioKnob    .setTooltip ("Ratio — Verhältnis der Pegelreduktion");
    attackKnob   .setTooltip ("Attack — wie schnell der Kompressor zugreift");
    releaseKnob  .setTooltip ("Release — wie schnell er wieder loslässt");
    kneeKnob     .setTooltip ("Knee — weicher Übergang um den Threshold");
    makeupKnob   .setTooltip ("Makeup Gain — Pegelausgleich am Ausgang");
    mixKnob      .setTooltip ("Dry/Wet Mix — Parallelkompression");
    scHpfKnob    .setTooltip ("Sidechain Hochpassfilter");

    // ---------- Sidechain group ----------
    scGroup.setText ("SIDECHAIN");
    addAndMakeVisible (scGroup);

    stylePillToggle (extScBtn,    "EXT INPUT");
    stylePillToggle (scListenBtn, "LISTEN");
    addAndMakeVisible (extScBtn);
    addAndMakeVisible (scListenBtn);

    extScBtn   .setTooltip ("Externen Sidechain-Eingang verwenden (statt internem Signal)");
    scListenBtn.setTooltip ("Sidechain-Signal abhören (zum Einstellen des HPF)");

    scStatusLbl.setText ("Ext: not connected", juce::dontSendNotification);
    scStatusLbl.setFont (juce::Font (11.0f));
    scStatusLbl.setColour (juce::Label::textColourId, kTextFaded);
    scStatusLbl.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (scStatusLbl);

    // ---------- Global / detector group ----------
    ctrlGroup.setText ("DETECTOR");
    addAndMakeVisible (ctrlGroup);

    detectorBox.addItem ("Peak", 1);
    detectorBox.addItem ("RMS",  2);
    detectorBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (detectorBox);

    detectorLbl.setText ("MODE", juce::dontSendNotification);
    detectorLbl.setFont (juce::Font (11.0f, juce::Font::bold));
    detectorLbl.setColour (juce::Label::textColourId, kTextFaded);
    detectorLbl.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (detectorLbl);

    stylePillToggle (stereoLinkBtn, "STEREO LINK");
    stylePillToggle (autoRelBtn,    "AUTO RELEASE");
    addAndMakeVisible (stereoLinkBtn);
    addAndMakeVisible (autoRelBtn);

    detectorBox  .setTooltip ("Detektor-Modus: Peak (schnell) oder RMS (musikalisch)");
    stereoLinkBtn.setTooltip ("Stereo Link — gleiche Pegelreduktion auf L/R für stabiles Stereobild");
    autoRelBtn   .setTooltip ("Auto Release — programmabhängige, musikalische Loslasszeit");

    // ---------- Meters group ----------
    meterGroup.setText ("METERING");
    addAndMakeVisible (meterGroup);

    addAndMakeVisible (grMeter);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    auto setupMeterLbl = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (10.5f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, kTextFaded);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupMeterLbl (grMeterLbl, "GR");
    setupMeterLbl (inMeterLbl, "IN");
    setupMeterLbl (outMeterLbl,"OUT");

    // ---------- Parameter attachments ----------
    auto& vts = proc.apvts;
    thrAtt    = std::make_unique<SliderAtt>(vts, "threshold", thresholdKnob);
    ratioAtt  = std::make_unique<SliderAtt>(vts, "ratio",     ratioKnob);
    atkAtt    = std::make_unique<SliderAtt>(vts, "attack",    attackKnob);
    relAtt    = std::make_unique<SliderAtt>(vts, "release",   releaseKnob);
    kneeAtt   = std::make_unique<SliderAtt>(vts, "knee",      kneeKnob);
    makeupAtt = std::make_unique<SliderAtt>(vts, "makeup",    makeupKnob);
    mixAtt    = std::make_unique<SliderAtt>(vts, "mix",       mixKnob);
    scHpfAtt  = std::make_unique<SliderAtt>(vts, "scHpf",     scHpfKnob);

    bypassAtt    = std::make_unique<ButtonAtt>(vts, "bypass",      bypassBtn);
    extScAtt     = std::make_unique<ButtonAtt>(vts, "extSc",       extScBtn);
    scListenAtt  = std::make_unique<ButtonAtt>(vts, "scListen",    scListenBtn);
    stereoLinkAtt= std::make_unique<ButtonAtt>(vts, "stereoLink",  stereoLinkBtn);
    autoRelAtt   = std::make_unique<ButtonAtt>(vts, "autoRelease", autoRelBtn);

    detAtt       = std::make_unique<ComboAtt>(vts, "detector", detectorBox);

    setSize (kEditorWidth, kEditorHeight);
    setResizable (false, false);

    startTimerHz (30);
}

CP1AudioProcessorEditor::~CP1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void CP1AudioProcessorEditor::styleRotary (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, kTextBright);
    s.setVelocityModeParameters (0.7, 1, 0.09, false);
    s.setDoubleClickReturnValue (true, s.getDoubleClickReturnValue());
    s.setMouseDragSensitivity (180);
}

void CP1AudioProcessorEditor::styleCaption (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::Font (11.5f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, kTextFaded);
    l.setJustificationType (juce::Justification::centred);
}

void CP1AudioProcessorEditor::stylePillToggle (juce::TextButton& b, const juce::String& text)
{
    b.setButtonText (text);
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1c2438));
    b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a52a8));
    b.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff8b9bb5));
    b.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
}

//==============================================================================
void CP1AudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient bg (kBgTop, 0.0f, 0.0f,
                             kBgBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Header strip with subtle accent line at the bottom
    auto header = getLocalBounds().removeFromTop (kHeaderHeight);
    g.setColour (juce::Colour (0xff10131a).withAlpha (0.55f));
    g.fillRect (header);

    // Accent underline
    g.setColour (kAccent.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<int> (0, kHeaderHeight - 1, getWidth(), 1));

    // Knob row "panel" (subtle inset matches resized() layout exactly)
    auto knobArea = juce::Rectangle<int> (kPad, kHeaderHeight + kPad,
                                          getWidth() - kPad * 2,
                                          kKnobRowHeight - 2 * kPad);
    g.setColour (kPanel);
    g.fillRoundedRectangle (knobArea.toFloat(), 8.0f);
    g.setColour (kSeparator);
    g.drawRoundedRectangle (knobArea.toFloat(), 8.0f, 1.0f);

    // Soft inner glow at the top of the knob panel
    {
        auto innerHeader = knobArea.removeFromTop (28).toFloat();
        juce::ColourGradient innerGrad (
            juce::Colours::white.withAlpha (0.04f), innerHeader.getCentreX(), innerHeader.getY(),
            juce::Colours::transparentBlack,        innerHeader.getCentreX(), innerHeader.getBottom(),
            false);
        g.setGradientFill (innerGrad);
        g.fillRoundedRectangle (innerHeader, 6.0f);
    }
}

//==============================================================================
void CP1AudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // ---------- Header ----------
    auto header = area.removeFromTop (kHeaderHeight).reduced (kPad, 8);

    // Title block on the left
    auto titleBlock = header.removeFromLeft (180);
    titleLabel.setBounds (titleBlock.removeFromLeft (70));
    subtitleLabel.setBounds (titleBlock.translated (-6, 4));

    // Bypass on the right
    bypassBtn.setBounds (header.removeFromRight (90).reduced (0, 4));
    header.removeFromRight (8);

    // Preset controls in the centre
    auto presetArea = header.reduced (8, 2);
    presetPrevBtn  .setBounds (presetArea.removeFromLeft (30));
    presetArea.removeFromLeft (3);
    presetNextBtn  .setBounds (presetArea.removeFromLeft (30));
    presetArea.removeFromLeft (6);
    presetDeleteBtn.setBounds (presetArea.removeFromRight (46));
    presetArea.removeFromRight (4);
    presetSaveBtn  .setBounds (presetArea.removeFromRight (52));
    presetArea.removeFromRight (6);
    presetBox      .setBounds (presetArea);

    // ---------- Knob row ----------
    auto knobs = area.removeFromTop (kKnobRowHeight).reduced (kPad, kPad);

    // We display 8 cells: 7 main knobs + (later) we keep the last one for SC HPF
    // Actually 7 main knobs only — SC HPF lives in the sidechain group below.
    const int numKnobs = 7;
    juce::Slider* knobs7[numKnobs] = {
        &thresholdKnob, &ratioKnob, &attackKnob, &releaseKnob,
        &kneeKnob, &makeupKnob, &mixKnob
    };
    juce::Label*  caps7 [numKnobs] = {
        &thresholdLbl, &ratioLbl, &attackLbl, &releaseLbl,
        &kneeLbl, &makeupLbl, &mixLbl
    };

    const int cellW = knobs.getWidth() / numKnobs;
    for (int i = 0; i < numKnobs; ++i)
    {
        auto cell = juce::Rectangle<int> (knobs.getX() + i * cellW, knobs.getY(),
                                          cellW, knobs.getHeight());
        cell.reduce (4, 4);

        auto cap = cell.removeFromTop (16);
        caps7[i]->setBounds (cap);

        knobs7[i]->setBounds (cell);
    }

    // ---------- Bottom panel ----------
    auto bottom = area.reduced (kPad, kPad);

    // Three columns: SC | DETECTOR | METERS, with weights 36 / 28 / 36
    const int totalW = bottom.getWidth();
    const int scW    = (int) (totalW * 0.36f);
    const int ctrlW  = (int) (totalW * 0.28f);
    const int meterW = totalW - scW - ctrlW - 2 * kPad;

    auto scBox    = bottom.removeFromLeft (scW);
    bottom.removeFromLeft (kPad);
    auto ctrlBox  = bottom.removeFromLeft (ctrlW);
    bottom.removeFromLeft (kPad);
    auto meterBox = bottom.withWidth (meterW);

    // ---- Sidechain group ----
    scGroup.setBounds (scBox);
    auto scInner = scBox.reduced (12, 22);

    // Status label under group title
    scStatusLbl.setBounds (scInner.removeFromTop (16));

    // HPF knob on the left
    auto hpfArea = scInner.removeFromLeft ((int) (scInner.getWidth() * 0.45f));
    {
        auto cap = hpfArea.removeFromTop (16);
        scHpfLbl.setBounds (cap);
        scHpfKnob.setBounds (hpfArea.reduced (4, 0));
    }

    // EXT INPUT and LISTEN toggles stacked on the right
    scInner.removeFromLeft (8);
    auto togglesCol = scInner;
    const int btnH = 34;
    const int gap = 8;

    extScBtn   .setBounds (togglesCol.removeFromTop (btnH));
    togglesCol.removeFromTop (gap);
    scListenBtn.setBounds (togglesCol.removeFromTop (btnH));

    // ---- Detector group ----
    ctrlGroup.setBounds (ctrlBox);
    auto cInner = ctrlBox.reduced (12, 22);

    detectorLbl.setBounds (cInner.removeFromTop (14));
    detectorBox.setBounds (cInner.removeFromTop (28));
    cInner.removeFromTop (10);

    stereoLinkBtn.setBounds (cInner.removeFromTop (btnH));
    cInner.removeFromTop (gap);
    autoRelBtn   .setBounds (cInner.removeFromTop (btnH));

    // ---- Meters group ----
    meterGroup.setBounds (meterBox);
    auto mInner = meterBox.reduced (10, 22);

    // Three columns: GR (largest) | IN | OUT
    const int colGap = 10;
    const int totalMw = mInner.getWidth();
    const int grColW  = (int) (totalMw * 0.50f);
    const int sideW   = (totalMw - grColW - 2 * colGap) / 2;

    auto grCol  = mInner.removeFromLeft (grColW);
    mInner.removeFromLeft (colGap);
    auto inCol  = mInner.removeFromLeft (sideW);
    mInner.removeFromLeft (colGap);
    auto outCol = mInner;

    auto layoutMeterCol = [] (juce::Rectangle<int> col, juce::Component& meter, juce::Label& lbl)
    {
        auto labelArea = col.removeFromTop (16);
        lbl.setBounds (labelArea);
        meter.setBounds (col.reduced (4, 4));
    };
    layoutMeterCol (grCol,  grMeter,    grMeterLbl);
    layoutMeterCol (inCol,  inputMeter, inMeterLbl);
    layoutMeterCol (outCol, outputMeter, outMeterLbl);
}

//==============================================================================
void CP1AudioProcessorEditor::timerCallback()
{
    grMeter.grDb        = proc.grDb.load();
    inputMeter.levelDb  = proc.inLevelDb.load();
    outputMeter.levelDb = proc.outLevelDb.load();

    // Update SC connection status label colour/text
    const bool extOn  = proc.apvts.getRawParameterValue ("extSc")->load() > 0.5f;
    const bool extCon = proc.scExternalConnected.load();

    juce::String txt;
    juce::Colour col = kTextFaded;

    if (extOn && extCon)        { txt = "Ext: connected";       col = kAccent;       }
    else if (extOn && ! extCon) { txt = "Ext: not connected";   col = juce::Colour (0xffff7a3d); }
    else                        { txt = "Internal sidechain";   col = kTextFaded;    }

    if (scStatusLbl.getText() != txt) scStatusLbl.setText (txt, juce::dontSendNotification);
    scStatusLbl.setColour (juce::Label::textColourId, col);

    grMeter.repaint();
    inputMeter.repaint();
    outputMeter.repaint();
}
//==============================================================================
void CP1AudioProcessorEditor::populatePresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    auto& pm = proc.presetManager;
    auto names = pm.getAllPresetNames();

    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
        presetBox.addItem (names[i], i + 1);

    if (names.size() > pm.getNumFactoryPresets())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("User");
        for (int i = pm.getNumFactoryPresets(); i < names.size(); ++i)
            presetBox.addItem (names[i], i + 1);
    }
}

void CP1AudioProcessorEditor::updatePresetBox()
{
    int idx = proc.presetManager.getCurrentPresetIndex();
    presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
    presetDeleteBtn.setEnabled (! proc.presetManager.isFactoryPreset (idx));
}

