/*
  ==============================================================================

    Modules.h
    All MicroMod synth modules.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

namespace mm
{

constexpr int kNumVoices = 4;
constexpr int kMaxParams = 8;

enum class ModuleType
{
    Oscillator = 0,
    Noise,
    Filter,
    AmpEnv,
    Distortion,
    Delay,
    Reverb,
    LFO,
    Mixer,
    NumTypes
};

inline juce::String moduleTypeName (ModuleType t)
{
    switch (t)
    {
        case ModuleType::Oscillator: return "Oscillator";
        case ModuleType::Noise:      return "Noise";
        case ModuleType::Filter:     return "Filter";
        case ModuleType::AmpEnv:     return "Amp/Env";
        case ModuleType::Distortion: return "Distortion";
        case ModuleType::Delay:      return "Delay";
        case ModuleType::Reverb:     return "Reverb";
        case ModuleType::LFO:        return "LFO";
        case ModuleType::Mixer:      return "Mixer";
        default:                     return "?";
    }
}

//==============================================================================
/** Voice state shared between all modules in the chain. */
struct VoiceState
{
    int   midiNote = -1;
    float velocity = 0.0f;
    bool  gate     = false;
    int   ageSamples = 0;

    // Phase accumulators (per voice, used by oscillators)
    float phase = 0.0f;

    // Envelope state (used by amp/env)
    enum class EnvStage { Idle, Attack, Decay, Sustain, Release };
    EnvStage envStage = EnvStage::Idle;
    float    envValue = 0.0f;
};

/** Aggregated voice manager: 4-voice paraphonic with per-voice gate and
    per-voice envelope. Note allocation uses oldest-voice stealing. */
struct VoiceManager
{
    std::array<VoiceState, kNumVoices> voices;

    void reset()
    {
        for (auto& v : voices) v = {};
    }

    int findVoiceForNote (int note) const
    {
        for (int i = 0; i < kNumVoices; ++i)
            if (voices[(size_t) i].midiNote == note && voices[(size_t) i].gate)
                return i;
        return -1;
    }

    int allocateVoice (int note, float vel)
    {
        int existing = findVoiceForNote (note);
        if (existing >= 0)
        {
            voices[(size_t) existing].velocity = vel;
            voices[(size_t) existing].gate = true;
            return existing;
        }
        for (int i = 0; i < kNumVoices; ++i)
            if (! voices[(size_t) i].gate)
            {
                auto& v = voices[(size_t) i];
                v.midiNote = note;
                v.velocity = vel;
                v.gate = true;
                v.ageSamples = 0;
                v.envStage = VoiceState::EnvStage::Attack;
                return i;
            }
        // Steal oldest
        int oldest = 0;
        int oldestAge = -1;
        for (int i = 0; i < kNumVoices; ++i)
            if (voices[(size_t) i].ageSamples > oldestAge)
            {
                oldestAge = voices[(size_t) i].ageSamples;
                oldest = i;
            }
        auto& v = voices[(size_t) oldest];
        v.midiNote = note;
        v.velocity = vel;
        v.gate = true;
        v.ageSamples = 0;
        v.envStage = VoiceState::EnvStage::Attack;
        return oldest;
    }

    void releaseNote (int note)
    {
        for (auto& v : voices)
            if (v.midiNote == note && v.gate)
            {
                v.gate = false;
                v.envStage = VoiceState::EnvStage::Release;
            }
    }

    void releaseAll()
    {
        for (auto& v : voices)
        {
            v.gate = false;
            if (v.envStage != VoiceState::EnvStage::Idle)
                v.envStage = VoiceState::EnvStage::Release;
        }
    }
};

//==============================================================================
/** Per-call processing context shared with every module. */
struct ProcessContext
{
    double         sampleRate = 44100.0;
    int            numSamples = 0;
    VoiceManager*  voices = nullptr;

    // LFO modulation: an LFO writes a [-1..1] value here, and the *next*
    // module reads + clears it during its own process call.
    float          pendingModValue = 0.0f;
    bool           hasPendingMod   = false;
};

