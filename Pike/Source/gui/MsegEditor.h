/*
  ==============================================================================

    MsegEditor.h
    Interactive canvas for one MSEG (multi-segment envelope):
      - click empty area      : insert a point
      - drag a point          : move it (t clamped between neighbours; the
                                first/last point are pinned to t=0 / t=1)
      - double-click a point  : delete it (never the first/last)
      - vertical drag on a segment's mid-handle : bend the curve
      - double-click a mid-handle : reset the curve to linear
      - drag the loop flags in the top marker lane : move loop start/end
      - right-click a point   : Set loop start / Set loop end / Clear loop /
                                Delete point

    The editor is bound to one MSEG subtree (see MsegStore.h) and writes all
    edits straight into it; MsegStore's listener publishes the audio snapshot.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GlassStyle.h"
#include "VisualState.h"
#include "../dsp/Mseg.h"
#include "../params/MsegStore.h"

namespace pike::gui
{
    class MsegEditor : public juce::Component,
                       private juce::ValueTree::Listener,
                       private juce::Timer
    {
    public:
        MsegEditor() = default;

        ~MsegEditor() override
        {
            if (tree.isValid())
                tree.removeListener (this);
        }

        void setTree (juce::ValueTree newTree)
        {
            if (tree.isValid())
                tree.removeListener (this);
            tree = std::move (newTree);
            if (tree.isValid())
                tree.addListener (this);
            repaint();
        }

        /** Binds the live playhead to the given VisualState / MSEG index. */
        void setPlayheadSource (VisualState* vs, int index)
        {
            visualState = vs;
            playheadIndex = index;
            shownPhase = -1.0f;
            if (vs != nullptr && ! isTimerRunning())
                startTimerHz (30);
            repaint();
        }

        //======================================================================
        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            fillGlassPanel (g, b.reduced (1.5f), 5.0f);

            const auto plot = plotArea();
            drawHoloGrid (g, plot, 26.0f, theme::gridLine);

            if (! tree.isValid())
            {
                g.setColour (theme::textDim);
                g.setFont (hudFont (12.0f, false));
                g.drawText ("NO MSEG", plot, juce::Justification::centred, false);
                return;
            }

            const int n = numPoints();
            const int ls = loopStart(), le = loopEnd();
            const bool loopValid = isLoopValid (ls, le, n);

            // Loop region: shaded span + flag markers in the lane.
            const auto lane = laneArea();
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRect (lane);

            if (loopValid)
            {
                const float x0 = xForT (pointT (ls));
                const float x1 = xForT (pointT (le));
                g.setColour (theme::accent.withAlpha (0.09f));
                g.fillRect (juce::Rectangle<float> (x0, plot.getY(), x1 - x0, plot.getHeight()));
                g.setColour (theme::accent.withAlpha (0.45f));
                g.drawVerticalLine ((int) x0, plot.getY(), plot.getBottom());
                g.drawVerticalLine ((int) x1, plot.getY(), plot.getBottom());

                drawLoopFlag (g, x0, true,  dragMode == Drag::loopStartFlag || hover == Hover::loopStartFlag);
                drawLoopFlag (g, x1, false, dragMode == Drag::loopEndFlag   || hover == Hover::loopEndFlag);
            }

            if (n < 2)
                return;

            // Envelope path, sampled per segment with the DSP's shaping.
            juce::Path path;
            path.startNewSubPath (xForT (pointT (0)), yForV (pointV (0)));
            for (int s = 0; s + 1 < n; ++s)
            {
                const float t0 = pointT (s),     v0 = pointV (s);
                const float t1 = pointT (s + 1), v1 = pointV (s + 1);
                const float c  = pointC (s);

                if (c == 0.0f)
                {
                    path.lineTo (xForT (t1), yForV (v1));
                }
                else
                {
                    constexpr int steps = 24;
                    for (int k = 1; k <= steps; ++k)
                    {
                        const float u = (float) k / (float) steps;
                        const float t = t0 + (t1 - t0) * u;
                        const float v = v0 + (v1 - v0) * mseg::shapeU (u, c);
                        path.lineTo (xForT (t), yForV (v));
                    }
                }
            }

            g.setColour (theme::accent.withAlpha (0.25f));   // soft glow pass
            g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
            g.setColour (theme::accent);
            g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

            // Fill under the curve, very faint.
            juce::Path fill (path);
            fill.lineTo (xForT (1.0f), plot.getBottom());
            fill.lineTo (xForT (0.0f), plot.getBottom());
            fill.closeSubPath();
            g.setColour (theme::accent.withAlpha (0.06f));
            g.fillPath (fill);

            // Segment mid-handles (diamonds on the curve).
            for (int s = 0; s + 1 < n; ++s)
            {
                const auto pos = midHandlePos (s);
                const bool hot = (dragMode == Drag::curve && dragIndex == s)
                              || (hover == Hover::midHandle && hoverIndex == s);
                juce::Path d;
                const float r = hot ? 5.0f : 3.5f;
                d.addQuadrilateral (pos.x, pos.y - r, pos.x + r, pos.y, pos.x, pos.y + r, pos.x - r, pos.y);
                g.setColour (hot ? juce::Colours::white : theme::accentWarm.withAlpha (0.8f));
                g.fillPath (d);
            }

            // Breakpoint handles.
            for (int i = 0; i < n; ++i)
            {
                const float x = xForT (pointT (i));
                const float y = yForV (pointV (i));
                const bool hot = (dragMode == Drag::point && dragIndex == i)
                              || (hover == Hover::point && hoverIndex == i);
                const float r = hot ? 6.0f : 4.5f;

                if (loopValid && (i == ls || i == le))
                {
                    g.setColour (theme::accent.withAlpha (0.35f));
                    g.fillEllipse (x - r - 3.0f, y - r - 3.0f, 2.0f * (r + 3.0f), 2.0f * (r + 3.0f));
                }

                g.setColour (theme::accent);
                g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
                g.setColour (hot ? juce::Colours::white : theme::bgDeep);
                g.drawEllipse (x - r, y - r, 2.0f * r, 2.0f * r, 1.5f);
            }

            // Live playhead (driven by the most recent sounding voice).
            if (shownPhase >= 0.0f && shownPhase <= 1.0f)
            {
                const float px = xForT (shownPhase);
                const float py = yForV (valueAtT (shownPhase));
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.drawVerticalLine ((int) px, plot.getY(), plot.getBottom());
                g.setColour (juce::Colours::white);
                g.fillEllipse (px - 3.0f, py - 3.0f, 6.0f, 6.0f);
            }
        }

        //======================================================================
        void mouseMove (const juce::MouseEvent& e) override
        {
            const auto [h, idx] = hitTest (e.position);
            if (h != hover || idx != hoverIndex)
            {
                hover = h;
                hoverIndex = idx;
                repaint();
            }

            setMouseCursor (h == Hover::point || h == Hover::loopStartFlag || h == Hover::loopEndFlag
                              ? juce::MouseCursor::DraggingHandCursor
                              : h == Hover::midHandle ? juce::MouseCursor::UpDownResizeCursor
                                                      : juce::MouseCursor::CrosshairCursor);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            hover = Hover::none;
            repaint();
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! tree.isValid())
                return;

            const auto [h, idx] = hitTest (e.position);

            if (e.mods.isPopupMenu())
            {
                if (h == Hover::point)
                    showPointMenu (idx);
                return;
            }

            dragMode  = Drag::none;
            dragIndex = -1;

            switch (h)
            {
                case Hover::point:
                    dragMode = Drag::point;
                    dragIndex = idx;
                    break;

                case Hover::midHandle:
                    dragMode = Drag::curve;
                    dragIndex = idx;
                    dragStartY = e.position.y;
                    dragStartCurve = pointC (idx);
                    break;

                case Hover::loopStartFlag:
                    dragMode = Drag::loopStartFlag;
                    break;

                case Hover::loopEndFlag:
                    dragMode = Drag::loopEndFlag;
                    break;

                case Hover::none:
                default:
                    if (plotArea().contains (e.position) && numPoints() < mseg::maxPoints)
                    {
                        const int newIdx = insertPoint (tForX (e.position.x), vForY (e.position.y));
                        if (newIdx >= 0)
                        {
                            dragMode = Drag::point;
                            dragIndex = newIdx;
                        }
                    }
                    break;
            }
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! tree.isValid())
                return;

            switch (dragMode)
            {
                case Drag::point:
                    movePoint (dragIndex, tForX (e.position.x), vForY (e.position.y));
                    break;

                case Drag::curve:
                {
                    // 100 px of vertical travel = full curve range. Drag up bends
                    // the curve up (earlier rise) regardless of segment direction.
                    const float delta = (dragStartY - e.position.y) / 100.0f;
                    const float rising = pointV (dragIndex + 1) >= pointV (dragIndex) ? 1.0f : -1.0f;
                    setPointProperty (dragIndex, MsegStore::propC,
                                      juce::jlimit (-1.0f, 1.0f, dragStartCurve - delta * rising));
                    break;
                }

                case Drag::loopStartFlag:
                case Drag::loopEndFlag:
                {
                    const int target = nearestPointIndex (e.position.x);
                    const int ls = loopStart(), le = loopEnd();
                    if (dragMode == Drag::loopStartFlag && target < le)
                        tree.setProperty (MsegStore::propLoopS, target, nullptr);
                    else if (dragMode == Drag::loopEndFlag && target > ls)
                        tree.setProperty (MsegStore::propLoopE, target, nullptr);
                    break;
                }

                case Drag::none:
                default:
                    break;
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            dragMode = Drag::none;
            dragIndex = -1;
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (! tree.isValid())
                return;

            const auto [h, idx] = hitTest (e.position);
            if (h == Hover::point)
                deletePoint (idx);
            else if (h == Hover::midHandle)
                setPointProperty (idx, MsegStore::propC, 0.0f);
        }

    private:
        //======================================================================
        // Geometry
        static constexpr float laneH = 16.0f;
        static constexpr float padX  = 14.0f;
        static constexpr float padY  = 10.0f;

        juce::Rectangle<float> laneArea() const
        {
            auto b = getLocalBounds().toFloat().reduced (padX, padY);
            return b.removeFromTop (laneH);
        }

        juce::Rectangle<float> plotArea() const
        {
            auto b = getLocalBounds().toFloat().reduced (padX, padY);
            b.removeFromTop (laneH + 4.0f);
            return b;
        }

        float xForT (float t) const { const auto p = plotArea(); return p.getX() + t * p.getWidth(); }
        float yForV (float v) const { const auto p = plotArea(); return p.getBottom() - v * p.getHeight(); }
        float tForX (float x) const { const auto p = plotArea(); return juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()); }
        float vForY (float y) const { const auto p = plotArea(); return juce::jlimit (0.0f, 1.0f, (p.getBottom() - y) / p.getHeight()); }

        juce::Point<float> midHandlePos (int seg) const
        {
            const float t0 = pointT (seg), t1 = pointT (seg + 1);
            const float v0 = pointV (seg), v1 = pointV (seg + 1);
            const float v  = v0 + (v1 - v0) * mseg::shapeU (0.5f, pointC (seg));
            return { xForT ((t0 + t1) * 0.5f), yForV (v) };
        }

        //======================================================================
        // Tree access
        int numPoints() const { return tree.isValid() ? tree.getNumChildren() : 0; }
        int loopStart() const { return (int) tree.getProperty (MsegStore::propLoopS, -1); }
        int loopEnd()   const { return (int) tree.getProperty (MsegStore::propLoopE, -1); }

        static bool isLoopValid (int ls, int le, int n) { return ls >= 0 && le > ls && le < n; }

        float pointT (int i) const { return (float) tree.getChild (i).getProperty (MsegStore::propT, 0.0); }
        float pointV (int i) const { return (float) tree.getChild (i).getProperty (MsegStore::propV, 0.0); }
        float pointC (int i) const { return (float) tree.getChild (i).getProperty (MsegStore::propC, 0.0); }

        /** Envelope value at normalized time t (mirrors the DSP's shaping). */
        float valueAtT (float t) const
        {
            const int n = numPoints();
            if (n < 2)        return 0.0f;
            if (t <= 0.0f)    return pointV (0);
            if (t >= 1.0f)    return pointV (n - 1);

            int s = 0;
            while (s + 1 < n && t >= pointT (s + 1))
                ++s;
            if (s >= n - 1)
                return pointV (n - 1);

            const float t0 = pointT (s), t1 = pointT (s + 1);
            const float u  = t1 > t0 ? (t - t0) / (t1 - t0) : 0.0f;
            return pointV (s) + (pointV (s + 1) - pointV (s)) * mseg::shapeU (u, pointC (s));
        }

        void setPointProperty (int i, const juce::Identifier& prop, float value)
        {
            auto pt = tree.getChild (i);
            if (pt.isValid())
                pt.setProperty (prop, value, nullptr);
        }

        //======================================================================
        // Hit testing
        enum class Hover { none, point, midHandle, loopStartFlag, loopEndFlag };
        enum class Drag  { none, point, curve, loopStartFlag, loopEndFlag };

        std::pair<Hover, int> hitTest (juce::Point<float> pos) const
        {
            if (! tree.isValid())
                return { Hover::none, -1 };

            const int n = numPoints();
            const int ls = loopStart(), le = loopEnd();

            // Loop flags live in the marker lane.
            if (isLoopValid (ls, le, n) && laneArea().expanded (0.0f, 4.0f).contains (pos))
            {
                if (std::abs (pos.x - xForT (pointT (ls))) < 9.0f) return { Hover::loopStartFlag, ls };
                if (std::abs (pos.x - xForT (pointT (le))) < 9.0f) return { Hover::loopEndFlag, le };
            }

            for (int i = 0; i < n; ++i)
                if (pos.getDistanceFrom ({ xForT (pointT (i)), yForV (pointV (i)) }) < 9.0f)
                    return { Hover::point, i };

            for (int s = 0; s + 1 < n; ++s)
                if (pos.getDistanceFrom (midHandlePos (s)) < 8.0f)
                    return { Hover::midHandle, s };

            return { Hover::none, -1 };
        }

        int nearestPointIndex (float x) const
        {
            int best = 0;
            float bestDist = 1.0e9f;
            for (int i = 0; i < numPoints(); ++i)
            {
                const float d = std::abs (x - xForT (pointT (i)));
                if (d < bestDist) { bestDist = d; best = i; }
            }
            return best;
        }

        //======================================================================
        // Edits (write to the tree only; MsegStore republishes the snapshot)
        int insertPoint (float t, float v)
        {
            const int n = numPoints();
            int idx = n;                       // insertion index keeps times sorted
            for (int i = 0; i < n; ++i)
                if (t < pointT (i)) { idx = i; break; }
            idx = juce::jlimit (1, n - 1, idx);   // never before the first / after the last

            juce::ValueTree pt (MsegStore::ptType);
            pt.setProperty (MsegStore::propT, t, nullptr);
            pt.setProperty (MsegStore::propV, v, nullptr);
            pt.setProperty (MsegStore::propC, 0.0f, nullptr);
            tree.addChild (pt, idx, nullptr);

            // Shift loop indices at/after the insertion.
            const int ls = loopStart(), le = loopEnd();
            if (ls >= idx) tree.setProperty (MsegStore::propLoopS, ls + 1, nullptr);
            if (le >= idx) tree.setProperty (MsegStore::propLoopE, le + 1, nullptr);
            return idx;
        }

        void movePoint (int i, float t, float v)
        {
            const int n = numPoints();
            if (i < 0 || i >= n)
                return;

            if (i == 0)          t = 0.0f;     // pinned endpoints
            else if (i == n - 1) t = 1.0f;
            else                 t = juce::jlimit (pointT (i - 1) + 0.001f,
                                                   pointT (i + 1) - 0.001f, t);

            setPointProperty (i, MsegStore::propT, t);
            setPointProperty (i, MsegStore::propV, v);
        }

        void deletePoint (int i)
        {
            const int n = numPoints();
            if (i <= 0 || i >= n - 1)          // first/last are not deletable
                return;

            tree.removeChild (i, nullptr);

            int ls = loopStart(), le = loopEnd();
            if (ls == i || le == i) { ls = -1; le = -1; }   // loop anchor gone
            else
            {
                if (ls > i) --ls;
                if (le > i) --le;
            }
            if (! isLoopValid (ls, le, n - 1)) { ls = -1; le = -1; }
            tree.setProperty (MsegStore::propLoopS, ls, nullptr);
            tree.setProperty (MsegStore::propLoopE, le, nullptr);
        }

        void showPointMenu (int i)
        {
            const int n = numPoints();
            const int ls = loopStart(), le = loopEnd();

            juce::PopupMenu m;
            m.addItem (1, "Set Loop Start", i < n - 1 && (le < 0 || i < le));
            m.addItem (2, "Set Loop End",   i > 0     && (ls < 0 || i > ls));
            m.addItem (3, "Clear Loop", isLoopValid (ls, le, n));
            m.addSeparator();
            m.addItem (4, "Delete Point", i > 0 && i < n - 1);

            juce::Component::SafePointer<MsegEditor> self (this);
            m.showMenuAsync (juce::PopupMenu::Options(), [self, i] (int r)
            {
                if (self == nullptr || ! self->tree.isValid())
                    return;

                auto& t = self->tree;
                const int n2 = self->numPoints();
                switch (r)
                {
                    case 1:
                        t.setProperty (MsegStore::propLoopS, i, nullptr);
                        if (self->loopEnd() <= i)
                            t.setProperty (MsegStore::propLoopE, juce::jmin (i + 1, n2 - 1), nullptr);
                        break;
                    case 2:
                        t.setProperty (MsegStore::propLoopE, i, nullptr);
                        if (self->loopStart() < 0 || self->loopStart() >= i)
                            t.setProperty (MsegStore::propLoopS, juce::jmax (0, i - 1), nullptr);
                        break;
                    case 3:
                        t.setProperty (MsegStore::propLoopS, -1, nullptr);
                        t.setProperty (MsegStore::propLoopE, -1, nullptr);
                        break;
                    case 4:
                        self->deletePoint (i);
                        break;
                    default:
                        break;
                }
            });
        }

        //======================================================================
        void drawLoopFlag (juce::Graphics& g, float x, bool isStart, bool hot) const
        {
            const auto lane = laneArea();
            const float dir = isStart ? 1.0f : -1.0f;
            juce::Path f;
            f.startNewSubPath (x, lane.getY());
            f.lineTo (x, lane.getBottom());
            f.lineTo (x + dir * 8.0f, lane.getCentreY());
            f.closeSubPath();
            g.setColour (hot ? juce::Colours::white : theme::accent.withAlpha (0.85f));
            g.fillPath (f);
        }

        //======================================================================
        // ValueTree::Listener — repaint on any change to the bound subtree.
        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { repaint(); }
        void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override             { repaint(); }
        void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override      { repaint(); }
        void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override              { repaint(); }

        //======================================================================
        // Timer — poll the live playhead and repaint only on a real change.
        void timerCallback() override
        {
            if (visualState == nullptr)
                return;

            const float p = visualState->msegPhase[playheadIndex].load (std::memory_order_relaxed);
            const bool wasActive = shownPhase >= 0.0f;
            const bool isActive  = p >= 0.0f;
            if (isActive != wasActive || (isActive && std::abs (p - shownPhase) > 0.002f))
            {
                shownPhase = p;
                repaint();
            }
        }

        juce::ValueTree tree;

        VisualState* visualState = nullptr;
        int   playheadIndex = 0;
        float shownPhase = -1.0f;

        Hover hover = Hover::none;
        int   hoverIndex = -1;
        Drag  dragMode = Drag::none;
        int   dragIndex = -1;
        float dragStartY = 0.0f;
        float dragStartCurve = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MsegEditor)
    };
}
