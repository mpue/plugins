# Pike

**Pike** ist ein 8-stimmig polyphoner Hybrid-Synthesizer (Virtual-Analog +
Wavetable) für macOS, gebaut mit [JUCE](https://juce.com). Funktionell orientiert
er sich am Novation Peak: drei digitale Oszillatoren mit Wavetables, ein
multimode-resonantes Filter, drei Hüllkurven, zwei LFOs, eine Mod-Matrix, eine
Effektsektion und ein Arpeggiator. Pike ist das Flaggschiff der Plugin-Suite.

- **Formate:** VST3, AU (AudioUnit), Standalone
- **Plattform:** macOS (Universal: arm64 + x86_64)
- **Sprache/Framework:** C++20, JUCE (inkl. `juce::dsp`)
- **Build:** Projucer (`Pike.jucer`) → Xcode (`Builds/MacOSX`)

---

## Build

Das Projekt wird mit dem Projucer verwaltet. Nach Änderungen an `Pike.jucer`:

```bash
# Xcode-Projekt aus dem .jucer regenerieren
../../JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer.app/Contents/MacOS/Projucer \
    --resave Pike.jucer

# Bauen (Release, Universal)
xcodebuild -project Builds/MacOSX/Pike.xcodeproj -scheme "Pike - All" \
    -configuration Release build
```

Alternativ über das Repo-weite Skript `../build_all.sh --plugins Pike`.

> Die JUCE-Quellen werden über `../../JUCE/modules` referenziert (Git-Working-Copy
> auf Repo-Ebene), nicht über FetchContent.

---

## Projektstruktur

```
Pike/
├── Pike.jucer                  # Projucer-Projekt (Single Source of Truth)
├── README.md
├── Images/                     # GUI-Assets (Knob-Strip etc.)
├── JuceLibraryCode/            # generiert vom Projucer
├── Builds/MacOSX/              # generiertes Xcode-Projekt
└── Source/
    ├── PluginProcessor.{h,cpp} # AudioProcessor, APVTS, Synthesiser-Host
    ├── PluginEditor.{h,cpp}    # Top-Level-Editor, Sektions-Layout
    ├── ElegantDarkLookAndFeel.h# Custom LookAndFeel (dunkles Theme, Knob-Strip)
    │
    │   # ── ab Phase 2 schrittweise hinzukommend ──
    ├── dsp/                     # reine DSP-Klassen, ohne GUI-Abhängigkeiten
    │   ├── Oscillator.h         # PolyBLEP-Osc + Wavetable
    │   ├── Wavetable.h          # gemippte Wavetables
    │   ├── Filter.h             # Multimode SVF / Ladder
    │   ├── Envelope.h           # ADSR
    │   ├── Lfo.h
    │   ├── ModMatrix.h
    │   └── fx/                  # Distortion, Chorus, Delay, Reverb
    ├── synth/
    │   ├── PikeVoice.{h,cpp}    # eine Stimme (3 Osc → Mixer → Filter → Amp)
    │   ├── PikeSound.h          # SynthesiserSound
    │   └── VoiceManager…        # Poly/Mono/Legato/Unison/Glide, Voice-Stealing
    ├── params/
    │   └── ParameterLayout.{h,cpp} # APVTS-Parameter-IDs + createLayout()
    ├── gui/                     # Sektions-Components (Osc, Filter, Env, …)
    └── presets/
        └── PresetManager.{h,cpp}
```

**Architektur-Prinzipien**

- Strikte Trennung **DSP ↔ Processor ↔ Editor**. DSP-Klassen sind frei von
  JUCE-GUI-Abhängigkeiten und unit-test-bar.
- Parameter ausschließlich über **APVTS**; State/Presets als **ValueTree (XML)**.
- Realtime-safer Audio-Pfad: keine Allokationen, Locks oder Exceptions in
  `processBlock`; sauberes `prepareToPlay`/`reset`.
- RAII, const-correctness, keine rohen `new`/`delete`.

---

## Architektur pro Stimme (Zielbild)

```
        ┌─ Osc1 (Sin/Tri/Saw/Pulse/Wavetable) ─┐
MIDI ─▶ ├─ Osc2 ────────────────────────────────┤─▶ Mixer ─▶ Filter ─▶ Amp ─▶ out
        ├─ Osc3 ────────────────────────────────┤   (+Drive) (LP/BP/HP,    (Amp-Env)
        └─ Noise ───────────────────────────────┘             12/24 dB)
          Sync / FM (3→1) / RingMod (1×2)
```

Modulation: 3× ADSR (Amp, Filter, Aux), 2× LFO (Sync/frei, Key-Sync, Fade-In),
Mod-Matrix mit ~16 Slots. Global: FX-Kette (Distortion → Chorus → Delay → Reverb)
und Arpeggiator.

---

## Phasenplan

Es wird in Phasen gebaut; nach jeder Phase wird committet und auf Feedback
gewartet.

| Phase | Inhalt | Status |
|------:|--------|:------:|
| **1** | Projucer-Projekt, leeres Synth-Plugin (VST3/AU/Standalone), erzeugt Stille, `ElegantDarkLookAndFeel` eingebunden, gebrandeter UI-Rahmen, Build verifiziert | ✅ |
| **2** | APVTS-Parameter-Layout + Voice/Synthesiser-Gerüst (1 Osc → Amp-Env → Ausgang), polyphon spielbar | ☐ |
| **3** | Oszillator-Sektion vollständig (3 Osc, alle Wellenformen, PolyBLEP, Wavetable, Sync/FM/RingMod/Noise, Mixer) | ☐ |
| **4** | Multimode-Filter + Filter-Env + Drive | ☐ |
| **5** | LFOs + 3. Hüllkurve + Mod-Matrix | ☐ |
| **6** | FX-Kette (Distortion, Chorus, Delay, Reverb) | ☐ |
| **7** | Arpeggiator + Voice-Modi (Mono/Legato/Unison/Glide) | ☐ |
| **8** | GUI ausbauen, alle Parameter anbinden | ☐ |
| **9** | Preset-System + Factory-Presets | ☐ |

---

## Phase 1 — Status

- Plugin als **Instrument/Synth** konfiguriert (`pluginIsSynth`,
  `pluginWantsMidiIn`, VST3-Kategorie *Instrument*, AU-Typ *Music Device*).
- C++20.
- `ElegantDarkLookAndFeel` global im Editor gesetzt.
- Gebrandeter, resizable Editor-Rahmen mit Header und Sektions-Platzhaltern,
  die das spätere Layout vorzeichnen.
- `processBlock` erzeugt Stille (MIDI wird bereits angenommen, aber noch nicht
  klingend verarbeitet).
