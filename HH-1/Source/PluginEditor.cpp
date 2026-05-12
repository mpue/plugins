/*
  ==============================================================================

    PluginEditor.cpp
    HH-1 Luxury Hi-Hat Machine

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
HH1AudioProcessorEditor::HH1AudioProcessorEditor (HH1AudioProcessor& p)
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

    configureKnob (tune,      "TUNE");
    configureKnob (metal,     "METAL");
    configureKnob (harmonics, "SPREAD");
    configureKnob (hpCut,     "HP CUT");
    configureKnob (bpCut,     "BRIGHT");
    configureKnob (shimmerQ,  "SHIMMER");

    configureKnob (decay,     "DECAY");
    configureKnob (hold,      "HOLD");
    configureKnob (noise,     "AIR");
    configureKnob (color,     "COLOR");

    configureKnob (drive,     "DRIVE");
    configureKnob (tone,      "TONE");
    configureKnob (width,     "WIDTH");
    configureKnob (output,    "OUTPUT");

    auto& apvts = audioProcessor.apvts;
    tuneAtt      = std::make_unique<SAtt> (apvts, "tune",      tune     .slider);
    metalAtt     = std::make_unique<SAtt> (apvts, "metal",     metal    .slider);
    harmonicsAtt = std::make_unique<SAtt> (apvts, "harmonics", harmonics.slider);
    hpCutAtt     = std::make_unique<SAtt> (apvts, "hpCut",     hpCut    .slider);
    bpCutAtt     = std::make_unique<SAtt> (apvts, "bpCut",     bpCut    .slider);
    shimmerQAtt  = std::make_unique<SAtt> (apvts, "shimmerQ",  shimmerQ .slider);

    decayAtt     = std::make_unique<SAtt> (apvts, "decay",     decay    .slider);
    holdAtt      = std::make_unique<SAtt> (apvts, "hold",      hold     .slider);
    noiseAtt     = std::make_unique<SAtt> (apvts, "noise",     noise    .slider);
    colorAtt     = std::make_unique<SAtt> (apvts, "color",     color    .slider);

    driveAtt     = std::make_unique<SAtt> (apvts, "drive",     drive    .slider);
    toneAtt      = std::make_unique<SAtt> (apvts, "tone",      tone     .slider);
    widthAtt     = std::make_unique<SAtt> (apvts, "width",     width    .slider);
    outputAtt    = std::make_unique<SAtt> (apvts, "output",    output   .slider);

    for (auto* id : { "tune","metal","harmonics","hpCut","bpCut","shimmerQ",
                      "decay","hold","noise","color",
                      "drive","tone","width","output" })
        apvts.addParameterListener (id, this);

    pushParamsToScope();

    setSize (1080, 660);
    startTimerHz (30);
}

HH1AudioProcessorEditor::~HH1AudioProcessorEditor()
{
    auto& apvts = audioProcessor.apvts;
    for (auto* id : { "tune","metal","harmonics","hpCut","bpCut","shimmerQ",
                      "decay","hold","noise","color",
                      "drive","tone","width","output" })
        apvts.removeParameterListener (id, this);

    setLookAndFeel (nullptr);
}

//==============================================================================
void HH1AudioProcessorEditor::configureKnob (KnobControl& k, const juce::String& titleText)
{
    addAndMakeVisible (k.title);
    k.title.setText (titleText, juce::dontSendNotification);
    k.title.setJustificationType (juce::Justification::centred);
    k.title.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    k.title.setColour (juce::Label::textColourId, juce::Colour (0xffd0a878));
    k.title.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k.slider);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f,
                                   true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x00000000));
    k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffffe6c2));
    k.slider.setVelocityBasedMode (true);
    k.slider.setVelocityModeParameters (0.7, 1, 0.09, false);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setDoubleClickReturnValue (false, 0.0);
}

void HH1AudioProcessorEditor::layoutKnob (KnobControl& k, juce::Rectangle<int> slot)
{
    auto title = slot.removeFromTop (20);
    k.title.setBounds (title);
    k.slider.setBounds (slot);
}

//==============================================================================
void HH1AudioProcessorEditor::pushParamsToScope()
{
    scope.setParams (audioProcessor.buildParamsSnapshot());
}

void HH1AudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    paramsDirty.store (true);
}

void HH1AudioProcessorEditor::timerCallback()
{
    if (paramsDirty.exchange (false))
        pushParamsToScope();

    float vel = 1.0f;
    if (audioProcessor.consumeTriggerEvent (vel) > 0)
        scope.notifyTriggered (vel);
}

//==============================================================================
void HH1AudioProcessorEditor::drawSectionPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                                const juce::String& title) const
{
    juce::ColourGradient pbg (juce::Colour (0xff1a212c), r.getCentreX(), (float) r.getY(),
                               juce::Colour (0xff0d1118), r.getCentreX(), (float) r.getBottom(), false);
    g.setGradientFill (pbg);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff262e3a));
    g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);

    g.setColour (juce::Colour (0x33ffa860));
    g.drawLine ((float) r.getX() + 14.0f, (float) r.getY() + 1.0f,
                (float) r.getRight() - 14.0f, (float) r.getY() + 1.0f, 1.0f);

    g.setColour (juce::Colour (0xaaffc97a));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (title,
                juce::Rectangle<int> (r.getX() + 14, r.getY() + 6, 240, 14),
                juce::Justification::centredLeft);
}

void HH1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    juce::ColourGradient bg (juce::Colour (0xff141a23), 0.0f, 0.0f,
                              juce::Colour (0xff0a0d12), 0.0f, (float) bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    {
        juce::ColourGradient sheen (juce::Colour (0x18ffd4a0), bounds.getWidth() * 0.5f, -120.0f,
                                     juce::Colour (0x00000000), bounds.getWidth() * 0.5f, 240.0f, true);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    {
        auto header = bounds.withHeight (62).reduced (12, 8);
        juce::ColourGradient hbg (juce::Colour (0xff1d2533), header.getCentreX(), (float) header.getY(),
                                   juce::Colour (0xff10141d), header.getCentreX(), (float) header.getBottom(), false);
        g.setGradientFill (hbg);
        g.fillRoundedRectangle (header.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (header.toFloat(), 8.0f, 1.0f);

        auto title = header.reduced (16, 6);

        g.setColour (juce::Colour (0xffffd29c));
        g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        g.drawText ("HH-1",
                    juce::Rectangle<int> (title.getX(), title.getY() + 2, 70, 24),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xffffa860));
        g.fillRect (juce::Rectangle<int> (title.getX() + 70, title.getY() + 6, 1, 18));

        g.setColour (juce::Colour (0xffe2c8a8));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("LUXURY",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 4, 60, 12),
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffc8a878));
        g.drawText ("HI-HAT MACHINE",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 16, 130, 12),
                    juce::Justification::centredLeft);
    }

    if (! metalPanelBounds.isEmpty())
        drawSectionPanel (g, metalPanelBounds,    "METAL  \xc2\xb7  BODY");
    if (! texturePanelBounds.isEmpty())
        drawSectionPanel (g, texturePanelBounds,  "DECAY  \xc2\xb7  TEXTURE");
    if (! masterPanelBounds.isEmpty())
        drawSectionPanel (g, masterPanelBounds,   "MASTER");
}

void HH1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto header = bounds.removeFromTop (62).reduced (12, 8);
    {
        auto h = header;
        h.removeFromLeft (220);

        auditionButton.setBounds (h.removeFromRight (110).reduced (4, 6));
        h.removeFromRight (10);
        presetBar.setBounds (h.reduced (4, 0));
    }

    bounds.removeFromTop (10);

    auto scopeArea = bounds.removeFromTop (260).reduced (12, 0);
    scope.setBounds (scopeArea);

    bounds.removeFromTop (12);

    auto knobArea = bounds.reduced (12, 0);
    knobArea.removeFromBottom (12);

    const int gap         = 10;
    const int totalWidth  = knobArea.getWidth();
    const int metalW      = (int) (totalWidth * 0.43f);
    const int textureW    = (int) (totalWidth * 0.30f);
    const int masterW     = totalWidth - metalW - textureW - 2 * gap;
    juce::ignoreUnused (masterW);

    auto metalArea   = knobArea.removeFromLeft (metalW);
    knobArea.removeFromLeft (gap);
    auto textureArea = knobArea.removeFromLeft (textureW);
    knobArea.removeFromLeft (gap);
    auto masterArea  = knobArea;

    metalPanelBounds   = metalArea;
    texturePanelBounds = textureArea;
    masterPanelBounds  = masterArea;

    auto layoutKnobsInPanel = [this] (juce::Rectangle<int> panel,
                                       std::initializer_list<KnobControl*> knobs)
    {
        panel.reduce (12, 18);
        panel.removeFromTop (8);

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

    layoutKnobsInPanel (metalArea,
                        { &tune, &metal, &harmonics, &hpCut, &bpCut, &shimmerQ });
    layoutKnobsInPanel (textureArea,
                        { &decay, &hold, &noise, &color });
    layoutKnobsInPanel (masterArea,
                        { &drive, &tone, &width, &output });
}
