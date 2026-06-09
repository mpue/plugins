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
| **2** | APVTS-Parameter-Layout + Voice/Synthesiser-Gerüst (1 Osc → Amp-Env → Ausgang), polyphon spielbar | ✅ |
| **3** | Oszillator-Sektion vollständig (3 Osc, alle Wellenformen, PolyBLEP, Wavetable, Sync/FM/RingMod/Noise, Mixer) | ✅ |
| **4** | Multimode-Filter + Filter-Env + Drive | ✅ |
| **5** | LFOs + 3. Hüllkurve + Mod-Matrix | ✅ |
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

---

## Phase 2 — Status

- **APVTS-Parameter-Layout** (`Source/params/`): stabile String-IDs
  (`ParameterIDs.h`) und `createParameterLayout()`. Parameter dieser Phase:
  Master Gain (dB) + Amp-ADSR (Attack/Decay/Sustain/Release) mit musikalischer
  Skew und lesbaren Wertanzeigen.
- **Synthesiser-Gerüst**: `juce::Synthesiser` mit **8 Stimmen** und
  Voice-Stealing. `PikeSound` (spricht auf alle Noten/Kanäle an) und `PikeVoice`
  (1 Oszillator → Amp-Hüllkurve, velocity-sensitiv).
- **DSP**: `pike::Oscillator` (Phasen-Akkumulator, sauberer Sinus) — JUCE-frei,
  in Phase 3 um PolyBLEP/Wavetables erweiterbar. Hüllkurve via `juce::ADSR`.
- **Realtime-safe**: die Stimmen lesen ihre ADSR-Werte direkt aus den
  APVTS-Atomics (`getRawParameterValue`), keine Allokationen/Locks im Audio-Pfad.
- **State**: `getStateInformation`/`setStateInformation` serialisieren den
  kompletten Parameter-Baum als XML.
- Verifiziert: Build (VST3/AU/Standalone) + `auval` (Render-Tests über alle
  Sample-Rates und MIDI-Test bestanden, „AU VALIDATION SUCCEEDED").

> Klangbild bewusst noch schlicht (ein Sinus pro Stimme). Das wird ab Phase 3
> zur vollen Oszillator-Sektion ausgebaut. Die GUI zeigt weiterhin den
> Sektions-Rahmen; das Anbinden der Regler folgt in Phase 8.

---

## Phase 3 — Status

Vollständige Oszillator-Sektion, alles realtime-safe und JUCE-frei in `dsp/`
(damit unit-test-bar).

- **3 Oszillatoren**, je mit Wellenform-Auswahl **Sine / Triangle / Saw / Pulse
  / Wavetable**, plus Octave (-3..+3), Semitone (±12), Fine-Tune (±100 ct),
  Level und Pulsweite.
- **Anti-Aliasing**: `pike::Oscillator` mit **PolyBLEP** für Saw/Pulse und einem
  band-limitierten, integrierten Dreieck (auf Einheitspegel normiert).
- **Wavetable-Modus**: `pike::Wavetable` — morphbare Bank (Sine→Tri→Saw→Square)
  mit **mip-gemappten, band-limitierten Tabellen** pro Oktave (additive
  Synthese, kein Aliasing). Einmal im Processor gebaut und read-only über alle
  Stimmen/Oszillatoren geteilt.
- **Modulation/Routing**: **Hard-Sync** (Osc1 = Master → Osc2/3), **FM**
  (Osc3 → Osc1, regelbar), **Ring-Modulation** (Osc1 × Osc2), separater
  **Noise-Generator** (`pike::Noise`, xorshift).
- **Mixer**: Per-Osc-Level + Ring-Mod-Level + Noise-Level.
- **Parameter-Updates** auf Block-Rate (Tuning/Shape), DSP auf Sample-Rate;
  Stimmen lesen weiterhin direkt aus den APVTS-Atomics.
- **Verifiziert**:
  - Offline-DSP-Test (JUCE-frei kompiliert): alle Wellenformen finite,
    Pegel korrekt (Sine 1.00, Triangle 1.01/RMS 0.577, Saw 0.98, Pulse 1.00),
    Anti-Aliasing greift (Saw @ 8 kHz: Peak 0.67), Wavetable-Morph, FM+Sync und
    Noise stabil.
  - Build VST3/AU/Standalone + `auval` „AU VALIDATION SUCCEEDED" (31 Parameter,
    Render- und MIDI-Test bestanden).

