/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KM1AudioProcessorEditor::KM1AudioProcessorEditor (KM1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetBar (p.presetManager)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (scope);

    auditionButton.setButtonText ("AUDITION");
    auditionButton.onClick = [this] { audioProcessor.requestAudition (1.0f); };
    addAndMakeVisible (auditionButton);

    configureKnob (tune,       "TUNE");
    configureKnob (pitchAmt,   "PITCH AMT");
    configureKnob (pitchTime,  "PITCH TIME");
    configureKnob (bodyDecay,  "DECAY");
    configureKnob (bodyShape,  "SHAPE");
    configureKnob (clickLevel, "CLICK");
    configureKnob (clickTone,  "CLICK TONE");
    configureKnob (subLevel,   "SUB");
    configureKnob (drive,      "DRIVE");
    configureKnob (punch,      "PUNCH");
    configureKnob (tone,       "TONE");
    configureKnob (output,     "OUTPUT");

    auto& apvts = audioProcessor.apvts;
    tuneAtt       = std::make_unique<SAtt> (apvts, "tune",       tune      .slider);
    pitchAmtAtt   = std::make_unique<SAtt> (apvts, "pitchAmt",   pitchAmt  .slider);
    pitchTimeAtt  = std::make_unique<SAtt> (apvts, "pitchTime",  pitchTime .slider);
    bodyDecayAtt  = std::make_unique<SAtt> (apvts, "bodyDecay",  bodyDecay .slider);
    bodyShapeAtt  = std::make_unique<SAtt> (apvts, "bodyShape",  bodyShape .slider);
    clickLevelAtt = std::make_unique<SAtt> (apvts, "clickLevel", clickLevel.slider);
    clickToneAtt  = std::make_unique<SAtt> (apvts, "clickTone",  clickTone .slider);
    subLevelAtt   = std::make_unique<SAtt> (apvts, "subLevel",   subLevel  .slider);
    driveAtt      = std::make_unique<SAtt> (apvts, "drive",      drive     .slider);
    punchAtt      = std::make_unique<SAtt> (apvts, "punch",      punch     .slider);
    toneAtt       = std::make_unique<SAtt> (apvts, "tone",       tone      .slider);
    outputAtt     = std::make_unique<SAtt> (apvts, "output",     output    .slider);

    for (auto* id : { "tune","pitchAmt","pitchTime","bodyDecay","bodyShape",
                      "clickLevel","clickTone","subLevel",
                      "drive","punch","tone","output" })
        apvts.addParameterListener (id, this);

    pushParamsToScope();

    setSize (980, 620);
    startTimerHz (30);
}

KM1AudioProcessorEditor::~KM1AudioProcessorEditor()
{
    auto& apvts = audioProcessor.apvts;
    for (auto* id : { "tune","pitchAmt","pitchTime","bodyDecay","bodyShape",
                      "clickLevel","clickTone","subLevel",
                      "drive","punch","tone","output" })
        apvts.removeParameterListener (id, this);

    setLookAndFeel (nullptr);
}

//==============================================================================
void KM1AudioProcessorEditor::configureKnob (KnobControl& k, const juce::String& titleText)
{
    addAndMakeVisible (k.title);
    k.title.setText (titleText, juce::dontSendNotification);
    k.title.setJustificationType (juce::Justification::centred);
    k.title.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    k.title.setColour (juce::Label::textColourId, juce::Colour (0xff8aa6c8));
    k.title.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k.slider);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f,
                                   true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x00000000));
    k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd6e6ff));
    k.slider.setVelocityBasedMode (true);
    k.slider.setVelocityModeParameters (0.7, 1, 0.09, false);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setDoubleClickReturnValue (false, 0.0);
}

void KM1AudioProcessorEditor::layoutKnob (KnobControl& k, juce::Rectangle<int> slot)
{
    auto title = slot.removeFromTop (20);
    k.title.setBounds (title);
    k.slider.setBounds (slot);
}

//==============================================================================
void KM1AudioProcessorEditor::pushParamsToScope()
{
    scope.setParams (audioProcessor.buildParamsSnapshot());
}

void KM1AudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    paramsDirty.store (true);
}

void KM1AudioProcessorEditor::timerCallback()
{
    if (paramsDirty.exchange (false))
        pushParamsToScope();

    float vel = 1.0f;
    if (audioProcessor.consumeTriggerEvent (vel) > 0)
        scope.notifyTriggered (vel);
}

//==============================================================================
void KM1AudioProcessorEditor::drawSectionPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                                const juce::String& title) const
{
    juce::ColourGradient pbg (juce::Colour (0xff1a212c), r.getCentreX(), (float) r.getY(),
                               juce::Colour (0xff0d1118), r.getCentreX(), (float) r.getBottom(), false);
    g.setGradientFill (pbg);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff262e3a));
    g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);

    g.setColour (juce::Colour (0x224d9eff));
    g.drawLine ((float) r.getX() + 14.0f, (float) r.getY() + 1.0f,
                (float) r.getRight() - 14.0f, (float) r.getY() + 1.0f, 1.0f);

    g.setColour (juce::Colour (0xaa4d9eff));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (title,
                juce::Rectangle<int> (r.getX() + 14, r.getY() + 6, 200, 14),
                juce::Justification::centredLeft);
}

