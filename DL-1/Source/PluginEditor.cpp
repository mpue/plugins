/*
  ==============================================================================

    PluginEditor.cpp
    DL-1 — Luxury Stereo Delay Editor

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DL1AudioProcessorEditor::DL1AudioProcessorEditor (DL1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel(&laf);

    // ---- Header ----
    titleLabel.setText("DL-1", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(36.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe8f0ff));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Luxury Stereo Delay", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7fa6cf));
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitleLabel);

    // ---- Preset bar ----
    addAndMakeVisible(presetCombo);
    presetCombo.setTextWhenNothingSelected("— Select Preset —");
    presetCombo.onChange = [this]
    {
        const int id = presetCombo.getSelectedId();
        if (id <= 0) return;
        const int idx = id - 1;
        audioProcessor.getPresetManager().loadByIndex(idx);
    };

    auto styleHeaderBtn = [this](juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c2438));
        addAndMakeVisible(b);
    };

    styleHeaderBtn(prevPresetBtn);
    styleHeaderBtn(nextPresetBtn);
    styleHeaderBtn(savePresetBtn);
    styleHeaderBtn(saveAsPresetBtn);
    styleHeaderBtn(deletePresetBtn);

    prevPresetBtn.onClick     = [this] { audioProcessor.getPresetManager().previousPreset(); };
    nextPresetBtn.onClick     = [this] { audioProcessor.getPresetManager().nextPreset(); };
    savePresetBtn.onClick     = [this]
    {
        auto& pm = audioProcessor.getPresetManager();
        auto name = pm.getCurrentPresetName();
        if (name.isEmpty() || name == "Init")
            showSaveAsDialog();
        else
            pm.savePreset(name);
    };
    saveAsPresetBtn.onClick   = [this] { showSaveAsDialog(); };
    deletePresetBtn.onClick   = [this] { confirmAndDelete(); };

    // ---- Visualizer ----
    visualizer = std::make_unique<DelayVisualizer>(audioProcessor.getVisualState());
    addAndMakeVisible(visualizer.get());

    // ---- Knobs ----
    auto& v = audioProcessor.getAPVTS();

    configureSlider(inGainKnob,   " dB");
    configureSlider(outGainKnob,  " dB");
    configureSlider(timeLKnob,    " ms");
    configureSlider(timeRKnob,    " ms");
    configureSlider(feedbackKnob, " %");
    configureSlider(crossKnob,    " %");
    configureSlider(mixKnob,      " %");
    configureSlider(lowCutKnob,   " Hz");
    configureSlider(highCutKnob,  " Hz");
    configureSlider(driveKnob,    " %");
    configureSlider(modRateKnob,  " Hz");
    configureSlider(modDepthKnob, " %");
    configureSlider(widthKnob,    "");
    configureSlider(duckingKnob,  " %");

    addAndMakeVisible(inGainKnob);
    addAndMakeVisible(outGainKnob);
    addAndMakeVisible(timeLKnob);
    addAndMakeVisible(timeRKnob);
    addAndMakeVisible(feedbackKnob);
    addAndMakeVisible(crossKnob);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(lowCutKnob);
    addAndMakeVisible(highCutKnob);
    addAndMakeVisible(driveKnob);
    addAndMakeVisible(modRateKnob);
    addAndMakeVisible(modDepthKnob);
    addAndMakeVisible(widthKnob);
    addAndMakeVisible(duckingKnob);

    // Format helpers
    auto pct = [](juce::Slider& s)
    {
        s.textFromValueFunction = [](double v) { return juce::String((int)std::round(v * 100.0)) + " %"; };
        s.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() / 100.0; };
        s.updateText();
    };
    pct(feedbackKnob.slider);
    pct(crossKnob.slider);
    pct(mixKnob.slider);
    pct(driveKnob.slider);
    pct(modDepthKnob.slider);
    pct(duckingKnob.slider);

    // ms text
    timeLKnob.slider.textFromValueFunction = [](double v){ return juce::String(v, 1) + " ms"; };
    timeRKnob.slider.textFromValueFunction = [](double v){ return juce::String(v, 1) + " ms"; };
    timeLKnob.slider.updateText();
    timeRKnob.slider.updateText();

    // Hz pretty
    auto hz = [](juce::Slider& s)
    {
        s.textFromValueFunction = [](double v)
        {
            if (v >= 1000.0) return juce::String(v / 1000.0, 2) + " kHz";
            return juce::String((int)v) + " Hz";
        };
        s.updateText();
    };
    hz(lowCutKnob.slider);
    hz(highCutKnob.slider);
    hz(modRateKnob.slider);

    inGainAt   = std::make_unique<SliderAttachment>(v, "inGain",    inGainKnob.slider);
    outGainAt  = std::make_unique<SliderAttachment>(v, "outGain",   outGainKnob.slider);
    timeLAt    = std::make_unique<SliderAttachment>(v, "timeL",     timeLKnob.slider);
    timeRAt    = std::make_unique<SliderAttachment>(v, "timeR",     timeRKnob.slider);
    fbAt       = std::make_unique<SliderAttachment>(v, "feedback",  feedbackKnob.slider);
    xfAt       = std::make_unique<SliderAttachment>(v, "crossfeed", crossKnob.slider);
    mixAt      = std::make_unique<SliderAttachment>(v, "mix",       mixKnob.slider);
    lowCutAt   = std::make_unique<SliderAttachment>(v, "lowCut",    lowCutKnob.slider);
    highCutAt  = std::make_unique<SliderAttachment>(v, "highCut",   highCutKnob.slider);
    driveAt    = std::make_unique<SliderAttachment>(v, "drive",     driveKnob.slider);
    modRateAt  = std::make_unique<SliderAttachment>(v, "modRate",   modRateKnob.slider);
    modDepthAt = std::make_unique<SliderAttachment>(v, "modDepth",  modDepthKnob.slider);
    widthAt    = std::make_unique<SliderAttachment>(v, "width",     widthKnob.slider);
    duckingAt  = std::make_unique<SliderAttachment>(v, "ducking",   duckingKnob.slider);

    // ---- Sync controls ----
    addAndMakeVisible(linkBtn);
    addAndMakeVisible(syncBtn);

    auto setupCombo = [this](juce::ComboBox& cb)
    {
        cb.addItemList(DL1AudioProcessor::getSyncDivisionNames(), 1);
        addAndMakeVisible(cb);
    };
    setupCombo(divLCombo);
    setupCombo(divRCombo);

    auto setupLabel = [this](juce::Label& l)
    {
        l.setFont(juce::Font(10.0f, juce::Font::bold));
        l.setColour(juce::Label::textColourId, juce::Colour(0xff9bb6d8));
        l.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(l);
    };
    setupLabel(divLLabel);
    setupLabel(divRLabel);

    linkAt = std::make_unique<ButtonAttachment>(v, "linkTimes", linkBtn);
    syncAt = std::make_unique<ButtonAttachment>(v, "sync",      syncBtn);
    divLAt = std::make_unique<ComboBoxAttachment>(v, "divisionL", divLCombo);
    divRAt = std::make_unique<ComboBoxAttachment>(v, "divisionR", divRCombo);

    // Preset listener and initial UI sync
    audioProcessor.getPresetManager().addListener(this);
    rebuildPresetCombo();
    currentPresetChanged(audioProcessor.getPresetManager().getCurrentPresetName());

    setSize (940, 580);
}

DL1AudioProcessorEditor::~DL1AudioProcessorEditor()
{
    audioProcessor.getPresetManager().removeListener(this);
    setLookAndFeel(nullptr);
}

void DL1AudioProcessorEditor::configureSlider(LabeledKnob& k, const juce::String& /*suffix*/)
{
    k.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd0e0f5));
    k.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000));
    k.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
    k.slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                 juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setVelocityBasedMode(true);
    k.slider.setVelocityModeParameters(0.6, 1, 0.05, false);
}

