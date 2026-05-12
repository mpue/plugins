/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    constexpr int kEditorWidth  = 1000;
    constexpr int kEditorHeight = 620;
}

TR1AudioProcessorEditor::TR1AudioProcessorEditor (TR1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      visualizer (p.getTrashEngine())
{
    setLookAndFeel (&lookAndFeel);

    // Title
    titleLabel.setText ("TR - 1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffe6c4));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Luxury Trash", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::italic)));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc4a988));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    // Preset bar
    addAndMakeVisible (presetCombo);
    presetCombo.setTextWhenNothingSelected ("Select preset...");
    presetCombo.onChange = [this] { onPresetComboChanged(); };

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

    // Character
    addAndMakeVisible (characterCombo);
    characterCombo.addItem ("Tube",        1);
    characterCombo.addItem ("Tape",        2);
    characterCombo.addItem ("Fuzz",        3);
    characterCombo.addItem ("Crush",       4);
    characterCombo.addItem ("Telephone",   5);
    characterCombo.addItem ("Radio",       6);
    characterCombo.addItem ("Mangler",     7);
    characterCombo.addItem ("Vintage Amp", 8);
    characterAttach = std::make_unique<APVTS::ComboBoxAttachment> (
        audioProcessor.apvts, "character", characterCombo);

    characterLabel.setText ("Character", juce::dontSendNotification);
    characterLabel.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    characterLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc4a988));
    characterLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (characterLabel);

    addAndMakeVisible (visualizer);

    // Primary knobs
    configureKnob (driveKnob,  "drive",   "DRIVE");
    configureKnob (crunchKnob, "crunch",  "CRUNCH");
    configureKnob (toneKnob,   "tone",    "TONE");
    configureKnob (bodyKnob,   "body",    "BODY");
    configureKnob (motionKnob, "motion",  "MOTION");
    configureKnob (mixKnob,    "mix",     "MIX");

    // Secondary knobs
    configureKnob (textureKnob,    "texture",    "TEXTURE");
    configureKnob (motionRateKnob, "motionRate", "RATE");
    configureKnob (ageKnob,        "age",        "AGE");
    configureKnob (widthKnob,      "width",      "WIDTH");
    configureKnob (outputKnob,     "output",     "OUTPUT");

    audioProcessor.getPresetManager().addChangeListener (this);
    rebuildPresetCombo();

    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
}

TR1AudioProcessorEditor::~TR1AudioProcessorEditor()
{
    audioProcessor.getPresetManager().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void TR1AudioProcessorEditor::configureKnob (Knob& k, const juce::String& paramID, const juce::String& displayName)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff181010));
    k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe6dccc));
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe6a25c));
    k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff32261a));
    k.slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xfffacd83));
    addAndMakeVisible (k.slider);

    k.label.setText (displayName, juce::dontSendNotification);
    k.label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    k.label.setColour (juce::Label::textColourId, juce::Colour (0xffc4a988));
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<APVTS::SliderAttachment> (
        audioProcessor.apvts, paramID, k.slider);
}

void TR1AudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildPresetCombo();
}

void TR1AudioProcessorEditor::rebuildPresetCombo()
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

void TR1AudioProcessorEditor::onPresetComboChanged()
{
    auto& mgr = audioProcessor.getPresetManager();
    auto name = presetCombo.getText();
    if (name.isEmpty()) return;
    if (name == mgr.getCurrentPresetName()) return;
    mgr.loadPresetByName (name);
}

void TR1AudioProcessorEditor::showSavePresetDialog()
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

void TR1AudioProcessorEditor::showDeletePresetDialog()
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

void TR1AudioProcessorEditor::openPresetsFolder()
{
    audioProcessor.getPresetManager().getUserPresetsFolder().revealToUser();
}