void KM1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // ---------- main background ----------
    juce::ColourGradient bg (juce::Colour (0xff141a23), 0.0f, 0.0f,
                              juce::Colour (0xff0a0d12), 0.0f, (float) bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    {
        juce::ColourGradient sheen (juce::Colour (0x14a8c8ff), bounds.getWidth() * 0.5f, -120.0f,
                                     juce::Colour (0x00000000), bounds.getWidth() * 0.5f, 240.0f, true);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    // ---------- header band ----------
    {
        auto header = bounds.withHeight (62).reduced (12, 8);
        juce::ColourGradient hbg (juce::Colour (0xff1d2533), header.getCentreX(), (float) header.getY(),
                                   juce::Colour (0xff10141d), header.getCentreX(), (float) header.getBottom(), false);
        g.setGradientFill (hbg);
        g.fillRoundedRectangle (header.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (header.toFloat(), 8.0f, 1.0f);

        auto title = header.reduced (16, 6);

        g.setColour (juce::Colour (0xff8fc7ff));
        g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        g.drawText ("KM-1",
                    juce::Rectangle<int> (title.getX(), title.getY() + 2, 70, 24),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff4d9eff));
        g.fillRect (juce::Rectangle<int> (title.getX() + 70, title.getY() + 6, 1, 18));

        g.setColour (juce::Colour (0xffb6c8e2));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("LUXURY",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 4, 60, 12),
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff8aa6c8));
        g.drawText ("KICK MACHINE",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 16, 100, 12),
                    juce::Justification::centredLeft);
    }

    // ---------- knob section panels ----------
    if (! bodyPanelBounds.isEmpty())
        drawSectionPanel (g, bodyPanelBounds,   "BODY");
    if (! clickPanelBounds.isEmpty())
        drawSectionPanel (g, clickPanelBounds,  "TRANSIENT  ·  SUB");
    if (! masterPanelBounds.isEmpty())
        drawSectionPanel (g, masterPanelBounds, "MASTER");
}

void KM1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ---------- header row ----------
    auto header = bounds.removeFromTop (62).reduced (12, 8);
    {
        auto h = header;
        h.removeFromLeft (200); // logo area

        auditionButton.setBounds (h.removeFromRight (110).reduced (4, 6));
        h.removeFromRight (10);
        presetBar.setBounds (h.reduced (4, 0));
    }

    bounds.removeFromTop (10);

    // ---------- scope ----------
    auto scopeArea = bounds.removeFromTop (260).reduced (12, 0);
    scope.setBounds (scopeArea);

    bounds.removeFromTop (12);

    // ---------- knob area: 3 panels side by side ----------
    auto knobArea = bounds.reduced (12, 0);
    knobArea.removeFromBottom (12);

    const int gap         = 10;
    const int totalWidth  = knobArea.getWidth();
    const int bodyW       = (int) (totalWidth * 0.40f);
    const int clickW      = (int) (totalWidth * 0.27f);
    const int masterW     = totalWidth - bodyW - clickW - 2 * gap;

    auto bodyArea   = knobArea.removeFromLeft (bodyW);
    knobArea.removeFromLeft (gap);
    auto clickArea  = knobArea.removeFromLeft (clickW);
    knobArea.removeFromLeft (gap);
    auto masterArea = knobArea;

    bodyPanelBounds   = bodyArea;
    clickPanelBounds  = clickArea;
    masterPanelBounds = masterArea;

    auto layoutKnobsInPanel = [this] (juce::Rectangle<int> panel,
                                       std::initializer_list<KnobControl*> knobs)
    {
        panel.reduce (12, 18);
        panel.removeFromTop (8);  // panel title

        const int n = (int) knobs.size();
        if (n == 0) return;

        const int slotWidth = panel.getWidth() / n;
        int i = 0;
        for (auto* k : knobs)
        {
            auto slot = juce::Rectangle<int> (panel.getX() + i * slotWidth,
                                               panel.getY(),
                                               slotWidth,
                                               panel.getHeight()).reduced (4, 0);
            layoutKnob (*k, slot);
            ++i;
        }
    };

    layoutKnobsInPanel (bodyArea,
                        { &tune, &pitchAmt, &pitchTime, &bodyDecay, &bodyShape });
    layoutKnobsInPanel (clickArea,
                        { &clickLevel, &clickTone, &subLevel });
    layoutKnobsInPanel (masterArea,
                        { &drive, &punch, &tone, &output });
}
