/*
  ==============================================================================

    MsegStore.h
    Owns the MSEG point data and bridges it between the message side and the
    audio thread.

    - Structure lives as a non-parameter child of apvts.state:
        <MSEGS>
          <MSEG loopStart="1" loopEnd="2">
            <PT t="0.0" v="0.0" c="0.0"/> ...
          </MSEG>
        </MSEGS>
      Because it is part of apvts.state it flows through DAW sessions
      (getStateInformation) and user presets (PresetManager saves
      apvts.copyState() XML) without any extra serialization code.

    - Every tree change rebuilds a sanitized, fixed-size mseg::Data snapshot
      (sorted points, pinned endpoints, validated loop indices) and publishes
      it through a seqlock (mseg::SharedData) the audio thread polls once per
      block. All writers are serialized by writeLock; the audio thread never
      takes a lock.

    - valueTreeRedirected (fired by apvts.replaceState on preset/session load)
      re-creates defaults when the incoming state has no MSEGS child (legacy
      presets) and republishes everything.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include "../dsp/Mseg.h"
#include "ParameterIDs.h"

namespace pike
{
    class MsegStore : private juce::ValueTree::Listener
    {
    public:
        explicit MsegStore (juce::AudioProcessorValueTreeState& s) : apvts (s)
        {
            apvts.state.addListener (this);
            ensureDefaults();
        }

        ~MsegStore() override { apvts.state.removeListener (this); }

        //======================================================================
        // Tree identifiers
        static inline const juce::Identifier msegsType  { "MSEGS" };
        static inline const juce::Identifier msegType   { "MSEG" };
        static inline const juce::Identifier ptType     { "PT" };
        static inline const juce::Identifier propT      { "t" };
        static inline const juce::Identifier propV      { "v" };
        static inline const juce::Identifier propC      { "c" };
        static inline const juce::Identifier propLoopS  { "loopStart" };
        static inline const juce::Identifier propLoopE  { "loopEnd" };

        //======================================================================
        // Message-side access (GUI)

        int activeCount() const
        {
            auto msegs = apvts.state.getChildWithName (msegsType);
            return msegs.isValid() ? juce::jmin ((int) mseg::maxMsegs, msegs.getNumChildren()) : 0;
        }

        /** The i-th MSEG subtree, or an invalid tree if it does not exist. */
        juce::ValueTree getMsegTree (int i) const
        {
            auto msegs = apvts.state.getChildWithName (msegsType);
            return msegs.isValid() && i >= 0 && i < msegs.getNumChildren() ? msegs.getChild (i)
                                                                           : juce::ValueTree();
        }

        /** Appends a new default MSEG (capped at maxMsegs); returns its tree. */
        juce::ValueTree addMseg()
        {
            auto msegs = getOrCreateMsegsTree();
            if (msegs.getNumChildren() >= mseg::maxMsegs)
                return juce::ValueTree();

            auto t = makeDefaultMseg();
            msegs.appendChild (t, nullptr);    // listener publishes
            return t;
        }

        /** Factory-preset reset: back to a single default MSEG. */
        void resetAll()
        {
            auto msegs = getOrCreateMsegsTree();
            msegs.removeAllChildren (nullptr);
            msegs.appendChild (makeDefaultMseg(), nullptr);
        }

        /** Creates the MSEGS child and the first default MSEG when missing
            (fresh instance or legacy state without MSEG data). */
        void ensureDefaults()
        {
            auto msegs = getOrCreateMsegsTree();
            if (msegs.getNumChildren() == 0)
                msegs.appendChild (makeDefaultMseg(), nullptr);
            publishAll();
        }

        static juce::ValueTree makeDefaultMseg()
        {
            juce::ValueTree t (msegType);
            t.setProperty (propLoopS, -1, nullptr);
            t.setProperty (propLoopE, -1, nullptr);
            auto addPt = [&t] (float pt, float pv)
            {
                juce::ValueTree p (ptType);
                p.setProperty (propT, pt, nullptr);
                p.setProperty (propV, pv, nullptr);
                p.setProperty (propC, 0.0f, nullptr);
                t.appendChild (p, nullptr);
            };
            addPt (0.0f, 0.0f);
            addPt (0.25f, 1.0f);
            addPt (1.0f, 0.0f);
            return t;
        }

        //======================================================================
        // Audio-side snapshots (read with mseg::SharedData::readIfChanged)
        mseg::SharedData shared[mseg::maxMsegs];

    private:
        juce::ValueTree getOrCreateMsegsTree()
        {
            auto msegs = apvts.state.getChildWithName (msegsType);
            if (! msegs.isValid())
            {
                msegs = juce::ValueTree (msegsType);
                apvts.state.appendChild (msegs, nullptr);
            }
            return msegs;
        }

        /** Sanitized snapshot from one MSEG subtree. Never mutates the tree. */
        static mseg::Data buildData (const juce::ValueTree& t)
        {
            mseg::Data d;
            if (! t.isValid())
                return d;

            for (int i = 0; i < t.getNumChildren() && d.numPoints < mseg::maxPoints; ++i)
            {
                auto pt = t.getChild (i);
                if (! pt.hasType (ptType))
                    continue;
                auto& p = d.points[d.numPoints++];
                p.t     = juce::jlimit (0.0f, 1.0f, (float) pt.getProperty (propT, 0.0));
                p.v     = juce::jlimit (0.0f, 1.0f, (float) pt.getProperty (propV, 0.0));
                p.curve = juce::jlimit (-1.0f, 1.0f, (float) pt.getProperty (propC, 0.0));
            }

            if (d.numPoints < 2)
            {
                d.numPoints = 0;
                return d;
            }

            std::stable_sort (d.points, d.points + d.numPoints,
                              [] (const mseg::Point& a, const mseg::Point& b) { return a.t < b.t; });
            d.points[0].t = 0.0f;
            d.points[d.numPoints - 1].t = 1.0f;

            const int ls = (int) t.getProperty (propLoopS, -1);
            const int le = (int) t.getProperty (propLoopE, -1);
            if (ls >= 0 && le > ls && le < d.numPoints)
            {
                d.loopStart = ls;
                d.loopEnd   = le;
            }
            return d;
        }

        void publish (int i)
        {
            if (i < 0 || i >= mseg::maxMsegs)
                return;
            const juce::ScopedLock sl (writeLock);
            shared[i].write (buildData (getMsegTree (i)));
        }

        void publishAll()
        {
            const juce::ScopedLock sl (writeLock);
            for (int i = 0; i < mseg::maxMsegs; ++i)
                shared[i].write (buildData (getMsegTree (i)));
        }

        /** Index of the MSEG subtree containing (or being) the given tree,
            or -1 if it is not part of the MSEGS branch. */
        int msegIndexFor (juce::ValueTree t) const
        {
            auto msegs = apvts.state.getChildWithName (msegsType);
            if (! msegs.isValid())
                return -1;
            while (t.isValid() && ! t.hasType (msegType))
                t = t.getParent();
            return t.isValid() && t.getParent() == msegs ? msegs.indexOf (t) : -1;
        }

        //======================================================================
        // ValueTree::Listener — fires synchronously on the mutating thread.
        void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&) override
        {
            const int i = msegIndexFor (tree);
            if (i >= 0)
                publish (i);
        }

        void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override
        {
            if (parent.hasType (msegsType))
                publishAll();                       // indices may have shifted
            else if (const int i = msegIndexFor (parent); i >= 0)
                publish (i);
        }

        void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override
        {
            if (parent.hasType (msegsType))
                publishAll();
            else if (const int i = msegIndexFor (parent); i >= 0)
                publish (i);
        }

        void valueTreeChildOrderChanged (juce::ValueTree& parent, int, int) override
        {
            if (parent.hasType (msegsType))
                publishAll();
            else if (const int i = msegIndexFor (parent); i >= 0)
                publish (i);
        }

        void valueTreeRedirected (juce::ValueTree&) override
        {
            // apvts.replaceState() (preset or session load). Repair legacy
            // states without MSEG data and republish all snapshots.
            ensureDefaults();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::CriticalSection writeLock;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MsegStore)
    };
}
