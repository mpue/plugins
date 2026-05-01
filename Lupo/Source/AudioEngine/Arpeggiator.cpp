#include "Arpeggiator.h"

Arpeggiator::Arpeggiator() {}
Arpeggiator::~Arpeggiator() {}

void Arpeggiator::prepareToPlay(double sr, int)
{
    sampleRate = static_cast<float> (sr);
    timeSamples = 0;
    clockCounter = 0;
    isPlaying = false;
    currentNote = -1;
    lastNote = -1;
    octave = 0;
    lastPpqPosition = -1.0;
    notes.clear();
    physicallyHeld.clear();
    sentChordNotes.clear();
    sentNote = -1;
    gateCountdown = -1;
    noteIsOn = false;
}

void Arpeggiator::setDivisionIndex(int idx) noexcept
{
    static const int table[]{ 24, 12, 6, 3 }; // 1/4, 1/8, 1/16, 1/32
idx = juce::jlimit(0, 3, idx);
    ticksPerStep = table[idx];
}

double Arpeggiator::getStepLengthInQuarterNotes() const noexcept
{
    // ticksPerStep is in 24-PPQN ticks
// 24 ticks = 1 quarter note
    return static_cast<double>(ticksPerStep) / 24.0;
}

static int getNextNoteIndex(int current, int size, Arpeggiator::Mode mode, int& dir)
{
 if (size == 0) return -1;

    switch (mode)
    {
    case Arpeggiator::Mode::Up:
        return (current + 1) % size;

    case Arpeggiator::Mode::Down:
        if (current <= 0) dir = 1;
      else if (current >= size - 1) dir = -1;
      return juce::jlimit(0, size - 1, current + dir);

    case Arpeggiator::Mode::Random:
        return juce::Random::getSystemRandom().nextInt(size);
    }

    return 0;
}

