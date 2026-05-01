/*
  ==============================================================================

    ModuleGrid.cpp

  ==============================================================================
*/

#include "ModuleGrid.h"

namespace
{
    constexpr int kCardSpacing = 10;
    constexpr int kPadding = 12;
}

ModuleGrid::ModuleGrid (MicroModAudioProcessor& proc) : processor (proc)
{
    processor.addChainListener (this);
    rebuildFromProcessor();
}

ModuleGrid::~ModuleGrid()
{
    processor.removeChainListener (this);
}

void ModuleGrid::chainChanged()
{
    rebuildFromProcessor();
    if (auto* p = getParentComponent())
        p->resized();
}

void ModuleGrid::rebuildFromProcessor()
{
    auto snapshot = processor.getChainSnapshot();

    // Reuse existing cards by id where possible.
    std::vector<std::unique_ptr<ModuleCard>> newCards;
    newCards.reserve (snapshot.size());

    for (auto& entry : snapshot)
    {
        std::unique_ptr<ModuleCard> card;
        for (auto& c : cards)
        {
            if (c && c->getModuleId() == entry.id)
            {
                card = std::move (c);
                break;
            }
        }
        if (! card)
        {
            card = std::make_unique<ModuleCard> (processor, entry.id, *this);
            addAndMakeVisible (*card);
        }
        newCards.push_back (std::move (card));
    }

    // Cards left in `cards` were removed.
    for (auto& c : cards)
        if (c) removeChildComponent (c.get());
    cards = std::move (newCards);

    resized();
    repaint();
}

int ModuleGrid::getRequiredWidth() const noexcept
{
    const int n = (int) cards.size();
    if (n == 0) return 2 * kPadding + ModuleCard::kCardWidth;
    return 2 * kPadding + n * ModuleCard::kCardWidth + (n - 1) * kCardSpacing;
}

void ModuleGrid::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14171e));

    // Subtle grid pattern.
    g.setColour (juce::Colour (0xff1c2030));
    for (int x = 0; x < getWidth(); x += 24)
        g.drawVerticalLine (x, 0.0f, (float) getHeight());

    // Drop indicator.
    if (draggingCard != nullptr && dragInsertIndex >= 0)
    {
        const int x = kPadding + dragInsertIndex * (ModuleCard::kCardWidth + kCardSpacing) - kCardSpacing / 2;
        g.setColour (juce::Colour (0xff4d9eff));
        g.fillRect ((float) x - 1.5f, (float) kPadding,
                    3.0f, (float) (getHeight() - 2 * kPadding));
    }
}

void ModuleGrid::resized()
{
    int x = kPadding;
    const int y = kPadding;
    for (auto& c : cards)
    {
        if (! c) continue;
        if (c.get() == draggingCard) { x += ModuleCard::kCardWidth + kCardSpacing; continue; }
        c->setBounds (x, y, ModuleCard::kCardWidth, ModuleCard::kCardHeight);
        x += ModuleCard::kCardWidth + kCardSpacing;
    }
}

int ModuleGrid::cardSlotInsertionAt (juce::Point<int> pos) const
{
    const int n = (int) cards.size();
    for (int i = 0; i < n; ++i)
    {
        const int cardX = kPadding + i * (ModuleCard::kCardWidth + kCardSpacing);
        const int midX  = cardX + ModuleCard::kCardWidth / 2;
        if (pos.x < midX) return i;
    }
    return n;
}

void ModuleGrid::beginCardDrag (ModuleCard* card, const juce::MouseEvent& e)
{
    draggingCard = card;
    dragStartLocalPos = e.getEventRelativeTo (this).getPosition();
    dragInsertIndex = -1;

    card->toFront (false);

    auto cardLocal = e.getEventRelativeTo (card).getPosition();
    activeDragListener = std::make_unique<DragListener> (*this, card, cardLocal);
    card->addMouseListener (activeDragListener.get(), true);
}

ModuleGrid::DragListener::DragListener (ModuleGrid& g, ModuleCard* c, juce::Point<int> off)
    : grid (g), card (c), grabOffset (off) {}

void ModuleGrid::DragListener::mouseDrag (const juce::MouseEvent& ev)
{
    const auto p = ev.getEventRelativeTo (&grid).getPosition();
    card->setTopLeftPosition (p.x - grabOffset.x, juce::jmax (4, p.y - grabOffset.y));
    grid.updateDropIndicator (p);
}

void ModuleGrid::DragListener::mouseUp (const juce::MouseEvent& ev)
{
    const auto p = ev.getEventRelativeTo (&grid).getPosition();
    juce::WeakReference<ModuleGrid> weakGrid (&grid);
    juce::Component::SafePointer<ModuleCard> safeCard (card);
    juce::MessageManager::callAsync ([weakGrid, safeCard, p]() mutable
    {
        if (auto* g = weakGrid.get())
        {
            if (safeCard != nullptr && g->activeDragListener)
                safeCard->removeMouseListener (g->activeDragListener.get());
            g->finishDrag (p);
            g->activeDragListener.reset();
        }
    });
}

void ModuleGrid::updateDropIndicator (juce::Point<int> pos)
{
    int slot = cardSlotInsertionAt (pos);
    // Account for the dragged card's own slot — collapse it.
    int draggingIdx = -1;
    for (size_t i = 0; i < cards.size(); ++i)
        if (cards[i].get() == draggingCard) { draggingIdx = (int) i; break; }
    if (draggingIdx >= 0 && slot > draggingIdx) slot--;
    dragInsertIndex = slot;
    repaint();
}

void ModuleGrid::clearDropIndicator()
{
    dragInsertIndex = -1;
    repaint();
}

void ModuleGrid::finishDrag (juce::Point<int> pos)
{
    if (draggingCard == nullptr) return;

    int slot = cardSlotInsertionAt (pos);
    int draggingIdx = -1;
    for (size_t i = 0; i < cards.size(); ++i)
        if (cards[i].get() == draggingCard) { draggingIdx = (int) i; break; }
    if (draggingIdx >= 0 && slot > draggingIdx) slot--;

    const int id = draggingCard->getModuleId();
    draggingCard = nullptr;
    clearDropIndicator();

    if (draggingIdx >= 0 && slot != draggingIdx)
        processor.moveModule (id, slot);
    else
        resized(); // snap back
}

int ModuleGrid::cardIndexAt (juce::Point<int> pos) const
{
    for (int i = 0; i < (int) cards.size(); ++i)
        if (cards[(size_t) i] && cards[(size_t) i]->getBounds().contains (pos))
            return i;
    return -1;
}
