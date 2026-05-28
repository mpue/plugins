/*
  ==============================================================================

    IRLibrary.cpp

  ==============================================================================
*/

#include "IRLibrary.h"
#include <cmath>

IRLibrary::IRLibrary()
{
    buildFactoryEntries();
}

void IRLibrary::initialise (double sampleRate)
{
    currentSampleRate = sampleRate;
    rescanUserIRs();
}

void IRLibrary::setSampleRate (double sampleRate)
{
    currentSampleRate = sampleRate;
}

juce::File IRLibrary::getUserIRFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("CRV-1")
                   .getChildFile ("IRs");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

void IRLibrary::buildFactoryEntries()
{
    entries.clear();

    auto addFactory = [this] (const juce::String& name, int type, float len)
    {
        Entry e;
        e.name = name;
        e.isFactory = true;
        e.factoryType = type;
        e.baseLengthSec = len;
        entries.push_back (e);
    };

    // Twelve hand-tuned factory IRs covering the gamut of musical spaces.
    addFactory ("Grand Cathedral",   0, 8.5f);
    addFactory ("Symphony Hall",     1, 4.5f);
    addFactory ("Concert Hall",      2, 3.2f);
    addFactory ("Opera House",       3, 3.8f);
    addFactory ("Vintage Plate",     4, 2.6f);
    addFactory ("Studio Plate",      5, 1.8f);
    addFactory ("Bright Chamber",    6, 1.6f);
    addFactory ("Wood Chamber",      7, 2.0f);
    addFactory ("Drum Room",         8, 0.9f);
    addFactory ("Vocal Room",        9, 1.2f);
    addFactory ("Ambient Cavern",   10, 6.5f);
    addFactory ("Infinite Bloom",   11, 12.0f);
}

void IRLibrary::rescanUserIRs()
{
    // Strip any old user entries
    entries.erase (std::remove_if (entries.begin(), entries.end(),
                                   [] (const Entry& e) { return ! e.isFactory; }),
                   entries.end());

    auto dir = getUserIRFolder();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
    files.sort();

    for (auto& f : files)
    {
        Entry e;
        e.name = "[User] " + f.getFileNameWithoutExtension();
        e.isFactory = false;
        e.userFile = f;
        e.baseLengthSec = 3.0f; // discovered at load time
        entries.push_back (e);
    }
}

bool IRLibrary::importUserIR (const juce::File& source)
{
    if (! source.existsAsFile()) return false;
    auto dest = getUserIRFolder().getChildFile (source.getFileName());
    if (dest.existsAsFile()) dest.deleteFile();
    if (! source.copyFileTo (dest))
        return false;
    rescanUserIRs();
    return true;
}

bool IRLibrary::deleteUserIR (const juce::String& displayName)
{
    for (auto& e : entries)
    {
        if (! e.isFactory && e.name == displayName && e.userFile.existsAsFile())
        {
            bool ok = e.userFile.deleteFile();
            if (ok) rescanUserIRs();
            return ok;
        }
    }
    return false;
}

int IRLibrary::findEntryByName (const juce::String& name) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].name == name)
            return (int) i;
    return -1;
}

// ============================================================================
// Rendering
// ============================================================================

