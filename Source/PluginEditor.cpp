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
    trackLabel.setFont (juce::FontOptions(12.0f));
    trackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    
    addAndMakeVisible (trackSelector);
    trackSelector.onChange = [this] { trackSelectionChanged(); };
    trackSelector.setTextWhenNothingSelected ("-- Select Track --");
    
    // Settings Button (opens track MIDI settings panel)
    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this] { toggleSettingsPanel(); };
    
    // Info Label
    addAndMakeVisible (infoLabel);
    infoLabel.setFont (juce::FontOptions(12.0f));
    infoLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    infoLabel.setText ("Load a GP5 file to see the tablature", juce::dontSendNotification);
    
    // Transport Label
    addAndMakeVisible (transportLabel);
    transportLabel.setFont (juce::FontOptions(11.0f));
    transportLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
    transportLabel.setText ("Stopped", juce::dontSendNotification);
    
    // Auto-Scroll Toggle
    addAndMakeVisible (autoScrollButton);
    autoScrollButton.setToggleState (true, juce::dontSendNotification);
    autoScrollButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    autoScrollButton.setColour (juce::ToggleButton::tickColourId, juce::Colours::lightgreen);
    
    // Tabulatur-Ansicht
    addAndMakeVisible (tabView);
    tabView.onMeasureClicked = [this](int measureIndex) {
        DBG("Takt " << (measureIndex + 1) << " angeklickt");
    };

    // Fenstergröße setzen (größer für die Tab-Ansicht)
    setSize (900, 450);
    setResizable (true, true);
    setResizeLimits (700, 350, 1920, 1080);
    
    // Wenn bereits eine Datei geladen ist, UI aktualisieren
    refreshFromProcessor();
    
    // Timer für DAW-Sync starten (30 Hz Update-Rate)
    startTimerHz (30);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
    stopTimer();
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
    loadButton.setBounds (toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(10); // Spacer
    
    // Zoom Buttons
    zoomOutButton.setBounds (toolbar.removeFromLeft(30));
    toolbar.removeFromLeft(5);
    zoomInButton.setBounds (toolbar.removeFromLeft(30));
    toolbar.removeFromLeft(15); // Spacer
    
    // Track Selector
    trackLabel.setBounds (toolbar.removeFromLeft(40));
    trackSelector.setBounds (toolbar.removeFromLeft(160));
    toolbar.removeFromLeft(5); // Spacer
    
    // Settings Button
    settingsButton.setBounds (toolbar.removeFromLeft(70));
    toolbar.removeFromLeft(10); // Spacer
    
    // Auto-Scroll Toggle
    autoScrollButton.setBounds (toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(15); // Spacer
    
    // Transport Label
    transportLabel.setBounds (toolbar.removeFromLeft(200));
    toolbar.removeFromLeft(10); // Spacer
    
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
            
            // GP5-Parser im Processor aufrufen
            if (audioProcessor.loadGP5File(file))
            {
                refreshFromProcessor();
                DBG("GP5 erfolgreich geladen!");
            }
            else
            {
                infoLabel.setText("Fehler: " + audioProcessor.getGP5Parser().getLastError(), juce::dontSendNotification);
                DBG("Fehler: " << audioProcessor.getGP5Parser().getLastError());
            }
        }
    });
}

void NewProjectAudioProcessorEditor::refreshFromProcessor()
{
    if (!audioProcessor.isFileLoaded())
        return;
    
    const auto& gp5Parser = audioProcessor.getGP5Parser();
    const auto& info = gp5Parser.getSongInfo();
    
    DBG("Refreshing UI from processor. Track count: " << gp5Parser.getTrackCount());
    
    // Info-Label aktualisieren
    juce::String infoText = info.title;
    if (info.artist.isNotEmpty())
        infoText += " - " + info.artist;
    infoText += " | " + juce::String(info.tempo) + " BPM";
    infoText += " | " + juce::String(gp5Parser.getTrackCount()) + " Tracks";
    infoText += " | " + juce::String(gp5Parser.getMeasureCount()) + " Measures";
    infoLabel.setText(infoText, juce::dontSendNotification);
    
    // Track-Auswahl aktualisieren
    updateTrackSelector();
    
    // Bestimme welchen Track wir anzeigen sollen:
    // 1. Aktuell ausgewählter Track im Processor (wenn Editor geschlossen/geöffnet wurde)
    // 2. Gespeicherter Track aus dem State (wenn Projekt neu geladen wurde)
    // 3. Erster Track (Fallback)
    int trackToSelect = audioProcessor.getSelectedTrack();
    
    // Prüfe ob gespeicherter Track vorhanden (nach setStateInformation)
    int savedTrack = audioProcessor.getSavedSelectedTrack();
    if (savedTrack >= 0 && savedTrack < gp5Parser.getTrackCount())
    {
        trackToSelect = savedTrack;
        audioProcessor.clearSavedSelectedTrack();  // Mark as consumed
        DBG("Using saved track from state: " << savedTrack);
    }
    
    // Validiere den Track-Index
    if (trackToSelect < 0 || trackToSelect >= gp5Parser.getTrackCount())
    {
        trackToSelect = 0;  // Fallback auf ersten Track
    }
    
    if (gp5Parser.getTrackCount() > 0)
    {
        trackSelector.setSelectedId(trackToSelect + 1, juce::dontSendNotification);
        trackSelectionChanged();
        DBG("Selected track: " << trackToSelect);
    }
    
    // Auto-Scroll Status wiederherstellen
    autoScrollButton.setToggleState(audioProcessor.isAutoScrollEnabled(), juce::dontSendNotification);
}

