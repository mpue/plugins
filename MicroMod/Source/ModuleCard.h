/*
  ==============================================================================

    ModuleCard.h
    A compact card UI rendering a single module's controls.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Modules.h"

class MicroModAudioProcessor;
class ModuleGrid;

class ModuleCard : public juce::Component
{
public:
    ModuleCard (MicroModAudioProcessor& proc, int moduleId, ModuleGrid& grid);
    ~ModuleCard() override;

    int getModuleId() const noexcept { return moduleId; }

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    static constexpr int kCardWidth  = 180;
    static constexpr int kCardHeight = 220;

private:
    void buildKnobs (mm::Module& m);
    void refreshFromModel();

    MicroModAudioProcessor& processor;
    int moduleId;
    ModuleGrid& grid;

    juce::Label  titleLabel;
    juce::ToggleButton enabledButton { "" };
    juce::TextButton removeButton { "x" };

    struct KnobUi
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  label;
        int paramIndex = 0;
    };
    std::vector<KnobUi> knobs;

    bool isDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleCard)
};
