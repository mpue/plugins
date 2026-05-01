/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    constexpr int kEditorWidth  = 1040;
    constexpr int kEditorHeight = 660;
}

PS1AudioProcessorEditor::PS1AudioProcessorEditor (PS1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      visualizer (p.getPitchShifter())
{
    setLookAndFeel (&lookAndFeel);

    // Title
    titleLabel.setText ("PS - 1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6f0ff));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Luxury Pitch Shifter", juce::dontSendNotification);
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
    characterCombo.addItem ("Smooth",  1);
    characterCombo.addItem ("Tight",   2);
    characterCombo.addItem ("Wide",    3);
    characterCombo.addItem ("Shimmer", 4);
    characterCombo.addItem ("Crystal", 5);
    characterAttach = std::make_unique<APVTS::ComboBoxAttachment> (
        audioProcessor.apvts, "character", characterCombo);

    characterLabel.setText ("Character", juce::dontSendNotification);
    characterLabel.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    characterLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
    characterLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (characterLabel);

    addAndMakeVisible (visualizer);

    // Knobs (main row)
    configureKnob (pitchKnob,    "pitch",    "PITCH",    " st");
    configureKnob (fineKnob,     "fine",     "FINE",     " ct");
    configureKnob (formantKnob,  "formant",  "FORMANT");
    configureKnob (widthKnob,    "width",    "WIDTH");
    configureKnob (feedbackKnob, "feedback", "FEEDBACK");
    configureKnob (mixKnob,      "mix",      "MIX");

    // Bottom row
    configureKnob (lowKnob,     "lowcut",  "LOW CUT");
    configureKnob (highKnob,    "highcut", "HIGH CUT");
    configureKnob (qualityKnob, "quality", "QUALITY");
    configureKnob (driveKnob,   "drive",   "DRIVE");

    addQuickButtons();

    audioProcessor.getPresetManager().addChangeListener (this);
    rebuildPresetCombo();

    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
}

PS1AudioProcessorEditor::~PS1AudioProcessorEditor()
{
    audioProcessor.getPresetManager().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void PS1AudioProcessorEditor::configureKnob (Knob& k, const juce::String& paramID,
                                             const juce::String& displayName,
                                             const juce::String& /*suffix*/)
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

void PS1AudioProcessorEditor::addQuickButtons()
{
    struct Spec { const char* label; int st; };
    static const Spec specs[] = {
        { "-12", -12 }, { "-7", -7 }, { "-5", -5 }, { "-3", -3 },
        { " 0",   0 },
        { "+3",  +3 }, { "+5", +5 }, { "+7", +7 }, { "+12", +12 }, { "+24", +24 }
    };

    for (const auto& s : specs)
    {
        auto qb = std::make_unique<QuickButton>();
        qb->btn.setButtonText (s.label);
        qb->semitones = s.st;
        qb->btn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff141d2a));
        qb->btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a52a8));
        qb->btn.setClickingTogglesState (false);
        const int target = s.st;
        qb->btn.onClick = [this, target]
        {
            if (auto* param = audioProcessor.apvts.getParameter ("pitch"))
            {
                auto range = audioProcessor.apvts.getParameterRange ("pitch");
                float norm = range.convertTo0to1 ((float) target);
                param->beginChangeGesture();
                param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
                param->endChangeGesture();
            }
        };
        addAndMakeVisible (qb->btn);
        quickButtons.push_back (std::move (qb));
    }
}

void PS1AudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildPresetCombo();
}

void PS1AudioProcessorEditor::rebuildPresetCombo()
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

void PS1AudioProcessorEditor::onPresetComboChanged()
{
    auto& mgr = audioProcessor.getPresetManager();
    auto name = presetCombo.getText();
    if (name.isEmpty()) return;
    if (name == mgr.getCurrentPresetName()) return;
    mgr.loadPresetByName (name);
}

void PS1AudioProcessorEditor::showSavePresetDialog()
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

void PS1AudioProcessorEditor::showDeletePresetDialog()
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

void PS1AudioProcessorEditor::openPresetsFolder()
{
    audioProcessor.getPresetManager().getUserPresetsFolder().revealToUser();
}

