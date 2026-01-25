/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "PluginProcessor.h"
#include "GP5Parser.h"
#include "TabViewComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
*/
class NewProjectAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    NewProjectAudioProcessorEditor (NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // 1. Der Button
    juce::TextButton loadButton { "Load GP5 File" };
    
    // 2. Zoom Buttons
    juce::TextButton zoomInButton { "+" };
    juce::TextButton zoomOutButton { "-" };
    
    // 3. Track-Auswahl ComboBox
    juce::ComboBox trackSelector;
    juce::Label trackLabel;

    // 4. Der FileChooser (smart pointer, um Speicherlecks zu vermeiden)
    std::unique_ptr<juce::FileChooser> fileChooser;

    // 5. Der GP5 Parser
    GP5Parser gp5Parser;
    
    // 6. Die Tabulatur-Ansicht
    TabViewComponent tabView;
    
    // 7. Info-Label für Song-Informationen
    juce::Label infoLabel;

    // 8. Hilfsfunktionen
    void loadButtonClicked();
    void trackSelectionChanged();
    void updateTrackSelector();

    NewProjectAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessorEditor)
};
