/*
  ==============================================================================

    AF-1 — Luxurious AutoFilter
    Editor implementation

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AF1AudioProcessorEditor::AF1AudioProcessorEditor (AF1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetManager  (p.apvts),
      visualizer     (p)
{
    setLookAndFeel (&laf);

    // ── Title
    titleLabel.setText ("AF-1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (28.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8f0ff));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Luxury AutoFilter", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (12.0f, juce::Font::italic));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7da2d4));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    // ── Preset combo + buttons
    presetCombo.setTextWhenNothingSelected ("Select Preset...");
    presetCombo.onChange = [this] { onPresetSelected(); };
    addAndMakeVisible (presetCombo);

    prevButton.onClick   = [this] { presetManager.loadPrevious(); };
    nextButton.onClick   = [this] { presetManager.loadNext(); };
    saveAsButton.onClick = [this] { onSaveAsClicked(); };
    saveButton.onClick   = [this] { onSaveClicked(); };
    deleteButton.onClick = [this] { onDeleteClicked(); };
    initButton.onClick   = [this] { onInitClicked(); };

    for (auto* b : { &prevButton, &nextButton, &saveAsButton, &saveButton, &deleteButton, &initButton })
        addAndMakeVisible (*b);

    // ── Visualizer
    addAndMakeVisible (visualizer);

    // ── Sections
    addAndMakeVisible (filterSection);
    addAndMakeVisible (lfoSection);
    addAndMakeVisible (envSection);
    addAndMakeVisible (outSection);

    // ── Knobs
    setupKnob (cutoff,     "cutoff",     aCutoff);
    setupKnob (resonance,  "resonance",  aResonance);
    setupKnob (drive,      "drive",      aDrive);
    setupKnob (lfoRate,    "lfoRate",    aLfoRate);
    setupKnob (lfoDepth,   "lfoDepth",   aLfoDepth);
    setupKnob (envAmount,  "envAmount",  aEnvAmount);
    setupKnob (envAttack,  "envAttack",  aEnvAttack);
    setupKnob (envRelease, "envRelease", aEnvRelease);
    setupKnob (mix,        "mix",        aMix);
    setupKnob (output,     "output",     aOutput);

    // Add knobs to their sections
    filterSection.addAndMakeVisible (cutoff);
    filterSection.addAndMakeVisible (resonance);
    filterSection.addAndMakeVisible (drive);
    filterSection.addAndMakeVisible (filterTypeBox);
    filterSection.addAndMakeVisible (slopeBox);

    lfoSection.addAndMakeVisible (lfoRate);
    lfoSection.addAndMakeVisible (lfoDepth);
    lfoSection.addAndMakeVisible (lfoShapeBox);

    envSection.addAndMakeVisible (envAmount);
    envSection.addAndMakeVisible (envAttack);
    envSection.addAndMakeVisible (envRelease);

    outSection.addAndMakeVisible (mix);
    outSection.addAndMakeVisible (output);

    // ── Combo boxes
    setupCombo (filterTypeBox, "filterType",
                juce::StringArray { "Low Pass", "Band Pass", "High Pass", "Notch" }, aFilterType);
    setupCombo (slopeBox,      "slope",
                juce::StringArray { "12 dB/oct", "24 dB/oct" }, aSlope);
    setupCombo (lfoShapeBox,   "lfoShape",
                juce::StringArray { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" }, aLfoShape);

    // ── Preset listener
    presetManager.addListener (this);
    rebuildPresetCombo();

    setResizable (false, false);
    setSize (880, 560);
}

AF1AudioProcessorEditor::~AF1AudioProcessorEditor()
{
    presetManager.removeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void AF1AudioProcessorEditor::setupKnob (LabeledKnob& k, const juce::String& paramId,
                                         std::unique_ptr<SliderAttachment>& attachment)
{
    attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, paramId, k.slider);
}

void AF1AudioProcessorEditor::setupCombo (juce::ComboBox& box, const juce::String& paramId,
                                          const juce::StringArray& items,
                                          std::unique_ptr<ComboAttachment>& attachment)
{
    box.addItemList (items, 1);
    attachment = std::make_unique<ComboAttachment> (audioProcessor.apvts, paramId, box);
}

//==============================================================================
void AF1AudioProcessorEditor::paint (juce::Graphics& g)
{
    // Luxurious background — vertical gradient with subtle vignette
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient bg (
        juce::Colour (0xff20262e), bounds.getCentreX(), bounds.getY(),
        juce::Colour (0xff0d1015), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    // Top header strip darker
    g.setColour (juce::Colour (0xff141821));
    g.fillRect (bounds.removeFromTop (56.0f));

    // Hairline under header
    g.setColour (juce::Colour (0xff2a323c));
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());

    // Bottom hairline
    g.setColour (juce::Colour (0xff2a323c));
    g.drawHorizontalLine (getHeight() - 28, 0.0f, (float) getWidth());

    // Footer text
    g.setColour (juce::Colour (0xff556070));
    g.setFont (juce::Font (10.5f));
    g.drawText ("AF-1  •  Luxury AutoFilter  •  Matthias Pueski",
                getLocalBounds().removeFromBottom (24), juce::Justification::centred);
}

void AF1AudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // ── Header
    auto header = area.removeFromTop (56);
    {
        auto leftBox = header.removeFromLeft (200).reduced (16, 6);
        titleLabel.setBounds (leftBox.removeFromTop (32));
        subtitleLabel.setBounds (leftBox);

        auto presetBar = header.reduced (8, 12);
        const int btnW = 56;
        const int gap  = 4;

        initButton  .setBounds (presetBar.removeFromRight (btnW)); presetBar.removeFromRight (gap);
        deleteButton.setBounds (presetBar.removeFromRight (btnW)); presetBar.removeFromRight (gap);
        saveAsButton.setBounds (presetBar.removeFromRight (74));   presetBar.removeFromRight (gap);
        saveButton  .setBounds (presetBar.removeFromRight (btnW)); presetBar.removeFromRight (8);

        nextButton.setBounds (presetBar.removeFromRight (28)); presetBar.removeFromRight (4);
        prevButton.setBounds (presetBar.removeFromRight (28)); presetBar.removeFromRight (8);

        presetCombo.setBounds (presetBar);
    }

    area.removeFromTop (4);

    // ── Visualizer
    auto vizArea = area.removeFromTop (180).reduced (12, 0);
    visualizer.setBounds (vizArea);

    area.removeFromTop (8);

    // ── Sections row
    auto sections = area.removeFromTop (260).reduced (12, 0);

    const int filterW = 256;
    const int lfoW    = 200;
    const int envW    = 220;

    auto filterRect = sections.removeFromLeft (filterW); sections.removeFromLeft (8);
    auto lfoRect    = sections.removeFromLeft (lfoW);    sections.removeFromLeft (8);
    auto envRect    = sections.removeFromLeft (envW);    sections.removeFromLeft (8);
    auto outRect    = sections;

    filterSection.setBounds (filterRect);
    lfoSection   .setBounds (lfoRect);
    envSection   .setBounds (envRect);
    outSection   .setBounds (outRect);

    // ── Filter section internals
    {
        auto r = filterSection.getLocalBounds().reduced (10);
        r.removeFromTop (24); // title plate

        auto knobs = r.removeFromTop (130);
        const int kW = knobs.getWidth() / 3;
        cutoff   .setBounds (knobs.removeFromLeft (kW).reduced (4));
        resonance.setBounds (knobs.removeFromLeft (kW).reduced (4));
        drive    .setBounds (knobs.removeFromLeft (kW).reduced (4));

        r.removeFromTop (8);
        auto combos = r.removeFromTop (56);
        auto top = combos.removeFromTop (26);
        filterTypeBox.setBounds (top.reduced (4, 0));
        combos.removeFromTop (4);
        slopeBox.setBounds (combos.reduced (4, 0));
    }

    // ── LFO section internals
    {
        auto r = lfoSection.getLocalBounds().reduced (10);
        r.removeFromTop (24);

        auto knobs = r.removeFromTop (130);
        const int kW = knobs.getWidth() / 2;
        lfoRate .setBounds (knobs.removeFromLeft (kW).reduced (4));
        lfoDepth.setBounds (knobs.removeFromLeft (kW).reduced (4));

        r.removeFromTop (8);
        lfoShapeBox.setBounds (r.removeFromTop (26).reduced (4, 0));
    }

    // ── Envelope section internals
    {
        auto r = envSection.getLocalBounds().reduced (10);
        r.removeFromTop (24);

        auto knobs = r.removeFromTop (130);
        const int kW = knobs.getWidth() / 3;
        envAmount .setBounds (knobs.removeFromLeft (kW).reduced (4));
        envAttack .setBounds (knobs.removeFromLeft (kW).reduced (4));
        envRelease.setBounds (knobs.removeFromLeft (kW).reduced (4));
    }

    // ── Output section internals
    {
        auto r = outSection.getLocalBounds().reduced (10);
        r.removeFromTop (24);

        auto knobs = r.removeFromTop (130);
        const int kW = knobs.getWidth() / 2;
        mix   .setBounds (knobs.removeFromLeft (kW).reduced (4));
        output.setBounds (knobs.removeFromLeft (kW).reduced (4));
    }
}

//==============================================================================
// Preset combo handling
void AF1AudioProcessorEditor::rebuildPresetCombo()
{
    presetCombo.clear (juce::dontSendNotification);

    int id = 1;
    if (presetManager.getNumFactory() > 0)
    {
        presetCombo.addSectionHeading ("Factory");
        for (int i = 0; i < presetManager.getNumFactory(); ++i)
        {
            const auto names = presetManager.getAllPresetNames();
            presetCombo.addItem (names[i], id++);
        }
    }

    if (presetManager.getNumUser() > 0)
    {
        presetCombo.addSeparator();
        presetCombo.addSectionHeading ("User");
        for (int i = 0; i < presetManager.getNumUser(); ++i)
        {
            const auto names = presetManager.getAllPresetNames();
            const int globalIdx = presetManager.getNumFactory() + i;
            presetCombo.addItem (names[globalIdx], id++);
        }
    }

    // Show current selection
    const int curIdx = presetManager.getCurrentIndex();
    if (curIdx >= 0)
        presetCombo.setSelectedId (curIdx + 1, juce::dontSendNotification);
}

void AF1AudioProcessorEditor::onPresetSelected()
{
    const int sel = presetCombo.getSelectedId();
    if (sel <= 0) return;
    presetManager.loadByIndex (sel - 1);
}

void AF1AudioProcessorEditor::presetListChanged()
{
    rebuildPresetCombo();
}

void AF1AudioProcessorEditor::currentPresetChanged (const juce::String& /*name*/)
{
    const int curIdx = presetManager.getCurrentIndex();
    if (curIdx >= 0)
        presetCombo.setSelectedId (curIdx + 1, juce::dontSendNotification);
}

