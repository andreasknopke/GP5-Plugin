/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "PluginProcessor.h"
#include "TabViewComponent.h"
#include "TrackSettingsComponent.h"
#include "ExportPanelComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
*/
class NewProjectAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    NewProjectAudioProcessorEditor (NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Timer callback für DAW-Sync
    void timerCallback() override;
    
    // 1. Der Button
    juce::TextButton loadButton { "Load GuitarPro File" };
    juce::TextButton unloadButton { "-" };
    
    // Mode Label (Player/Editor)
    juce::Label modeLabel;
    
    // 2. Zoom Buttons
    juce::TextButton zoomInButton { "+" };
    juce::TextButton zoomOutButton { "-" };
    
    // 3. Track-Auswahl ComboBox
    juce::ComboBox trackSelector;
    juce::Label trackLabel;
    
    // 3b. Track Settings Button
    juce::TextButton settingsButton { "Settings" };
    
    // 3c. Save MIDI Button (Player Mode only)
    juce::TextButton saveMidiButton { "Save MIDI" };

    // 4. Der FileChooser (smart pointer, um Speicherlecks zu vermeiden)
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::FileChooser> midiFileChooser;
    
    // 5. Die Tabulatur-Ansicht
    TabViewComponent tabView;
    
    // 6. Info-Label für Song-Informationen
    juce::Label infoLabel;
    
    // 7. Transport-Info Label (Position, Tempo)
    juce::Label transportLabel;
    
    // 8. Auto-Scroll Toggle
    juce::ToggleButton autoScrollButton { "Auto-Scroll" };
    
    // 8b. Recording Indicator (Editor Mode only) - shows DAW record status
    juce::ToggleButton recordButton { "REC" };
    juce::TextButton clearRecordingButton { "Clear" };
    juce::TextButton saveGpButton { "Save GP" };  // Save as Guitar Pro file
    
    // 8c. Fret Position Selector (Editor Mode only)
    juce::ComboBox fretPositionSelector;
    juce::Label fretPositionLabel;
    
    // 8d. Legato Quantization Selector (Editor Mode only)
    juce::ComboBox legatoQuantizeSelector;
    juce::Label legatoQuantizeLabel;
    
    // 8e. Position Lookahead Selector (Editor Mode only)
    juce::ComboBox posLookaheadSelector;
    juce::Label posLookaheadLabel;
    
    // 8f. Note Edit Toggle (Player Mode)
    juce::ToggleButton noteEditButton { "Edit Notes" };
    
    // 9. Track Settings Panel (popup)
    std::unique_ptr<TrackSettingsComponent> trackSettingsPanel;
    bool settingsPanelVisible = false;
    
    // 9b. Export Panel (after recording)
    std::unique_ptr<ExportPanelComponent> exportPanel;
    bool exportPanelVisible = false;
    void showExportPanel();
    void hideExportPanel();
    void doExportWithMetadata(const juce::String& title, const std::vector<std::pair<juce::String, int>>& trackData);

    // 10. Hilfsfunktionen
    void loadButtonClicked();
    void unloadButtonClicked();
    void trackSelectionChanged();
    void updateTrackSelector();
    void updateTrackSelectorForRecording();  // Track-Selector für aufgezeichnete Tracks
    void refreshFromProcessor();
    void updateTransportDisplay();
    void toggleSettingsPanel();
    void updateModeDisplay();
    void saveMidiButtonClicked();
    void saveGpButtonClicked();
    void noteEditToggled();
    
    // 11. State
    int lastDisplayedMeasure = -1;
    double lastPositionInBeats = -1.0;  // Für Erkennung von Positionssprüngen
    bool wasPlaying = false;            // Letzter Play-Status
    bool wasRecording = false;          // Letzter Recording-Status (für UI-Update)
    bool hadRecordedNotes = false;      // Ob Aufnahmen vorhanden waren (für UI-Update)

    NewProjectAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessorEditor)
};
