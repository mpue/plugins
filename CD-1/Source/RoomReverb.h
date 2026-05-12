/*
  ==============================================================================

    RoomReverb.h
    A 4-line FDN reverb with all-pass diffusion, low-pass damping and a
    one-pole high-pass on input.  Tuned for big cinematic drum spaces —
    long, dark, lush — without the metallic ring of a Schroeder reverb.

    The reverb writes directly into the wet path; the host mixes wet/dry.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace cd1
{
    class RoomReverb
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;

            // ---- delay lengths (prime ms, scaled by sample rate) ----
            // Tuned around ~110 ms feedback loop length for a "big hall" feel.
            const float msL[NumLines]   = { 71.0f, 89.0f, 113.0f, 149.0f };
            const float msAP[NumAP]     = { 5.0f, 11.0f, 23.0f, 37.0f };

            for (int i = 0; i < NumLines; ++i)
            {
                const int len = juce::jmax (8, (int) std::round (msL[i] * 0.001f * sr));
                lines[i].assign ((size_t) len, 0.0f);
                writePos[i] = 0;
            }

            for (int i = 0; i < NumAP; ++i)
            {
                const int len = juce::jmax (4, (int) std::round (msAP[i] * 0.001f * sr));
                ap[i].buf.assign ((size_t) len, 0.0f);
                ap[i].pos = 0;
            }

            for (auto& l : lpL) l = 0.0f;
            for (auto& l : lpR) l = 0.0f;
            inHpL = inHpR = 0.0f;
            inHpStateL = inHpStateR = 0.0f;
        }

        void reset() noexcept
        {
            for (auto& d : lines)
                std::fill (d.begin(), d.end(), 0.0f);
            for (auto& a : ap)
                std::fill (a.buf.begin(), a.buf.end(), 0.0f);
            for (int i = 0; i < NumLines; ++i)
                writePos[i] = 0;
            for (int i = 0; i < NumAP; ++i)
                ap[i].pos = 0;
            for (auto& l : lpL) l = 0.0f;
            for (auto& l : lpR) l = 0.0f;
            inHpL = inHpR = 0.0f;
            inHpStateL = inHpStateR = 0.0f;
        }

        // ---- runtime controls ----
        void setSize     (float v) noexcept { size     = juce::jlimit (0.0f, 1.0f, v); updateCoeffs(); }
        void setDamping  (float v) noexcept { damping  = juce::jlimit (0.0f, 1.0f, v); updateCoeffs(); }
        void setLowCut   (float hz) noexcept { lowCutHz = juce::jlimit (20.0f, 600.0f, hz); updateCoeffs(); }
        void setWidth    (float v) noexcept { stereoWidth = juce::jlimit (0.0f, 1.0f, v); }
        void setMix      (float v) noexcept { wetMix   = juce::jlimit (0.0f, 1.0f, v); }

        // Process a single stereo sample. Adds wet*mix to outL/outR.
        void process (float inL, float inR, float& outL, float& outR) noexcept
        {
            // input HPF (one-pole)
            const float hpA = juce::jlimit (0.0f, 0.999f, hpCoeff);
            inHpStateL = inHpStateL * hpA + inL * (1.0f - hpA);
            inHpStateR = inHpStateR * hpA + inR * (1.0f - hpA);
            const float xL = inL - inHpStateL;
            const float xR = inR - inHpStateR;

            // mono in for the FDN, with slight L/R bias
            const float in = (xL + xR) * 0.5f;

            // read taps
            float r[NumLines];
            for (int i = 0; i < NumLines; ++i)
            {
                const int n = (int) lines[i].size();
                const int rp = (writePos[i] + 1) % n;
                r[i] = lines[i][(size_t) rp];
            }

            // damping (LP per line)
            for (int i = 0; i < NumLines; ++i)
            {
                lpL[i] = lpL[i] * dampCoeff + r[i] * (1.0f - dampCoeff);
                r[i]   = lpL[i];
            }

            // Hadamard 4x4 mix matrix (norm 0.5)
            const float a = r[0], b = r[1], c = r[2], d = r[3];
            const float m0 = ( a + b + c + d) * 0.5f;
            const float m1 = ( a - b + c - d) * 0.5f;
            const float m2 = ( a + b - c - d) * 0.5f;
            const float m3 = ( a - b - c + d) * 0.5f;

            // write back with feedback gain (size)
            const float fb = feedback;
            const float wIn = in;
            const int n0 = (int) lines[0].size();
            const int n1 = (int) lines[1].size();
            const int n2 = (int) lines[2].size();
            const int n3 = (int) lines[3].size();

            writePos[0] = (writePos[0] + 1) % n0; lines[0][(size_t) writePos[0]] = wIn + m0 * fb;
            writePos[1] = (writePos[1] + 1) % n1; lines[1][(size_t) writePos[1]] = wIn + m1 * fb;
            writePos[2] = (writePos[2] + 1) % n2; lines[2][(size_t) writePos[2]] = wIn + m2 * fb;
            writePos[3] = (writePos[3] + 1) % n3; lines[3][(size_t) writePos[3]] = wIn + m3 * fb;

            // build stereo output from line taps
            float wetL = (r[0] + r[2]) * 0.5f;
            float wetR = (r[1] + r[3]) * 0.5f;

            // diffusion all-pass chain (improves echo density)
            wetL = processAP (ap[0], wetL);
            wetL = processAP (ap[1], wetL);
            wetR = processAP (ap[2], wetR);
            wetR = processAP (ap[3], wetR);

            // stereo width (mid/side)
            const float mid  = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR);
            const float sCoef = stereoWidth * 1.4f;
            wetL = mid + side * sCoef;
            wetR = mid - side * sCoef;

            outL += wetL * wetMix;
            outR += wetR * wetMix;
        }

    private:
        struct AllPass
        {
            std::vector<float> buf;
            int   pos = 0;
            float g   = 0.55f;
        };

        float processAP (AllPass& a, float in) noexcept
        {
            const int n = (int) a.buf.size();
            const int rp = (a.pos + 1) % n;
            const float bufOut = a.buf[(size_t) rp];
            const float v = in + bufOut * a.g;
            a.buf[(size_t) a.pos] = v;
            a.pos = rp;
            return -v * a.g + bufOut;
        }

        void updateCoeffs() noexcept
        {
            // map size 0..1 -> feedback 0.55..0.92 (long but stable)
            feedback = 0.55f + size * 0.37f;

            // damping 0..1 -> LP cutoff 12 kHz .. 1.2 kHz
            const float fc = juce::jmap (damping, 0.0f, 1.0f, 12000.0f, 1200.0f);
            dampCoeff = std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sampleRate);

            // input HPF
            hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * lowCutHz / (float) sampleRate);
        }

        static constexpr int NumLines = 4;
        static constexpr int NumAP    = 4;

        std::vector<float> lines[NumLines];
        int                writePos[NumLines] { 0, 0, 0, 0 };
        AllPass            ap[NumAP];
        float              lpL[NumLines] { 0, 0, 0, 0 };
        float              lpR[NumLines] { 0, 0, 0, 0 };

        float inHpL = 0, inHpR = 0;
        float inHpStateL = 0, inHpStateR = 0;
        float hpCoeff = 0.99f;

        float size = 0.5f, damping = 0.45f, stereoWidth = 0.7f, wetMix = 0.3f;
        float feedback = 0.7f, dampCoeff = 0.85f, lowCutHz = 90.0f;
        double sampleRate = 44100.0;
    };
} // namespace cd1