//==============================================================================
// Save / Save As / Delete
void AF1AudioProcessorEditor::onSaveAsClicked()
{
    auto* aw = new juce::AlertWindow ("Save Preset",
                                      "Choose a name for your preset:",
                                      juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", presetManager.getCurrentName().isEmpty()
                                  ? juce::String ("My Preset")
                                  : presetManager.getCurrentName(),
                       "Name:");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, aw](int result)
        {
            if (result != 1) return;

            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            const juce::File target = presetManager.getUserDir().getChildFile (name + ".afpreset");
            const bool exists = target.existsAsFile();

            auto doSave = [this, name](bool overwrite)
            {
                if (presetManager.saveAs (name, overwrite) == juce::File())
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                        "Save failed", "Could not write the preset file.");
                }
            };

            if (exists)
            {
                juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon,
                    "Overwrite preset?",
                    "A user preset named \"" + name + "\" already exists.\nOverwrite it?",
                    "Overwrite", "Cancel", nullptr,
                    juce::ModalCallbackFunction::create ([doSave](int r) { if (r == 1) doSave (true); }));
            }
            else
            {
                doSave (false);
            }
        }), true); // last 'true' = delete the AlertWindow when dismissed
}

void AF1AudioProcessorEditor::onSaveClicked()
{
    if (presetManager.getCurrentName().isEmpty()
        || presetManager.getCurrentIndex() < presetManager.getNumFactory())
    {
        // Factory or unset → fallback to Save As
        onSaveAsClicked();
        return;
    }
    if (! presetManager.saveCurrent())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Save failed", "Could not write the preset file.");
    }
}

void AF1AudioProcessorEditor::onDeleteClicked()
{
    const int idx = presetManager.getCurrentIndex();
    if (idx < presetManager.getNumFactory())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
            "Cannot delete", "Factory presets cannot be deleted.");
        return;
    }

    juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon,
        "Delete preset?",
        "Permanently delete \"" + presetManager.getCurrentName() + "\"?",
        "Delete", "Cancel", nullptr,
        juce::ModalCallbackFunction::create ([this, idx](int r)
        {
            if (r == 1) presetManager.deleteUserPreset (idx);
        }));
}

void AF1AudioProcessorEditor::onInitClicked()
{
    // Load factory preset 0 ("Init") if present
    if (presetManager.getNumFactory() > 0)
        presetManager.loadByIndex (0);
}
