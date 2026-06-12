/*
  ==============================================================================

    MidiLearnManager.h
    MIDI Learn for any APVTS parameter. The audio thread scans incoming CC
    messages: if a "learn target" is armed, the next CC binds to that parameter;
    otherwise a mapped CC drives its parameter. Mappings persist in plugin state.

    Thread-safe: the CC->parameter table and the learn target are atomics. The UI
    arms/clears learn; the audio thread reads and applies. Mappings are keyed by
    parameter ID so they survive parameter-list changes.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace pike
{
    class MidiLearnManager
    {
    public:
        explicit MidiLearnManager (juce::AudioProcessor& proc) : processor (proc)
        {
            for (auto& c : ccToParam)
                c.store (-1, std::memory_order_relaxed);
        }

        //======================================================================
        // Audio thread: handle CC messages in the block.
        void processMidi (const juce::MidiBuffer& midi)
        {
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (! m.isController())
                    continue;

                const int cc  = m.getControllerNumber();
                const int val = m.getControllerValue();

                const int target = learnTargetIdx.load (std::memory_order_relaxed);
                if (target >= 0)
                {
                    // Bind this CC to the armed parameter (one CC per parameter).
                    for (int c = 0; c < 128; ++c)
                        if (ccToParam[c].load (std::memory_order_relaxed) == target)
                            ccToParam[c].store (-1, std::memory_order_relaxed);

                    ccToParam[cc].store (target, std::memory_order_relaxed);
                    learnTargetIdx.store (-1, std::memory_order_relaxed);
                }
                else
                {
                    const int idx = ccToParam[cc].load (std::memory_order_relaxed);
                    if (idx >= 0)
                        if (auto* p = getParam (idx))
                            p->setValueNotifyingHost ((float) val / 127.0f);
                }
            }
        }

        //======================================================================
        // UI thread.
        void arm (int paramIndex)        { learnTargetIdx.store (paramIndex, std::memory_order_relaxed); }
        void cancel()                    { learnTargetIdx.store (-1, std::memory_order_relaxed); }
        int  armedParam() const          { return learnTargetIdx.load (std::memory_order_relaxed); }

        void clearMapping (int paramIndex)
        {
            for (int c = 0; c < 128; ++c)
                if (ccToParam[c].load (std::memory_order_relaxed) == paramIndex)
                    ccToParam[c].store (-1, std::memory_order_relaxed);
        }

        /** Returns the CC mapped to a parameter, or -1. */
        int ccForParam (int paramIndex) const
        {
            for (int c = 0; c < 128; ++c)
                if (ccToParam[c].load (std::memory_order_relaxed) == paramIndex)
                    return c;
            return -1;
        }

        //======================================================================
        // State: keyed by parameter ID for robustness.
        void writeTo (juce::XmlElement& root) const
        {
            auto* maps = root.createNewChildElement ("MidiLearn");
            for (int c = 0; c < 128; ++c)
            {
                const int idx = ccToParam[c].load (std::memory_order_relaxed);
                if (idx >= 0)
                    if (auto id = paramId (idx); id.isNotEmpty())
                    {
                        auto* e = maps->createNewChildElement ("Map");
                        e->setAttribute ("cc", c);
                        e->setAttribute ("param", id);
                    }
            }
        }

        void readFrom (const juce::XmlElement& root)
        {
            for (auto& c : ccToParam) c.store (-1, std::memory_order_relaxed);

            if (auto* maps = root.getChildByName ("MidiLearn"))
                for (auto* e : maps->getChildIterator())
                {
                    const int cc = e->getIntAttribute ("cc", -1);
                    const int idx = paramIndexForId (e->getStringAttribute ("param"));
                    if (cc >= 0 && cc < 128 && idx >= 0)
                        ccToParam[cc].store (idx, std::memory_order_relaxed);
                }
        }

    private:
        juce::AudioProcessorParameter* getParam (int idx) const
        {
            auto& params = processor.getParameters();
            return juce::isPositiveAndBelow (idx, params.size()) ? params[idx] : nullptr;
        }

        juce::String paramId (int idx) const
        {
            if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (getParam (idx)))
                return wid->paramID;
            return {};
        }

        int paramIndexForId (const juce::String& id) const
        {
            auto& params = processor.getParameters();
            for (int i = 0; i < params.size(); ++i)
                if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[i]))
                    if (wid->paramID == id)
                        return i;
            return -1;
        }

        juce::AudioProcessor&               processor;
        std::array<std::atomic<int>, 128>   ccToParam;
        std::atomic<int>                    learnTargetIdx { -1 };
    };
}
