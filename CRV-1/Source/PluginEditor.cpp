/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    constexpr int kEditorWidth  = 1040;
    constexpr int kEditorHeight = 680;
}

CRV1AudioProcessorEditor::CRV1AudioProcessorEditor (CRV1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      visualizer (p.getEngine())
{
    setLookAndFeel (&lookAndFeel);

    // ---- Title ----
    titleLabel.setText ("CRV - 1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6f0ff));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Luxury Convolution Reverb", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::italic)));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    // ---- Preset bar ----
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

    // ---- IR selector row ----
    irLabel.setText ("Impulse:", juce::dontSendNotification);
    irLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    irLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fa8c4));
    irLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (irLabel);

    addAndMakeVisible (irCombo);
    irCombo.setTextWhenNothingSelected ("Select impulse response...");
    irCombo.onChange = [this] { onIRComboChanged(); };

    addAndMakeVisible (loadIRBtn);
    loadIRBtn.onClick = [this] { loadCustomIRFromDisk(); };

    addAndMakeVisible (delIRBtn);
    delIRBtn.onClick = [this] { deleteSelectedUserIR(); };

    addAndMakeVisible (irFolderBtn);
    irFolderBtn.onClick = [this] { openIRFolder(); };

    // ---- Visualizer ----
    addAndMakeVisible (visualizer);

    // ---- Knobs ----
    configureKnob (sizeKnob,     "size",       "SIZE");
    configureKnob (decayKnob,    "decay",      "DECAY");
    configureKnob (predelayKnob, "predelay",   "PRE-DELAY");
    configureKnob (modKnob,      "modulation", "MODULATION");
    configureKnob (mixKnob,      "mix",        "MIX");

    configureKnob (lowKnob,    "lowcut",  "LOW CUT");
    configureKnob (highKnob,   "highcut", "HIGH CUT");
    configureKnob (widthKnob,  "width",   "WIDTH");
    configureKnob (outputKnob, "output",  "OUTPUT");

    // ---- Wire processor callback for IR updates ----
    // The callback may fire from any thread; bounce it onto the message
    // thread before touching components.
    audioProcessor.setOnIRRendered (
        [safeThis = juce::Component::SafePointer<CRV1AudioProcessorEditor> (this)]
        (const juce::AudioBuffer<float>& ir, double sr, const juce::String& info)
        {
            juce::MessageManager::callAsync (
                [safeThis, irCopy = ir, sr, info]() mutable
                {
                    if (auto* ed = safeThis.getComponent())
                        ed->irRenderedFromProcessor (irCopy, sr, info);
                });
        });

    audioProcessor.getPresetManager().addChangeListener (this);

    rebuildIRCombo();
    rebuildPresetCombo();

    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);

    // Ask the processor to re-emit its current IR so the visualizer paints
    // immediately on open.
    audioProcessor.requestIRRefresh();
}

CRV1AudioProcessorEditor::~CRV1AudioProcessorEditor()
{
    audioProcessor.getPresetManager().removeChangeListener (this);
    audioProcessor.setOnIRRendered (nullptr);
    setLookAndFeel (nullptr);
}

void CRV1AudioProcessorEditor::configureKnob (Knob& k,
                                              const juce::String& paramID,
                                              const juce::String& displayName)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
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

void CRV1AudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildPresetCombo();
    rebuildIRCombo();
}

void CRV1AudioProcessorEditor::rebuildPresetCombo()
{
    presetCombo.clear (juce::dontSendNotification);

    auto& mgr = audioProcessor.getPresetManager();
    const auto& fact = mgr.getFactoryPresets();
    const auto& user = mgr.getUserPresets();

    int id = 1;
    if (! fact.empty())
    {
        presetCombo.addSectionHeading ("FACTORY");
        for (auto& pr : fact)
            presetCombo.addItem (pr.name, id++);
    }
    if (! user.empty())
    {
        presetCombo.addSectionHeading ("USER");
        for (auto& pr : user)
            presetCombo.addItem (pr.name, id++);
    }

    auto current = mgr.getCurrentPresetName();
    for (int i = 1; i < id; ++i)
        if (presetCombo.getItemText (i - 1) == current)
        {
            presetCombo.setSelectedId (i, juce::dontSendNotification);
            break;
        }
}