//==============================================================================
void DL1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Deep luxe gradient background
    juce::ColourGradient bg(juce::Colour(0xff1a1f2a), b.getCentreX(), b.getY(),
                            juce::Colour(0xff0a0d12), b.getCentreX(), b.getBottom(), false);
    g.setGradientFill(bg);
    g.fillAll();

    // Soft top-edge sheen
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x224d9eff), b.getCentreX(), 0.0f,
                                           juce::Colour(0x004d9eff), b.getCentreX(), 80.0f, false));
    g.fillRect(b.removeFromTop(80.0f));

    // Header divider
    g.setColour(juce::Colour(0x224d9eff));
    g.drawHorizontalLine(72, 16.0f, (float)getWidth() - 16.0f);

    // Brand mark on the right
    g.setColour(juce::Colour(0x554d9eff));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("by Matthias Pueski", getLocalBounds().reduced(20, 10),
               juce::Justification::topRight, false);

    // Section labels
    auto drawSection = [&g](const juce::String& text, juce::Rectangle<int> r)
    {
        g.setColour(juce::Colour(0x66b8d4f0));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(text, r, juce::Justification::topLeft, false);
    };

    drawSection("MAIN",       juce::Rectangle<int>(20, 286, 200, 14));
    drawSection("CHARACTER",  juce::Rectangle<int>(20, 416, 200, 14));
}

