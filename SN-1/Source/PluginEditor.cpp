/*
  ==============================================================================

    PluginEditor.cpp
    SN-1 Luxury Snare Machine

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SN1AudioProcessorEditor::SN1AudioProcessorEditor (SN1AudioProcessor& p)
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
    configureKnob (pitchAmt,   "PITCH");
    configureKnob (pitchTime,  "P. TIME");
    configureKnob (bodyDecay,  "DECAY");
    configureKnob (bodyLevel,  "BODY");

    configureKnob (noiseLevel, "SIZZLE");
    configureKnob (noiseColor, "COLOR");
    configureKnob (noiseDecay, "TAIL");
    configureKnob (buzzQ,      "BUZZ");
    configureKnob (snapLevel,  "SNAP");

    configureKnob (drive,      "DRIVE");
    configureKnob (tone,       "TONE");
    configureKnob (output,     "OUTPUT");

    auto& apvts = audioProcessor.apvts;
    tuneAtt       = std::make_unique<SAtt> (apvts, "tune",       tune      .slider);
    pitchAmtAtt   = std::make_unique<SAtt> (apvts, "pitchAmt",   pitchAmt  .slider);
    pitchTimeAtt  = std::make_unique<SAtt> (apvts, "pitchTime",  pitchTime .slider);
    bodyDecayAtt  = std::make_unique<SAtt> (apvts, "bodyDecay",  bodyDecay .slider);
    bodyLevelAtt  = std::make_unique<SAtt> (apvts, "bodyLevel",  bodyLevel .slider);

    noiseLevelAtt = std::make_unique<SAtt> (apvts, "noiseLevel", noiseLevel.slider);
    noiseColorAtt = std::make_unique<SAtt> (apvts, "noiseColor", noiseColor.slider);
    noiseDecayAtt = std::make_unique<SAtt> (apvts, "noiseDecay", noiseDecay.slider);
    buzzQAtt      = std::make_unique<SAtt> (apvts, "buzzQ",      buzzQ     .slider);
    snapLevelAtt  = std::make_unique<SAtt> (apvts, "snapLevel",  snapLevel .slider);

    driveAtt      = std::make_unique<SAtt> (apvts, "drive",      drive     .slider);
    toneAtt       = std::make_unique<SAtt> (apvts, "tone",       tone      .slider);
    outputAtt     = std::make_unique<SAtt> (apvts, "output",     output    .slider);

    for (auto* id : { "tune","pitchAmt","pitchTime","bodyDecay","bodyLevel",
                      "noiseLevel","noiseColor","noiseDecay","buzzQ","snapLevel",
                      "drive","tone","output" })
        apvts.addParameterListener (id, this);

    pushParamsToScope();

    setSize (1020, 640);
    startTimerHz (30);
}

SN1AudioProcessorEditor::~SN1AudioProcessorEditor()
{
    auto& apvts = audioProcessor.apvts;
    for (auto* id : { "tune","pitchAmt","pitchTime","bodyDecay","bodyLevel",
                      "noiseLevel","noiseColor","noiseDecay","buzzQ","snapLevel",
                      "drive","tone","output" })
        apvts.removeParameterListener (id, this);

    setLookAndFeel (nullptr);
}

//==============================================================================
void SN1AudioProcessorEditor::configureKnob (KnobControl& k, const juce::String& titleText)
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

void SN1AudioProcessorEditor::layoutKnob (KnobControl& k, juce::Rectangle<int> slot)
{
    auto title = slot.removeFromTop (20);
    k.title.setBounds (title);
    k.slider.setBounds (slot);
}

//==============================================================================
void SN1AudioProcessorEditor::pushParamsToScope()
{
    scope.setParams (audioProcessor.buildParamsSnapshot());
}

void SN1AudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    paramsDirty.store (true);
}

void SN1AudioProcessorEditor::timerCallback()
{
    if (paramsDirty.exchange (false))
        pushParamsToScope();

    float vel = 1.0f;
    if (audioProcessor.consumeTriggerEvent (vel) > 0)
        scope.notifyTriggered (vel);
}

//==============================================================================
void SN1AudioProcessorEditor::drawSectionPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                                const juce::String& title) const
{
    juce::ColourGradient pbg (juce::Colour (0xff1a212c), r.getCentreX(), (float) r.getY(),
                               juce::Colour (0xff0d1118), r.getCentreX(), (float) r.getBottom(), false);
    g.setGradientFill (pbg);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff262e3a));
    g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);

    g.setColour (juce::Colour (0x226db6ff));
    g.drawLine ((float) r.getX() + 14.0f, (float) r.getY() + 1.0f,
                (float) r.getRight() - 14.0f, (float) r.getY() + 1.0f, 1.0f);

    g.setColour (juce::Colour (0xaa6db6ff));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (title,
                juce::Rectangle<int> (r.getX() + 14, r.getY() + 6, 220, 14),
                juce::Justification::centredLeft);
}

void SN1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // ---------- main background ----------
    juce::ColourGradient bg (juce::Colour (0xff141a23), 0.0f, 0.0f,
                              juce::Colour (0xff0a0d12), 0.0f, (float) bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    {
        juce::ColourGradient sheen (juce::Colour (0x14b4cdff), bounds.getWidth() * 0.5f, -120.0f,
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

        g.setColour (juce::Colour (0xff9fcaff));
        g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        g.drawText ("SN-1",
                    juce::Rectangle<int> (title.getX(), title.getY() + 2, 70, 24),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff6db6ff));
        g.fillRect (juce::Rectangle<int> (title.getX() + 70, title.getY() + 6, 1, 18));

        g.setColour (juce::Colour (0xffb6c8e2));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("LUXURY",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 4, 60, 12),
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff8aa6c8));
        g.drawText ("SNARE MACHINE",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 16, 120, 12),
                    juce::Justification::centredLeft);
    }

    // ---------- knob section panels ----------
    if (! tonePanelBounds.isEmpty())
        drawSectionPanel (g, tonePanelBounds,    "TONE  ·  BODY");
    if (! snaresPanelBounds.isEmpty())
        drawSectionPanel (g, snaresPanelBounds,  "SNARES  ·  WIRES");
    if (! masterPanelBounds.isEmpty())
        drawSectionPanel (g, masterPanelBounds,  "MASTER");
}

void SN1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ---------- header row ----------
    auto header = bounds.removeFromTop (62).reduced (12, 8);
    {
        auto h = header;
        h.removeFromLeft (210); // logo area

        auditionButton.setBounds (h.removeFromRight (110).reduced (4, 6));
        h.removeFromRight (10);
        presetBar.setBounds (h.reduced (4, 0));
    }

    bounds.removeFromTop (10);

    // ---------- scope ----------
    auto scopeArea = bounds.removeFromTop (260).reduced (12, 0);
    scope.setBounds (scopeArea);

    bounds.removeFromTop (12);

    // ---------- knob area: 3 panels side by side (5 / 5 / 3 knobs) ----------
    auto knobArea = bounds.reduced (12, 0);
    knobArea.removeFromBottom (12);

    const int gap         = 10;
    const int totalWidth  = knobArea.getWidth();
    const int toneW       = (int) (totalWidth * 0.385f);
    const int snaresW     = (int) (totalWidth * 0.385f);
    const int masterW     = totalWidth - toneW - snaresW - 2 * gap;
    juce::ignoreUnused (masterW);

    auto toneArea   = knobArea.removeFromLeft (toneW);
    knobArea.removeFromLeft (gap);
    auto snaresArea = knobArea.removeFromLeft (snaresW);
    knobArea.removeFromLeft (gap);
    auto masterArea = knobArea;

    tonePanelBounds   = toneArea;
    snaresPanelBounds = snaresArea;
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

    layoutKnobsInPanel (toneArea,
                        { &tune, &pitchAmt, &pitchTime, &bodyDecay, &bodyLevel });
    layoutKnobsInPanel (snaresArea,
                        { &noiseLevel, &noiseColor, &noiseDecay, &buzzQ, &snapLevel });
    layoutKnobsInPanel (masterArea,
                        { &drive, &tone, &output });
}
