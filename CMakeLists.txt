cmake_minimum_required(VERSION 3.22)

project(GP5_VST_Editor VERSION 1.0.0)

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

# Source-Dateien hinzufügen
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
        Source/AudioToMidiProcessor.cpp
        Source/AudioToMidiProcessor.h
        Source/TabModels.h
        Source/TabLayoutEngine.h
        Source/TabRenderer.h
        Source/TabViewComponent.h
)

# Get git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# If not in a git repo, use "unknown"
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

message(STATUS "Git commit hash: ${GIT_COMMIT_HASH}")

# Compiler-Definitionen
target_compile_definitions(GP5_VST_Editor
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
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
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
