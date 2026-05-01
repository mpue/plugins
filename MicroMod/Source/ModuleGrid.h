/*
  ==============================================================================

    ModuleGrid.h
    Hosts a row of ModuleCard components, supports drag-to-reorder.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ModuleCard.h"
#include "PluginProcessor.h"

class ModuleGrid : public juce::Component,
                   public ChainListener
{
public:
    ModuleGrid (MicroModAudioProcessor& proc);
    ~ModuleGrid() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void chainChanged() override;

    /** Called by a ModuleCard when the user begins dragging it. */
    void beginCardDrag (ModuleCard* card, const juce::MouseEvent& e);

    int getRequiredWidth() const noexcept;

private:
    void rebuildFromProcessor();
    int  cardIndexAt (juce::Point<int> pos) const;
    int  cardSlotInsertionAt (juce::Point<int> pos) const;
    void updateDropIndicator (juce::Point<int> pos);
    void clearDropIndicator();
    void finishDrag (juce::Point<int> pos);

    MicroModAudioProcessor& processor;

    std::vector<std::unique_ptr<ModuleCard>> cards;

    // Drag state
    ModuleCard*               draggingCard = nullptr;
    int                       dragInsertIndex = -1;
    juce::Point<int>          dragStartLocalPos;

    struct DragListener : public juce::MouseListener
    {
        DragListener (ModuleGrid& g, ModuleCard* c, juce::Point<int> off);
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp   (const juce::MouseEvent&) override;
        ModuleGrid& grid;
        ModuleCard* card;
        juce::Point<int> grabOffset;
    };
    std::unique_ptr<DragListener> activeDragListener;

    JUCE_DECLARE_WEAK_REFERENCEABLE (ModuleGrid)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleGrid)
};