//==============================================================================
void TR1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Warm dark gradient — copper/charcoal feel
    juce::ColourGradient bg (
        juce::Colour (0xff1d1410), bounds.getCentreX(), bounds.getY(),
        juce::Colour (0xff070506), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Top header strip
    g.setColour (juce::Colour (0xff15100c));
    g.fillRect (bounds.removeFromTop (70.0f));
    g.setColour (juce::Colour (0xff44321c));
    g.drawHorizontalLine (70, 0.0f, (float) getWidth());
    g.setColour (juce::Colour (0x55e6a25c));
    g.drawHorizontalLine (71, 0.0f, (float) getWidth());

    // Title underline shimmer
    juce::ColourGradient titleAccent (
        juce::Colour (0xffe6a25c), 24.0f, 60.0f,
        juce::Colour (0x00e6a25c), 220.0f, 60.0f, false);
    g.setGradientFill (titleAccent);
    g.fillRect (juce::Rectangle<float> (24.0f, 58.0f, 200.0f, 1.5f));

    // Main knobs panel
    auto mainKnobsArea = juce::Rectangle<int> (
        20, 380,
        getWidth() - 40, 150).toFloat();
    g.setColour (juce::Colour (0xff181010));
    g.fillRoundedRectangle (mainKnobsArea, 8.0f);
    g.setColour (juce::Colour (0xff3a2a1c));
    g.drawRoundedRectangle (mainKnobsArea, 8.0f, 1.0f);

    // Bottom panel
    auto bottomBar = juce::Rectangle<int> (
        20, 540, getWidth() - 40, 60).toFloat();
    g.setColour (juce::Colour (0xff181010));
    g.fillRoundedRectangle (bottomBar, 8.0f);
    g.setColour (juce::Colour (0xff3a2a1c));
    g.drawRoundedRectangle (bottomBar, 8.0f, 1.0f);
}

void TR1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header
    auto header = bounds.removeFromTop (70);
    auto titleArea = header.removeFromLeft (240).reduced (24, 8);
    titleLabel.setBounds (titleArea.removeFromTop (38));
    subtitleLabel.setBounds (titleArea);

    // Preset bar
    auto presetArea = header.reduced (12, 16);
    folderBtn.setBounds  (presetArea.removeFromRight (70));
    presetArea.removeFromRight (6);
    deleteBtn.setBounds  (presetArea.removeFromRight (70));
    presetArea.removeFromRight (6);
    saveBtn.setBounds    (presetArea.removeFromRight (70));
    presetArea.removeFromRight (10);
    nextBtn.setBounds    (presetArea.removeFromRight (28));
    presetArea.removeFromRight (3);
    prevBtn.setBounds    (presetArea.removeFromRight (28));
    presetArea.removeFromRight (8);
    presetCombo.setBounds (presetArea);

    // Visualizer
    visualizer.setBounds (juce::Rectangle<int> (20, 84, getWidth() - 40, 286));

    // Primary knobs (6)
    auto knobsArea = juce::Rectangle<int> (20, 380, getWidth() - 40, 150).reduced (10, 10);
    const int n = 6;
    const int knobW = knobsArea.getWidth() / n;

    auto layoutKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        auto labelArea = r.removeFromTop (18);
        k.label.setBounds (labelArea);
        k.slider.setBounds (r.reduced (4));
    };

    layoutKnob (driveKnob,  knobsArea.removeFromLeft (knobW));
    layoutKnob (crunchKnob, knobsArea.removeFromLeft (knobW));
    layoutKnob (toneKnob,   knobsArea.removeFromLeft (knobW));
    layoutKnob (bodyKnob,   knobsArea.removeFromLeft (knobW));
    layoutKnob (motionKnob, knobsArea.removeFromLeft (knobW));
    layoutKnob (mixKnob,    knobsArea.removeFromLeft (knobW));

    // Bottom bar — character + secondary
    auto bottomBar = juce::Rectangle<int> (20, 540, getWidth() - 40, 60).reduced (12, 8);

    // Character on left
    auto charArea = bottomBar.removeFromLeft (220);
    auto charLabelArea = charArea.removeFromLeft (75);
    characterLabel.setBounds (charLabelArea.withSizeKeepingCentre (75, 20));
    characterCombo.setBounds (charArea.reduced (4, 8));

    bottomBar.removeFromLeft (8);

    auto layoutSecondary = [] (Knob& k, juce::Rectangle<int> r)
    {
        auto labelArea = r.removeFromLeft (66);
        k.label.setBounds (labelArea.withSizeKeepingCentre (66, 16));
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
        k.slider.setBounds (r.reduced (2, 4));
    };

    auto sliderRow = bottomBar;
    int secW = sliderRow.getWidth() / 5;
    layoutSecondary (textureKnob,    sliderRow.removeFromLeft (secW));
    layoutSecondary (motionRateKnob, sliderRow.removeFromLeft (secW));
    layoutSecondary (ageKnob,        sliderRow.removeFromLeft (secW));
    layoutSecondary (widthKnob,      sliderRow.removeFromLeft (secW));
    layoutSecondary (outputKnob,     sliderRow.removeFromLeft (secW));
}
