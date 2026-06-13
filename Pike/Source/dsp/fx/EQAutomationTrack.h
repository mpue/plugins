/*
  ==============================================================================

    EQAutomationTrack.h
    Records and plays back per-band EQ (freq + gain) automation — the "animated
    equalizer". Ported from the Lupo synth (was EQAutomationTrack.{h,cpp}).

    Usage:
      Press REC  -> drag EQ handles for as long as you like -> press REC again.
      The elapsed time becomes the loop length.
      Press PLAY -> the recorded motion loops (note-triggered: restarts on the
      first note, runs only while notes sound).

    Thread safety:
      UI thread   : startRecording / stopRecording / recordEvent / clearAll / setPlaying
      Audio thread: prepareToPlay / advance()
      A SpinLock protects the event lists; the audio thread uses a try-lock so it
      never blocks.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ParametricEQ.h"
#include <array>
#include <vector>
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace pike
{
    class EQAutomationTrack
    {
    public:
        EQAutomationTrack() = default;

        static constexpr int numBands = ParametricEQ::numBands;

        struct Event
        {
            float timeSec = 0.0f;     // seconds from the start of the recording
            float freq    = 1000.0f;
            float gainDb  = 0.0f;
        };

        // ── Audio thread ───────────────────────────────────────────────────

        void prepareToPlay (double sr) { sampleRate_ = sr > 0.0 ? sr : 44100.0; }

        /** Advance playback / recording clocks and apply automation to eq. */
        void advance (int numSamples, ParametricEQ* eq)
        {
            if (recording.load())
                recordSamplePos.fetch_add (numSamples);

            if (! playing.load())
                return;

            const int64_t loopLen = loopDurationSamples.load();
            if (loopLen <= 0)
                return;

            if (playbackResetPending.exchange (false))
                playbackSamplePos = 0.0;

            playbackSamplePos += numSamples;
            while (playbackSamplePos >= (double) loopLen)
                playbackSamplePos -= (double) loopLen;

            const float posSec = (float) (playbackSamplePos / sampleRate_);
            playbackPosSec.store (posSec);

            const juce::SpinLock::ScopedTryLockType tryLock (lock);
            if (! tryLock.isLocked())
                return;

            for (int i = 0; i < numBands; ++i)
            {
                float freq = 0.0f, gain = 0.0f;
                if (interpolate (i, posSec, freq, gain))
                {
                    eq->setBandFrequency (i, freq);
                    eq->setBandGain      (i, gain);
                }
            }
        }

        // ── UI thread ──────────────────────────────────────────────────────

        void startRecording()
        {
            {
                const juce::SpinLock::ScopedLockType sl (lock);
                for (auto& v : events) v.clear();
            }
            recordSamplePos.store (0);
            loopDurationSamples.store (0);
            recording.store (true);
        }

        void stopRecording()
        {
            loopDurationSamples.store (recordSamplePos.load());
            recording.store (false);
        }

        /** Record the current EQ state for one band at the recording clock.
            Call from the UI thread during a mouse-drag on an EQ handle. */
        void recordEvent (int band, float freq, float gainDb)
        {
            if (! recording.load())          return;
            if (band < 0 || band >= numBands) return;

            const float timeSec = (float) ((double) recordSamplePos.load() / sampleRate_);

            // 20 ms dedup window — prevents piling up identical points per UI frame.
            static constexpr float kWindow = 0.020f;

            const juce::SpinLock::ScopedLockType sl (lock);
            auto& v = events[(size_t) band];

            v.erase (std::remove_if (v.begin(), v.end(),
                                     [timeSec] (const Event& e) { return std::abs (e.timeSec - timeSec) < kWindow; }),
                     v.end());

            auto it = std::lower_bound (v.begin(), v.end(), timeSec,
                                        [] (const Event& e, float t) { return e.timeSec < t; });
            v.insert (it, { timeSec, freq, gainDb });
        }

        void clearAll()
        {
            {
                const juce::SpinLock::ScopedLockType sl (lock);
                for (auto& v : events) v.clear();
            }
            loopDurationSamples.store (0);
            recordSamplePos.store (0);
            playbackResetPending.store (true);
            playbackPosSec.store (0.0f);
        }

        /** Start or stop looped playback; resets playback position when starting. */
        void setPlaying (bool b)
        {
            if (b) playbackResetPending.store (true);
            playing.store (b);
        }

        void resetPlaybackPosition() { playbackResetPending.store (true); }

        bool  isRecording()        const { return recording.load(); }
        bool  isPlaying()          const { return playing.load(); }
        float getPlaybackPosSec()  const { return playbackPosSec.load(); }

        bool hasData() const
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            for (const auto& v : events)
                if (! v.empty()) return true;
            return false;
        }

        float getLoopDurationSec() const
        {
            const int64_t s = loopDurationSamples.load();
            return s <= 0 ? 0.0f : (float) ((double) s / sampleRate_);
        }

        // ── Preset persistence ─────────────────────────────────────────────

        juce::String getStateAsString() const
        {
            const juce::SpinLock::ScopedLockType sl (lock);

            const int64_t loopSamples = loopDurationSamples.load();
            const float   durationSec = loopSamples > 0 ? (float) ((double) loopSamples / sampleRate_) : 0.0f;

            juce::String s = juce::String ("play=") + (playing.load() ? "1" : "0") + ";";
            s += juce::String (durationSec, 4);

            for (int b = 0; b < numBands; ++b)
            {
                s += ";";
                const auto& v = events[(size_t) b];
                for (int i = 0; i < (int) v.size(); ++i)
                {
                    if (i > 0) s += "|";
                    s += juce::String (v[(size_t) i].timeSec, 4) + ","
                       + juce::String (v[(size_t) i].freq,    2) + ","
                       + juce::String (v[(size_t) i].gainDb,  3);
                }
            }
            return s;
        }

        void loadStateFromString (const juce::String& s)
        {
            if (s.trim().isEmpty())
            {
                clearAll();
                playing.store (false);
                return;
            }

            juce::String data = s.trim();
            bool shouldPlay = false;
            if (data.startsWith ("play="))
            {
                const int sep = data.indexOf (";");
                if (sep > 0)
                {
                    shouldPlay = data.substring (5, sep).getIntValue() != 0;
                    data = data.substring (sep + 1);
                }
            }
            playing.store (shouldPlay);

            juce::StringArray tokens;
            tokens.addTokens (data, ";", "");
            if (tokens.isEmpty())
                return;

            const juce::SpinLock::ScopedLockType sl (lock);
            for (auto& v : events) v.clear();

            const float durationSec = tokens[0].getFloatValue();
            loopDurationSamples.store ((int64_t) ((double) durationSec * sampleRate_));

            for (int b = 0; b < juce::jmin (numBands, tokens.size() - 1); ++b)
            {
                if (tokens[b + 1].trim().isEmpty())
                    continue;

                juce::StringArray evs;
                evs.addTokens (tokens[b + 1].trim(), "|", "");
                for (const auto& ev : evs)
                {
                    juce::StringArray vals;
                    vals.addTokens (ev.trim(), ",", "");
                    if (vals.size() < 3) continue;
                    events[(size_t) b].push_back ({ vals[0].getFloatValue(),
                                                    vals[1].getFloatValue(),
                                                    vals[2].getFloatValue() });
                }
            }
            playbackResetPending.store (true);
        }

    private:
        bool interpolate (int band, float timeSec, float& outFreq, float& outGain) const
        {
            const auto& v = events[(size_t) band];
            if (v.empty()) return false;

            if (v.size() == 1) { outFreq = v[0].freq; outGain = v[0].gainDb; return true; }

            if (timeSec <= v.front().timeSec) { outFreq = v.front().freq; outGain = v.front().gainDb; return true; }
            if (timeSec >= v.back().timeSec)  { outFreq = v.back().freq;  outGain = v.back().gainDb;  return true; }

            for (int i = 0; i + 1 < (int) v.size(); ++i)
            {
                if (timeSec >= v[(size_t) i].timeSec && timeSec < v[(size_t) (i + 1)].timeSec)
                {
                    const float span = v[(size_t) (i + 1)].timeSec - v[(size_t) i].timeSec;
                    const float t    = span > 0.0f ? (timeSec - v[(size_t) i].timeSec) / span : 0.0f;

                    const float logF0 = std::log10 (juce::jmax (20.0f, v[(size_t) i].freq));
                    const float logF1 = std::log10 (juce::jmax (20.0f, v[(size_t) (i + 1)].freq));
                    outFreq = std::pow (10.0f, logF0 + t * (logF1 - logF0));
                    outGain = v[(size_t) i].gainDb + t * (v[(size_t) (i + 1)].gainDb - v[(size_t) i].gainDb);
                    return true;
                }
            }
            return false;
        }

        std::array<std::vector<Event>, numBands> events;
        mutable juce::SpinLock                   lock;

        std::atomic<bool>    recording            { false };
        std::atomic<bool>    playing              { false };
        std::atomic<bool>    playbackResetPending { false };
        std::atomic<int64_t> recordSamplePos      { 0 };
        std::atomic<int64_t> loopDurationSamples  { 0 };
        std::atomic<float>   playbackPosSec       { 0.0f };

        double playbackSamplePos = 0.0;
        double sampleRate_       = 44100.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAutomationTrack)
    };
}