juce::AudioBuffer<float> IRLibrary::renderIR (int entryIndex,
                                              float sizeNorm,
                                              float decayNorm,
                                              juce::String& outDisplayInfo)
{
    if (entryIndex < 0 || entryIndex >= (int) entries.size())
        entryIndex = 0;

    const auto& e = entries[(size_t) entryIndex];

    // Size 0..1 -> stretch 0.4..2.2
    const float stretch = juce::jlimit (0.4f, 2.2f, 0.4f + 1.8f * sizeNorm);
    // Decay 0..1 -> envelope multiplier 0.5..2.0 (relative T60)
    const float decayMul = juce::jlimit (0.5f, 2.0f, 0.5f + 1.5f * decayNorm);

    juce::AudioBuffer<float> buf;
    if (e.isFactory)
    {
        const float baseLen = e.baseLengthSec;
        const float renderedLen = juce::jlimit (0.2f, 22.0f, baseLen * stretch);
        buf = renderFactoryIR (e.factoryType, renderedLen, decayMul, currentSampleRate);
        outDisplayInfo = e.name + "  ·  "
                       + juce::String (renderedLen, 2) + " s  ·  "
                       + juce::String (buf.getNumChannels()) + " ch";
    }
    else
    {
        float originalLen = 0.0f;
        auto raw = loadUserIRFile (e.userFile, currentSampleRate, originalLen);
        if (raw.getNumSamples() < 16)
        {
            // Fallback: tiny click so the convolver does not choke
            raw.setSize (2, 64, false, true, true);
            raw.clear();
            raw.setSample (0, 0, 1.0f);
            if (raw.getNumChannels() > 1) raw.setSample (1, 0, 1.0f);
        }

        if (std::abs (stretch - 1.0f) > 0.01f)
            stretchInPlace (raw, stretch);

        applyExtraEnvelope (raw, decayMul);
        buf = std::move (raw);

        const float renderedLen = (float) buf.getNumSamples() / (float) currentSampleRate;
        outDisplayInfo = e.name + "  ·  "
                       + juce::String (renderedLen, 2) + " s  ·  "
                       + juce::String (buf.getNumChannels()) + " ch";
    }

    normaliseRMS (buf, 0.85f);
    return buf;
}

// ============================================================================
// Procedural IR generator – the "luxury" sound generator
// ============================================================================