//==============================================================================
/** Base class for all processing modules. Modules are stored polymorphically
    in the chain and rebuilt lazily by the audio processor. */
class Module
{
public:
    Module() { for (auto& p : params) p.store (0.5f); }
    virtual ~Module() = default;

    virtual ModuleType getType() const = 0;
    virtual void prepare (double sampleRate, int blockSize) = 0;
    virtual void reset() = 0;

    /** Process a stereo block in place. */
    virtual void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) = 0;

    bool isEnabled() const noexcept { return enabled.load(); }
    void setEnabled (bool e) noexcept { enabled.store (e); }

    float getParam (int idx) const noexcept
    {
        return params[(size_t) juce::jlimit (0, kMaxParams - 1, idx)].load();
    }

    void setParam (int idx, float v) noexcept
    {
        params[(size_t) juce::jlimit (0, kMaxParams - 1, idx)].store (v);
    }

    /** Number of user-visible parameters. Must match getParamName/Range below. */
    virtual int getNumParams() const = 0;
    virtual juce::String getParamName (int idx) const = 0;
    virtual juce::NormalisableRange<float> getParamRange (int idx) const = 0;
    virtual juce::String getParamSuffix (int idx) const { (void) idx; return {}; }

protected:
    std::array<std::atomic<float>, kMaxParams> params;
    std::atomic<bool> enabled { true };
};

//==============================================================================
class OscillatorModule : public Module
{
public:
    enum Param { Wave = 0, Tune, Fine, Level };

    OscillatorModule()
    {
        setParam (Wave, 0.0f);   // 0..3 -> sine, saw, square, triangle
        setParam (Tune, 0.0f);   // semitones, -24..24
        setParam (Fine, 0.0f);   // cents, -100..100
        setParam (Level, 0.7f);  // 0..1
    }

    ModuleType getType() const override { return ModuleType::Oscillator; }
    int getNumParams() const override { return 4; }

    juce::String getParamName (int i) const override
    {
        switch (i) { case Wave: return "Wave"; case Tune: return "Tune"; case Fine: return "Fine"; case Level: return "Level"; }
        return {};
    }

    juce::NormalisableRange<float> getParamRange (int i) const override
    {
        switch (i)
        {
            case Wave:  return { 0.0f, 3.0f, 1.0f };
            case Tune:  return { -24.0f, 24.0f, 1.0f };
            case Fine:  return { -100.0f, 100.0f, 0.1f };
            case Level: return { 0.0f, 1.0f };
        }
        return { 0.0f, 1.0f };
    }

    juce::String getParamSuffix (int i) const override
    {
        switch (i) { case Tune: return " st"; case Fine: return " ct"; }
        return {};
    }

    void prepare (double sr, int /*blockSize*/) override { sampleRate = sr; }
    void reset() override {}

    void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) override
    {
        if (! isEnabled() || ctx.voices == nullptr) return;

        // Read modulation pending from preceding LFO (modulates Tune).
        float modSemi = 0.0f;
        if (ctx.hasPendingMod)
        {
            modSemi = ctx.pendingModValue * 12.0f; // up to ±12 semitones
            ctx.hasPendingMod = false;
            ctx.pendingModValue = 0.0f;
        }

        const int wave  = (int) std::round (juce::jlimit (0.0f, 3.0f, getParam (Wave)));
        const float tuneSemi = getParam (Tune) + modSemi;
        const float fineCent = getParam (Fine);
        const float level    = getParam (Level);

        const int n = ctx.numSamples;
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int s = 0; s < n; ++s)
        {
            float sample = 0.0f;
            for (int v = 0; v < kNumVoices; ++v)
            {
                auto& voice = ctx.voices->voices[(size_t) v];
                if (voice.midiNote < 0 || (voice.envStage == VoiceState::EnvStage::Idle && ! voice.gate))
                    continue;

                const float baseHz = juce::MidiMessage::getMidiNoteInHertz (voice.midiNote);
                const float ratio  = std::pow (2.0f, (tuneSemi + fineCent * 0.01f) / 12.0f);
                const float freq   = baseHz * ratio;
                const float inc    = freq / (float) sampleRate;

                voice.phase += inc;
                if (voice.phase >= 1.0f) voice.phase -= 1.0f;

                sample += oscShape (voice.phase, wave) * voice.velocity;
            }
            sample *= level * 0.25f; // scale for 4 voice headroom
            L[s] += sample;
            if (R) R[s] += sample;
        }
    }

