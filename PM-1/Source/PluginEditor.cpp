/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    constexpr int kEditorWidth  = 1100;
    constexpr int kEditorHeight = 760;
}

PM1AudioProcessorEditor::PM1AudioProcessorEditor (PM1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      visualizer (p.getSynth()),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    // Title
    titleLabel.setText ("PM - 1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6f0ff));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Luxury Pad Synthesizer", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::italic)));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
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
    characterCombo.addItem ("Warm Pad",   1);
    characterCombo.addItem ("Bright Pad", 2);
    characterCombo.addItem ("Strings",    3);
    characterCombo.addItem ("Choir",      4);
    characterCombo.addItem ("Glass",      5);
    characterCombo.addItem ("Air",        6);
    characterAttach = std::make_unique<APVTS::ComboBoxAttachment> (
        audioProcessor.apvts, "character", characterCombo);

    characterLabel.setText ("CHARACTER", juce::dontSendNotification);
    characterLabel.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    characterLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
    characterLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (characterLabel);

    // Octave
    addAndMakeVisible (octaveSlider);
    octaveSlider.setSliderStyle (juce::Slider::IncDecButtons);
    octaveSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 22);
    octaveSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    octaveSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff10171f));
    octaveSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd2dbe6));
    octaveAttach = std::make_unique<APVTS::SliderAttachment> (
        audioProcessor.apvts, "octave", octaveSlider);

    octaveLabel.setText ("OCTAVE", juce::dontSendNotification);
    octaveLabel.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    octaveLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
    octaveLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (octaveLabel);

    addAndMakeVisible (visualizer);

    // Knobs
    configureKnob (textureKnob,   "texture",     "TEXTURE");
    configureKnob (warmthKnob,    "warmth",      "WARMTH");
    configureKnob (brightKnob,    "brightness",  "BRIGHTNESS");
    configureKnob (driveKnob,     "drive",       "DRIVE");

    configureKnob (attackKnob,    "attack",      "ATTACK");
    configureKnob (releaseKnob,   "release",     "RELEASE");
    configureKnob (movementKnob,  "movement",    "MOVEMENT");
    configureKnob (lfoKnob,       "lforate",     "LFO RATE");

    configureKnob (lushnessKnob,  "lushness",    "LUSHNESS");
    configureKnob (delayKnob,     "delay",       "DELAY");
    configureKnob (spaceKnob,     "space",       "SPACE");
    configureKnob (widthKnob,     "width",       "WIDTH");

    configureKnob (volumeKnob,    "volume",      "VOLUME");

    addAndMakeVisible (keyboard);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (16.0f);

    audioProcessor.getPresetManager().addChangeListener (this);
    rebuildPresetCombo();

    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
}

PM1AudioProcessorEditor::~PM1AudioProcessorEditor()
{
    audioProcessor.getPresetManager().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void PM1AudioProcessorEditor::configureKnob (Knob& k, const juce::String& paramID, const juce::String& displayName)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff10171f));
    k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd2dbe6));
    addAndMakeVisible (k.slider);

    k.label.setText (displayName, juce::dontSendNotification);
    k.label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    k.label.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<APVTS::SliderAttachment> (
        audioProcessor.apvts, paramID, k.slider);
}

void PM1AudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildPresetCombo();
}

void PM1AudioProcessorEditor::rebuildPresetCombo()
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

void PM1AudioProcessorEditor::onPresetComboChanged()
{
    auto& mgr = audioProcessor.getPresetManager();
    auto name = presetCombo.getText();
    if (name.isEmpty()) return;
    if (name == mgr.getCurrentPresetName()) return;
    mgr.loadPresetByName (name);
}

void PM1AudioProcessorEditor::showSavePresetDialog()
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

void PM1AudioProcessorEditor::showDeletePresetDialog()
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

void PM1AudioProcessorEditor::openPresetsFolder()
{
    audioProcessor.getPresetManager().getUserPresetsFolder().revealToUser();
}

//==============================================================================
namespace LayoutPM1 {
    constexpr int kHeader   = 70;
    constexpr int kVizY     = 84;
    constexpr int kVizH     = 248;
    constexpr int kPanelGap = 8;
    constexpr int kRowH     = 130;

    inline int row1Y() { return kVizY + kVizH + 12; }                   // 344
    inline int row2Y() { return row1Y() + kRowH + kPanelGap; }          // 482
    inline int bottomY() { return row2Y() + kRowH + kPanelGap; }        // 620
    constexpr int kBottomH = 56;
}