void CRV1AudioProcessorEditor::rebuildIRCombo()
{
    irCombo.clear (juce::dontSendNotification);

    const auto& entries = audioProcessor.getIRLibrary().getEntries();
    int id = 1;

    bool addedFactoryHeader = false;
    bool addedUserHeader    = false;

    for (auto& e : entries)
    {
        if (e.isFactory)
        {
            if (! addedFactoryHeader) { irCombo.addSectionHeading ("FACTORY IMPULSES"); addedFactoryHeader = true; }
            irCombo.addItem (e.name, id++);
        }
    }
    for (auto& e : entries)
    {
        if (! e.isFactory)
        {
            if (! addedUserHeader) { irCombo.addSectionHeading ("USER IMPULSES"); addedUserHeader = true; }
            irCombo.addItem (e.name, id++);
        }
    }

    auto current = audioProcessor.getSelectedIRName();
    for (int i = 1; i < id; ++i)
        if (irCombo.getItemText (i - 1) == current)
        {
            irCombo.setSelectedId (i, juce::dontSendNotification);
            break;
        }
}

void CRV1AudioProcessorEditor::onPresetComboChanged()
{
    auto& mgr = audioProcessor.getPresetManager();
    auto name = presetCombo.getText();
    if (name.isEmpty()) return;
    if (name == mgr.getCurrentPresetName()) return;
    mgr.loadPresetByName (name);
}

void CRV1AudioProcessorEditor::onIRComboChanged()
{
    auto name = irCombo.getText();
    if (name.isEmpty()) return;
    if (name == audioProcessor.getSelectedIRName()) return;
    audioProcessor.setSelectedIRName (name);
}

void CRV1AudioProcessorEditor::showSavePresetDialog()
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
                    audioProcessor.getPresetManager().saveUserPreset (
                        name, audioProcessor.getSelectedIRName());
            }
            delete aw;
        }), false);
}

void CRV1AudioProcessorEditor::showDeletePresetDialog()
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

void CRV1AudioProcessorEditor::openPresetsFolder()
{
    audioProcessor.getPresetManager().getUserPresetsFolder().revealToUser();
}

void CRV1AudioProcessorEditor::loadCustomIRFromDisk()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose an impulse response (WAV, AIFF, FLAC)",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (! file.existsAsFile()) return;

            if (! audioProcessor.getIRLibrary().importUserIR (file))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::WarningIcon,
                    "Could not import IR",
                    "The file could not be copied into the user IR folder. "
                    "Check that the file is readable and not in use.");
                return;
            }

            rebuildIRCombo();

            const juce::String displayName = "[User] " + file.getFileNameWithoutExtension();
            audioProcessor.setSelectedIRName (displayName);
            rebuildIRCombo();
        });
}

void CRV1AudioProcessorEditor::deleteSelectedUserIR()
{
    auto name = audioProcessor.getSelectedIRName();
    if (! name.startsWith ("[User]"))
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "Cannot delete factory IR",
            "Only user-imported IRs can be deleted. Select a [User] IR from the list first.");
        return;
    }

    juce::AlertWindow::showOkCancelBox (
        juce::AlertWindow::QuestionIcon,
        "Delete User IR",
        "Delete user IR \"" + name + "\"?\n"
        "The file will be removed from your user IR folder.",
        "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create ([this, name] (int result)
        {
            if (result == 1)
            {
                audioProcessor.getIRLibrary().deleteUserIR (name);
                rebuildIRCombo();

                const auto& entries = audioProcessor.getIRLibrary().getEntries();
                if (! entries.empty())
                    audioProcessor.setSelectedIRName (entries.front().name);
                rebuildIRCombo();
            }
        }));
}

void CRV1AudioProcessorEditor::openIRFolder()
{
    audioProcessor.getIRLibrary().getUserIRFolder().revealToUser();
}

void CRV1AudioProcessorEditor::irRenderedFromProcessor (const juce::AudioBuffer<float>& ir,
                                                        double sampleRate,
                                                        const juce::String& info)
{
    visualizer.setImpulseResponse (ir, sampleRate);
    visualizer.setIRInfoText (info);
    visualizer.repaint();
}

