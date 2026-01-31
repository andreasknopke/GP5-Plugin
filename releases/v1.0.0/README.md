# GP5 VST Editor

A VST3 plugin for loading, displaying, and playing Guitar Pro files (.gp3, .gp4, .gp5) directly in your DAW with realistic MIDI output for guitar samplers like HALion.

---

## Description

The GP5 VST Editor is a JUCE-based VST3 instrument plugin that reads Guitar Pro files and displays them as interactive tablature. The plugin synchronizes with your DAW's transport and generates expressive MIDI output with real-time pitch bend interpolation for authentic guitar playback.

### Main Features

- **Load GP3/GP4/GP5 Files**: Open Guitar Pro 3, 4, and 5 files directly in the plugin
- **Tablature Display**: Professional rendering of notes as guitar tablature with effects notation
- **DAW Synchronization**: Automatic scrolling and position sync during playback
- **Click-to-Seek**: Click anywhere on the tablature to jump to that position
- **Multi-Track Support**: Switch between different tracks with per-track MIDI channel, volume, pan, mute, and solo
- **Zoom Function**: Adjust the display size to your needs
- **MIDI Output**: Generates MIDI with expressive articulations:
  - Real-time pitch bend interpolation for bends
  - All bend types: Normal, Bend+Release, Pre-bend, Release
  - Legato mode (CC68) for hammer-ons/pull-offs
  - Portamento (CC65) for slides
  - Modulation (CC1) for vibrato
  - Velocity layering for dynamics (ghost notes, accents)

---

## Requirements

- **JUCE Framework** (Version 7.x or higher)
- **CMake** (Version 3.22 or higher)
- **C++17 compatible compiler**  
  - Windows: Visual Studio 2019 or newer  
  - macOS: Xcode 12 or newer  
  - Linux: GCC 9 or newer

---

## Build Instructions

### 1. Clone Repository

```bash
git clone https://github.com/andreasknopke/GP5-Plugin.git
cd GP5-Plugin
```

### 2. Configure JUCE Path

Edit the `CMakeLists.txt` and adjust the JUCE path:

```cmake
set(JUCE_PATH "C:/GitHub/JUCE" CACHE PATH "Path to JUCE")
```

### 3. Build with CMake

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The completed VST3 plugin will be automatically copied to your system's VST3 folder.

---

## Usage

1. Load the plugin in your DAW as an instrument
2. Route the MIDI output to a guitar sampler (HALion, Kontakt, etc.)
3. Click on "Load GP5 File" and select a .gp3, .gp4, or .gp5 file
4. Select the desired track from the dropdown list
5. Configure per-track settings (MIDI channel, volume, pan, mute/solo)
6. Enable "Auto-Scroll" for automatic scrolling during playback
7. Use the +/- buttons to zoom the tablature
8. Click on the tablature to seek to that position

### Sampler Configuration

For best results with pitch bends:
- Set your sampler's **Pitch Bend Range to ±2 semitones** (standard MIDI)
- Enable legato/mono mode for realistic hammer-ons
- Configure portamento for slide effects

---

## Project Structure

```
GP5_VST_Editor/
├── Source/
│   ├── PluginProcessor.cpp/h    # Audio processor with MIDI generation
│   ├── PluginEditor.cpp/h       # Plugin GUI
│   ├── GP5Parser.cpp/h          # Guitar Pro 3/4/5 file parser
│   ├── TabModels.h              # Data models for tablature
│   ├── TabLayoutEngine.h        # Layout calculation
│   ├── TabRenderer.h            # Tablature rendering with effects
│   └── TabViewComponent.h       # Tablature view with click-to-seek
├── CMakeLists.txt               # CMake build configuration
└── README.md
```

---

## Technical Details

### Supported Guitar Pro Formats

- **Guitar Pro 3** (.gp3)
- **Guitar Pro 4** (.gp4)
- **Guitar Pro 5** (.gp5)

### Supported Guitar Pro Features

- Song information (title, artist, album, tempo, etc.)
- Multiple tracks with individual settings
- Standard notes and rests
- Dotted notes
- Triplets and other tuplets
- **Pitch Bends with real-time interpolation:**
  - Normal Bend (pitch up during note)
  - Bend + Release (pitch up, then back down)
  - Pre-Bend (start at target pitch)
  - Release / Pre-Bend Release (pitch down during note)
  - Quarter-tone, half-step, full-step, 1.5-step bends
- Playing techniques:
  - Hammer-On / Pull-Off (with CC68 legato)
  - Slide (with CC65 portamento)
  - Vibrato (with CC1 modulation)
  - Palm Mute
  - Ghost Notes (reduced velocity)
  - Accents / Heavy Accents (increased velocity)
  - Natural and Artificial Harmonics
  - Dead Notes
- Repeats and repeat brackets
- Time signature changes
- Tempo changes
- Markers
- Chord diagrams

### MIDI Implementation

| Feature | MIDI Message |
|---------|-------------|
| Notes | Note On/Off with velocity |
| Bends | Pitch Wheel (real-time interpolation) |
| Vibrato | CC1 (Modulation) |
| Legato | CC68 (Legato Footswitch) |
| Slides | CC65 (Portamento) + CC5 (Time) |
| Pan | CC10 |
| Volume | Velocity scaling per track |

### Parser

The GP5 parser is based on the specification from [PyGuitarPro](https://github.com/Perlence/PyGuitarPro) and was implemented in C++/JUCE with extensions for GP3 and GP4 formats.

---

## License

This project is licensed under the MIT License.

---

## Contributing

Contributions are welcome! Please create a pull request or open an issue for suggestions and bug reports.

---

## Author

AR-Sounds

---

## Acknowledgements

- [JUCE Framework](https://juce.com/) - Cross-Platform Audio Framework
- [PyGuitarPro](https://github.com/Perlence/PyGuitarPro) - Reference for the GP file format specification