juce::AudioBuffer<float> IRLibrary::renderFactoryIR (int factoryType,
                                                     float lengthSec,
                                                     float decayMul,
                                                     double sampleRate)
{
    const int N = juce::jmax (1024, (int) (lengthSec * sampleRate));
    juce::AudioBuffer<float> ir (2, N);
    ir.clear();

    // Use a deterministic seed per type so the IR is stable across reloads
    juce::Random rng ((juce::int64) (1009 + factoryType * 4271));

    // ---- Character parameters per type ----
    struct Profile
    {
        float earlyDensity;       // reflections per second in first 150ms
        float earlyLevel;         // amplitude of early reflections
        float predelayMs;         // gap before late tail
        float lateLevel;          // late tail amplitude
        float lowDecayMul;        // multiplier applied to low frequency band decay
        float midDecayMul;        // multiplier for mid band
        float highDecayMul;       // multiplier for high band (always <= mid)
        float highCutHz;          // initial high cut of tail
        float airAbsorption;      // rate at which high cut drops over time (Hz/s)
        float stereoSpread;       // 0..1 amount of L/R decorrelation
        float diffusionSmooth;    // 0..1 smoothing across samples (denser, less grainy)
        float modalDensity;       // 0..1 small modal resonances bringing colour
    };

    Profile pf {};
    switch (factoryType)
    {
        case 0: // Grand Cathedral
            pf = { 18.f, 0.55f,  35.f, 0.95f, 1.20f, 1.00f, 0.55f,  5000.f, 220.f, 0.85f, 0.55f, 0.35f };
            break;
        case 1: // Symphony Hall
            pf = { 30.f, 0.65f,  18.f, 0.90f, 1.10f, 1.00f, 0.65f,  7000.f, 260.f, 0.75f, 0.45f, 0.25f };
            break;
        case 2: // Concert Hall
            pf = { 26.f, 0.60f,  14.f, 0.88f, 1.05f, 1.00f, 0.70f,  7500.f, 280.f, 0.70f, 0.45f, 0.20f };
            break;
        case 3: // Opera House
            pf = { 34.f, 0.72f,  16.f, 0.85f, 1.08f, 1.00f, 0.70f,  8000.f, 250.f, 0.78f, 0.50f, 0.30f };
            break;
        case 4: // Vintage Plate
            pf = { 90.f, 0.45f,   4.f, 0.95f, 0.95f, 1.00f, 0.85f, 10000.f, 200.f, 0.55f, 0.65f, 0.55f };
            break;
        case 5: // Studio Plate
            pf = {110.f, 0.40f,   2.f, 0.92f, 0.95f, 1.00f, 0.90f, 12000.f, 240.f, 0.55f, 0.65f, 0.45f };
            break;
        case 6: // Bright Chamber
            pf = { 60.f, 0.55f,   8.f, 0.85f, 0.95f, 1.00f, 0.80f, 10500.f, 320.f, 0.60f, 0.45f, 0.30f };
            break;
        case 7: // Wood Chamber
            pf = { 55.f, 0.60f,   9.f, 0.86f, 1.10f, 1.00f, 0.65f,  8500.f, 380.f, 0.62f, 0.48f, 0.40f };
            break;
        case 8: // Drum Room
            pf = { 80.f, 0.80f,   3.f, 0.78f, 1.10f, 1.00f, 0.75f, 11000.f, 500.f, 0.50f, 0.40f, 0.20f };
            break;
        case 9: // Vocal Room
            pf = { 70.f, 0.55f,   6.f, 0.80f, 1.00f, 1.00f, 0.75f,  9500.f, 360.f, 0.55f, 0.50f, 0.30f };
            break;
        case 10: // Ambient Cavern
            pf = { 16.f, 0.50f,  40.f, 1.00f, 1.30f, 1.00f, 0.45f,  4500.f, 180.f, 0.92f, 0.62f, 0.40f };
            break;
        case 11: // Infinite Bloom
            pf = { 12.f, 0.45f,  55.f, 1.00f, 1.40f, 1.00f, 0.40f,  4200.f, 120.f, 0.95f, 0.70f, 0.30f };
            break;
        default:
            pf = { 30.f, 0.60f,  18.f, 0.90f, 1.10f, 1.00f, 0.70f,  7000.f, 280.f, 0.75f, 0.50f, 0.30f };
            break;
    }

    const float lateT60 = lengthSec * 0.95f;
    const int   tailStart = juce::jlimit (8, N - 16,
        (int) ((pf.predelayMs * 0.001f) * (float) sampleRate));

    // ---- 1) Early reflections (sparse, decorrelated) ----
    const int earlyWindow = juce::jmin (N - 1, (int) (0.18 * sampleRate));
    const int numER = juce::jmax (1, (int) (pf.earlyDensity * 0.18f));
    for (int i = 0; i < numER; ++i)
    {
        const float t = (float) i / (float) numER;
        // bias toward earlier reflections
        const int posL = juce::jlimit (1, earlyWindow,
            (int) (std::pow (rng.nextFloat(), 0.6f) * (float) earlyWindow));
        const int posR = juce::jlimit (1, earlyWindow,
            posL + rng.nextInt (40) - 20);

        const float envEarly = std::pow (1.0f - t, 1.4f);
        const float pol     = rng.nextBool() ? 1.0f : -1.0f;
        const float ampL    = envEarly * pf.earlyLevel * pol * (0.6f + 0.4f * rng.nextFloat());
        const float ampR    = envEarly * pf.earlyLevel * (rng.nextBool() ? 1.0f : -1.0f)
                            * (0.6f + 0.4f * rng.nextFloat());

        ir.addSample (0, posL, ampL);
        ir.addSample (1, posR, ampR);
    }

    // ---- 2) Dense late tail: three decorrelated noise streams per channel,
    //         each filtered with a frequency-dependent decay envelope.
    //         The three bands (low/mid/high) decay at different rates,
    //         which is what gives a "huge", musical, non-metallic feel.
    auto tailEnv = [decayMul, lateT60] (float t, float bandDecayMul)
    {
        const float t60 = juce::jmax (0.05f, lateT60 * decayMul * bandDecayMul);
        return std::exp (-6.9078f * t / t60);
    };

    // 1-pole low/high coefficients
    auto computeLPa = [sampleRate] (float fc)
    {
        const float x = std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sampleRate);
        return 1.0f - x; // y = a*x + (1-a)*y
    };

    const float lowFc  = 220.0f;
    const float highFc = 2200.0f;
    const float aLow   = computeLPa (lowFc);
    const float aHigh  = computeLPa (highFc);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = ir.getWritePointer (ch);

        // Per-channel different seed for decorrelation
        juce::Random chRng ((juce::int64) (factoryType * 13 + ch * 9931 + 7));

        float lpLowState  = 0.0f;
        float lpHighState = 0.0f;
        float smooth      = 0.0f;

        for (int i = tailStart; i < N; ++i)
        {
            const float t = (float) (i - tailStart) / (float) sampleRate;

            // White noise source
            const float n = (chRng.nextFloat() * 2.0f - 1.0f);

            // Split into 3 bands using cascaded 1-pole filters
            lpLowState  += aLow  * (n - lpLowState);
            lpHighState += aHigh * (n - lpHighState);
            const float lowBand  = lpLowState * 2.5f;          // boost low band (low-freq energy)
            const float midBand  = lpHighState - lpLowState;
            const float highBand = n - lpHighState;

            const float band = lowBand  * tailEnv (t, pf.lowDecayMul)
                             + midBand  * tailEnv (t, pf.midDecayMul) * 1.1f
                             + highBand * tailEnv (t, pf.highDecayMul) * 0.7f;

            // Smoothing — denser, less grainy late field
            smooth += (band - smooth) * (1.0f - pf.diffusionSmooth * 0.65f);

            data[i] += smooth * pf.lateLevel * 0.45f;
        }

        // Stereo decorrelation: cross-feed slightly less, keep wide stereo
        // (independent random sequences already handled this; nothing further)

        // ---- 3) Air absorption: a 1-pole LPF whose cutoff drops over time ----
        float airState = 0.0f;
        for (int i = tailStart; i < N; ++i)
        {
            const float t = (float) (i - tailStart) / (float) sampleRate;
            const float fc = juce::jmax (700.0f, pf.highCutHz - pf.airAbsorption * t);
            const float a = computeLPa (fc);
            airState += a * (data[i] - airState);
            data[i] = airState;
        }

        // ---- 4) Small modal resonances – add subtle character ----
        const int numModes = juce::jlimit (0, 6, (int) (pf.modalDensity * 5.0f + 0.5f));
        for (int m = 0; m < numModes; ++m)
        {
            const float freq  = 80.0f + chRng.nextFloat() * 900.0f;
            const float modeT60 = lateT60 * (0.4f + 0.4f * chRng.nextFloat()) * decayMul;
            const float damp  = std::exp (-1.0f / (modeT60 * (float) sampleRate * 0.5f));
            const float w     = juce::MathConstants<float>::twoPi * freq / (float) sampleRate;
            const float lvl   = 0.05f + 0.05f * chRng.nextFloat();
            const float phase = chRng.nextFloat() * juce::MathConstants<float>::twoPi;

            float y1 = 0.0f, y2 = 0.0f;
            // y[n] = 2*r*cos(w) y[n-1] - r^2 y[n-2] + impulse
            const float a1 = 2.0f * damp * std::cos (w);
            const float a2 = -damp * damp;

            for (int i = tailStart; i < N; ++i)
            {
                const float input = (i == tailStart) ? std::sin (phase) * lvl : 0.0f;
                const float y0 = input + a1 * y1 + a2 * y2;
                data[i] += y0;
                y2 = y1;
                y1 = y0;
            }
        }
    }

    // ---- 5) Apply gentle fade-in at the very start of the tail so there is
    //         no audible discontinuity, plus a fade-out across the last 5% ----
    const int fadeIn  = juce::jmin (256, (int) (0.005 * sampleRate));
    for (int i = 0; i < fadeIn && (tailStart + i) < N; ++i)
    {
        const float g = (float) i / (float) fadeIn;
        for (int ch = 0; ch < 2; ++ch)
            ir.setSample (ch, tailStart + i,
                          ir.getSample (ch, tailStart + i) * g);
    }
    const int fadeOut = juce::jmax (64, N / 20);
    for (int i = 0; i < fadeOut; ++i)
    {
        const int idx = N - fadeOut + i;
        const float g = 1.0f - (float) i / (float) fadeOut;
        for (int ch = 0; ch < 2; ++ch)
            ir.setSample (ch, idx, ir.getSample (ch, idx) * g * g);
    }

    return ir;
}

