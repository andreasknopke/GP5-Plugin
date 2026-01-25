# GP5 VST Editor

Ein VST3-Plugin zum Laden, Anzeigen und Abspielen von Guitar Pro 5 (.gp5) Dateien direkt in deiner DAW.

---

## Beschreibung

Der GP5 VST Editor ist ein JUCE-basiertes VST3-Instrument-Plugin, das Guitar Pro 5 Dateien einlesen und als interaktive Tabulatur darstellen kann. Das Plugin synchronisiert sich mit dem Transport deiner DAW und scrollt automatisch zur aktuellen Position im Song.

### Hauptfunktionen

- **GP5-Datei laden**: Öffne beliebige Guitar Pro 5 Dateien direkt im Plugin
- **Tabulatur-Darstellung**: Professionelle Darstellung der Noten als Gitarren-Tabulatur
- **DAW-Synchronisation**: Automatisches Mitscrollen mit der Wiedergabe
- **Multi-Track Unterstützung**: Wechsle zwischen verschiedenen Spuren (Gitarre, Bass, etc.)
- **Zoom-Funktion**: Passe die Darstellungsgrösse an deine Bedürfnisse an
- **MIDI-Output**: Das Plugin kann MIDI-Noten ausgeben

---

## Voraussetzungen

- **JUCE Framework** (Version 7.x oder höher)
- **CMake** (Version 3.22 oder höher)
- **C++17 kompatibler Compiler**
  - Windows: Visual Studio 2019 oder neuer
  - macOS: Xcode 12 oder neuer
  - Linux: GCC 9 oder neuer

---

## Build-Anleitung

### 1. Repository klonen

```bash
git clone https://github.com/yourusername/GP5_VST_Editor.git
cd GP5_VST_Editor
```

### 2. JUCE-Pfad konfigurieren

Bearbeite die `CMakeLists.txt` und passe den JUCE-Pfad an:

```cmake
set(JUCE_PATH "D:/GitHub/JUCE" CACHE PATH "Path to JUCE")
```

### 3. Build mit CMake

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Das fertige VST3-Plugin wird automatisch in den VST3-Ordner deines Systems kopiert.

---

## Verwendung

1. Lade das Plugin in deiner DAW als Instrument
2. Klicke auf "Load GP5 File" und wähle eine .gp5 Datei
3. Wähle die gewünschte Spur aus der Dropdown-Liste
4. Aktiviere "Auto-Scroll" für automatisches Mitscrollen bei der Wiedergabe
5. Nutze die +/- Buttons zum Zoomen der Tabulatur

---

## Projektstruktur

```
GP5_VST_Editor/
├── Source/
│   ├── PluginProcessor.cpp/h    # Audio-Processor (DAW-Integration)
│   ├── PluginEditor.cpp/h       # GUI des Plugins
│   ├── GP5Parser.cpp/h          # Guitar Pro 5 Datei-Parser
│   ├── TabModels.h              # Datenmodelle für Tabulaturen
│   ├── TabLayoutEngine.h        # Layout-Berechnung
│   ├── TabRenderer.h            # Tabulatur-Rendering
│   └── TabViewComponent.h       # Tabulatur-Ansicht Komponente
├── CMakeLists.txt               # CMake Build-Konfiguration
└── README.md
```

---

## Technische Details

### Unterstützte Guitar Pro Features

- Song-Informationen (Titel, Artist, Album, etc.)
- Mehrere Spuren mit individuellen Einstellungen
- Standard-Noten und Pausenzeichen
- Punktierte Noten
- Triolen und andere Tuplets
- Spieltechniken:
  - Hammer-On / Pull-Off
  - Slide
  - Bend
  - Vibrato
  - Palm Mute
  - Ghost Notes
  - Akzente
  - Harmonics
- Wiederholungen und Wiederholungsklammern
- Taktartänderungen
- Marker

### Parser

Der GP5-Parser basiert auf der Spezifikation von [PyGuitarPro](https://github.com/Perlence/PyGuitarPro) und wurde für C++/JUCE portiert.

---

## Lizenz

Dieses Projekt steht unter der MIT-Lizenz.

---

## Mitwirken

Beiträge sind willkommen! Bitte erstelle einen Pull Request oder öffne ein Issue für Vorschläge und Fehlermeldungen.

---

## Autor

AR-Sounds

---

## Danksagung

- [JUCE Framework](https://juce.com/) - Cross-Platform Audio Framework
- [PyGuitarPro](https://github.com/Perlence/PyGuitarPro) - Referenz für das GP5-Dateiformat