void PM1AudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace LayoutPM1;

    auto bounds = getLocalBounds().toFloat();

    // Background: deep gradient
    juce::ColourGradient bg (
        juce::Colour (0xff141a23), bounds.getCentreX(), bounds.getY(),
        juce::Colour (0xff060a10), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Top accent bar
    g.setColour (juce::Colour (0xff0c1118));
    g.fillRect (bounds.removeFromTop ((float) kHeader));
    g.setColour (juce::Colour (0xff2a3f5c));
    g.drawHorizontalLine (kHeader, 0.0f, (float) getWidth());
    g.setColour (juce::Colour (0x554d9eff));
    g.drawHorizontalLine (kHeader + 1, 0.0f, (float) getWidth());

    // Decorative title underline (gradient shimmer)
    juce::ColourGradient titleAccent (
        juce::Colour (0xff4d9eff), 24.0f, 60.0f,
        juce::Colour (0x004d9eff), 280.0f, 60.0f, false);
    g.setGradientFill (titleAccent);
    g.fillRect (juce::Rectangle<float> (24.0f, 58.0f, 260.0f, 1.5f));

    auto drawPanel = [&] (int y, int height, const juce::String& label)
    {
        auto rect = juce::Rectangle<int> (20, y, getWidth() - 40, height).toFloat();
        g.setColour (juce::Colour (0xff10161f));
        g.fillRoundedRectangle (rect, 8.0f);
        g.setColour (juce::Colour (0xff2a3340));
        g.drawRoundedRectangle (rect, 8.0f, 1.0f);

        // Label tab in top-left
        g.setColour (juce::Colour (0xff8fa8c4));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (label, (int) rect.getX() + 12, (int) rect.getY() + 4, 200, 14, juce::Justification::topLeft);
    };

    drawPanel (row1Y(), kRowH, "TONE");
    drawPanel (row2Y(), kRowH, "MOTION  &  SPACE");
    drawPanel (bottomY(), kBottomH, "PERFORMANCE");
}

void PM1AudioProcessorEditor::resized()
{
    using namespace LayoutPM1;

    auto bounds = getLocalBounds();

    // Header
    auto header = bounds.removeFromTop (kHeader);
    auto titleArea = header.removeFromLeft (260).reduced (24, 8);
    titleLabel.setBounds (titleArea.removeFromTop (38));
    subtitleLabel.setBounds (titleArea);

    // Preset bar (right side of header)
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

    // Visualizer area
    visualizer.setBounds (juce::Rectangle<int> (20, kVizY, getWidth() - 40, kVizH));

    auto layoutKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        auto labelArea = r.removeFromTop (18);
        k.label.setBounds (labelArea);
        k.slider.setBounds (r.reduced (4));
    };

    auto layoutRow = [&] (juce::Rectangle<int> rowArea, std::initializer_list<Knob*> knobs)
    {
        rowArea = rowArea.reduced (16, 22);
        const int n = (int) knobs.size();
        const int knobW = rowArea.getWidth() / n;
        for (auto* k : knobs)
            layoutKnob (*k, rowArea.removeFromLeft (knobW));
    };

    // Row 1 - TONE: 6 knobs (Texture, Warmth, Brightness, Drive, Attack, Release)
    auto row1 = juce::Rectangle<int> (20, row1Y(), getWidth() - 40, kRowH);
    layoutRow (row1, { &textureKnob, &warmthKnob, &brightKnob, &driveKnob, &attackKnob, &releaseKnob });

    // Row 2 - MOTION & SPACE: 6 knobs
    auto row2 = juce::Rectangle<int> (20, row2Y(), getWidth() - 40, kRowH);
    layoutRow (row2, { &movementKnob, &lfoKnob, &lushnessKnob, &delayKnob, &spaceKnob, &widthKnob });

    // Bottom: Character | Octave | Volume knob | (filler) | keyboard
    auto bottom = juce::Rectangle<int> (20, bottomY(), getWidth() - 40, kBottomH).reduced (12, 10);

    // Character section (left)
    auto charSection = bottom.removeFromLeft (200);
    {
        auto top = charSection.removeFromTop (16);
        characterLabel.setBounds (top);
        characterCombo.setBounds (charSection.reduced (0, 4));
    }

    bottom.removeFromLeft (16);

    // Octave section
    auto octSection = bottom.removeFromLeft (170);
    {
        auto top = octSection.removeFromTop (16);
        octaveLabel.setBounds (top);
        octaveSlider.setBounds (octSection.reduced (0, 4));
    }

    bottom.removeFromLeft (16);

    // Volume - inline slider rather than rotary in this strip
    auto volSection = bottom.removeFromRight (180);
    {
        // Use a rotary knob look in the bottom area too
        auto top = volSection.removeFromTop (16);
        volumeKnob.label.setBounds (top);
        volumeKnob.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        volumeKnob.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
        volumeKnob.slider.setBounds (volSection.reduced (0, 6));
    }

    // Keyboard fills remaining bottom area beneath everything
    const int kbY = bottomY() + kBottomH + 8;
    const int kbH = juce::jmax (40, getHeight() - kbY - 12);
    keyboard.setBounds (juce::Rectangle<int> (20, kbY, getWidth() - 40, kbH));
}
