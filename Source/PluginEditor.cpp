/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewProjectAudioProcessorEditor::NewProjectAudioProcessorEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Button zum Fenster hinzufügen
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { loadButtonClicked(); };
    
    // Zoom Buttons
    addAndMakeVisible (zoomInButton);
    addAndMakeVisible (zoomOutButton);
    zoomInButton.onClick = [this] { tabView.setZoom(tabView.getZoom() + 0.2f); };
    zoomOutButton.onClick = [this] { tabView.setZoom(tabView.getZoom() - 0.2f); };
    
    // Track Selector
    addAndMakeVisible (trackLabel);
    trackLabel.setText ("Track:", juce::dontSendNotification);
    trackLabel.setFont (juce::Font(12.0f));
    trackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    
    addAndMakeVisible (trackSelector);
    trackSelector.onChange = [this] { trackSelectionChanged(); };
    trackSelector.setTextWhenNothingSelected ("-- Select Track --");
    
    // Info Label
    addAndMakeVisible (infoLabel);
    infoLabel.setFont (juce::Font(12.0f));
    infoLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    infoLabel.setText ("Load a GP5 file to see the tablature", juce::dontSendNotification);
    
    // Tabulatur-Ansicht
    addAndMakeVisible (tabView);
    tabView.onMeasureClicked = [this](int measureIndex) {
        DBG("Takt " << (measureIndex + 1) << " angeklickt");
    };

    // Fenstergröße setzen (größer für die Tab-Ansicht)
    setSize (800, 400);
    setResizable (true, true);
    setResizeLimits (600, 300, 1920, 1080);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
}

//==============================================================================
void NewProjectAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Hintergrundfarbe (Dunkelgrau für den Pro-Look)
    g.fillAll (juce::Colour(0xFF2D2D30));
}

void NewProjectAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Toolbar oben (50px hoch)
    auto toolbar = bounds.removeFromTop(50);
    toolbar = toolbar.reduced(5);
    
    // Load Button
    loadButton.setBounds (toolbar.removeFromLeft(120));
    toolbar.removeFromLeft(10); // Spacer
    
    // Zoom Buttons
    zoomOutButton.setBounds (toolbar.removeFromLeft(30));
    toolbar.removeFromLeft(5);
    zoomInButton.setBounds (toolbar.removeFromLeft(30));
    toolbar.removeFromLeft(20); // Spacer
    
    // Track Selector
    trackLabel.setBounds (toolbar.removeFromLeft(40));
    trackSelector.setBounds (toolbar.removeFromLeft(180));
    toolbar.removeFromLeft(20); // Spacer
    
    // Info Label (Rest der Toolbar)
    infoLabel.setBounds (toolbar);
    
    // Tabulatur-Ansicht (Rest des Fensters)
    bounds = bounds.reduced(5);
    tabView.setBounds (bounds);
}

// Die eigentliche Lade-Logik
void NewProjectAudioProcessorEditor::loadButtonClicked()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Wähle eine Guitar Pro 5 Datei...",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.gp5");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        
        if (file.existsAsFile())
        {
            DBG ("Datei ausgewählt: " << file.getFullPathName());
            
            // GP5-Parser aufrufen
            if (gp5Parser.parse(file))
            {
                const auto& info = gp5Parser.getSongInfo();
                
                DBG("Parsed successfully. Track count: " << gp5Parser.getTrackCount());
                DBG("Tracks array size: " << gp5Parser.getTracks().size());
                
                // Info-Label aktualisieren
                juce::String infoText = info.title;
                if (info.artist.isNotEmpty())
                    infoText += " - " + info.artist;
                infoText += " | " + juce::String(info.tempo) + " BPM";
                infoText += " | " + juce::String(gp5Parser.getTrackCount()) + " Tracks";
                infoText += " | " + juce::String(gp5Parser.getMeasureCount()) + " Measures";
                infoText += " | Parsed: " + juce::String(gp5Parser.getTracks().size());
                infoLabel.setText(infoText, juce::dontSendNotification);
                
                // Track-Auswahl aktualisieren
                updateTrackSelector();
                
                DBG("After updateTrackSelector, combobox items: " << trackSelector.getNumItems());
                
                // Ersten Track laden, wenn vorhanden
                if (gp5Parser.getTrackCount() > 0)
                {
                    trackSelector.setSelectedId(1, juce::dontSendNotification);
                    trackSelectionChanged();
                }
                
                DBG("GP5 erfolgreich geladen!");
            }
            else
            {
                infoLabel.setText("Fehler: " + gp5Parser.getLastError(), juce::dontSendNotification);
                DBG("Fehler: " << gp5Parser.getLastError());
            }
        }
    });
}

void NewProjectAudioProcessorEditor::updateTrackSelector()
{
    trackSelector.clear(juce::dontSendNotification);
    
    const auto& tracks = gp5Parser.getTracks();
    
    DBG("updateTrackSelector: " << tracks.size() << " tracks found");
    
    for (int i = 0; i < tracks.size(); ++i)
    {
        const auto& track = tracks[i];
        juce::String itemText = juce::String(i + 1) + ": " + track.name;
        
        // MIDI-Instrument-Info hinzufügen
        if (track.isPercussion)
            itemText += " (Drums)";
        else if (track.stringCount > 0)
            itemText += " (" + juce::String(track.stringCount) + " Strings)";
        
        DBG("  Adding track: " << itemText);
        trackSelector.addItem(itemText, i + 1);  // ID ist 1-basiert
    }
    
    DBG("Track-Selector mit " << tracks.size() << " Tracks aktualisiert");
}

void NewProjectAudioProcessorEditor::trackSelectionChanged()
{
    int selectedId = trackSelector.getSelectedId();
    if (selectedId <= 0)
        return;
    
    int trackIndex = selectedId - 1;  // Zurück zu 0-basiert
    
    if (trackIndex >= 0 && trackIndex < gp5Parser.getTrackCount())
    {
        TabTrack track = gp5Parser.convertToTabTrack(trackIndex);
        tabView.setTrack(track);
        
        const auto& gp5Track = gp5Parser.getTracks()[trackIndex];
        DBG("Track " << (trackIndex + 1) << " geladen: " << gp5Track.name 
            << " mit " << track.measures.size() << " Takten");
    }
}