private:
    static float oscShape (float phase, int wave)
    {
        switch (wave)
        {
            case 0: return std::sin (phase * juce::MathConstants<float>::twoPi);     // sine
            case 1: return 2.0f * phase - 1.0f;                                       // saw
            case 2: return phase < 0.5f ? 1.0f : -1.0f;                               // square
            case 3: return 4.0f * std::abs (phase - 0.5f) - 1.0f;                     // triangle
            default: return 0.0f;
        }
    }

    double sampleRate = 44100.0;
};

//==============================================================================
class NoiseModule : public Module
{
public:
    enum Param { Type = 0, Level };

    NoiseModule()
    {
        setParam (Type, 0.0f);   // 0=white, 1=pink
        setParam (Level, 0.5f);
    }

    ModuleType getType() const override { return ModuleType::Noise; }
    int getNumParams() const override { return 2; }
    juce::String getParamName (int i) const override { return i == Type ? "Type" : "Level"; }
    juce::NormalisableRange<float> getParamRange (int i) const override
    {
        return i == Type ? juce::NormalisableRange<float> (0.0f, 1.0f, 1.0f)
                         : juce::NormalisableRange<float> (0.0f, 1.0f);
    }

    void prepare (double, int) override {}
    void reset() override { for (auto& s : pinkState) s = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) override
    {
        if (! isEnabled()) return;

        const int kind = (int) std::round (getParam (Type));
        const float lvl = getParam (Level) * 0.5f;
        const int n = ctx.numSamples;

        // Only emit when at least one voice is active, so noise is gated.
        bool anyActive = false;
        if (ctx.voices)
            for (auto& v : ctx.voices->voices)
                if (v.envStage != VoiceState::EnvStage::Idle || v.gate) { anyActive = true; break; }
        if (! anyActive) return;

        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int s = 0; s < n; ++s)
        {
            float w = rng.nextFloat() * 2.0f - 1.0f;
            float sample;
            if (kind == 0)
            {
                sample = w;
            }
            else
            {
                // Paul Kellet pink noise approximation
                pinkState[0] = 0.99886f * pinkState[0] + w * 0.0555179f;
                pinkState[1] = 0.99332f * pinkState[1] + w * 0.0750759f;
                pinkState[2] = 0.96900f * pinkState[2] + w * 0.1538520f;
                pinkState[3] = 0.86650f * pinkState[3] + w * 0.3104856f;
                pinkState[4] = 0.55000f * pinkState[4] + w * 0.5329522f;
                pinkState[5] = -0.7616f * pinkState[5] - w * 0.0168980f;
                sample = pinkState[0] + pinkState[1] + pinkState[2] + pinkState[3]
                       + pinkState[4] + pinkState[5] + pinkState[6] + w * 0.5362f;
                pinkState[6] = w * 0.115926f;
                sample *= 0.11f;
            }
            sample *= lvl;
            L[s] += sample;
            if (R) R[s] += sample;
        }
    }

private:
    juce::Random rng;
    std::array<float, 7> pinkState { };
};

//==============================================================================
class FilterModule : public Module
{
public:
    enum Param { TypeIdx = 0, Cutoff, Resonance };

    FilterModule()
    {
        setParam (TypeIdx, 0.0f);     // 0=LP, 1=HP, 2=BP
        setParam (Cutoff, 0.7f);
        setParam (Resonance, 0.2f);
    }

