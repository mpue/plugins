/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FP1AudioProcessorEditor::FP1AudioProcessorEditor (FP1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetBar (p.getPresetManager()),
      visualizer (p.getEngine())
{
    setLookAndFeel (&lookAndFeel);

    // Header
    titleLabel.setText ("FP-1", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (28.0f, juce::Font::FontStyleFlags::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8f0ff));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Flanger + Phaser", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (12.0f, juce::Font::FontStyleFlags::plain));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7a99c0));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (visualizer);

    auto& apvts = audioProcessor.getAPVTS();

    // Mode buttons — manual three-state toggle, written straight to the Mode parameter.
    for (auto* b : { &flangerBtn, &phaserBtn, &hybridBtn })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1c2438));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a52a8));
        addAndMakeVisible (b);
    }
    flangerBtn.onClick = [this] { setMode (0); };
    phaserBtn .onClick = [this] { setMode (1); };
    hybridBtn .onClick = [this] { setMode (2); };
    syncModeButtons();

    // Knobs + APVTS attachments
    buildKnob (rateKnob,     "rate",     rateAtt);
    buildKnob (depthKnob,    "depth",    depthAtt);
    buildKnob (manualKnob,   "manual",   manualAtt);
    buildKnob (feedbackKnob, "feedback", feedbackAtt);
    buildKnob (widthKnob,    "width",    widthAtt);
    buildKnob (toneKnob,     "tone",     toneAtt);
    buildKnob (mixKnob,      "mix",      mixAtt);
    buildKnob (outKnob,      "gain",     outAtt);

    // LFO shape
    shapeLabel.setText ("LFO", juce::dontSendNotification);
    shapeLabel.setJustificationType (juce::Justification::centred);
    shapeLabel.setFont (juce::Font (12.0f));
    addAndMakeVisible (shapeLabel);
    shapeBox.addItem ("Sine",     1);
    shapeBox.addItem ("Triangle", 2);
    shapeBox.addItem ("Drift",    3);
    addAndMakeVisible (shapeBox);
    shapeAtt = std::make_unique<ComboBoxAttachment> (apvts, "lfoShape", shapeBox);

    // Stages
    stagesLabel.setText ("Stages", juce::dontSendNotification);
    stagesLabel.setJustificationType (juce::Justification::centred);
    stagesLabel.setFont (juce::Font (12.0f));
    addAndMakeVisible (stagesLabel);
    stagesBox.addItem ("4",  1);
    stagesBox.addItem ("6",  2);
    stagesBox.addItem ("8",  3);
    stagesBox.addItem ("12", 4);
    addAndMakeVisible (stagesBox);
    stagesAtt = std::make_unique<ComboBoxAttachment> (apvts, "stages", stagesBox);

    // Bypass
    addAndMakeVisible (bypassButton);
    bypassAtt = std::make_unique<ButtonAttachment> (apvts, "bypass", bypassButton);

    // Polling timer keeps the mode buttons in sync with presets / automation
    startTimerHz (15);

    setSize (820, 560);
}

FP1AudioProcessorEditor::~FP1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void FP1AudioProcessorEditor::buildKnob (LabelledKnob& k, const juce::String& paramID,
                                         std::unique_ptr<SliderAttachment>& attachment)
{
    addAndMakeVisible (k);
    attachment = std::make_unique<SliderAttachment> (audioProcessor.getAPVTS(), paramID, k.slider);
}

void FP1AudioProcessorEditor::timerCallback()
{
    syncModeButtons();
}

void FP1AudioProcessorEditor::setMode (int newMode)
{
    auto* p = audioProcessor.getAPVTS().getParameter ("mode");
    if (p == nullptr) return;
    const auto normalised = p->convertTo0to1 ((float) newMode);
    p->beginChangeGesture();
    p->setValueNotifyingHost (normalised);
    p->endChangeGesture();
    syncModeButtons();
}

void FP1AudioProcessorEditor::syncModeButtons()
{
    auto* p = audioProcessor.getAPVTS().getRawParameterValue ("mode");
    if (p == nullptr) return;
    const int mode = juce::jlimit (0, 2, (int) p->load());

    flangerBtn.setToggleState (mode == 0, juce::dontSendNotification);
    phaserBtn .setToggleState (mode == 1, juce::dontSendNotification);
    hybridBtn .setToggleState (mode == 2, juce::dontSendNotification);
}

//==============================================================================
void FP1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Deep blue gradient background
    juce::ColourGradient bg (juce::Colour (0xff1a2030), bounds.getX(), bounds.getY(),
                             juce::Colour (0xff0d1118), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    // Top accent
    g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.25f));
    g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, bounds.getWidth(), 1.5f));

    // Section divider under the header
    g.setColour (juce::Colour (0xff2a3344));
    g.drawLine (12.0f, 60.0f, bounds.getWidth() - 12.0f, 60.0f, 1.0f);

    // Card background under the controls row
    auto card = getLocalBounds().reduced (10).removeFromBottom (250).toFloat().reduced (2.0f);
    g.setColour (juce::Colour (0xff141a26));
    g.fillRoundedRectangle (card, 10.0f);
    g.setColour (juce::Colour (0xff2a3344));
    g.drawRoundedRectangle (card, 10.0f, 1.0f);
}

void FP1AudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);

    // ---- Header ----
    auto header = r.removeFromTop (50);
    auto titleArea = header.removeFromLeft (220);
    titleLabel   .setBounds (titleArea.removeFromTop (32));
    subtitleLabel.setBounds (titleArea);

    header.removeFromLeft (10);
    presetBar.setBounds (header.reduced (0, 6));

    r.removeFromTop (8);

    // ---- Mode strip ----
    auto modeStrip = r.removeFromTop (34);
    {
        auto modeArea = modeStrip.reduced (4, 4);
        const int btnW = juce::jmin (110, modeArea.getWidth() / 4);
        flangerBtn.setBounds (modeArea.removeFromLeft (btnW));
        modeArea.removeFromLeft (6);
        phaserBtn .setBounds (modeArea.removeFromLeft (btnW));
        modeArea.removeFromLeft (6);
        hybridBtn .setBounds (modeArea.removeFromLeft (btnW));
    }

    r.removeFromTop (6);

    // ---- Visualizer ----
    auto visArea = r.removeFromTop (220);
    visualizer.setBounds (visArea);

    r.removeFromTop (6);

    // ---- Bottom controls card ----
    auto controls = r.reduced (8, 8);

    // Right column
    auto right = controls.removeFromRight (130);
    {
        auto sSection = right.removeFromTop (44);
        shapeLabel.setBounds (sSection.removeFromTop (16));
        shapeBox  .setBounds (sSection.reduced (4, 2));

        right.removeFromTop (4);

        auto stSection = right.removeFromTop (44);
        stagesLabel.setBounds (stSection.removeFromTop (16));
        stagesBox  .setBounds (stSection.reduced (4, 2));

        right.removeFromTop (6);
        bypassButton.setBounds (right.removeFromTop (28).reduced (4, 2));
        right.removeFromTop (6);
        outKnob.setBounds (right);
    }

    controls.removeFromRight (10);

    LabelledKnob* knobs[] = { &rateKnob, &depthKnob, &manualKnob, &feedbackKnob,
                              &widthKnob, &toneKnob, &mixKnob };
    const int n = juce::numElementsInArray (knobs);
    const int knobW = controls.getWidth() / n;

    for (int i = 0; i < n; ++i)
    {
        auto cell = controls.removeFromLeft (knobW).reduced (2, 4);
        knobs[i]->setBounds (cell);
    }
}
