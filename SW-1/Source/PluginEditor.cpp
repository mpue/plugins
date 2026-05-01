/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    constexpr int kEditorWidth  = 1140;
    constexpr int kEditorHeight = 720;

    constexpr int kHeaderHeight = 76;
    constexpr int kPad          = 16;
    constexpr int kVizWidth     = 540;
    constexpr int kBottomHeight = 196;
}

SW1AudioProcessorEditor::SW1AudioProcessorEditor (SW1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      visualizer (p.getWidener())
{
    setLookAndFeel (&lookAndFeel);

    // Title
    titleLabel.setText ("SW - 1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6f0ff));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Luxury Stereo Widener", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::italic)));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    // Preset bar
    presetCombo.setTextWhenNothingSelected ("Select preset...");
    presetCombo.onChange = [this] { onPresetComboChanged(); };
    addAndMakeVisible (presetCombo);

    addAndMakeVisible (prevBtn);
    prevBtn.onClick = [this] { audioProcessor.getPresetManager().previousPreset(); };
    addAndMakeVisible (nextBtn);
    nextBtn.onClick = [this] { audioProcessor.getPresetManager().nextPreset(); };
    addAndMakeVisible (saveBtn);
    saveBtn.onClick = [this] { showSavePresetDialog(); };
    addAndMakeVisible (deleteBtn);
    deleteBtn.onClick = [this] { showDeletePresetDialog(); };
    addAndMakeVisible (folderBtn);
    folderBtn.onClick = [this] { openPresetsFolder(); };

    // Visualiser
    addAndMakeVisible (visualizer);

    // Main knobs (right panel, 3x2 grid)
    configureKnob (widthKnob,    "width",      "WIDTH");
    configureKnob (bassMonoKnob, "bassMonoHz", "BASS MONO");
    configureKnob (shimmerKnob,  "shimmer",    "SHIMMER");
    configureKnob (haasKnob,     "haas",       "HAAS");
    configureKnob (rotationKnob, "rotation",   "ROTATION");
    configureKnob (outputKnob,   "output",     "OUTPUT");

    // Multi-band (bottom-left)
    configureKnob (lowWidthKnob,  "lowWidth",  "LOW",  true);
    configureKnob (midWidthKnob,  "midWidth",  "MID",  true);
    configureKnob (highWidthKnob, "highWidth", "HIGH", true);
    configureKnob (xLowKnob,      "xLow",      "L|M",  true);
    configureKnob (xHighKnob,     "xHigh",     "M|H",  true);

    // Mix (bottom-right)
    configureKnob (mixKnob, "mix", "MIX", true);

    // Toggles
    addAndMakeVisible (bassMonoOnToggle);
    addAndMakeVisible (bypassToggle);
    addAndMakeVisible (monoCheckToggle);
    bassMonoOnAttach = std::make_unique<APVTS::ButtonAttachment> (
        audioProcessor.apvts, "bassMonoOn", bassMonoOnToggle);
    bypassAttach     = std::make_unique<APVTS::ButtonAttachment> (
        audioProcessor.apvts, "bypass",     bypassToggle);
    monoCheckAttach  = std::make_unique<APVTS::ButtonAttachment> (
        audioProcessor.apvts, "monoCheck",  monoCheckToggle);

    audioProcessor.getPresetManager().addChangeListener (this);
    rebuildPresetCombo();

    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
}

SW1AudioProcessorEditor::~SW1AudioProcessorEditor()
{
    audioProcessor.getPresetManager().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void SW1AudioProcessorEditor::configureKnob (Knob& k, const juce::String& paramID,
                                             const juce::String& displayName, bool small)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                              small ? 64 : 76, small ? 16 : 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff10171f));
    k.slider.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffd2dbe6));
    addAndMakeVisible (k.slider);

    k.label.setText (displayName, juce::dontSendNotification);
    k.label.setFont (juce::Font (juce::FontOptions (small ? 10.5f : 11.5f, juce::Font::bold)));
    k.label.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<APVTS::SliderAttachment> (
        audioProcessor.apvts, paramID, k.slider);
}