    ModuleType getType() const override { return ModuleType::Filter; }
    int getNumParams() const override { return 3; }
    juce::String getParamName (int i) const override
    {
        switch (i) { case TypeIdx: return "Type"; case Cutoff: return "Cutoff"; case Resonance: return "Reso"; }
        return {};
    }
    juce::NormalisableRange<float> getParamRange (int i) const override
    {
        if (i == TypeIdx)   return { 0.0f, 2.0f, 1.0f };
        if (i == Cutoff)    return { 0.0f, 1.0f };
        return { 0.0f, 1.0f };
    }

    void prepare (double sr, int /*blockSize*/) override
    {
        sampleRate = sr;
        for (auto& f : filters) f.reset();
    }

    void reset() override { for (auto& f : filters) f.reset(); }

    void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) override
    {
        if (! isEnabled()) return;

        float cutoffMod = 0.0f;
        if (ctx.hasPendingMod)
        {
            cutoffMod = ctx.pendingModValue * 0.5f;
            ctx.hasPendingMod = false;
            ctx.pendingModValue = 0.0f;
        }

        const int t = (int) std::round (getParam (TypeIdx));
        float cutoffNorm = juce::jlimit (0.0f, 1.0f, getParam (Cutoff) + cutoffMod);
        const float minHz = 30.0f, maxHz = 18000.0f;
        const float freq = minHz * std::pow (maxHz / minHz, cutoffNorm);
        const float q = juce::jmap (getParam (Resonance), 0.0f, 1.0f, 0.4f, 8.0f);

        juce::IIRCoefficients coeffs;
        switch (t)
        {
            case 0: coeffs = juce::IIRCoefficients::makeLowPass  (sampleRate, freq, q); break;
            case 1: coeffs = juce::IIRCoefficients::makeHighPass (sampleRate, freq, q); break;
            case 2: coeffs = juce::IIRCoefficients::makeBandPass (sampleRate, freq, q); break;
            default: coeffs = juce::IIRCoefficients::makeLowPass (sampleRate, freq, q); break;
        }
        for (auto& f : filters) f.setCoefficients (coeffs);

        const int numCh = juce::jmin (2, buffer.getNumChannels());
        for (int ch = 0; ch < numCh; ++ch)
            filters[(size_t) ch].processSamples (buffer.getWritePointer (ch), ctx.numSamples);
    }

private:
    double sampleRate = 44100.0;
    std::array<juce::IIRFilter, 2> filters;
};

//==============================================================================
class AmpEnvModule : public Module
{
public:
    enum Param { Attack = 0, Decay, Sustain, Release, Level };

    AmpEnvModule()
    {
        setParam (Attack, 0.05f);
        setParam (Decay, 0.2f);
        setParam (Sustain, 0.7f);
        setParam (Release, 0.3f);
        setParam (Level, 0.8f);
    }

    ModuleType getType() const override { return ModuleType::AmpEnv; }
    int getNumParams() const override { return 5; }
    juce::String getParamName (int i) const override
    {
        switch (i) { case Attack: return "Attack"; case Decay: return "Decay";
                     case Sustain: return "Sustain"; case Release: return "Release";
                     case Level: return "Level"; }
        return {};
    }
    juce::NormalisableRange<float> getParamRange (int i) const override
    {
        return { 0.0f, 1.0f };
    }

    void prepare (double sr, int) override { sampleRate = sr; }
    void reset() override {}

