/*
  Standalone unit test for Source/dsp/Mseg.h (not part of the Projucer build).

  Build & run:
    clang++ -std=c++20 -O2 -o /tmp/MsegTest Source/dsp/tests/MsegTest.cpp && /tmp/MsegTest
*/

#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <cstring>
#include <cmath>

#include "../Mseg.h"

using namespace pike::mseg;

static int checksRun = 0;

#define CHECK(cond) do { ++checksRun; if (! (cond)) { \
    std::printf ("FAILED line %d: %s\n", __LINE__, #cond); return 1; } } while (0)

static bool near (float a, float b, float eps = 1.0e-4f) { return std::fabs (a - b) < eps; }

// 3-point default shape: (0,0) -> (0.25,1) -> (1,0)
static Data defaultData()
{
    Data d;
    d.numPoints = 3;
    d.points[0] = { 0.0f,  0.0f, 0.0f };
    d.points[1] = { 0.25f, 1.0f, 0.0f };
    d.points[2] = { 1.0f,  0.0f, 0.0f };
    return d;
}

int main()
{
    // --- inactive data outputs 0 -------------------------------------------
    {
        Data d;                       // numPoints = 0
        Player p;
        p.noteOn();
        CHECK (p.process (d, 0.01, false) == 0.0f);
        d.numPoints = 1;
        d.points[0] = { 0.0f, 0.7f, 0.0f };
        CHECK (p.process (d, 0.01, false) == 0.0f);
    }

    // --- linear ramp values at known phases --------------------------------
    {
        Data d = defaultData();
        Player p;
        p.noteOn();
        const double inc = 1.0 / 1000.0;   // 1000 samples for the whole envelope

        // First sample is exactly phase 0.
        CHECK (near (p.process (d, inc, false), 0.0f));

        // Run to phase 0.125 (sample index 125): halfway up the attack ramp.
        float v = 0.0f;
        for (int i = 1; i <= 125; ++i) v = p.process (d, inc, false);
        CHECK (near (v, 0.5f, 5.0e-3f));

        // Phase 0.25: peak.
        for (int i = 126; i <= 250; ++i) v = p.process (d, inc, false);
        CHECK (near (v, 1.0f, 5.0e-3f));

        // Phase 0.625: halfway down the decay ramp.
        for (int i = 251; i <= 625; ++i) v = p.process (d, inc, false);
        CHECK (near (v, 0.5f, 5.0e-3f));

        // Past the end: holds the final value.
        for (int i = 0; i < 2000; ++i) v = p.process (d, inc, false);
        CHECK (near (v, 0.0f));
    }

    // --- curve shaping: endpoint exactness and monotonicity ----------------
    {
        CHECK (shapeU (0.0f,  1.0f) == 0.0f);
        CHECK (shapeU (1.0f,  1.0f) == 1.0f);
        CHECK (shapeU (0.0f, -1.0f) == 0.0f);
        CHECK (shapeU (1.0f, -1.0f) == 1.0f);
        CHECK (near (shapeU (0.5f, 0.0f), 0.5f));
        CHECK (shapeU (0.5f,  1.0f) < 0.5f);   // exp2(3) = 8 -> slow start
        CHECK (shapeU (0.5f, -1.0f) > 0.5f);   // exponent 1/8 -> fast start

        for (float curve : { -1.0f, -0.3f, 0.4f, 1.0f })
        {
            float prev = -1.0f;
            for (int i = 0; i <= 100; ++i)
            {
                const float s = shapeU ((float) i / 100.0f, curve);
                CHECK (s >= prev);
                prev = s;
            }
        }

        // Player applies the curve of the segment's start point.
        Data d;
        d.numPoints = 2;
        d.points[0] = { 0.0f, 0.0f, 1.0f };
        d.points[1] = { 1.0f, 1.0f, 0.0f };
        Player p;
        p.noteOn();
        const double inc = 1.0 / 1000.0;
        float v = 0.0f;
        for (int i = 0; i <= 500; ++i) v = p.process (d, inc, false);
        CHECK (near (v, shapeU (0.5f, 1.0f), 5.0e-3f));
    }

    // --- loop wrap while gate held ------------------------------------------
    {
        Data d = defaultData();
        d.loopStart = 0;     // loop over 0..0.25 (the attack ramp)
        d.loopEnd   = 1;
        CHECK (d.hasLoop());

        Player p;
        p.noteOn();
        const double inc = 1.0 / 1000.0;

        // Run for 10 full envelope lengths: without the loop we'd be far past
        // the end; with it we must still be inside 0..0.25 and ramping.
        float v = 0.0f, prev = 0.0f;
        bool sawReset = false;
        for (int i = 0; i < 10000; ++i)
        {
            v = p.process (d, inc, true);
            if (i > 0 && v < prev - 0.5f) sawReset = true;   // wrap = big downward jump
            prev = v;
        }
        CHECK (p.position() < 0.25 + 1.0e-6);
        CHECK (sawReset);
        CHECK (v >= 0.0f && v <= 1.0f);
    }

    // --- release runs through and past the loop -----------------------------
    {
        Data d = defaultData();
        d.loopStart = 0;
        d.loopEnd   = 1;

        Player p;
        p.noteOn();
        const double inc = 1.0 / 1000.0;
        for (int i = 0; i < 5000; ++i) p.process (d, inc, true);
        CHECK (p.position() < 0.25 + 1.0e-6);   // still looping

        p.noteOff();
        float v = 1.0f;
        for (int i = 0; i < 2000; ++i) v = p.process (d, inc, true);
        CHECK (p.position() >= 1.0);            // ran out past the loop to the end
        CHECK (near (v, 0.0f));                 // and holds the final value
    }

    // --- degenerate loops are ignored ---------------------------------------
    {
        Data d = defaultData();
        d.loopStart = 1; d.loopEnd = 1;          // equal indices
        CHECK (! d.hasLoop());
        d.loopStart = 2; d.loopEnd = 1;          // reversed
        CHECK (! d.hasLoop());
        d.loopStart = -1; d.loopEnd = -1;        // none
        CHECK (! d.hasLoop());
        d.loopStart = 1; d.loopEnd = 5;          // out of range
        CHECK (! d.hasLoop());

        // Zero-width time span.
        Data z;
        z.numPoints = 3;
        z.points[0] = { 0.0f, 0.0f, 0.0f };
        z.points[1] = { 0.5f, 1.0f, 0.0f };
        z.points[2] = { 1.0f, 0.0f, 0.0f };
        z.points[1].t = z.points[2].t = 0.5f;    // builder wouldn't emit this, but be safe
        z.loopStart = 1; z.loopEnd = 2;
        CHECK (! z.hasLoop());

        // A player over an ignored loop just runs to the end.
        Player p;
        p.noteOn();
        float v = 1.0f;
        for (int i = 0; i < 3000; ++i) v = p.process (d, 1.0 / 1000.0, true);
        CHECK (p.position() >= 1.0);
    }

    // --- seqlock round-trip --------------------------------------------------
    {
        SharedData sh;
        Data d = defaultData();
        d.points[1].curve = -0.4f;
        sh.write (d);

        Data out;
        uint32_t lastSeen = 0;
        CHECK (sh.readIfChanged (out, lastSeen));
        CHECK (std::memcmp (&out, &d, sizeof (Data)) == 0);
        CHECK (! sh.readIfChanged (out, lastSeen));   // unchanged: no re-read

        d.points[1].v = 0.5f;
        sh.write (d);
        CHECK (sh.readIfChanged (out, lastSeen));
        CHECK (near (out.points[1].v, 0.5f));
    }

    std::printf ("MsegTest: all %d checks passed\n", checksRun);
    return 0;
}