void DL1AudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // ---- Header ----
    auto header = area.removeFromTop(72);
    titleLabel   .setBounds(header.removeFromLeft(140).reduced(16, 6));
    subtitleLabel.setBounds(20, 50, 220, 18);

    // Preset bar (right of header)
    auto presetBar = juce::Rectangle<int>(260, 22, getWidth() - 280, 36);
    prevPresetBtn  .setBounds(presetBar.removeFromLeft(28).reduced(0, 4));
    presetBar.removeFromLeft(4);
    presetCombo    .setBounds(presetBar.removeFromLeft(280).reduced(0, 4));
    presetBar.removeFromLeft(4);
    nextPresetBtn  .setBounds(presetBar.removeFromLeft(28).reduced(0, 4));
    presetBar.removeFromLeft(12);
    savePresetBtn  .setBounds(presetBar.removeFromLeft(60).reduced(0, 4));
    presetBar.removeFromLeft(4);
    saveAsPresetBtn.setBounds(presetBar.removeFromLeft(70).reduced(0, 4));
    presetBar.removeFromLeft(4);
    deletePresetBtn.setBounds(presetBar.removeFromLeft(70).reduced(0, 4));

    // ---- Visualizer ----
    auto vizArea = area.removeFromTop(190).reduced(16, 8);
    if (visualizer) visualizer->setBounds(vizArea);

    // Section: MAIN row
    area.removeFromTop(18); // section label space

    area.removeFromTop(120);

    // Sync sub-bar (above main row, on the left under the visualizer)
    auto syncBar = juce::Rectangle<int>(20, 304, getWidth() - 40, 24);
    linkBtn   .setBounds(syncBar.removeFromLeft(80));
    syncBar.removeFromLeft(8);
    syncBtn   .setBounds(syncBar.removeFromLeft(80));
    syncBar.removeFromLeft(20);
    divLLabel .setBounds(syncBar.removeFromLeft(46));
    divLCombo .setBounds(syncBar.removeFromLeft(80));
    syncBar.removeFromLeft(12);
    divRLabel .setBounds(syncBar.removeFromLeft(46));
    divRCombo .setBounds(syncBar.removeFromLeft(80));

    // The 7 main knobs: input, timeL, timeR, feedback, cross, mix, output
    auto mainKnobs = juce::Rectangle<int>(20, 334, getWidth() - 40, 110);
    const int n1 = 7;
    const int kw = mainKnobs.getWidth() / n1;
    inGainKnob   .setBounds(mainKnobs.removeFromLeft(kw).reduced(4));
    timeLKnob    .setBounds(mainKnobs.removeFromLeft(kw).reduced(4));
    timeRKnob    .setBounds(mainKnobs.removeFromLeft(kw).reduced(4));
    feedbackKnob .setBounds(mainKnobs.removeFromLeft(kw).reduced(4));
    crossKnob    .setBounds(mainKnobs.removeFromLeft(kw).reduced(4));
    mixKnob      .setBounds(mainKnobs.removeFromLeft(kw).reduced(4));
    outGainKnob  .setBounds(mainKnobs.reduced(4));

    // Section: CHARACTER row
    auto charKnobs = juce::Rectangle<int>(20, 462, getWidth() - 40, 110);
    const int n2 = 7;
    const int kw2 = charKnobs.getWidth() / n2;
    lowCutKnob   .setBounds(charKnobs.removeFromLeft(kw2).reduced(4));
    highCutKnob  .setBounds(charKnobs.removeFromLeft(kw2).reduced(4));
    driveKnob    .setBounds(charKnobs.removeFromLeft(kw2).reduced(4));
    modRateKnob  .setBounds(charKnobs.removeFromLeft(kw2).reduced(4));
    modDepthKnob .setBounds(charKnobs.removeFromLeft(kw2).reduced(4));
    widthKnob    .setBounds(charKnobs.removeFromLeft(kw2).reduced(4));
    duckingKnob  .setBounds(charKnobs.reduced(4));
}