    void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) override
    {
        if (! isEnabled() || ctx.voices == nullptr) return;

        const float a = std::max (0.001f, getParam (Attack)  * 2.0f); // 0..2s
        const float d = std::max (0.001f, getParam (Decay)   * 2.0f);
        const float s = getParam (Sustain);
        const float r = std::max (0.001f, getParam (Release) * 3.0f);
        const float lvl = getParam (Level);

        const float aInc = 1.0f / (a * (float) sampleRate);
        const float dInc = 1.0f / (d * (float) sampleRate);
        const float rInc = 1.0f / (r * (float) sampleRate);

        const int n = ctx.numSamples;
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        // Compute summed envelope across voices each sample. Per-voice envelope
        // tracks state; we apply the summed envelope as a single VCA on the bus
        // to keep a strict left-to-right chain.
        for (int i = 0; i < n; ++i)
        {
            float env = 0.0f;
            for (auto& voice : ctx.voices->voices)
            {
                using Stage = VoiceState::EnvStage;
                switch (voice.envStage)
                {
                    case Stage::Attack:
                        voice.envValue += aInc;
                        if (voice.envValue >= 1.0f) { voice.envValue = 1.0f; voice.envStage = Stage::Decay; }
                        break;
                    case Stage::Decay:
                        voice.envValue -= dInc * (1.0f - s);
                        if (voice.envValue <= s) { voice.envValue = s; voice.envStage = Stage::Sustain; }
                        break;
                    case Stage::Sustain:
                        voice.envValue = s;
                        if (! voice.gate) voice.envStage = Stage::Release;
                        break;
                    case Stage::Release:
                        voice.envValue -= rInc;
                        if (voice.envValue <= 0.0f) { voice.envValue = 0.0f; voice.envStage = Stage::Idle; voice.midiNote = -1; }
                        break;
                    case Stage::Idle:
                        voice.envValue = 0.0f;
                        break;
                }
                env += voice.envValue;
                voice.ageSamples++;
            }
            env = juce::jlimit (0.0f, (float) kNumVoices, env) / (float) kNumVoices;
            const float g = env * lvl;
            L[i] *= g;
            if (R) R[i] *= g;
        }
    }

private:
    double sampleRate = 44100.0;
};

//==============================================================================
class DistortionModule : public Module
{
public:
    enum Param { Drive = 0, Tone, Mix };

    DistortionModule()
    {
        setParam (Drive, 0.3f);
        setParam (Tone, 0.5f);
        setParam (Mix, 1.0f);
    }

    ModuleType getType() const override { return ModuleType::Distortion; }
    int getNumParams() const override { return 3; }
    juce::String getParamName (int i) const override
    {
        switch (i) { case Drive: return "Drive"; case Tone: return "Tone"; case Mix: return "Mix"; }
        return {};
    }
    juce::NormalisableRange<float> getParamRange (int) const override { return { 0.0f, 1.0f }; }

    void prepare (double sr, int /*blockSize*/) override
    {
        sampleRate = sr;
        for (auto& f : tone) f.reset();
    }

    void reset() override { for (auto& f : tone) f.reset(); }

