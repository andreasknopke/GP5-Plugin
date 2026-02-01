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

## Bekannte Probleme

### Parser-Problem (Stand 25.01.2026)
Die Bundnummern werden falsch gelesen (0, 1 statt 14, 18, 19 etc.).
Muss noch debuggt werden.

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