//==============================================================================
void DL1AudioProcessorEditor::presetListChanged()
{
    rebuildPresetCombo();
}

void DL1AudioProcessorEditor::currentPresetChanged(const juce::String& name)
{
    auto& pm = audioProcessor.getPresetManager();
    const int idx = pm.getPresetNames().indexOf(name);
    const juce::String shown = pm.isCurrentPresetDirty()
        ? (name.isEmpty() ? juce::String("*Untitled") : name + "*")
        : (name.isEmpty() ? juce::String("Init") : name);

    if (idx >= 0 && ! pm.isCurrentPresetDirty())
        presetCombo.setSelectedId(idx + 1, juce::dontSendNotification);
    else
        presetCombo.setText(shown, juce::dontSendNotification);
}

void DL1AudioProcessorEditor::rebuildPresetCombo()
{
    presetCombo.clear(juce::dontSendNotification);
    auto& pm = audioProcessor.getPresetManager();
    const auto& names = pm.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetCombo.addItem(names[i], i + 1);

    currentPresetChanged(pm.getCurrentPresetName());
}

void DL1AudioProcessorEditor::showSaveAsDialog()
{
    auto* aw = new juce::AlertWindow("Save Preset",
                                     "Enter a name for this preset:",
                                     juce::AlertWindow::NoIcon);
    aw->addTextEditor("name", audioProcessor.getPresetManager().getCurrentPresetName(), "Name:");
    aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true,
        juce::ModalCallbackFunction::create([this, aw](int result)
        {
            std::unique_ptr<juce::AlertWindow> owner(aw);
            if (result == 1)
            {
                auto name = aw->getTextEditorContents("name").trim();
                if (name.isNotEmpty())
                    audioProcessor.getPresetManager().savePreset(name);
            }
        }), true);
}

void DL1AudioProcessorEditor::confirmAndDelete()
{
    auto& pm = audioProcessor.getPresetManager();
    const auto name = pm.getCurrentPresetName();
    if (name.isEmpty() || pm.getPresetNames().indexOf(name) < 0) return;

    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Delete Preset")
            .withMessage("Delete preset \"" + name + "\"?")
            .withButton("Delete")
            .withButton("Cancel"),
        [this, name](int result)
        {
            if (result == 1)
                audioProcessor.getPresetManager().deletePreset(name);
        });
}