> Default-Klang weiterhin Osc1 (Sine, Level 0.8), Osc2/3 stumm — bestehende
> Presets/Verhalten bleiben kompatibel. Multimode-Filter folgt in Phase 4.

---

## Phase 4 — Status

Multimode-Filter, Filter-Hüllkurve und Pre-Filter-Drive, alles per Stimme.

- **Filter** ([Filter.h](Source/dsp/Filter.h)): State-Variable-Filter in
  TPT-Topologie (Cytomic/Zavalishin) — **LP / BP / HP**, umschaltbar
  **12 / 24 dB/Okt**, Resonanz, Key-Tracking.
  - 24 dB cascadiert zwei Stufen, aber nur die erste trägt die Resonanz (zweite
    = Butterworth) → musikalischer Peak statt quadriertem Q.
  - Resonanz mit exponentiellem Q-Verlauf bis zur **echten Selbstoszillation**
    bei 100 %; der Resonanz-Integrator wird sanft sättigend begrenzt → saubere,
    gebündelte Selbstoszillation, bei normalen Pegeln linear.
- **Pre-Filter-Drive**: weiche `tanh`-Übersteuerung vor dem Filter (clean bei 0,
  charaktervoll beim Aufdrehen).
- **Filter-Hüllkurve**: eigene ADSR, bipolarer Env-Amount (±6 Oktaven),
  Key-Tracking addiert sich; Cutoff-Modulation auf Sample-Rate.
- **Signalkette pro Stimme**: 3 Osc → Mixer (+RingMod/Noise) → Drive → Filter →
  Amp-VCA (Amp-Env × Velocity).
- **Verifiziert**:
  - Offline-DSP-Test: LP/BP/HP filtern korrekt (LP12 @500 Hz: RMS-Ratio 0.14,
    LP24 0.12), Resonanz bei 100 % bleibt **begrenzt** (Peak 1.4–1.9 statt
    Explosion) und **oszilliert selbst** (Tail-RMS 0.18), alle finite.
  - Build VST3/AU/Standalone + `auval` „AU VALIDATION SUCCEEDED" (42 Parameter,
    Render- und MIDI-Test bestanden).

> Default: Filter LP, 12 dB, Cutoff 20 kHz (offen), Resonanz/Drive/Env 0 →
> klanglich transparent, bestehendes Verhalten bleibt kompatibel.
> Als Nächstes: LFOs, 3. Hüllkurve und Mod-Matrix (Phase 5).

---

## Phase 5 — Status

Komplettes Modulationssystem. **108 Parameter** insgesamt.

- **3. Hüllkurve (Aux)**: eigene ADSR, frei in der Mod-Matrix zuweisbar
  (zusätzlich zu Amp- und Filter-Env).
- **2 LFOs** ([Lfo.h](Source/dsp/Lfo.h)): Formen **Sine/Tri/Saw/Square/S&H**,
  **Tempo-Sync** (1/1…1/32, inkl. Triolen) **oder freie Rate** (Hz),
  **Key-Sync** (Retrigger), **Fade-In** und **Mono/Poly**.
  - Mono = gemeinsame Phase über alle Stimmen (vom Processor getrieben, inkl.
    deterministischem S&H per Hash); Poly = pro Stimme eigene Phase.
- **Mod-Matrix**: **16 Slots**, je *Source → Destination + Tiefe* (bipolar),
  **per Sample** ausgewertet.
  - Sources: Env1-3, LFO1/2, Velocity, ModWheel (CC1), Aftertouch, Key-Track.
  - Destinations: Osc1-3-Pitch, Pulsweite, Wavetable-Pos, FM-Amount,
    Filter-Cutoff/Resonance, Osc1-3-Level, LFO1/2-Rate.
- **Architektur**: Parameter werden pro Block in einfache Member gecacht (je ein
  Atomic-Read); der Sample-Loop nutzt nur diese Caches → keine Atomic-Loads im
  Audio-Pfad. MIDI-Sources (ModWheel/Aftertouch) und Tempo werden im Processor
  erfasst und über Atomics an die Stimmen gereicht.
- **Verifiziert**: Offline-LFO-Test (alle Formen ∈ [-1,1], Mono-S&H stimmt über
  Stimmen überein), Build VST3/AU/Standalone, `auval` „AU VALIDATION SUCCEEDED"
  (108 Parameter, Render- und MIDI-Test bestanden).

> Default: alle Matrix-Slots auf *None*, LFOs nicht geroutet → Klang identisch zu
> Phase 4. Als Nächstes: FX-Kette (Phase 6).