//==============================================================================
void SW1AudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildPresetCombo();
}

void SW1AudioProcessorEditor::rebuildPresetCombo()
{
    presetCombo.clear (juce::dontSendNotification);

    auto& mgr = audioProcessor.getPresetManager();
    const auto& fact = mgr.getFactoryPresets();
    const auto& user = mgr.getUserPresets();

    int id = 1;
    if (! fact.empty())
    {
        presetCombo.addSectionHeading ("FACTORY");
        for (auto& p : fact)
            presetCombo.addItem (p.name, id++);
    }
    if (! user.empty())
    {
        presetCombo.addSectionHeading ("USER");
        for (auto& p : user)
            presetCombo.addItem (p.name, id++);
    }

    auto current = mgr.getCurrentPresetName();
    for (int i = 1; i < id; ++i)
        if (presetCombo.getItemText (i - 1) == current)
        {
            presetCombo.setSelectedId (i, juce::dontSendNotification);
            break;
        }
}

void SW1AudioProcessorEditor::onPresetComboChanged()
{
    auto& mgr = audioProcessor.getPresetManager();
    auto name = presetCombo.getText();
    if (name.isEmpty()) return;
    if (name == mgr.getCurrentPresetName()) return;
    mgr.loadPresetByName (name);
}

void SW1AudioProcessorEditor::showSavePresetDialog()
{
    auto* aw = new juce::AlertWindow ("Save Preset",
                                     "Enter a name for the new preset:",
                                     juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", audioProcessor.getPresetManager().getCurrentPresetName(), "Preset name:");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, aw] (int result)
        {
            if (result == 1)
            {
                auto name = aw->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                    audioProcessor.getPresetManager().saveUserPreset (name);
            }
            delete aw;
        }), false);
}

void SW1AudioProcessorEditor::showDeletePresetDialog()
{
    auto& mgr = audioProcessor.getPresetManager();
    if (mgr.currentPresetIsFactory())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "Cannot delete factory preset",
            "Factory presets cannot be deleted. Save your changes as a new user preset instead.");
        return;
    }

    auto name = mgr.getCurrentPresetName();
    if (name.isEmpty()) return;

    juce::AlertWindow::showOkCancelBox (
        juce::AlertWindow::QuestionIcon,
        "Delete Preset",
        "Delete user preset \"" + name + "\"?",
        "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create ([this, name] (int result)
        {
            if (result == 1)
                audioProcessor.getPresetManager().deleteUserPreset (name);
        }));
}

void SW1AudioProcessorEditor::openPresetsFolder()
{
    audioProcessor.getPresetManager().getUserPresetsFolder().revealToUser();
}

//==============================================================================
void SW1AudioProcessorEditor::drawPanel (juce::Graphics& g,
                                         juce::Rectangle<float> r,
                                         const juce::String& caption)
{
    juce::ColourGradient panel (
        juce::Colour (0xff141a23), r.getCentreX(), r.getY(),
        juce::Colour (0xff0a0e15), r.getCentreX(), r.getBottom(), false);
    g.setGradientFill (panel);
    g.fillRoundedRectangle (r, 10.0f);

    g.setColour (juce::Colour (0xff223046));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    if (caption.isNotEmpty())
    {
        g.setColour (juce::Colour (0xff5778a3));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (caption, r.reduced (12.0f, 8.0f), juce::Justification::topLeft);

        // Caption underline accent
        juce::ColourGradient accent (
            juce::Colour (0xff4d9eff), r.getX() + 12.0f, r.getY() + 22.0f,
            juce::Colour (0x004d9eff), r.getX() + 90.0f, r.getY() + 22.0f, false);
        g.setGradientFill (accent);
        g.fillRect (juce::Rectangle<float> (r.getX() + 12.0f, r.getY() + 22.0f, 78.0f, 1.0f));
    }
}