void NewProjectAudioProcessorEditor::updateTrackSelector()
{
    trackSelector.clear(juce::dontSendNotification);
    
    const auto& tracks = audioProcessor.getGP5Parser().getTracks();
    
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
    
    const auto& gp5Parser = audioProcessor.getGP5Parser();
    
    if (trackIndex >= 0 && trackIndex < gp5Parser.getTrackCount())
    {
        // Tab-Ansicht aktualisieren
        TabTrack track = gp5Parser.convertToTabTrack(trackIndex);
        tabView.setTrack(track);
        
        // MIDI-Output auf diesen Track setzen
        audioProcessor.setSelectedTrack(trackIndex);
        
        const auto& gp5Track = gp5Parser.getTracks()[trackIndex];
        DBG("Track " << (trackIndex + 1) << " geladen: " << gp5Track.name 
            << " mit " << track.measures.size() << " Takten (MIDI Output aktiviert)");
    }
}

void NewProjectAudioProcessorEditor::timerCallback()
{
    updateTransportDisplay();
    
    // Aktuellen Takt und Position immer aktualisieren (auch wenn gestoppt)
    int currentMeasure = audioProcessor.getCurrentMeasureIndex();
    double positionInMeasure = audioProcessor.getPositionInCurrentMeasure();
    double currentPositionInBeats = audioProcessor.getHostPositionInBeats();
    bool isPlaying = audioProcessor.isHostPlaying();
    
    // Playhead-Position immer aktualisieren (für flüssige Bewegung)
    tabView.setPlayheadPosition(positionInMeasure);
    tabView.setCurrentMeasure(currentMeasure);
    
    // Auto-Scroll Status mit Processor synchronisieren
    audioProcessor.setAutoScrollEnabled(autoScrollButton.getToggleState());
    
    // Erkennen von manuellen Positionssprüngen (auch wenn gestoppt)
    bool positionJumped = false;
    if (std::abs(currentPositionInBeats - lastPositionInBeats) > 0.5)
    {
        positionJumped = true;
    }
    
    // Auto-Scroll wenn:
    // 1. Aktiviert UND DAW spielt, ODER
    // 2. Aktiviert UND Position wurde manuell geändert (Sprung erkannt)
    if (autoScrollButton.getToggleState())
    {
        if (isPlaying || positionJumped)
        {
            tabView.scrollToMeasure(currentMeasure);
        }
    }
    
    // Update tracking variables
    lastPositionInBeats = currentPositionInBeats;
    wasPlaying = isPlaying;
}

void NewProjectAudioProcessorEditor::updateTransportDisplay()
{
    bool isPlaying = audioProcessor.isHostPlaying();
    double tempo = audioProcessor.getHostTempo();
    int dawTimeSigNum = audioProcessor.getHostTimeSignatureNumerator();
    int dawTimeSigDen = audioProcessor.getHostTimeSignatureDenominator();
    
    // Hole GP5-basierte Position (synchronisiert mit MIDI-Ausgabe)
    int currentMeasure = audioProcessor.getCurrentMeasureIndex() + 1;  // 1-basiert für Anzeige
    double posInMeasure = audioProcessor.getPositionInCurrentMeasure();
    
    // Hole GP5-Taktart für aktuellen Takt
    auto [gp5Num, gp5Den] = audioProcessor.getGP5TimeSignature(currentMeasure - 1);
    int gp5Tempo = audioProcessor.getGP5Tempo();
    
    // Berechne Beat innerhalb des Taktes (1-basiert)
    int beatInMeasure = static_cast<int>(posInMeasure * gp5Num) + 1;
    beatInMeasure = juce::jlimit(1, gp5Num, beatInMeasure);
    
    juce::String statusText;
    
    if (isPlaying)
    {
        statusText = juce::String::charToString(0x25B6) + " ";  // Play-Symbol
        transportLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    }
    else
    {
        statusText = juce::String::charToString(0x25A0) + " ";  // Stop-Symbol
        transportLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
    
    statusText += "Bar " + juce::String(currentMeasure) + "." + juce::String(beatInMeasure);
    statusText += " | DAW: " + juce::String(tempo, 1) + " BPM " + juce::String(dawTimeSigNum) + "/" + juce::String(dawTimeSigDen);
    statusText += " | GP5: " + juce::String(gp5Tempo) + " BPM " + juce::String(gp5Num) + "/" + juce::String(gp5Den);
    
    // Warnung bei Taktart-Mismatch
    if (!audioProcessor.isTimeSignatureMatching())
    {
        statusText += " " + juce::String::charToString(0x26A0);  // Warning-Symbol
    }
    
    transportLabel.setText(statusText, juce::dontSendNotification);
}
void NewProjectAudioProcessorEditor::toggleSettingsPanel()
{
    if (settingsPanelVisible && trackSettingsPanel != nullptr)
    {
        // Hide panel
        removeChildComponent(trackSettingsPanel.get());
        trackSettingsPanel.reset();
        settingsPanelVisible = false;
    }
    else
    {
        // Show panel
        trackSettingsPanel = std::make_unique<TrackSettingsComponent>(audioProcessor);
        trackSettingsPanel->onClose = [this]() { toggleSettingsPanel(); };
        
        // Position the panel (overlay on right side)
        int panelWidth = 480;
        int panelHeight = juce::jmin(400, getHeight() - 60);
        trackSettingsPanel->setBounds(getWidth() - panelWidth - 10, 55, panelWidth, panelHeight);
        
        addAndMakeVisible(trackSettingsPanel.get());
        settingsPanelVisible = true;
    }
}