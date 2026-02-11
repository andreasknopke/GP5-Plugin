# Workspace Notizen für Copilot

Diese Datei enthält wichtige Informationen für die Arbeit in diesem Workspace.
**LIES DIESE DATEI IMMER ZUERST!**

## Python

Python 3.13 ist über Microsoft Store installiert. Verwende IMMER:

```powershell
& "C:\Users\Andre.AUDIO-WS1\AppData\Local\Microsoft\WindowsApps\python3.13.exe"
```

Beispiel:
```powershell
& "C:\Users\Andre.AUDIO-WS1\AppData\Local\Microsoft\WindowsApps\python3.13.exe" script.py
& "C:\Users\Andre.AUDIO-WS1\AppData\Local\Microsoft\WindowsApps\python3.13.exe" -m pip install paketname
```

Zusätzlich ist Python auch über Miniconda verfügbar:
```
C:\Users\Andre.AUDIO-WS1\miniconda3\python.exe
```

PyGuitarPro ist installiert (Paketname: `PyGuitarPro`, import als `guitarpro`).

## CMake / Build

CMake ist über Visual Studio 2026 installiert. Der Pfad ist:

```
C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

### Build-Befehle für dieses Projekt:

**WICHTIG: Der Ninja-Build im `build` Ordner hat Include-Probleme! NUR Visual Studio Build verwenden!**

**Release Build (mit Visual Studio - FUNKTIONIERT):**
```powershell
cd D:\GitHub\NewProject\build\build_release\release_build\release_build
Start-Process -FilePath "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -ArgumentList "--build",".","--config","Release" -NoNewWindow -Wait
```

**Debug Build (mit Visual Studio):**
```powershell
cd D:\GitHub\NewProject\build\build_release\release_build\release_build
Start-Process -FilePath "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -ArgumentList "--build",".","--config","Debug" -NoNewWindow -Wait
```

### VST3 Plugin Ausgabe:
- Release: `D:\GitHub\NewProject\build\build_release\release_build\release_build\GP5_VST_Editor_artefacts\Release\VST3\GP5_VST_Editor.vst3`
- Debug: `D:\GitHub\NewProject\build\build_release\release_build\release_build\GP5_VST_Editor_artefacts\Debug\VST3\GP5_VST_Editor.vst3`
- Installiert nach: `C:\Program Files\Common Files\VST3\GP5_VST_Editor.vst3`

### CMake Reconfigure (nötig nach Änderungen an CMakeLists.txt oder neuen Dateien):
```powershell
cd D:\GitHub\NewProject\build\build_release\release_build\release_build
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S D:\GitHub\NewProject -B . -G "Visual Studio 18 2026"
```

### Nach dem Build: onnxruntime.dll kopieren
Die DLL wird automatisch per POST_BUILD-Regel neben das Plugin kopiert.
Für den installierten VST3-Ordner manuell:
```powershell
Copy-Item "D:\GitHub\NewProject\ThirdParty\onnxruntime\lib\onnxruntime.dll" "C:\Program Files\Common Files\VST3\GP5_VST_Editor.vst3\Contents\x86_64-win\" -Force
```

## Inno Setup (Installer)

Inno Setup 6.7.0 ist über winget installiert. Der Compiler-Pfad ist:

```
C:\Users\Andre.AUDIO-WS1\AppData\Local\Programs\Inno Setup 6\ISCC.exe
```

### Installer bauen:
```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" "D:\GitHub\NewProject\installer\GP5_VST_Editor.iss"
```

### Installer-Script:
- Pfad: `D:\GitHub\NewProject\installer\GP5_VST_Editor.iss`
- Ausgabe: `D:\GitHub\NewProject\releases\GP5_VST_Editor_<version>_Setup.exe`
- Version anpassen: `#define MyAppVersion` und `VersionInfoVersion` in der .iss Datei

## Git

Git funktioniert normal mit `git add`, `git commit`, `git push`.

## GitHub CLI

