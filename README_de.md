# Funky Moose Rootflow Synth

![Funky Moose Rootflow Synth Banner](docs/rootflowsynthbanner.png)

![JUCE](https://img.shields.io/badge/Built%20with-JUCE-3A4E6A?style=for-the-badge)
![C++](https://img.shields.io/badge/C%2B%2B-17-3A4E6A?style=for-the-badge)
![VST3](https://img.shields.io/badge/VST3-supported-3A4E6A?style=for-the-badge)
![AudioUnit](https://img.shields.io/badge/AudioUnit-supported-3A4E6A?style=for-the-badge)
![Standalone](https://img.shields.io/badge/Standalone-supported-3A4E6A?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Evolving-5FD1C7?style=for-the-badge)
![Version](https://img.shields.io/badge/Version-1.3.0-blue?style=for-the-badge)

**Funky Moose Rootflow Synth** ist ein organisch reagierendes Software-Instrument, gebaut mit **JUCE** und **C++17**.

Es verbindet Ambient-Synthese, bio-inspirierte Sequenzen, lebendige Modulation und eine atmende Benutzeroberflaeche zu einem Instrument, das sich eher gewachsen als zusammengeschraubt anfuehlt. Rootflow soll nicht steril oder technisch wirken. Es soll atmen, pulsieren, driften und sich entwickeln.

## Features

- Organische Synth-Engine fuer Texturen, Drones, Pulse und atmosphaerische Sounds
- Bio-Sequencer fuer lebendige rhythmische Bewegung und mutationsgetriebene Pattern
- Reaktives Center Panel mit knotiger, organismischer Bewegung und visueller Rueckmeldung
- Root-, Pulse- und Ambient-Felder fuer tonale Basis, Bewegung und Raumanteil
- Patch-Workflow mit Save-, Delete- und Mutate-Aktionen direkt im Hauptfenster
- Spielbare Keyboard-Oberflaeche und direkte Performance-Kontrolle
- Verfuegbar als **Audio Unit**, **VST3** und **Standalone** auf macOS

## Interface

### Bio-Sequencer

Steuert Step-Aktivitaet, rhythmische Bewegung und die innere Lebendigkeit eines Patches.

### Center Panel

Das lebendige visuelle Feld. Es macht Bewegung, Modulation und Interaktion in Echtzeit sichtbar.

### Root Field / Pulse Field / Ambient Field

Diese Bereiche formen den Klangkoerper des Instruments:

- **Root Field**: Tiefe, Boden, Anker und tonale Erdung
- **Pulse Field**: Rate, Atem, Wachstum und Bewegung
- **Ambient Field**: Luft, Boden und Raumanteil

### Version 1.3.0 (Aktuell)
- **AU-Build wieder aktiv**: Audio Unit wird auf macOS wieder standardmaessig in den CMake-Presets gebaut.
- **Mac-Kompatibilitaet**: Release-Builds laufen als Universal Binary auf `arm64` und `x86_64` mit Deployment-Target macOS 12 Monterey.
- **Audio-Refactor stabilisiert**: Keyboard-Injection, Bio-Feedback, Master-Mix, Mono-Maker und oversampling-sicheres Beat-Timing sind wieder sauber verdrahtet.
- **Besser auf kleinen Displays**: Das Editor-Fenster waehlt auf kleineren Screens wie einem 13-Zoll-MacBook eine entspanntere Startgroesse.

## Screenshot

![Funky Moose Rootflow Synth Screenshot](docs/rootflow-screenshot-main.png)

## Installation

### macOS

- **Audio Unit**: das `.component`-Bundle nach `~/Library/Audio/Plug-Ins/Components` oder `/Library/Audio/Plug-Ins/Components` kopieren
- **VST3**: das `.vst3`-Bundle nach `~/Library/Audio/Plug-Ins/VST3` oder `/Library/Audio/Plug-Ins/VST3` kopieren
- **Standalone**: das `.app`-Bundle in `Applications` verschieben

### Hinweise

- Unsigned Builds muessen unter Umstaenden einmal ueber Finder geoeffnet oder in `System Settings > Privacy & Security` erlaubt werden
- Wenn du einen aelteren `RootFlow`-Build ersetzt, sollte die DAW nach der Installation von `Funky Moose Rootflow Synth` neu gescannt werden

## Build from Source

### Voraussetzungen

- CMake 3.22 oder neuer
- JUCE 8.0.10
- C++17 kompatibler Compiler
- Xcode oder Xcode Command Line Tools auf macOS

### Schnellstart

```bash
git clone https://github.com/blubass/funky-moose-rootflow-synth.git
cd funky-moose-rootflow-synth
cmake --preset default
cmake --build --preset default
```

Das mitgelieferte Preset erwartet JUCE hier:

```text
$HOME/Developer/JUCE/install/lib/cmake/JUCE-8.0.10
```

Falls JUCE bei dir woanders liegt, gib beim Konfigurieren `-DJUCE_DIR=/path/to/JUCE/lib/cmake/JUCE-8.0.10` an.

## Roadmap

- tiefere Modulationsverschaltung
- reichere Voice-Architektur
- erweitertes Sequencer-Verhalten
- besseres Patch- und Preset-Handling
- verfeinerte Visuals und Release-Pakete

## Changelog

### [1.3.0] - 2026-04-01
- **Audio Unit Build**: AU in den Standard-Presets wieder aktiviert und lokal per `auval` validiert.
- **macOS Packaging**: Universal-Build fuer Apple Silicon und Intel mit Monterey als Mindestziel.
- **Audio-Pfad-Fixes**: Keyboard-MIDI, Modulations-Feedback, Master-Sektion und Oversampling-Timing nach dem Refactor repariert.
- **UI-Skalierung**: Sicherere Startgroesse fuer kleinere MacBook-Displays.

## Autor

Uwe Arthur Felchle

Musiker, Komponist, Produzent und Entwickler

https://uwefelchle.at

## Lizenz

Dieses Projekt steht unter der MIT-Lizenz. Siehe [LICENSE](LICENSE).