// ============================================================================
// User IR loading
// ============================================================================

juce::AudioBuffer<float> IRLibrary::loadUserIRFile (const juce::File& f,
                                                    double targetSampleRate,
                                                    float& outOriginalLengthSec)
{
    juce::AudioBuffer<float> result;
    outOriginalLengthSec = 0.0f;

    if (! f.existsAsFile()) return result;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) return result;

    const int srcLen = (int) reader->lengthInSamples;
    const int srcCh  = juce::jmin (2, (int) reader->numChannels);
    if (srcLen <= 0 || srcCh <= 0) return result;

    juce::AudioBuffer<float> src (juce::jmax (1, srcCh), srcLen);
    src.clear();
    reader->read (&src, 0, srcLen, 0, true, srcCh > 1);

    // Force stereo for downstream uniformity
    juce::AudioBuffer<float> stereo (2, srcLen);
    stereo.clear();
    if (srcCh == 1)
    {
        stereo.copyFrom (0, 0, src, 0, 0, srcLen);
        stereo.copyFrom (1, 0, src, 0, 0, srcLen);
    }
    else
    {
        stereo.copyFrom (0, 0, src, 0, 0, srcLen);
        stereo.copyFrom (1, 0, src, 1, 0, srcLen);
    }

    outOriginalLengthSec = (float) srcLen / (float) reader->sampleRate;

    if (std::abs (reader->sampleRate - targetSampleRate) < 0.5)
    {
        result = std::move (stereo);
    }
    else
    {
        resampleTo (stereo, result, reader->sampleRate, targetSampleRate);
    }

    return result;
}

