/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "PluginProcessor.h"
#include "TabViewComponent.h"
#include "TrackSettingsComponent.h"
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
    
    // 2. Zoom Buttons
    juce::TextButton zoomInButton { "+" };
    juce::TextButton zoomOutButton { "-" };
    
    // 3. Track-Auswahl ComboBox
    juce::ComboBox trackSelector;
    juce::Label trackLabel;
    
    // 3b. Track Settings Button
    juce::TextButton settingsButton { "Settings" };

    // 4. Der FileChooser (smart pointer, um Speicherlecks zu vermeiden)
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // 5. Die Tabulatur-Ansicht
    TabViewComponent tabView;
    
    // 6. Info-Label für Song-Informationen
    juce::Label infoLabel;
    
    // 7. Transport-Info Label (Position, Tempo)
    juce::Label transportLabel;
    
    // 8. Auto-Scroll Toggle
    juce::ToggleButton autoScrollButton { "Auto-Scroll" };
    
    // 9. Track Settings Panel (popup)
    std::unique_ptr<TrackSettingsComponent> trackSettingsPanel;
    bool settingsPanelVisible = false;

    // 10. Hilfsfunktionen
    void loadButtonClicked();
    void trackSelectionChanged();
    void updateTrackSelector();
    void refreshFromProcessor();
    void updateTransportDisplay();
    void toggleSettingsPanel();
    
    // 11. State
    int lastDisplayedMeasure = -1;
    double lastPositionInBeats = -1.0;  // Für Erkennung von Positionssprüngen
    bool wasPlaying = false;            // Letzter Play-Status

    NewProjectAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessorEditor)
};