    void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) override
    {
        if (! isEnabled()) return;

        const float drive = 1.0f + getParam (Drive) * 24.0f;
        const float toneN = getParam (Tone);
        const float mix = getParam (Mix);

        const float toneFreq = juce::jmap (toneN, 0.0f, 1.0f, 800.0f, 8000.0f);
        const auto coeffs = juce::IIRCoefficients::makeLowPass (sampleRate, toneFreq, 0.7f);
        for (auto& f : tone) f.setCoefficients (coeffs);

        const int numCh = juce::jmin (2, buffer.getNumChannels());
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            auto& f = tone[(size_t) ch];
            for (int i = 0; i < ctx.numSamples; ++i)
            {
                const float in = d[i];
                float wet = std::tanh (in * drive) / std::tanh (drive);
                wet = f.processSingleSampleRaw (wet);
                d[i] = in * (1.0f - mix) + wet * mix;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    std::array<juce::IIRFilter, 2> tone;
};

//==============================================================================
class DelayModule : public Module
{
public:
    enum Param { Time = 0, Feedback, Mix };

    DelayModule()
    {
        setParam (Time, 0.3f);
        setParam (Feedback, 0.4f);
        setParam (Mix, 0.3f);
    }

    ModuleType getType() const override { return ModuleType::Delay; }
    int getNumParams() const override { return 3; }
    juce::String getParamName (int i) const override
    {
        switch (i) { case Time: return "Time"; case Feedback: return "Feedback"; case Mix: return "Mix"; }
        return {};
    }
    juce::NormalisableRange<float> getParamRange (int) const override { return { 0.0f, 1.0f }; }

    void prepare (double sr, int blockSize) override
    {
        sampleRate = sr;
        const int maxSamples = (int) (sr * 2.0); // 2 seconds max
        for (auto& b : buffers)
        {
            b.assign ((size_t) maxSamples, 0.0f);
        }
        writePos = 0;
        bufLen = maxSamples;
    }

    void reset() override
    {
        for (auto& b : buffers) std::fill (b.begin(), b.end(), 0.0f);
        writePos = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) override
    {
        if (! isEnabled()) return;

        const float timeS = juce::jmap (getParam (Time), 0.0f, 1.0f, 0.01f, 1.5f);
        const int delaySamples = juce::jlimit (1, bufLen - 1, (int) (timeS * sampleRate));
        const float fb = juce::jlimit (0.0f, 0.95f, getParam (Feedback));
        const float mix = getParam (Mix);

        const int numCh = juce::jmin (2, buffer.getNumChannels());
        for (int i = 0; i < ctx.numSamples; ++i)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& buf = buffers[(size_t) ch];
                int rp = writePos - delaySamples;
                while (rp < 0) rp += bufLen;
                const float dly = buf[(size_t) rp];
                float in = buffer.getWritePointer (ch)[i];
                buf[(size_t) writePos] = in + dly * fb;
                buffer.getWritePointer (ch)[i] = in * (1.0f - mix) + dly * mix;
            }
            writePos = (writePos + 1) % bufLen;
        }
    }

private:
    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> buffers;
    int writePos = 0;
    int bufLen = 0;
};

//==============================================================================
class ReverbModule : public Module
{
public:
    enum Param { Size = 0, Damping, Mix };

    ReverbModule()
    {
        setParam (Size, 0.5f);
        setParam (Damping, 0.5f);
        setParam (Mix, 0.3f);
    }

    ModuleType getType() const override { return ModuleType::Reverb; }
    int getNumParams() const override { return 3; }
    juce::String getParamName (int i) const override
    {
        switch (i) { case Size: return "Size"; case Damping: return "Damp"; case Mix: return "Mix"; }
        return {};
    }
    juce::NormalisableRange<float> getParamRange (int) const override { return { 0.0f, 1.0f }; }

    void prepare (double sr, int /*blockSize*/) override
    {
        reverb.reset();
        reverb.setSampleRate (sr);
    }

    void reset() override { reverb.reset(); }

    void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) override
    {
        if (! isEnabled()) return;

        juce::Reverb::Parameters p;
        p.roomSize = getParam (Size);
        p.damping  = getParam (Damping);
        p.wetLevel = getParam (Mix);
        p.dryLevel = 1.0f - getParam (Mix);
        p.width    = 1.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);

        if (buffer.getNumChannels() >= 2)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), ctx.numSamples);
        else
            reverb.processMono (buffer.getWritePointer (0), ctx.numSamples);
    }

private:
    juce::Reverb reverb;
};

//==============================================================================
class LFOModule : public Module
{
public:
    enum Param { Rate = 0, Depth, Target };

    LFOModule()
    {
        setParam (Rate, 0.3f);
        setParam (Depth, 0.5f);
        setParam (Target, 0.0f); // informational; the LFO writes mod for next module
    }

    ModuleType getType() const override { return ModuleType::LFO; }
    int getNumParams() const override { return 3; }
    juce::String getParamName (int i) const override
    {
        switch (i) { case Rate: return "Rate"; case Depth: return "Depth"; case Target: return "Target"; }
        return {};
    }
    juce::NormalisableRange<float> getParamRange (int i) const override
    {
        if (i == Target) return { 0.0f, 3.0f, 1.0f }; // 0=Pitch,1=Cutoff,2=Amp,3=Mix
        return { 0.0f, 1.0f };
    }

