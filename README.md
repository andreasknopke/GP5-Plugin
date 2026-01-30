# GP5 VST Editor

A VST3 plugin for loading, displaying, and playing Guitar Pro 5 (.gp5) files directly in your DAW.

---

## Description

The GP5 VST Editor is a JUCE-based VST3 instrument plugin that can read Guitar Pro 5 files and display them as interactive tablature. The plugin synchronizes with your DAW's transport for playback.

### Main Features

- **Load GP5 Files**: Open any Guitar Pro 5 file directly in the plugin
- **Tablature Display**: Professional display of notes as guitar tablature
- **DAW Synchronization**: Automatic scrolling during playback
- **Multi-Track Support**: Switch between different tracks (guitar, bass, etc.)
- **Zoom Function**: Adjust the display size to your needs
- **MIDI Output**: The plugin can output MIDI notes

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
git clone https://github.com/yourusername/GP5_VST_Editor.git
cd GP5_VST_Editor
```

### 2. Configure JUCE Path

Edit the `CMakeLists.txt` and adjust the JUCE path:

```cmake
set(JUCE_PATH "D:/GitHub/JUCE" CACHE PATH "Path to JUCE")
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
2. Click on "Load GP5 File" and select a .gp5 file
3. Select the desired track from the dropdown list
4. Enable "Auto-Scroll" for automatic scrolling during playback
5. Use the +/- buttons to zoom the tablature

---

## Project Structure

```
GP5_VST_Editor/
├── Source/
│   ├── PluginProcessor.cpp/h    # Audio processor (DAW integration)
│   ├── PluginEditor.cpp/h       # Plugin GUI
│   ├── GP5Parser.cpp/h          # Guitar Pro 5 file parser
│   ├── TabModels.h              # Data models for tablature
│   ├── TabLayoutEngine.h        # Layout calculation
│   ├── TabRenderer.h            # Tablature rendering
│   └── TabViewComponent.h       # Tablature view component
├── CMakeLists.txt               # CMake build configuration
└── README.md
```

---

## Technical Details

### Supported Guitar Pro Features

- Song information (title, artist, album, etc.)
- Multiple tracks with individual settings
- Standard notes and rests
- Dotted notes
- Triplets and other tuplets
- Playing techniques:
  - Hammer-On / Pull-Off
  - Slide
  - Bend
  - Vibrato
  - Palm Mute
  - Ghost Notes
  - Accents
  - Harmonics
- Repeats and repeat brackets
- Time signature changes
- Markers

### Parser

The GP5 parser is based on the specification from [PyGuitarPro](https://github.com/Perlence/PyGuitarPro) and was ported for C++/JUCE.

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
- [PyGuitarPro](https://github.com/Perlence/PyGuitarPro) - Reference for the GP5 file format
