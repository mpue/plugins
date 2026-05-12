/*
  ==============================================================================

    PluginEditor.cpp
    BS-1 Luxury Bass Synthesizer

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BS1AudioProcessorEditor::BS1AudioProcessorEditor (BS1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetBar (p.presetManager),
      keyboard  (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (scope);
    scope.setSampleRateHint (audioProcessor.getSampleRate() > 0.0
                              ? audioProcessor.getSampleRate() : 48000.0);

    addAndMakeVisible (keyboard);
    keyboard.setKeyWidth (16.0f);
    keyboard.setLowestVisibleKey (24); // C1
    keyboard.setOctaveForMiddleC (4);
    keyboard.setAvailableRange (0, 127);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe2eaf2));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xff4d9eff).withAlpha (0.85f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0xff4d9eff).withAlpha (0.30f));

    auditionButton.setButtonText ("AUDITION");
    auditionButton.setTriggeredOnMouseDown (true);
    auditionButton.addMouseListener (this, false);
    addAndMakeVisible (auditionButton);

    configureKnob (tone,        "TONE");
    configureKnob (drive,       "DRIVE");
    configureKnob (subLevel,    "SUB");
    configureKnob (noiseLevel,  "NOISE");

    configureKnob (cutoff,      "CUTOFF");
    configureKnob (resonance,   "RESO");
    configureKnob (envAmount,   "ENV AMT");
    configureKnob (filterDecay, "DECAY");

    configureKnob (ampAttack,   "ATTACK");
    configureKnob (ampSustain,  "SUSTAIN");
    configureKnob (ampRelease,  "RELEASE");

    configureKnob (glide,       "GLIDE");
    configureKnob (warmth,      "WARMTH");
    configureKnob (output,      "OUTPUT");

    addAndMakeVisible (octaveBox);
    octaveBox.addItem ("-2", 1);
    octaveBox.addItem ("-1", 2);
    octaveBox.addItem (" 0", 3);
    octaveBox.addItem ("+1", 4);
    octaveBox.addItem ("+2", 5);
    octaveBox.setJustificationType (juce::Justification::centred);

    auto& apvts = audioProcessor.apvts;
    toneAtt   = std::make_unique<SAtt> (apvts, "tone",        tone.slider);
    driveAtt  = std::make_unique<SAtt> (apvts, "drive",       drive.slider);
    subAtt    = std::make_unique<SAtt> (apvts, "subLevel",    subLevel.slider);
    noiseAtt  = std::make_unique<SAtt> (apvts, "noiseLevel",  noiseLevel.slider);

    cutoffAtt = std::make_unique<SAtt> (apvts, "cutoff",      cutoff.slider);
    resoAtt   = std::make_unique<SAtt> (apvts, "resonance",   resonance.slider);
    envAtt    = std::make_unique<SAtt> (apvts, "envAmount",   envAmount.slider);
    decAtt    = std::make_unique<SAtt> (apvts, "filterDecay", filterDecay.slider);

    atkAtt    = std::make_unique<SAtt> (apvts, "ampAttack",   ampAttack.slider);
    susAtt    = std::make_unique<SAtt> (apvts, "ampSustain",  ampSustain.slider);
    relAtt    = std::make_unique<SAtt> (apvts, "ampRelease",  ampRelease.slider);

    glideAtt  = std::make_unique<SAtt> (apvts, "glide",       glide.slider);
    warmthAtt = std::make_unique<SAtt> (apvts, "warmth",      warmth.slider);
    outAtt    = std::make_unique<SAtt> (apvts, "output",      output.slider);

    octAtt    = std::make_unique<BAtt> (apvts, "octave",      octaveBox);

    setSize (1080, 720);
    startTimerHz (45);
}

BS1AudioProcessorEditor::~BS1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void BS1AudioProcessorEditor::configureKnob (KnobControl& k, const juce::String& titleText)
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

void BS1AudioProcessorEditor::layoutKnob (KnobControl& k, juce::Rectangle<int> slot)
{
    auto title = slot.removeFromTop (20);
    k.title.setBounds (title);
    k.slider.setBounds (slot);
}

//==============================================================================
void BS1AudioProcessorEditor::timerCallback()
{
    // pull recent audio from processor → push to scope
    static thread_local std::array<float, 1024> tmp;
    audioProcessor.pullVisAudio (tmp.data(), (int) tmp.size());
    scope.pushAudio (tmp.data(), (int) tmp.size());

    float vel = 1.0f;
    if (audioProcessor.consumeTriggerEvent (vel) > 0)
        scope.notifyTriggered (vel);
}

void BS1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &auditionButton)
        audioProcessor.requestAudition (0.95f);
}

void BS1AudioProcessorEditor::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent == &auditionButton)
        audioProcessor.requestAuditionRelease();
}

//==============================================================================
void BS1AudioProcessorEditor::drawSectionPanel (juce::Graphics& g, juce::Rectangle<int> r,
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
                juce::Rectangle<int> (r.getX() + 14, r.getY() + 6, 240, 14),
                juce::Justification::centredLeft);
}

void BS1AudioProcessorEditor::paint (juce::Graphics& g)
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
        g.drawText ("BS-1",
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
        g.drawText ("BASS  SYNTHESIZER",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 16, 200, 12),
                    juce::Justification::centredLeft);
    }

    // ---------- knob section panels ----------
    if (! tonePanelBounds.isEmpty())   drawSectionPanel (g, tonePanelBounds,   "OSCILLATORS");
    if (! filterPanelBounds.isEmpty()) drawSectionPanel (g, filterPanelBounds, "FILTER");
    if (! envPanelBounds.isEmpty())    drawSectionPanel (g, envPanelBounds,    "AMP  ·  ENVELOPE");
    if (! masterPanelBounds.isEmpty()) drawSectionPanel (g, masterPanelBounds, "MASTER");

    // octave box label
    if (! tonePanelBounds.isEmpty())
    {
        const auto box = octaveBox.getBounds();
        g.setColour (juce::Colour (0xff8aa6c8));
        g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
        g.drawText ("OCTAVE",
                    juce::Rectangle<int> (box.getX(), box.getY() - 14, box.getWidth(), 12),
                    juce::Justification::centred);
    }
}

void BS1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ---------- header row ----------
    auto header = bounds.removeFromTop (62).reduced (12, 8);
    {
        auto h = header;
        h.removeFromLeft (220); // logo area

        auditionButton.setBounds (h.removeFromRight (110).reduced (4, 6));
        h.removeFromRight (10);
        presetBar.setBounds (h.reduced (4, 0));
    }

    bounds.removeFromTop (10);

    // ---------- scope ----------
    auto scopeArea = bounds.removeFromTop (270).reduced (12, 0);
    scope.setBounds (scopeArea);

    bounds.removeFromTop (12);

    // ---------- on-screen keyboard at the bottom ----------
    auto keyArea = bounds.removeFromBottom (84).reduced (12, 6);
    keyboard.setBounds (keyArea);

    bounds.removeFromBottom (4);

    // ---------- knob area: 4 panels side by side ----------
    auto knobArea = bounds.reduced (12, 0);
    knobArea.removeFromBottom (4);

    const int gap         = 10;
    const int totalWidth  = knobArea.getWidth();
    const int toneW       = (int) (totalWidth * 0.30f);
    const int filterW     = (int) (totalWidth * 0.27f);
    const int envW        = (int) (totalWidth * 0.22f);
    const int masterW     = totalWidth - toneW - filterW - envW - 3 * gap;

    auto toneArea   = knobArea.removeFromLeft (toneW);
    knobArea.removeFromLeft (gap);
    auto filterArea = knobArea.removeFromLeft (filterW);
    knobArea.removeFromLeft (gap);
    auto envArea    = knobArea.removeFromLeft (envW);
    knobArea.removeFromLeft (gap);
    auto masterArea = knobArea;

    tonePanelBounds   = toneArea;
    filterPanelBounds = filterArea;
    envPanelBounds    = envArea;
    masterPanelBounds = masterArea;

    auto layoutKnobsInPanel = [this] (juce::Rectangle<int> panel,
                                       std::initializer_list<KnobControl*> knobs,
                                       int extraTopReserve = 0)
    {
        panel.reduce (12, 18);
        panel.removeFromTop (8 + extraTopReserve);  // panel title + optional extras

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
                        { &tone, &drive, &subLevel, &noiseLevel });

    // place octave selector at top-right of the tone panel
    {
        const int boxW = 56, boxH = 22;
        octaveBox.setBounds (toneArea.getRight() - boxW - 14,
                              toneArea.getY() + 26,
                              boxW, boxH);
    }

    layoutKnobsInPanel (filterArea,
                        { &cutoff, &resonance, &envAmount, &filterDecay });

    layoutKnobsInPanel (envArea,
                        { &ampAttack, &ampSustain, &ampRelease });

    layoutKnobsInPanel (masterArea,
                        { &glide, &warmth, &output });
}
