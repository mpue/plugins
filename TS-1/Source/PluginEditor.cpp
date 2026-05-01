/*
  ==============================================================================

    TS-1 Transient Shaper – Editor implementation

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TS1AudioProcessorEditor::TS1AudioProcessorEditor (TS1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&laf);

    // -------- Header --------
    titleLabel.setText ("TS-1", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::Font (32.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff4d9eff));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("TRANSIENT  SHAPER", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8aa1c4));
    addAndMakeVisible (subtitleLabel);

    // -------- Preset bar --------
    addAndMakeVisible (presetCombo);
    presetCombo.setTextWhenNothingSelected ("— select preset —");
    presetCombo.setJustificationType (juce::Justification::centredLeft);
    presetCombo.onChange = [this] { onPresetChosen(); };

    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (saveAsBtn);
    addAndMakeVisible (deleteBtn);

    prevBtn.setTooltip   ("Previous preset");
    nextBtn.setTooltip   ("Next preset");
    saveBtn.setTooltip   ("Save current preset (factory presets are saved as new user preset)");
    saveAsBtn.setTooltip ("Save as new user preset");
    deleteBtn.setTooltip ("Delete current user preset");

    prevBtn.onClick   = [this] { shiftPreset (-1); };
    nextBtn.onClick   = [this] { shiftPreset (+1); };
    saveBtn.onClick   = [this] { onSavePreset(); };
    saveAsBtn.onClick = [this] { onSaveAsPreset(); };
    deleteBtn.onClick = [this] { onDeletePreset(); };

    // -------- Visualiser --------
    addAndMakeVisible (visualiser);

    // -------- Knobs --------
    auto setupKnob = [this] (juce::Slider& s, juce::Label& lbl, const juce::String& text,
                              const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setTextValueSuffix (suffix);
        s.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (s);

        lbl.setText (text, juce::dontSendNotification);
        lbl.setJustificationType (juce::Justification::centred);
        lbl.setFont (juce::Font (12.0f, juce::Font::bold));
        lbl.setColour (juce::Label::textColourId, juce::Colour (0xff8aa1c4));
        addAndMakeVisible (lbl);
    };

    setupKnob (attackKnob,      attackLbl,  "ATTACK",      " %");
    setupKnob (sustainKnob,     sustainLbl, "SUSTAIN",     " %");
    setupKnob (sensitivityKnob, sensLbl,    "SENSITIVITY", " %");
    sensitivityKnob.setDoubleClickReturnValue (true, 50.0);

    // -------- Output / Mix --------
    auto setupBar = [this] (juce::Slider& s, juce::Label& lbl, const juce::String& text,
                             const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
        s.setTextValueSuffix (suffix);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        lbl.setText (text, juce::dontSendNotification);
        lbl.setJustificationType (juce::Justification::centredRight);
        lbl.setFont (juce::Font (11.5f, juce::Font::bold));
        lbl.setColour (juce::Label::textColourId, juce::Colour (0xff8aa1c4));
        addAndMakeVisible (lbl);
    };

    setupBar (outputSlider, outputLbl, "OUTPUT", " dB");
    setupBar (mixSlider,    mixLbl,    "MIX",    " %");
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    mixSlider.setDoubleClickReturnValue (true, 100.0);

    addAndMakeVisible (bypassToggle);
    bypassToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffe8f0ff));

    // -------- Meters --------
    inputMeter.setLabel ("IN");
    inputMeter.setSource ([this] { return audioProcessor.getInputLevel(); });
    inputMeter.setMaxValue (1.0f);
    addAndMakeVisible (inputMeter);

    outputMeter.setLabel ("OUT");
    outputMeter.setSource ([this] { return audioProcessor.getOutputLevel(); });
    outputMeter.setMaxValue (1.0f);
    addAndMakeVisible (outputMeter);

    gainMeter.setLabel ("Δ dB");
    gainMeter.setSource ([this] { return audioProcessor.getGainChangeDb(); });
    gainMeter.setMaxValue (18.0f); // bipolar ±18 dB
    addAndMakeVisible (gainMeter);

    // -------- Attachments --------
    attackAtt      = std::make_unique<SAtt>(audioProcessor.parameters, "attack",       attackKnob);
    sustainAtt     = std::make_unique<SAtt>(audioProcessor.parameters, "sustain",      sustainKnob);
    sensitivityAtt = std::make_unique<SAtt>(audioProcessor.parameters, "sensitivity",  sensitivityKnob);
    outputAtt      = std::make_unique<SAtt>(audioProcessor.parameters, "output",       outputSlider);
    mixAtt         = std::make_unique<SAtt>(audioProcessor.parameters, "mix",          mixSlider);
    bypassAtt      = std::make_unique<BAtt>(audioProcessor.parameters, "bypass",       bypassToggle);

    rebuildPresetMenu();

    setSize (760, 560);
    startTimerHz (8);
}

TS1AudioProcessorEditor::~TS1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void TS1AudioProcessorEditor::rebuildPresetMenu()
{
    presetCombo.clear (juce::dontSendNotification);

    factoryNames = audioProcessor.getFactoryPresetNames();
    userNames    = audioProcessor.getUserPresetNames();

    int id = 1;
    presetCombo.addSectionHeading ("FACTORY");
    for (auto& n : factoryNames)
        presetCombo.addItem (n, id++);

    if (! userNames.isEmpty())
    {
        presetCombo.addSeparator();
        presetCombo.addSectionHeading ("USER");
        for (auto& n : userNames)
            presetCombo.addItem (n, id++);
    }

    // Restore current selection
    const auto cur = audioProcessor.getCurrentPresetName();
    int idx = factoryNames.indexOf (cur);
    if (idx >= 0)
    {
        presetCombo.setSelectedId (1 + idx, juce::dontSendNotification);
    }
    else
    {
        idx = userNames.indexOf (cur);
        if (idx >= 0)
            presetCombo.setSelectedId (1 + factoryNames.size() + idx, juce::dontSendNotification);
    }
}

void TS1AudioProcessorEditor::onPresetChosen()
{
    const int sel = presetCombo.getSelectedId();
    if (sel <= 0) return;

    const int factoryCount = factoryNames.size();
    const int idx = sel - 1;
    if (idx < factoryCount)
        audioProcessor.loadPreset (factoryNames[idx], true);
    else
        audioProcessor.loadPreset (userNames[idx - factoryCount], false);
}

void TS1AudioProcessorEditor::shiftPreset (int delta)
{
    const int total = factoryNames.size() + userNames.size();
    if (total == 0) return;

    int sel = presetCombo.getSelectedId();
    int curIdx = (sel <= 0) ? 0 : sel - 1;
    int newIdx = (curIdx + delta + total) % total;
    presetCombo.setSelectedId (newIdx + 1, juce::sendNotificationSync);
}

void TS1AudioProcessorEditor::onSavePreset()
{
    const auto cur = audioProcessor.getCurrentPresetName();
    if (cur.isEmpty() || factoryNames.contains (cur))
    {
        // Factory preset – fall through to "Save As"
        onSaveAsPreset();
        return;
    }

    if (audioProcessor.savePreset (cur))
        rebuildPresetMenu();
}

void TS1AudioProcessorEditor::onSaveAsPreset()
{
    auto* aw = new juce::AlertWindow ("Save Preset",
                                     "Enter a name for the user preset:",
                                     juce::AlertWindow::NoIcon, this);
    aw->addTextEditor ("name", audioProcessor.getCurrentPresetName(), "Name:");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->setVisible (true);

    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, aw] (int result)
        {
            if (result == 1)
            {
                const auto name = aw->getTextEditorContents ("name").trim();
                if (name.isNotEmpty() && ! factoryNames.contains (name))
                {
                    if (audioProcessor.savePreset (name))
                        rebuildPresetMenu();
                }
            }
        }), true);
}

void TS1AudioProcessorEditor::onDeletePreset()
{
    const auto cur = audioProcessor.getCurrentPresetName();
    if (cur.isEmpty() || factoryNames.contains (cur))
        return; // factory presets cannot be deleted

    auto* aw = new juce::AlertWindow ("Delete Preset",
                                     "Delete user preset \"" + cur + "\"?",
                                     juce::AlertWindow::QuestionIcon, this);
    aw->addButton ("Delete", 1);
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->setVisible (true);

    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, cur] (int result)
        {
            if (result == 1)
            {
                audioProcessor.deletePreset (cur);
                audioProcessor.loadPreset ("Init", true);
                rebuildPresetMenu();
            }
        }), true);
}

//==============================================================================
void TS1AudioProcessorEditor::timerCallback()
{
    // If the host changed the preset name through state restore, sync the combo.
    const auto cur = audioProcessor.getCurrentPresetName();
    int desiredId = -1;
    int idx = factoryNames.indexOf (cur);
    if (idx >= 0)
    {
        desiredId = 1 + idx;
    }
    else
    {
        idx = userNames.indexOf (cur);
        if (idx >= 0)
            desiredId = 1 + factoryNames.size() + idx;
    }
    if (desiredId > 0 && presetCombo.getSelectedId() != desiredId)
        presetCombo.setSelectedId (desiredId, juce::dontSendNotification);
}

//==============================================================================
void TS1AudioProcessorEditor::paint (juce::Graphics& g)
{
    // Vertical background gradient
    juce::ColourGradient bg (juce::Colour (0xff1f2734), 0.0f, 0.0f,
                             juce::Colour (0xff10141c), 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Side accents
    g.setColour (juce::Colour (0x224d9eff));
    g.fillRect (0, 0, 3, getHeight());
    g.fillRect (getWidth() - 3, 0, 3, getHeight());

    // Header divider
    g.setColour (juce::Colour (0xff222a3a));
    g.fillRect (12, 64, getWidth() - 24, 1);

    // Footer info text
    g.setColour (juce::Colour (0xff5a6680));
    g.setFont (juce::Font (10.0f));
    g.drawText ("TS-1  ·  Transient Shaper  ·  Matthias Pueski",
                juce::Rectangle<int> (12, getHeight() - 22, getWidth() - 24, 18),
                juce::Justification::centredRight);
}

void TS1AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12, 8);

    // ---- Header ----
    auto header = area.removeFromTop (60);
    auto leftHdr = header.removeFromLeft (220);
    titleLabel.setBounds    (leftHdr.removeFromTop (38));
    subtitleLabel.setBounds (leftHdr.removeFromTop (16));

    // Right side preset bar
    auto presetBar = header.reduced (4, 14);
    deleteBtn.setBounds   (presetBar.removeFromRight (72));
    presetBar.removeFromRight (4);
    saveAsBtn.setBounds   (presetBar.removeFromRight (78));
    presetBar.removeFromRight (4);
    saveBtn.setBounds     (presetBar.removeFromRight (62));
    presetBar.removeFromRight (8);
    nextBtn.setBounds     (presetBar.removeFromRight (28));
    prevBtn.setBounds     (presetBar.removeFromRight (28));
    presetBar.removeFromRight (4);
    presetCombo.setBounds (presetBar);

    area.removeFromTop (10);

    // ---- Visualiser ----
    auto vizArea = area.removeFromTop (150);
    visualiser.setBounds (vizArea);

    area.removeFromTop (10);

    // ---- Knob row + meters ----
    auto knobRow = area.removeFromTop (210);

    auto metersArea = knobRow.removeFromRight (180);
    metersArea.reduce (6, 8);
    const int meterW = 50;
    const int meterGap = (metersArea.getWidth() - 3 * meterW) / 2;
    auto m1 = metersArea.removeFromLeft (meterW);
    metersArea.removeFromLeft (meterGap);
    auto m2 = metersArea.removeFromLeft (meterW);
    metersArea.removeFromLeft (meterGap);
    auto m3 = metersArea.removeFromLeft (meterW);
    inputMeter.setBounds  (m1);
    outputMeter.setBounds (m2);
    gainMeter.setBounds   (m3);

    const int knobW = knobRow.getWidth() / 3;
    auto attCol = knobRow.removeFromLeft (knobW);
    auto susCol = knobRow.removeFromLeft (knobW);
    auto senCol = knobRow;

    auto layoutKnob = [] (juce::Rectangle<int> col, juce::Slider& s, juce::Label& l)
    {
        const int labelH = 18;
        l.setBounds (col.removeFromBottom (labelH));
        s.setBounds (col.reduced (8, 6));
    };
    layoutKnob (attCol, attackKnob,      attackLbl);
    layoutKnob (susCol, sustainKnob,     sustainLbl);
    layoutKnob (senCol, sensitivityKnob, sensLbl);

    area.removeFromTop (8);

    // ---- Bottom bar: output + mix + bypass ----
    auto bottom = area;
    auto outRow = bottom.removeFromTop (30);
    outputLbl.setBounds (outRow.removeFromLeft (66));
    outRow.removeFromLeft (6);
    bypassToggle.setBounds (outRow.removeFromRight (110));
    outRow.removeFromRight (8);
    outputSlider.setBounds (outRow.reduced (4, 4));

    bottom.removeFromTop (6);
    auto mixRow = bottom.removeFromTop (30);
    mixLbl.setBounds (mixRow.removeFromLeft (66));
    mixRow.removeFromLeft (6);
    mixRow.removeFromRight (118); // align with bypass column above
    mixSlider.setBounds (mixRow.reduced (4, 4));
}
