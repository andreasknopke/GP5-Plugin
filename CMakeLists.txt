cmake_minimum_required(VERSION 3.22)

project(GP5_VST_Editor VERSION 1.0.1)

# JUCE Pfad konfigurieren
set(JUCE_PATH "D:/GitHub/JUCE" CACHE PATH "Path to JUCE")

# JUCE als Subdirectory einbinden
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

# Plugin-Target erstellen
juce_add_plugin(GP5_VST_Editor
    # Plugin-Eigenschaften
    COMPANY_NAME "AR-Sounds"
    COMPANY_WEBSITE "AR-Sounds"
    BUNDLE_ID "AR-Sounds"
    
    # Plugin-Formate (nur VST3)
    FORMATS VST3
    
    # Plugin-Name
    PLUGIN_NAME "GP5_VST_Editor"
    PRODUCT_NAME "GP5_VST_Editor"
    
    # Plugin-Codes
    PLUGIN_MANUFACTURER_CODE ARSo
    PLUGIN_CODE Gp5e
    
    # MIDI-Konfiguration - Instrument mit MIDI UND optionalem Audio-Input
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    
    # Editor vorhanden
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    
    # VST3-Kategorien - Instrument mit MIDI
    VST3_CATEGORIES "Instrument" "Generator"
    
    # Kopiere Plugin in den VST3-Ordner nach dem Build
    COPY_PLUGIN_AFTER_BUILD TRUE
)

# ==============================================================================
# RTNeural (header-only, for Basic Pitch CNN inference)
# ==============================================================================
add_library(RTNeural INTERFACE)
target_include_directories(RTNeural INTERFACE ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/RTNeural)
target_compile_definitions(RTNeural INTERFACE RTNEURAL_USE_STL=1)

# ==============================================================================
# ONNX Runtime (for Basic Pitch CQT feature extraction)
# ==============================================================================
add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_IMPLIB "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/lib/onnxruntime.lib"
    IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/lib/onnxruntime.dll"
)

# ==============================================================================
# Binary Data (model weights for Basic Pitch) - must be before BasicPitchCNN
# ==============================================================================
juce_add_binary_data(bin_data SOURCES
    ThirdParty/ModelData/features_model.onnx
    ThirdParty/ModelData/cnn_contour_model.json
    ThirdParty/ModelData/cnn_note_model.json
    ThirdParty/ModelData/cnn_onset_1_model.json
    ThirdParty/ModelData/cnn_onset_2_model.json
    resources/chord-fingers.csv
)

# ==============================================================================
# BasicPitchCNN static library (separate to allow optimization flags)
# ==============================================================================
add_library(BasicPitchCNN STATIC
    Source/BasicPitch/BasicPitchCNN.cpp
)
target_include_directories(BasicPitchCNN PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/RTNeural
)
target_compile_definitions(BasicPitchCNN PUBLIC RTNEURAL_USE_STL=1)
target_link_libraries(BasicPitchCNN PUBLIC RTNeural bin_data)
# Always optimize BasicPitchCNN (even in Debug) because RTNeural is very slow without optimization
if (MSVC)
    target_compile_options(BasicPitchCNN PUBLIC /O2)
else()
    target_compile_options(BasicPitchCNN PUBLIC -O3)
endif()

# ==============================================================================
# PowerTab Document Library (wxWindows license - for .ptb file parsing)
# ==============================================================================
add_library(PowerTabLib STATIC
    ThirdParty/powertab/alternateending.cpp
    ThirdParty/powertab/barline.cpp
    ThirdParty/powertab/chorddiagram.cpp
    ThirdParty/powertab/chordname.cpp
    ThirdParty/powertab/chordtext.cpp
    ThirdParty/powertab/colour.cpp
    ThirdParty/powertab/direction.cpp
    ThirdParty/powertab/dynamic.cpp
    ThirdParty/powertab/floatingtext.cpp
    ThirdParty/powertab/fontsetting.cpp
    ThirdParty/powertab/guitar.cpp
    ThirdParty/powertab/guitarin.cpp
    ThirdParty/powertab/harmonics.cpp
    ThirdParty/powertab/keysignature.cpp
    ThirdParty/powertab/macros.cpp
    ThirdParty/powertab/note.cpp
    ThirdParty/powertab/position.cpp
    ThirdParty/powertab/powertabdocument.cpp
    ThirdParty/powertab/powertabfileheader.cpp
    ThirdParty/powertab/powertabinputstream.cpp
    ThirdParty/powertab/powertaboutputstream.cpp
    ThirdParty/powertab/rect.cpp
    ThirdParty/powertab/rehearsalsign.cpp
    ThirdParty/powertab/rhythmslash.cpp
    ThirdParty/powertab/score.cpp
    ThirdParty/powertab/staff.cpp
    ThirdParty/powertab/system.cpp
    ThirdParty/powertab/systemsymbol.cpp
    ThirdParty/powertab/tempomarker.cpp
    ThirdParty/powertab/timesignature.cpp
    ThirdParty/powertab/tuning.cpp
)
target_include_directories(PowerTabLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/powertab
)
# Require C++17 for std::filesystem
target_compile_features(PowerTabLib PUBLIC cxx_std_17)
# Suppress warnings from third-party code
if (MSVC)
    target_compile_options(PowerTabLib PRIVATE /W0)
else()
    target_compile_options(PowerTabLib PRIVATE -w)
endif()

# ==============================================================================
# Source-Dateien hinzufügen
# ==============================================================================
target_sources(GP5_VST_Editor
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/GP5Parser.cpp
        Source/GP5Parser.h
        Source/GP7Parser.cpp
        Source/GP7Parser.h
        Source/GP5Writer.cpp
        Source/GP5Writer.h
        Source/PTBParser.cpp
        Source/PTBParser.h
        Source/AudioToMidiProcessor.cpp
        Source/AudioToMidiProcessor.h
        Source/AudioTranscriber.cpp
        Source/AudioTranscriber.h
        Source/TabModels.h
        Source/TabLayoutEngine.h
        Source/TabRenderer.h
        Source/TabViewComponent.h
        # BasicPitch integration (NeuralNote-based polyphonic transcription)
        Source/BasicPitch/BasicPitchConstants.h
        Source/BasicPitch/Features.cpp
        Source/BasicPitch/Features.h
        Source/BasicPitch/BasicPitch.cpp
        Source/BasicPitch/BasicPitch.h
        Source/BasicPitch/Notes.cpp
        Source/BasicPitch/Notes.h
        Source/BasicPitch/NoteUtils.h
        Source/BasicPitch/Utils.h
        Source/BasicPitch/Resampler.cpp
        Source/BasicPitch/Resampler.h
)

# Include directories for BasicPitch headers
target_include_directories(GP5_VST_Editor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/include
    ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/RTNeural
    ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/powertab
    ${CMAKE_CURRENT_LIST_DIR}/Source/BasicPitch
)

# Compiler-Definitionen
target_compile_definitions(GP5_VST_Editor
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        RTNEURAL_USE_STL=1
)

# JUCE-Module verlinken
target_link_libraries(GP5_VST_Editor
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        onnxruntime
        BasicPitchCNN
        PowerTabLib
        bin_data
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ==============================================================================
# Copy ONNX Runtime DLL next to the built VST3 plugin
# ==============================================================================
add_custom_command(TARGET GP5_VST_Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/lib/onnxruntime.dll"
        "$<TARGET_FILE_DIR:GP5_VST_Editor>"
    COMMENT "Copying onnxruntime.dll to output directory"
)
