# GP5 VST Editor

A VST3 plugin for loading, displaying, playing, and editing Guitar Pro files (.gp3, .gp4, .gp5, .gp, .gpx) directly in your DAW with realistic MIDI output for guitar samplers. The plugin also offers **Audio-to-Tab transcription** (powered by BasicPitch polyphonic pitch detection), MIDI to GP5 transcription, and Guitar Pro to MIDI conversion features.

---

## Description

The GP5 VST Editor is a JUCE-based VST3 instrument plugin that reads Guitar Pro files and displays them as interactive tablature. It can also **transcribe live audio input** (guitar via sidechain) into tablature using a neural network (BasicPitch). The plugin synchronizes with your DAW's transport and generates expressive MIDI output with real-time pitch bend interpolation for authentic guitar playback.

### Main Features

- **Load GP3/GP4/GP5/GP7/GP8 Files**: Open Guitar Pro 3, 4, 5, 7, and 8 files directly in the plugin
- **Chord Name Display**: Shows chord names above the tablature (Am, E5+/G#, FM7, etc.)
- **Tablature Display**: Professional rendering of notes as guitar tablature with effects notation
- **Rhythm Notation**: Proper beam grouping by metric position (4 eighths per half-measure in 4/4)
- **Integrated Note Editing**: Click on notes to change string/fret positions with persistent changes
- **Fret Position Preference**: Choose Low (0-4), Mid (5-8), or High (9-12) fret region for transcription
- **DAW Synchronization**: Automatic scrolling and position sync during playback
- **Unified Recording Workflow**: Seamless transition between recording and player modes
- **Click-to-Seek**: Click anywhere on the tablature to jump to that position
- **Multi-Track Support**: Switch between different tracks with per-track MIDI channel, volume, pan, mute, and solo
- **Zoom Function**: Adjust the display size to your needs
- **Enhanced Export**: Export recordings with customizable metadata (Title, Artist, Track names)
- **Recording & Editing Mode**:
  - **Audio-to-Tab Transcription** (BasicPitch neural network):
    - Record audio via DAW sidechain input (guitar, bass, etc.)
    - Polyphonic pitch detection with automatic tablature generation
    - Intelligent bend filtering (ignores intonation artifacts, only detects real bends ≥ 2 semitones)
    - Vibrato detection from audio
  - **Live MIDI Input**: Real-time note display on tablature
  - **Smart Chord Recognition**: Automatically detects and displays chord names (C, Am, G7, etc.)
  - **Chord Shape Library**: Uses predefined guitar chord shapes (open, barre, power chords)
  - **Note Position Editing**: 
    - Click on any recorded note to select alternative string/fret combinations
    - Changes persist when plugin UI is closed/reopened
    - Edited positions are saved in GP5 exports
    - Multiple consecutive edits to the same note work correctly
  - **Rest Editing**:
    - Delete rests to merge with adjacent notes
    - Change rest durations with right-click
    - Edits persist in playback, GP5 export, and across UI refreshes
  - **Fret Position Preferences**:
    - **Low (0-4)**: Prefer open position and first frets
    - **Mid (5-8)**: Prefer middle fret positions (default)
    - **High (9-12)**: Prefer higher fret positions
    - **Open String Preference**: Strongly prefers open strings when available
  - **Position Lookahead**: Adjustable lookahead (1-8 notes) for optimal fretboard positioning
  - **Advanced Quantization**:
    - **Legato Quantization**: Automatically detects hammer-ons/pull-offs for smoother playback
    - **Intelligent Bar Quantization**: Ensures notes fit perfectly within measure boundaries
  - **Cost-based Position Optimization**: Minimizes hand movement between positions
  - **Seamless Workflow**: Automatically switches to player mode after recording for immediate review
  - **Clear with Confirmation**: Clear button with Yes/No dialog, fully resets all recording state
- **GP5 Export**: Save your recordings as Guitar Pro 5 (.gp5) files with:
  - **Export Panel**: Edit song title, artist, and track names before exporting
  - Compatibility with Guitar Pro software (all versions)
  - Soundslice and other GP5-compatible applications
- **MIDI Output**: Generates MIDI with expressive articulations:
  - Real-time pitch bend interpolation for bends
  - All bend types: Normal, Bend+Release, Pre-bend, Release
  - Legato mode (CC68) for hammer-ons/pull-offs
  - Portamento (CC65) for slides
  - Modulation (CC1) for vibrato
  - Velocity layering for dynamics (ghost notes, accents)
  - Recording playback uses edited track data (rest deletions, duration changes reflected in MIDI output)
  - Clean MIDI-only output (sidechain audio is not passed through)

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

### Player Mode (Guitar Pro Files)

1. Load the plugin in your DAW as an instrument
2. Route the MIDI output to a guitar sampler (HALion, Kontakt, etc.)
3. Click on "Load GuitarPro File" and select a .gp3, .gp4, .gp5, or .gp file
4. Select the desired track from the dropdown list
5. Configure per-track settings (MIDI channel, volume, pan, mute/solo)
6. Enable "Auto-Scroll" for automatic scrolling during playback
7. Use the +/- buttons to zoom the tablature
8. Click on the tablature to seek to that position

### Editor Mode (Audio-to-Tab Recording)

1. Load the plugin as an instrument and route MIDI output to a guitar sampler
2. Set up a **sidechain input** from your guitar/audio track to the plugin
3. Arm the track and press Record + Play in your DAW
4. Play your guitar — the plugin captures audio via sidechain
5. When you stop recording, **BasicPitch** (neural network) automatically transcribes the audio to tablature
6. Review the transcription, edit note positions by clicking on them
7. Delete unwanted rests (right-click or select and delete)
8. Export to GP5 via the Export panel with custom metadata

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
│   ├── PluginProcessor.cpp/h    # Audio processor with MIDI generation & recording
│   ├── PluginEditor.cpp/h       # Plugin GUI
│   ├── GP5Parser.cpp/h          # Guitar Pro 3/4/5 file parser
│   ├── GP5Writer.cpp/h          # Guitar Pro 5 file writer (export)
│   ├── GP7Parser.h              # Guitar Pro 7/8 (.gp) file parser
│   ├── AudioTranscriber.cpp/h   # Audio-to-MIDI transcription (BasicPitch wrapper)
│   ├── AudioToMidiProcessor.h   # Real-time YIN monophonic pitch detection
│   ├── FretPositionCalculator.h # String/fret optimization with cost model
│   ├── ChordMatcher.h           # Chord recognition and shape library
│   ├── NoteEditComponent.h      # Note and group editing popups
│   ├── ExportPanelComponent.h   # GP5 export with metadata editing
│   ├── TabModels.h              # Data models for tablature
│   ├── TabLayoutEngine.h        # Layout calculation
│   ├── TabRenderer.h            # Tablature rendering with effects
│   ├── TabViewComponent.h       # Tablature view with click-to-seek & rest editing
│   └── BasicPitch/              # BasicPitch CNN inference (polyphonic pitch detection)
│       ├── BasicPitch.cpp/h     # Main transcription pipeline
│       ├── Notes.cpp/h          # Note extraction with pitch bend contours
│       └── BasicPitchCNN.cpp/h  # ONNX Runtime neural network inference
├── ThirdParty/
│   └── RTNeural/                # Real-time neural network inference library
├── CMakeLists.txt               # CMake build configuration
└── README.md
```

---

## Technical Details

### Supported Guitar Pro Formats

- **Guitar Pro 3** (.gp3)
- **Guitar Pro 4** (.gp4)
- **Guitar Pro 5** (.gp5)
- **Guitar Pro 7** (.gp) - via embedded GPIF XML
- **Guitar Pro 8** (.gp) - via embedded GPIF XML

### Supported Guitar Pro Features

- Song information (title, artist, album, tempo, etc.)
- **Chord names** displayed above measures
- Multiple tracks with individual settings
- Standard notes and rests
- Dotted notes
- Triplets and other tuplets
- **Rhythm notation with proper beam grouping:**
  - 4/4: 4 eighths per beam group (half-measure)
  - 6/8, 9/8, 12/8: 3 eighths per beam group (dotted quarter)
  - 3/4, 5/4, 7/4: 2 eighths per beam group (per quarter beat)
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

The GP5 writer exports tablature data to the Guitar Pro 5 format, fully compatible with PyGuitarPro, Guitar Pro software, and online services like Soundslice.

The GP7/GP8 parser extracts the embedded GPIF XML from the .gp archive format (ZIP container with Content/score.gpif) and parses notes, chords, tracks, and effects.

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
- [BasicPitch](https://github.com/spotify/basic-pitch) - Polyphonic pitch detection neural network by Spotify
- [ONNX Runtime](https://onnxruntime.ai/) - Neural network inference engine
- [RTNeural](https://github.com/jatinchowdhury18/RTNeural) - Real-time neural network inference
- [PyGuitarPro](https://github.com/Perlence/PyGuitarPro) - Reference for the GP file format specification
- [tonal](https://github.com/tonaljs/tonal) - Music theory library (inspiration for chord detection algorithms)
- [Gemini](https://gemini.google.com/) - AI assistance for algorithm design discussions