void Arpeggiator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (!enabled) return;

    const int numSamples = buffer.getNumSamples();
    juce::MidiMessage msg;
    int pos;
    juce::MidiBuffer output;

    // Service pending gate note-off from a previous block
    if (gateCountdown >= 0)
    {
        if (gateCountdown < numSamples)
        {
            if (noteIsOn)
            {
                if (mode == Mode::Chord) {
                    for (int n : sentChordNotes)
                        output.addEvent(juce::MidiMessage::noteOff(1, n), gateCountdown);
                    sentChordNotes.clear();
                } else if (sentNote >= 0) {
                    output.addEvent(juce::MidiMessage::noteOff(1, sentNote), gateCountdown);
                }
                noteIsOn = false;
            }
            gateCountdown = -1;
        }
        else
        {
            gateCountdown -= numSamples;
        }
    }

    // Collect note-on/off from incoming MIDI, pass through everything else
    for (juce::MidiBuffer::Iterator it(midi); it.getNextEvent(msg, pos); )
    {
        if (msg.isNoteOn())
        {
            // In latch mode, pressing a new note after all keys were released
            // starts a fresh latch set (replaces the old latched notes).
            if (latch && physicallyHeld.isEmpty())
                notes.clearQuick();

            physicallyHeld.addIfNotAlreadyThere(msg.getNoteNumber());
            notes.addIfNotAlreadyThere(msg.getNoteNumber());
        }
        else if (msg.isNoteOff())
        {
            physicallyHeld.removeFirstMatchingValue(msg.getNoteNumber());
            // In latch mode, notes stay in the pool even after key release.
            if (!latch)
                notes.removeFirstMatchingValue(msg.getNoteNumber());
        }
        else
        {
            output.addEvent(msg, pos);
        }
    }

    // Helper lambda: fire a note-on + schedule gate note-off.
    // Saves actual MIDI note numbers (with octave offset) so gate/silence
    // note-offs always match, regardless of octave changes between steps.
    auto fireNoteOn = [&](int sample, int stepDurationSamples)
    {
        if (notes.size() == 0) return;

        int currentOctave = octave;  // snapshot before possible increment
        if (mode == Mode::Chord) {
            sentChordNotes.clear();
            for (int n : notes) {
                int midiNote = n + 12 * currentOctave;
                output.addEvent(juce::MidiMessage::noteOn(1, midiNote, (uint8)120), sample);
                sentChordNotes.add(midiNote);
            }
            sentNote = -1;
            lastNote = notes[0];
        } else {
            currentNote = getNextNoteIndex(currentNote, notes.size(), mode, direction);
            lastNote = notes[currentNote];
            sentNote = lastNote + 12 * currentOctave;
            output.addEvent(juce::MidiMessage::noteOn(1, sentNote, (uint8)120), sample);
        }
        noteIsOn = true;

        int gateSamples = juce::jmax(1, (int)(stepDurationSamples * gateTime));
        int noteOffSample = sample + gateSamples;
        if (noteOffSample < numSamples) {
            if (mode == Mode::Chord) {
                for (int n : sentChordNotes)
                    output.addEvent(juce::MidiMessage::noteOff(1, n), noteOffSample);
                sentChordNotes.clear();
            } else {
                output.addEvent(juce::MidiMessage::noteOff(1, sentNote), noteOffSample);
            }
            noteIsOn = false;
            gateCountdown = -1;
        } else {
            gateCountdown = noteOffSample - numSamples;
        }

        if (octaves > 0)
            octave = (octave + 1) % octaves;
    };

    // Helper lambda: send note-off for currently sounding note(s) if still on
    auto silenceNow = [&](int sample)
    {
        if (!noteIsOn) return;
        if (mode == Mode::Chord) {
            for (int n : sentChordNotes)
                output.addEvent(juce::MidiMessage::noteOff(1, n), sample);
            sentChordNotes.clear();
        } else if (sentNote >= 0) {
            output.addEvent(juce::MidiMessage::noteOff(1, sentNote), sample);
        }
        noteIsOn = false;
        gateCountdown = -1;
    };

    // --- Determine step triggers ---

    if (clockMode == ClockMode::Midi && playHead != nullptr)
    {
        auto posInfo = playHead->getPosition();
        if (posInfo.hasValue())
        {
            auto hostIsPlaying = posInfo->getIsPlaying();
            auto ppqOpt = posInfo->getPpqPosition();
            auto bpmOpt = posInfo->getBpm();

            if (hostIsPlaying && ppqOpt.hasValue() && bpmOpt.hasValue())
            {
                double ppqPosition = *ppqOpt;
                double hostBpm = *bpmOpt;
                double stepLen = getStepLengthInQuarterNotes();
                double samplesPerQuarterNote = (60.0 / hostBpm) * sampleRate;
                double samplesPerStep = stepLen * samplesPerQuarterNote;
                int stepDurationSamples = juce::jmax(1, (int)samplesPerStep);

                for (int sample = 0; sample < numSamples; ++sample)
                {
                    double currentPpq = ppqPosition + (static_cast<double>(sample) / samplesPerQuarterNote);
                    double currentStepIndex = std::floor(currentPpq / stepLen);
                    double prevPpq = currentPpq - (1.0 / samplesPerQuarterNote);
                    double prevStepIndex = std::floor(prevPpq / stepLen);

                    if (currentStepIndex != prevStepIndex || lastPpqPosition < 0.0)
                    {
                        silenceNow(sample);
                        fireNoteOn(sample, stepDurationSamples);
                    }
                }

                lastPpqPosition = ppqPosition + (static_cast<double>(numSamples) / samplesPerQuarterNote);
            }
            else
            {
                // Host stopped — release any held notes immediately
                silenceNow(0);
                sentNote = -1;
                lastNote = -1;
                lastPpqPosition = -1.0;
            }
        }
    }
    else if (clockMode == ClockMode::Internal)
    {
        float beatsPerSecond = tempo / 60.0f;
        float samplesPerBeat = sampleRate / beatsPerSecond;
        float samplesPerTick = samplesPerBeat / 24.0f;
        int duration = static_cast<int>(samplesPerTick * ticksPerStep);

        timeSamples += numSamples;
        if (timeSamples >= duration)
        {
            timeSamples %= duration;
            silenceNow(0);
            fireNoteOn(0, duration);
        }
    }

    midi.swapWith(output);
}
