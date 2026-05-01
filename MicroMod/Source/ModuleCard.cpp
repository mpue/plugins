/*
  ==============================================================================

    ModuleCard.cpp

  ==============================================================================
*/

#include "ModuleCard.h"
#include "ModuleGrid.h"
#include "PluginProcessor.h"

ModuleCard::ModuleCard (MicroModAudioProcessor& proc, int id, ModuleGrid& g)
    : processor (proc), moduleId (id), grid (g)
{
    auto* m = processor.findModuleById (moduleId);
    jassert (m != nullptr);

    titleLabel.setText (mm::moduleTypeName (m->getType()), juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8f0ff));
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    enabledButton.setToggleState (m->isEnabled(), juce::dontSendNotification);
    enabledButton.setTooltip ("Enable / bypass module");
    enabledButton.onClick = [this]
    {
        processor.setModuleEnabled (moduleId, enabledButton.getToggleState());
    };
    addAndMakeVisible (enabledButton);

    removeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff402020));
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff8080));
    removeButton.setTooltip ("Remove module");
    removeButton.onClick = [this]
    {
        // schedule removal so we don't delete ourselves while inside a callback
        const int idCopy = moduleId;
        juce::MessageManager::callAsync ([&proc = this->processor, idCopy]
        {
            proc.removeModule (idCopy);
        });
    };
    addAndMakeVisible (removeButton);

    buildKnobs (*m);

    setSize (kCardWidth, kCardHeight);
}

ModuleCard::~ModuleCard() = default;

void ModuleCard::buildKnobs (mm::Module& m)
{
    const int n = m.getNumParams();
    knobs.reserve ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        KnobUi k;
        k.paramIndex = i;
        k.slider = std::make_unique<juce::Slider>();
        k.slider->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        const auto range = m.getParamRange (i);
        k.slider->setNormalisableRange ({ (double) range.start, (double) range.end,
                                          (double) range.interval });
        k.slider->setTextValueSuffix (m.getParamSuffix (i));
        k.slider->setValue (m.getParam (i), juce::dontSendNotification);
        const int paramIdx = i;
        const int idCopy = moduleId;
        auto* sliderPtr = k.slider.get();
        k.slider->onValueChange = [this, paramIdx, idCopy, sliderPtr]
        {
            if (auto* mod = processor.findModuleById (idCopy))
                mod->setParam (paramIdx, (float) sliderPtr->getValue());
        };
        addAndMakeVisible (*k.slider);

        k.label = std::make_unique<juce::Label>();
        k.label->setText (m.getParamName (i), juce::dontSendNotification);
        k.label->setJustificationType (juce::Justification::centred);
        k.label->setFont (juce::FontOptions (10.0f));
        k.label->setColour (juce::Label::textColourId, juce::Colour (0xffaab0c0));
        k.label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*k.label);

        knobs.push_back (std::move (k));
    }
}

void ModuleCard::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    juce::ColourGradient grad (juce::Colour (0xff242a38), 0, 0,
                               juce::Colour (0xff1a1f2a), 0, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, 6.0f);

    auto* m = processor.findModuleById (moduleId);
    const bool on = m != nullptr && m->isEnabled();
    g.setColour (on ? juce::Colour (0xff4d9eff) : juce::Colour (0xff404040));
    g.drawRoundedRectangle (bounds, 6.0f, 1.5f);

    // Header strip
    auto header = bounds.removeFromTop (28.0f);
    g.setColour (juce::Colour (0xff2a3245));
    g.fillRoundedRectangle (header, 6.0f);
    g.setColour (juce::Colour (0xff2a3245));
    g.fillRect (header.withTop (header.getY() + 6.0f));
}

void ModuleCard::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto header = r.removeFromTop (28);
    enabledButton.setBounds (header.removeFromLeft (28).reduced (4));
    removeButton.setBounds  (header.removeFromRight (28).reduced (4));
    titleLabel.setBounds    (header.reduced (4, 2));

    r.removeFromTop (4);

    // Lay out knobs in a grid: 2 columns, up to 4 rows.
    const int n = (int) knobs.size();
    if (n == 0) return;

    const int cols = (n <= 3) ? n : 2;
    const int rows = (n + cols - 1) / cols;
    const int kw = r.getWidth() / cols;
    const int kh = r.getHeight() / rows;

    int idx = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols && idx < n; ++col, ++idx)
        {
            juce::Rectangle<int> cell (r.getX() + col * kw, r.getY() + row * kh, kw, kh);
            auto label = cell.removeFromBottom (14);
            knobs[(size_t) idx].slider->setBounds (cell.reduced (4));
            knobs[(size_t) idx].label->setBounds  (label);
        }
    }
}

void ModuleCard::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == this || e.eventComponent == &titleLabel)
        isDragging = false;
}

void ModuleCard::mouseDrag (const juce::MouseEvent& e)
{
    if (e.eventComponent != this && e.eventComponent != &titleLabel) return;
    if (e.getDistanceFromDragStart() < 6) return;
    if (isDragging) return;
    isDragging = true;
    grid.beginCardDrag (this, e);
}

void ModuleCard::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
}