void SW1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background — vertical gradient
    juce::ColourGradient bg (
        juce::Colour (0xff141a23), bounds.getCentreX(), bounds.getY(),
        juce::Colour (0xff060a10), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Header bar
    auto headerArea = juce::Rectangle<float> (0.0f, 0.0f, bounds.getWidth(), (float) kHeaderHeight);
    juce::ColourGradient headerGrad (
        juce::Colour (0xff0c1118), headerArea.getCentreX(), headerArea.getY(),
        juce::Colour (0xff080b10), headerArea.getCentreX(), headerArea.getBottom(), false);
    g.setGradientFill (headerGrad);
    g.fillRect (headerArea);

    // Header divider
    g.setColour (juce::Colour (0xff2a3f5c));
    g.drawHorizontalLine (kHeaderHeight, 0.0f, bounds.getWidth());
    g.setColour (juce::Colour (0x554d9eff));
    g.drawHorizontalLine (kHeaderHeight + 1, 0.0f, bounds.getWidth());

    // Title underline accent
    juce::ColourGradient titleAccent (
        juce::Colour (0xff4d9eff), 24.0f, 64.0f,
        juce::Colour (0x004d9eff), 240.0f, 64.0f, false);
    g.setGradientFill (titleAccent);
    g.fillRect (juce::Rectangle<float> (24.0f, 62.0f, 240.0f, 1.5f));

    // ========== MAIN PANELS =================================================
    auto bodyTop   = (float) (kHeaderHeight + 8);
    auto bodyLeft  = (float) kPad;
    auto bodyRight = bounds.getWidth() - (float) kPad;

    // Right panel (main knobs)
    auto rightPanel = juce::Rectangle<float> (
        bodyLeft + (float) kVizWidth + (float) kPad,
        bodyTop,
        bodyRight - (bodyLeft + (float) kVizWidth + (float) kPad),
        bounds.getHeight() - bodyTop - (float) kBottomHeight - 8.0f);
    drawPanel (g, rightPanel, "STEREO FIELD CONTROL");

    // Bottom panels (multi-band + mix/toggles)
    auto bottomY = rightPanel.getBottom() + 8.0f;
    auto bottomH = bounds.getHeight() - bottomY - (float) kPad;

    auto multiBand = juce::Rectangle<float> (
        bodyLeft, bottomY,
        rightPanel.getX() - bodyLeft - 8.0f, bottomH);
    drawPanel (g, multiBand, "MULTI-BAND WIDTH");

    auto mixPanel = juce::Rectangle<float> (
        rightPanel.getX(), bottomY,
        rightPanel.getWidth(), bottomH);
    drawPanel (g, mixPanel, "MASTER");
}

//==============================================================================
void SW1AudioProcessorEditor::layoutKnob (Knob& k, juce::Rectangle<int> r, int labelHeight)
{
    auto labelArea = r.removeFromTop (labelHeight);
    k.label.setBounds (labelArea);
    k.slider.setBounds (r.reduced (4));
}