// ============================================================================
// Helpers
// ============================================================================

void IRLibrary::applyExtraEnvelope (juce::AudioBuffer<float>& buf, float decayMul)
{
    // decayMul == 1 means "no change". <1 shortens, >1 elongates but cannot
    // create energy that isn't there – we apply only an additional decay so
    // user IRs always behave predictably.
    if (std::abs (decayMul - 1.0f) < 0.01f) return;

    const int N = buf.getNumSamples();
    if (N <= 0) return;

    // Build envelope: original implicit decay is what it is. We add a
    // multiplicative exp envelope that shifts the perceived T60.
    // If decayMul < 1, we shorten further.  If > 1, we apply a *milder* curve.
    const float endLevel = (decayMul >= 1.0f)
        ? std::pow (0.001f, 1.0f / decayMul)   // softer fade for longer feel
        : std::pow (0.001f, decayMul);          // more aggressive fade

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) N;
            const float env = std::pow (endLevel, t);
            d[i] *= env;
        }
    }
}

void IRLibrary::stretchInPlace (juce::AudioBuffer<float>& src, float ratio)
{
    if (std::abs (ratio - 1.0f) < 0.01f) return;
    const int inN = src.getNumSamples();
    if (inN <= 0) return;

    const int outN = juce::jlimit (16, (int) (22.0 * 96000.0),
                                   (int) ((float) inN * ratio));
    juce::AudioBuffer<float> dst (src.getNumChannels(), outN);
    dst.clear();

    for (int ch = 0; ch < src.getNumChannels(); ++ch)
    {
        const auto* in = src.getReadPointer (ch);
        auto* out = dst.getWritePointer (ch);
        for (int i = 0; i < outN; ++i)
        {
            const float srcIdx = (float) i / (float) outN * (float) (inN - 1);
            const int   i0 = (int) srcIdx;
            const int   i1 = juce::jmin (inN - 1, i0 + 1);
            const float f  = srcIdx - (float) i0;
            out[i] = in[i0] * (1.0f - f) + in[i1] * f;
        }
    }
    src = std::move (dst);
}

void IRLibrary::resampleTo (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out,
                            double srIn, double srOut)
{
    const int inN = in.getNumSamples();
    if (inN <= 0 || srIn <= 0 || srOut <= 0)
    {
        out.setSize (in.getNumChannels(), 0);
        return;
    }
    const double ratio = srOut / srIn;
    const int outN = juce::jmax (16, (int) ((double) inN * ratio));
    out.setSize (in.getNumChannels(), outN);
    out.clear();

    juce::LagrangeInterpolator interp;
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
    {
        interp.reset();
        interp.process (srIn / srOut,
                        in.getReadPointer (ch),
                        out.getWritePointer (ch),
                        outN);
    }
}

void IRLibrary::normaliseRMS (juce::AudioBuffer<float>& buf, float targetPeak)
{
    const int N = buf.getNumSamples();
    if (N <= 0) return;
    float peak = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buf.getMagnitude (ch, 0, N));
    if (peak < 1.0e-9f) return;
    buf.applyGain (targetPeak / peak);
}