//==============================================================================
void PS1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background: deep gradient
    juce::ColourGradient bg (
        juce::Colour (0xff141a23), bounds.getCentreX(), bounds.getY(),
        juce::Colour (0xff060a10), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Top accent bar
    g.setColour (juce::Colour (0xff0c1118));
    g.fillRect (bounds.removeFromTop (70.0f));
    g.setColour (juce::Colour (0xff2a3f5c));
    g.drawHorizontalLine (70, 0.0f, (float) getWidth());
    g.setColour (juce::Colour (0x554d9eff));
    g.drawHorizontalLine (71, 0.0f, (float) getWidth());

    // Decorative title underline
    juce::ColourGradient titleAccent (
        juce::Colour (0xff4d9eff), 24.0f, 60.0f,
        juce::Colour (0x004d9eff), 220.0f, 60.0f, false);
    g.setGradientFill (titleAccent);
    g.fillRect (juce::Rectangle<float> (24.0f, 58.0f, 220.0f, 1.5f));

    // Quick interval bar background
    auto quickArea = juce::Rectangle<int> (
        20, 414, getWidth() - 40, 38).toFloat();
    g.setColour (juce::Colour (0xff10161f));
    g.fillRoundedRectangle (quickArea, 6.0f);
    g.setColour (juce::Colour (0xff2a3340));
    g.drawRoundedRectangle (quickArea, 6.0f, 1.0f);

    // Main knobs panel
    auto mainKnobsArea = juce::Rectangle<int> (
        20, 460, getWidth() - 40, 130).toFloat();
    g.setColour (juce::Colour (0xff10161f));
    g.fillRoundedRectangle (mainKnobsArea, 8.0f);
    g.setColour (juce::Colour (0xff2a3340));
    g.drawRoundedRectangle (mainKnobsArea, 8.0f, 1.0f);

    // Bottom secondary panel
    auto bottomBar = juce::Rectangle<int> (
        20, 600, getWidth() - 40, 50).toFloat();
    g.setColour (juce::Colour (0xff10161f));
    g.fillRoundedRectangle (bottomBar, 8.0f);
    g.setColour (juce::Colour (0xff2a3340));
    g.drawRoundedRectangle (bottomBar, 8.0f, 1.0f);
}

void PS1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header
    auto header = bounds.removeFromTop (70);
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

    // Visualizer
    auto vizArea = juce::Rectangle<int> (20, 84, getWidth() - 40, 320);
    visualizer.setBounds (vizArea);

    // Quick interval buttons
    auto quickArea = juce::Rectangle<int> (20, 414, getWidth() - 40, 38).reduced (8, 4);
    if (! quickButtons.empty())
    {
        const int n = (int) quickButtons.size();
        const int gap = 4;
        const int totalW = quickArea.getWidth() - gap * (n - 1);
        const int btnW = totalW / n;
        for (int i = 0; i < n; ++i)
        {
            auto r = quickArea.removeFromLeft (btnW);
            quickButtons[(size_t) i]->btn.setBounds (r.reduced (1, 0));
            if (i < n - 1) quickArea.removeFromLeft (gap);
        }
    }

    // Main knob row (6 knobs)
    auto knobsArea = juce::Rectangle<int> (20, 460, getWidth() - 40, 130).reduced (10, 8);
    const int n = 6;
    const int knobW = knobsArea.getWidth() / n;

    auto layoutKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        auto labelArea = r.removeFromTop (16);
        k.label.setBounds (labelArea);
        k.slider.setBounds (r.reduced (4));
    };

    layoutKnob (pitchKnob,    knobsArea.removeFromLeft (knobW));
    layoutKnob (fineKnob,     knobsArea.removeFromLeft (knobW));
    layoutKnob (formantKnob,  knobsArea.removeFromLeft (knobW));
    layoutKnob (widthKnob,    knobsArea.removeFromLeft (knobW));
    layoutKnob (feedbackKnob, knobsArea.removeFromLeft (knobW));
    layoutKnob (mixKnob,      knobsArea.removeFromLeft (knobW));

    // Bottom secondary row
    auto bottomBar = juce::Rectangle<int> (20, 600, getWidth() - 40, 50).reduced (10, 6);

    // Character on left
    auto charArea = bottomBar.removeFromLeft (220);
    auto charLabelArea = charArea.removeFromLeft (75);
    characterLabel.setBounds (charLabelArea.withSizeKeepingCentre (75, 20));
    characterCombo.setBounds (charArea.reduced (4, 6));

    bottomBar.removeFromLeft (8);

    // Four small horizontal sliders for secondary
    int secW = bottomBar.getWidth() / 4;

    auto layoutSecondary = [] (Knob& k, juce::Rectangle<int> r)
    {
        auto labelArea = r.removeFromLeft (60);
        k.label.setBounds (labelArea.withSizeKeepingCentre (60, 16));
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 18);
        k.slider.setBounds (r.reduced (2, 4));
    };

    layoutSecondary (lowKnob,     bottomBar.removeFromLeft (secW));
    layoutSecondary (highKnob,    bottomBar.removeFromLeft (secW));
    layoutSecondary (qualityKnob, bottomBar.removeFromLeft (secW));
    layoutSecondary (driveKnob,   bottomBar.removeFromLeft (secW));
}