    void prepare (double sr, int) override { sampleRate = sr; }
    void reset() override { phase = 0.0f; }

    void process (juce::AudioBuffer<float>& /*buffer*/, ProcessContext& ctx) override
    {
        if (! isEnabled()) return;

        const float rateHz = juce::jmap (getParam (Rate), 0.0f, 1.0f, 0.05f, 20.0f);
        const float depth = getParam (Depth);
        const float inc = rateHz / (float) sampleRate;

        // Advance LFO across the block, output the value at end of block.
        for (int i = 0; i < ctx.numSamples; ++i)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
        }
        const float v = std::sin (phase * juce::MathConstants<float>::twoPi) * depth;

        ctx.pendingModValue = v;
        ctx.hasPendingMod = true;
    }

private:
    double sampleRate = 44100.0;
    float phase = 0.0f;
};

//==============================================================================
class MixerModule : public Module
{
public:
    /** 4 channels x (level, pan). Channel 1 affects the bus signal directly,
        channels 2..4 are reserved utility gain stages applied in series. The
        scaffold is laid out so a future revision can route per-source signals
        independently into each channel. */
    enum Param { L1 = 0, P1, L2, P2, L3, P3, L4, P4 };

    MixerModule()
    {
        for (int i = 0; i < 4; ++i)
        {
            setParam (L1 + i * 2, 0.8f);
            setParam (P1 + i * 2, 0.5f); // 0.5 = center
        }
    }

    ModuleType getType() const override { return ModuleType::Mixer; }
    int getNumParams() const override { return 8; }
    juce::String getParamName (int i) const override
    {
        const int ch = (i / 2) + 1;
        return ((i % 2) == 0 ? "L" : "Pan") + juce::String (ch);
    }
    juce::NormalisableRange<float> getParamRange (int) const override { return { 0.0f, 1.0f }; }

    void prepare (double, int) override {}
    void reset() override {}

    void process (juce::AudioBuffer<float>& buffer, ProcessContext& ctx) override
    {
        if (! isEnabled()) return;

        // Apply 4 series gain/pan stages to the bus.
        if (buffer.getNumChannels() < 2) return;
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);

        std::array<float, 4> gain;
        std::array<float, 4> panL;
        std::array<float, 4> panR;
        for (int c = 0; c < 4; ++c)
        {
            gain[(size_t) c] = getParam (L1 + c * 2);
            const float pan = getParam (P1 + c * 2) * 2.0f - 1.0f; // -1..1
            panL[(size_t) c] = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            panR[(size_t) c] = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        }

        // Apply only channel 1 here (single-bus signal). Channels 2-4 act as
        // additional master gain stages so the controls remain useful.
        const float g = gain[0];
        const float pl = panL[0];
        const float pr = panR[0];
        const float postGain = gain[1] * gain[2] * gain[3];

        for (int i = 0; i < ctx.numSamples; ++i)
        {
            const float l = L[i] * g * pl;
            const float r = R[i] * g * pr;
            L[i] = l * postGain;
            R[i] = r * postGain;
        }
    }
};

//==============================================================================
inline std::unique_ptr<Module> createModule (ModuleType t)
{
    switch (t)
    {
        case ModuleType::Oscillator: return std::make_unique<OscillatorModule>();
        case ModuleType::Noise:      return std::make_unique<NoiseModule>();
        case ModuleType::Filter:     return std::make_unique<FilterModule>();
        case ModuleType::AmpEnv:     return std::make_unique<AmpEnvModule>();
        case ModuleType::Distortion: return std::make_unique<DistortionModule>();
        case ModuleType::Delay:      return std::make_unique<DelayModule>();
        case ModuleType::Reverb:     return std::make_unique<ReverbModule>();
        case ModuleType::LFO:        return std::make_unique<LFOModule>();
        case ModuleType::Mixer:      return std::make_unique<MixerModule>();
        default:                     return nullptr;
    }
}

} // namespace mm