//==============================================================================
void CRV1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background: deep luxurious gradient
    juce::ColourGradient bg (
        juce::Colour (0xff141a23), bounds.getCentreX(), bounds.getY(),
        juce::Colour (0xff060a10), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Top header bar
    g.setColour (juce::Colour (0xff0c1118));
    g.fillRect (bounds.removeFromTop (70.0f));
    g.setColour (juce::Colour (0xff2a3f5c));
    g.drawHorizontalLine (70, 0.0f, (float) getWidth());
    g.setColour (juce::Colour (0x554d9eff));
    g.drawHorizontalLine (71, 0.0f, (float) getWidth());

    // Title underline shimmer
    juce::ColourGradient titleAccent (
        juce::Colour (0xff4d9eff), 24.0f, 60.0f,
        juce::Colour (0x004d9eff), 260.0f, 60.0f, false);
    g.setGradientFill (titleAccent);
    g.fillRect (juce::Rectangle<float> (24.0f, 58.0f, 240.0f, 1.5f));

    // IR bar background
    auto irBar = juce::Rectangle<float> (12.0f, 80.0f, (float) getWidth() - 24.0f, 36.0f);
    g.setColour (juce::Colour (0xff10161f));
    g.fillRoundedRectangle (irBar, 6.0f);
    g.setColour (juce::Colour (0xff2a3340));
    g.drawRoundedRectangle (irBar, 6.0f, 1.0f);

    // Main knob panel
    auto mainKnobsArea = juce::Rectangle<float> (
        12.0f, 430.0f,
        (float) getWidth() - 24.0f, 150.0f);
    g.setColour (juce::Colour (0xff10161f));
    g.fillRoundedRectangle (mainKnobsArea, 8.0f);
    g.setColour (juce::Colour (0xff2a3340));
    g.drawRoundedRectangle (mainKnobsArea, 8.0f, 1.0f);

    // Secondary controls panel
    auto bottomBar = juce::Rectangle<float> (
        12.0f, 590.0f, (float) getWidth() - 24.0f, 78.0f);
    g.setColour (juce::Colour (0xff10161f));
    g.fillRoundedRectangle (bottomBar, 8.0f);
    g.setColour (juce::Colour (0xff2a3340));
    g.drawRoundedRectangle (bottomBar, 8.0f, 1.0f);
}

void CRV1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ---- Header ----
    auto header = bounds.removeFromTop (70);
    auto titleArea = header.removeFromLeft (260).reduced (24, 8);
    titleLabel.setBounds (titleArea.removeFromTop (38));
    subtitleLabel.setBounds (titleArea);

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

    // ---- IR selector row ----
    auto irRow = juce::Rectangle<int> (16, 84, getWidth() - 32, 28);
    irLabel.setBounds (irRow.removeFromLeft (70));
    irRow.removeFromLeft (4);
    irFolderBtn.setBounds (irRow.removeFromRight (90));
    irRow.removeFromRight (6);
    delIRBtn.setBounds (irRow.removeFromRight (90));
    irRow.removeFromRight (6);
    loadIRBtn.setBounds (irRow.removeFromRight (110));
    irRow.removeFromRight (10);
    irCombo.setBounds (irRow);

    // ---- Visualizer ----
    auto vizArea = juce::Rectangle<int> (12, 122, getWidth() - 24, 300);
    visualizer.setBounds (vizArea);

    // ---- Main knobs (5 across the top of the panel) ----
    auto knobsArea = juce::Rectangle<int> (12, 430, getWidth() - 24, 150).reduced (10, 10);
    const int n = 5;
    const int knobW = knobsArea.getWidth() / n;

    auto layoutKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        auto labelArea = r.removeFromTop (18);
        k.label.setBounds (labelArea);
        k.slider.setBounds (r.reduced (4));
    };

    layoutKnob (sizeKnob,     knobsArea.removeFromLeft (knobW));
    layoutKnob (decayKnob,    knobsArea.removeFromLeft (knobW));
    layoutKnob (predelayKnob, knobsArea.removeFromLeft (knobW));
    layoutKnob (modKnob,      knobsArea.removeFromLeft (knobW));
    layoutKnob (mixKnob,      knobsArea.removeFromLeft (knobW));

    // ---- Bottom controls (4 secondary knobs) ----
    auto bottomBar = juce::Rectangle<int> (12, 590, getWidth() - 24, 78).reduced (10, 10);
    const int n2 = 4;
    const int sec = bottomBar.getWidth() / n2;
    layoutKnob (lowKnob,    bottomBar.removeFromLeft (sec));
    layoutKnob (highKnob,   bottomBar.removeFromLeft (sec));
    layoutKnob (widthKnob,  bottomBar.removeFromLeft (sec));
    layoutKnob (outputKnob, bottomBar.removeFromLeft (sec));
}