void SW1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ---------- Header ------------------------------------------------------
    auto header = bounds.removeFromTop (kHeaderHeight);
    auto titleArea = header.removeFromLeft (270).reduced (24, 8);
    titleLabel.setBounds (titleArea.removeFromTop (38));
    subtitleLabel.setBounds (titleArea);

    auto presetArea = header.reduced (12, 18);
    folderBtn.setBounds (presetArea.removeFromRight (70));
    presetArea.removeFromRight (6);
    deleteBtn.setBounds (presetArea.removeFromRight (70));
    presetArea.removeFromRight (6);
    saveBtn.setBounds   (presetArea.removeFromRight (70));
    presetArea.removeFromRight (10);
    nextBtn.setBounds   (presetArea.removeFromRight (28));
    presetArea.removeFromRight (3);
    prevBtn.setBounds   (presetArea.removeFromRight (28));
    presetArea.removeFromRight (8);
    presetCombo.setBounds (presetArea);

    // ---------- Body --------------------------------------------------------
    auto body = bounds;
    body.removeFromTop (8);

    // Left: visualizer
    const int bodyTop  = body.getY();
    const int bodyH    = body.getHeight() - kBottomHeight - 8 - kPad;
    auto vizArea = juce::Rectangle<int> (kPad, bodyTop, kVizWidth, bodyH);
    visualizer.setBounds (vizArea);

    // Right panel — main knobs
    auto rightPanel = juce::Rectangle<int> (
        vizArea.getRight() + kPad, bodyTop,
        getWidth() - kPad - vizArea.getRight() - kPad, bodyH);

    // 3 columns x 2 rows of knobs
    auto knobs = rightPanel.reduced (16, 30);
    knobs.removeFromTop (4);

    const int rowH = knobs.getHeight() / 2;
    auto row1 = knobs.removeFromTop (rowH);
    auto row2 = knobs;

    auto cellsForRow = [] (juce::Rectangle<int> r, int cells) -> std::vector<juce::Rectangle<int>>
    {
        std::vector<juce::Rectangle<int>> out;
        const int w = r.getWidth() / cells;
        for (int i = 0; i < cells; ++i)
            out.push_back (r.removeFromLeft (w));
        return out;
    };

    auto r1 = cellsForRow (row1, 3);
    auto r2 = cellsForRow (row2, 3);

    layoutKnob (widthKnob,    r1[0]);
    layoutKnob (bassMonoKnob, r1[1]);
    layoutKnob (shimmerKnob,  r1[2]);
    layoutKnob (haasKnob,     r2[0]);
    layoutKnob (rotationKnob, r2[1]);
    layoutKnob (outputKnob,   r2[2]);

    // ---------- Bottom panels -----------------------------------------------
    auto bottom = juce::Rectangle<int> (
        kPad, vizArea.getBottom() + 8,
        getWidth() - kPad * 2, kBottomHeight);

    // Multi-band on the left (matching width of viz)
    auto multiBand = juce::Rectangle<int> (
        kPad, bottom.getY(),
        kVizWidth, bottom.getHeight());

    // 5 cells: Low, Mid, High, then xLow, xHigh
    auto mb = multiBand.reduced (12, 30);
    const int mbCells = 5;
    const int mbCellW = mb.getWidth() / mbCells;
    auto mb1 = mb.removeFromLeft (mbCellW);
    auto mb2 = mb.removeFromLeft (mbCellW);
    auto mb3 = mb.removeFromLeft (mbCellW);
    auto mb4 = mb.removeFromLeft (mbCellW);
    auto mb5 = mb;

    layoutKnob (lowWidthKnob,  mb1, 14);
    layoutKnob (midWidthKnob,  mb2, 14);
    layoutKnob (highWidthKnob, mb3, 14);
    layoutKnob (xLowKnob,      mb4, 14);
    layoutKnob (xHighKnob,     mb5, 14);

    // Master on the right
    auto master = juce::Rectangle<int> (
        rightPanel.getX(), bottom.getY(),
        rightPanel.getWidth(), bottom.getHeight()).reduced (12, 30);

    // Layout: toggles column on the left, mix knob on the right
    auto togglesCol = master.removeFromLeft (master.getWidth() - 130);
    auto mixCol     = master;

    layoutKnob (mixKnob, mixCol, 14);

    // Toggles stacked vertically (nice ElegantDarkLookAndFeel LED style)
    const int btnH = 30;
    const int btnGap = 8;
    auto t1 = togglesCol.removeFromTop (btnH);
    togglesCol.removeFromTop (btnGap);
    auto t2 = togglesCol.removeFromTop (btnH);
    togglesCol.removeFromTop (btnGap);
    auto t3 = togglesCol.removeFromTop (btnH);

    bypassToggle.setBounds     (t1);
    bassMonoOnToggle.setBounds (t2);
    monoCheckToggle.setBounds  (t3);
}