GitHub CLI (`gh`) ist installiert, aber nicht authentifiziert. Für Releases:
1. Entweder `gh auth login` ausführen
2. Oder manuell auf GitHub unter Releases erstellen

## Projektstruktur

- Source-Code: `D:\GitHub\NewProject\Source\`
- JUCE Framework: `D:\GitHub\JUCE\`
- Test GP5 Datei: `D:\GitHub\NewProject\test_partial.gp5`

### ThirdParty Dependencies
- `ThirdParty/onnxruntime/` — ONNX Runtime v1.17.1 (Windows x64, prebuilt)
  - `include/` — Header (onnxruntime_cxx_api.h etc.)
  - `lib/` — onnxruntime.lib, onnxruntime.dll
- `ThirdParty/RTNeural/` — Header-only C++ Neural Network Inference (mit Conv2D, nlohmann::json)
  - Include-Pfad: `ThirdParty/RTNeural`
  - Compile-Definition: `RTNEURAL_USE_STL=1`
- `ThirdParty/ModelData/` — Basic Pitch Modellgewichte (als BinaryData eingebunden)
  - `features_model.onnx` (CQT + Harmonic Stacking, ONNX)
  - `cnn_contour_model.json`, `cnn_note_model.json`, `cnn_onset_1_model.json`, `cnn_onset_2_model.json` (RTNeural JSON)

### Source Verzeichnisse
- `Source/` — Haupt-Plugin-Code (PluginProcessor, PluginEditor, Parser, Writer, TabModels etc.)
- `Source/BasicPitch/` — Polyphone Audio-Transkription (portiert von NeuralNote)
  - `Features.h/.cpp` — ONNX Runtime Session für CQT + Harmonic Stacking
  - `BasicPitchCNN.h/.cpp` — RTNeural CNN (4 Sub-Modelle: contour, note, onset_input, onset_output)
  - `BasicPitch.h/.cpp` — Orchestrator: Features → CNN → Notes
  - `Notes.h/.cpp` — Posteriorgram → MIDI Note Events (onset detection, segmentation, pitch bends)
  - `Resampler.h/.cpp` — JUCE LagrangeInterpolator Resampler mit Lowpass-Filter
  - `NoteUtils.h` — Hilfsfunktionen (midiNoteToStr, Scales etc.)
  - `BasicPitchConstants.h` — Konstanten (SAMPLE_RATE=22050, FFT_HOP=256, NUM_FREQ_IN=264, NUM_FREQ_OUT=88)

## Audio-to-MIDI Architektur (Stand 07.02.2026)

### InputMode Auto-Erkennung
Das Plugin erkennt den Modus automatisch in processBlock:
- **Player** (default): Kein Sidechain, kein MIDI-Input → nur Wiedergabe
- **MIDI**: Kein Sidechain aktiv, aber MIDI NoteOn empfangen → MIDI-Aufnahme
- **Audio**: Sidechain Bus 1 (kAux) ist enabled → polyphone Audio-Transkription

### Sidechain-Konfiguration (VST3)
- Bus 0 = kMain (Dummy, disabled) — nötig damit Bus 1 als kAux registriert wird
- Bus 1 = kAux (Sidechain) — Cubase zeigt NUR kAux-Busse als Sidechain!
- In Cubase: Plugin als Instrument laden → Side Chain in Plugin-Fenster aktivieren → Audio-Track als Sidechain-Quelle wählen

### Audio-Transkription Flow
1. REC+Play in Cubase → `shouldRecordAudio` wird true
2. Sidechain-Audio wird resampled (Host-Rate → 22050 Hz) und akkumuliert
3. Stop in Cubase → `wasRecordingAudio && !shouldRecordAudio` → BasicPitch startet im Hintergrund-Thread
4. BasicPitch Pipeline: Features (ONNX: CQT) → BasicPitchCNN (RTNeural: frame-by-frame CNN) → Notes (pure C++: posteriorgrams → events)
5. `hasResults()` → `insertTranscribedNotesIntoTab()` konvertiert Events in RecordedNote structs
6. Noten erscheinen im Tab (gleiche Darstellung wie MIDI-Aufnahme)

### Wichtige Klassen
- `AudioTranscriber` (Source/AudioTranscriber.h/.cpp) — Wrapper um BasicPitch, Thread, Resampler, Buffer-Akkumulation
- `AudioToMidiProcessor` (Source/AudioToMidiProcessor.h/.cpp) — Einfache monophone YIN Pitch Detection (Live-Feedback)
- `BasicPitch` (Source/BasicPitch/BasicPitch.h/.cpp) — NeuralNote Orchestrator

### NICHT Real-Time
BasicPitch ist NICHT echtzeitfähig:
- CQT braucht >1s Chunks
- CNN hat ~120ms Latenz
- Note-Erstellung ist non-kausal (rückwärts-Verarbeitung)
→ Audio wird akkumuliert, bei Stop wird alles auf einmal transkribiert

## Bekannte Probleme

### Parser-Problem (Stand 25.01.2026)
Die Bundnummern werden falsch gelesen (0, 1 statt 14, 18, 19 etc.).
Muss noch debuggt werden.

### COPY_PLUGIN_AFTER_BUILD Permission
`COPY_PLUGIN_AFTER_BUILD TRUE` kopiert nach `C:\Program Files\Common Files\VST3\`.
Funktioniert nur wenn VS/Build als Administrator läuft. Wenn "Permission denied"-Fehler:
- Build war trotzdem erfolgreich! Die .vst3 liegt im Artefact-Ordner.
- Manuell kopieren oder Terminal als Admin starten.

## CMakeLists.txt Struktur (Stand 07.02.2026)

Die CMakeLists.txt hat folgende Reihenfolge (wichtig!):
1. `juce_add_plugin(GP5_VST_Editor ...)` — Plugin-Target
2. `add_library(RTNeural INTERFACE)` — Header-only RTNeural
3. `add_library(onnxruntime SHARED IMPORTED)` — ONNX Runtime (lib + dll)
4. `juce_add_binary_data(bin_data ...)` — Model-Daten als BinaryData (**muss vor BasicPitchCNN!**)
5. `add_library(BasicPitchCNN STATIC ...)` — Separates Target mit /O2 Optimierung
6. `target_sources(GP5_VST_Editor ...)` — Alle Source-Dateien
7. `target_include_directories(GP5_VST_Editor ...)` — onnxruntime/include, RTNeural, Source/BasicPitch
8. `target_compile_definitions` — inkl. RTNEURAL_USE_STL=1
9. `target_link_libraries` — JUCE Module + juce_dsp + onnxruntime + BasicPitchCNN + bin_data
10. `add_custom_command(POST_BUILD)` — Kopiert onnxruntime.dll neben Plugin

**Neue Dateien hinzufügen:** In `target_sources()` eintragen und CMake reconfigurieren.

## GP5 Writer (Stand 01.02.2026)

Der GP5Writer wurde nach der PyGuitarPro-Referenzimplementierung neu geschrieben.

**Wichtige Erkenntnisse:**
- PageSetup hat **10 Strings** (nicht 11!) - copyright ist als 2 Strings geschrieben
- Für GP5 v5.00: Kein RSE Master Effect schreiben
- Für GP5 v5.00: Kein `hideTempo` Bool schreiben
- Measure Header: Placeholder (1 Byte) **vor** jedem Header außer dem ersten
- Beat: `flags2` (short) am Ende jedes Beats
- Note: `flags2` (byte) nach dem Fret, vor den Note Effects

**Test-Skripte:**
- `test_gp5_structure.py` - Minimale GP5 Datei erstellen
- `test_gp5_with_notes.py` - GP5 mit echten Noten erstellen
- `debug_gp5_bytes.py` - Byte-für-Byte Analyse von GP5 Dateien
