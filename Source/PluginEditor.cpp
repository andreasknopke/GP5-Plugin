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
    
    // Unload Button (nur sichtbar wenn File geladen)
    addAndMakeVisible (unloadButton);
    unloadButton.onClick = [this] { unloadButtonClicked(); };
    unloadButton.setVisible(false);
    
    // Mode Label
    addAndMakeVisible (modeLabel);
    modeLabel.setFont (juce::FontOptions(12.0f, juce::Font::bold));
    modeLabel.setJustificationType(juce::Justification::centred);
    updateModeDisplay();
    
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
    
    // Save MIDI Button (Player Mode only)
    addAndMakeVisible (saveMidiButton);
    saveMidiButton.onClick = [this] { saveMidiButtonClicked(); };
    saveMidiButton.setVisible(false);  // Nur im Player-Modus mit geladenem File sichtbar
    
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
    
    // Recording Button (Editor Mode only)
    addAndMakeVisible (recordButton);
    recordButton.setColour (juce::ToggleButton::textColourId, juce::Colours::red);
    recordButton.setColour (juce::ToggleButton::tickColourId, juce::Colours::red);
    recordButton.onClick = [this] { 
        audioProcessor.setRecordingEnabled(recordButton.getToggleState());
    };
    recordButton.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Clear Recording Button
    addAndMakeVisible (clearRecordingButton);
    clearRecordingButton.onClick = [this] { 
        audioProcessor.clearRecording();
        refreshFromProcessor();
    };
    clearRecordingButton.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Fret Position Selector (Editor Mode only)
    addAndMakeVisible (fretPositionLabel);
    fretPositionLabel.setText("Fret:", juce::dontSendNotification);
    fretPositionLabel.setFont(juce::FontOptions(11.0f));
    fretPositionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    fretPositionLabel.setVisible(false);
    
    addAndMakeVisible (fretPositionSelector);
    fretPositionSelector.addItem("Low (0-4)", 1);
    fretPositionSelector.addItem("Mid (5-8)", 2);
    fretPositionSelector.addItem("High (9-12)", 3);
    fretPositionSelector.setSelectedId(1, juce::dontSendNotification);  // Default: Low
    fretPositionSelector.onChange = [this] {
        int selectedId = fretPositionSelector.getSelectedId();
        if (selectedId == 1)
            audioProcessor.setFretPosition(NewProjectAudioProcessor::FretPosition::Low);
        else if (selectedId == 2)
            audioProcessor.setFretPosition(NewProjectAudioProcessor::FretPosition::Mid);
        else if (selectedId == 3)
            audioProcessor.setFretPosition(NewProjectAudioProcessor::FretPosition::High);
    };
    fretPositionSelector.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Tabulatur-Ansicht
    addAndMakeVisible (tabView);
    tabView.onMeasureClicked = [this](int measureIndex) {
        DBG("Takt " << (measureIndex + 1) << " angeklickt");
    };
    
    // Position-Klick Callback - springt zur geklickten Position
    tabView.onPositionClicked = [this](int measureIndex, double positionInMeasure) {
        DBG("Position angeklickt: Takt " << (measureIndex + 1) << ", Position " << positionInMeasure);
        
        // Seek-Position im Processor setzen
        audioProcessor.setSeekPosition(measureIndex, positionInMeasure);
        
        // Transport-Anzeige aktualisieren
        updateTransportDisplay();
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
    // Unload Button (direkt neben Load Button)
    unloadButton.setBounds (toolbar.removeFromLeft(25));
    toolbar.removeFromLeft(5); // Spacer
    
    // Mode Label
    modeLabel.setBounds (toolbar.removeFromLeft(60));
    toolbar.removeFromLeft(5); // Spacer
    
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
    toolbar.removeFromLeft(5); // Spacer
    
    // Save MIDI Button
    saveMidiButton.setBounds (toolbar.removeFromLeft(70));
    toolbar.removeFromLeft(10); // Spacer
    
    // Auto-Scroll Toggle
    autoScrollButton.setBounds (toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(10); // Spacer
    
    // Recording Controls (nur im Editor-Modus sichtbar)
    recordButton.setBounds (toolbar.removeFromLeft(55));
    toolbar.removeFromLeft(5);
    clearRecordingButton.setBounds (toolbar.removeFromLeft(45));
    toolbar.removeFromLeft(5);
    
    // Fret Position Selector (nur im Editor-Modus sichtbar)
    fretPositionLabel.setBounds (toolbar.removeFromLeft(30));
    fretPositionSelector.setBounds (toolbar.removeFromLeft(90));
    toolbar.removeFromLeft(10); // Spacer
    
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
        "Select a Guitar Pro file...",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.gp;*.gp3;*.gp4;*.gp5");

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
                updateModeDisplay();
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

void NewProjectAudioProcessorEditor::unloadButtonClicked()
{
    audioProcessor.unloadFile();
    refreshFromProcessor();
    updateModeDisplay();
    tabView.resetScrollPosition();
    DBG("File unloaded");
}

void NewProjectAudioProcessorEditor::updateModeDisplay()
{
    if (audioProcessor.isFileLoaded())
    {
        // Player-Modus
        modeLabel.setText("Player", juce::dontSendNotification);
        modeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        modeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF1E5631));
        unloadButton.setVisible(true);
        saveMidiButton.setVisible(true);  // Save MIDI nur im Player-Modus
        
        // Recording und Fret-Selector nur im Editor-Modus
        recordButton.setVisible(false);
        clearRecordingButton.setVisible(false);
        fretPositionLabel.setVisible(false);
        fretPositionSelector.setVisible(false);
    }
    else
    {
        // Editor-Modus
        modeLabel.setText("Editor", juce::dontSendNotification);
        modeLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        modeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF5C3D00));
        unloadButton.setVisible(false);
        saveMidiButton.setVisible(false);  // Keine MIDI-Export im Editor-Modus
        
        // Recording und Fret-Selector im Editor-Modus verfügbar
        recordButton.setVisible(true);
        clearRecordingButton.setVisible(true);
        fretPositionLabel.setVisible(true);
        fretPositionSelector.setVisible(true);
    }
}

void NewProjectAudioProcessorEditor::refreshFromProcessor()
{
    // Modus-Anzeige aktualisieren
    updateModeDisplay();
    
    if (!audioProcessor.isFileLoaded())
    {
        // Zeige Editor-Modus Info
        infoLabel.setText("No file loaded - Play MIDI to see notes on tab", juce::dontSendNotification);
        
        // Setze leeren Track mit DAW-Taktart
        TabTrack emptyTrack = audioProcessor.getEmptyTabTrack();
        tabView.setTrack(emptyTrack);
        tabView.setEditorMode(true);
        return;
    }
    
    const auto& info = audioProcessor.getActiveSongInfo();
    int trackCount = audioProcessor.getActiveTracks().size();
    int measureCount = audioProcessor.getActiveMeasureHeaders().size();
    
    DBG("Refreshing UI from processor. Track count: " << trackCount);
    
    // Info-Label aktualisieren
    juce::String infoText = info.title;
    if (info.artist.isNotEmpty())
        infoText += " - " + info.artist;
    infoText += " | " + juce::String(info.tempo) + " BPM";
    infoText += " | " + juce::String(trackCount) + " Tracks";
    infoText += " | " + juce::String(measureCount) + " Measures";
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
    if (savedTrack >= 0 && savedTrack < trackCount)
    {
        trackToSelect = savedTrack;
        audioProcessor.clearSavedSelectedTrack();  // Mark as consumed
        DBG("Using saved track from state: " << savedTrack);
    }
    
    // Validiere den Track-Index
    if (trackToSelect < 0 || trackToSelect >= trackCount)
    {
        trackToSelect = 0;  // Fallback auf ersten Track
    }
    
    if (trackCount > 0)
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
    
    const auto& tracks = audioProcessor.getActiveTracks();
    
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
    
    const auto& tracks = audioProcessor.getActiveTracks();
    int trackCount = (int)tracks.size();
    
    if (trackIndex >= 0 && trackIndex < trackCount)
    {
        // Tab-Ansicht aktualisieren mit dem richtigen Parser
        TabTrack track;
        if (audioProcessor.isUsingGP7Parser())
        {
            const auto& gp7Tracks = audioProcessor.getGP7Parser().getTracks();
            juce::Array<TabMeasure> measures = audioProcessor.getGP7Parser().convertToTabMeasures(trackIndex);
            track.name = gp7Tracks[trackIndex].name;
            track.stringCount = gp7Tracks[trackIndex].stringCount;
            track.tuning = gp7Tracks[trackIndex].tuning;
            track.measures = measures;
        }
        else
        {
            track = audioProcessor.getGP5Parser().convertToTabTrack(trackIndex);
        }
        tabView.setTrack(track);
        
        // MIDI-Output auf diesen Track setzen
        audioProcessor.setSelectedTrack(trackIndex);
        
        const auto& gp5Track = tracks[trackIndex];
        DBG("Track " << (trackIndex + 1) << " geladen: " << gp5Track.name 
            << " mit " << track.measures.size() << " Takten (MIDI Output aktiviert)");
    }
}

void NewProjectAudioProcessorEditor::timerCallback()
{
    updateTransportDisplay();
    
    // ===========================================================================
    // Editor Mode: Zeige leeren Tab mit Live-MIDI-Noten wenn keine Datei geladen
    // ===========================================================================
    if (!audioProcessor.isFileLoaded())
    {
        bool isPlaying = audioProcessor.isHostPlaying();
        bool isRecording = audioProcessor.isRecording();
        
        // Update Recording-Button Farbe basierend auf Status
        if (isRecording)
        {
            recordButton.setColour(juce::ToggleButton::textColourId, juce::Colours::red);
            recordButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::darkred);
        }
        else
        {
            recordButton.setColour(juce::ToggleButton::textColourId, juce::Colours::grey);
        }
        
        // Zeige aufgezeichnete Noten wenn Recording aktiv oder Aufnahmen vorhanden
        auto recordedNotes = audioProcessor.getRecordedNotes();
        if (!recordedNotes.empty())
        {
            // Zeige aufgezeichnete Noten als Track
            TabTrack recordedTrack = audioProcessor.getRecordedTabTrack();
            tabView.setTrack(recordedTrack);
            tabView.setEditorMode(true);
        }
        else
        {
            // Setze leeren Track mit DAW-Taktart
            static bool emptyTrackSet = false;
            if (!emptyTrackSet || tabView.isEditorMode() == false)
            {
                TabTrack emptyTrack = audioProcessor.getEmptyTabTrack();
                tabView.setTrack(emptyTrack);
                tabView.setEditorMode(true);
                emptyTrackSet = true;
            }
        }
        
        // Live-MIDI-Noten aktualisieren (werden über den aufgezeichneten Noten angezeigt)
        auto midiNotes = audioProcessor.getLiveMidiNotes();
        std::vector<TabViewComponent::LiveNote> liveNotes;
        for (const auto& note : midiNotes)
        {
            TabViewComponent::LiveNote ln;
            ln.string = note.string;
            ln.fret = note.fret;
            ln.velocity = note.velocity;
            liveNotes.push_back(ln);
        }
        tabView.setLiveNotes(liveNotes);
        
        // Auch im Editor-Modus: Playhead-Position aktualisieren und bei Start zum ersten Takt scrollen
        double positionInBeats = audioProcessor.getHostPositionInBeats();
        
        // Berechne Takt basierend auf DAW-Taktart
        int numerator = audioProcessor.getHostTimeSignatureNumerator();
        int denominator = audioProcessor.getHostTimeSignatureDenominator();
        double beatsPerMeasure = numerator * (4.0 / denominator);
        
        int currentMeasure = (positionInBeats < 0.0) ? 0 : (int)(positionInBeats / beatsPerMeasure);
        double positionInMeasure = (positionInBeats < 0.0) ? 0.0 : 
            std::fmod(positionInBeats, beatsPerMeasure) / beatsPerMeasure;
        
        tabView.setPlayheadPosition(positionInMeasure);
        tabView.setCurrentMeasure(currentMeasure);
        
        // Bei Play-Start: Zum ersten Takt scrollen
        if (isPlaying && !wasPlaying)
        {
            tabView.scrollToMeasure(0);
        }
        
        wasPlaying = isPlaying;
        return;  // Keine weitere Verarbeitung nötig
    }
    else
    {
        // File ist geladen - Editor-Modus deaktivieren
        if (tabView.isEditorMode())
        {
            tabView.setEditorMode(false);
            tabView.setLiveNotes({});  // Clear live notes
        }
    }
    
    bool isPlaying = audioProcessor.isHostPlaying();
    
    int currentMeasure;
    double positionInMeasure;
    
    // Wenn DAW spielt, verwende DAW-Position; sonst verwende Seek-Position
    if (isPlaying)
    {
        currentMeasure = audioProcessor.getCurrentMeasureIndex();
        positionInMeasure = audioProcessor.getPositionInCurrentMeasure();
        
        // Seek-Position löschen wenn DAW spielt
        audioProcessor.clearSeekPosition();
    }
    else if (audioProcessor.hasSeekPosition())
    {
        // Verwende die geklickte Seek-Position
        currentMeasure = audioProcessor.getSeekMeasureIndex();
        positionInMeasure = audioProcessor.getSeekPositionInMeasure();
    }
    else
    {
        // Keine Seek-Position, verwende DAW-Position (gestoppt)
        currentMeasure = audioProcessor.getCurrentMeasureIndex();
        positionInMeasure = audioProcessor.getPositionInCurrentMeasure();
    }
    
    double currentPositionInBeats = audioProcessor.getHostPositionInBeats();
    
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
    
    // Bei Play-Start: Zum ersten Takt scrollen
    if (isPlaying && !wasPlaying)
    {
        // Playback wurde gerade gestartet - zum Anfang scrollen
        tabView.scrollToMeasure(0);
    }
    // Auto-Scroll wenn:
    // 1. Aktiviert UND DAW spielt, ODER
    // 2. Aktiviert UND Position wurde manuell geändert (Sprung erkannt)
    else if (autoScrollButton.getToggleState())
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
    
    int currentMeasure;
    double posInMeasure;
    
    // Verwende Seek-Position wenn verfügbar und DAW nicht spielt
    if (!isPlaying && audioProcessor.hasSeekPosition())
    {
        currentMeasure = audioProcessor.getSeekMeasureIndex() + 1;  // 1-basiert
        posInMeasure = audioProcessor.getSeekPositionInMeasure();
    }
    else
    {
        currentMeasure = audioProcessor.getCurrentMeasureIndex() + 1;  // 1-basiert
        posInMeasure = audioProcessor.getPositionInCurrentMeasure();
    }
    
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
    else if (audioProcessor.hasSeekPosition())
    {
        statusText = juce::String::charToString(0x2316) + " ";  // Position-Symbol (target)
        transportLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    }
    else
    {
        statusText = juce::String::charToString(0x25A0) + " ";  // Stop-Symbol
        transportLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
    
    statusText += "Bar " + juce::String(currentMeasure) + "." + juce::String(beatInMeasure);
    statusText += " | DAW: " + juce::String(tempo, 1) + " BPM " + juce::String(dawTimeSigNum) + "/" + juce::String(dawTimeSigDen);
    statusText += " | GP: " + juce::String(gp5Tempo) + " BPM " + juce::String(gp5Num) + "/" + juce::String(gp5Den);
    
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
        
        // Position the panel (centered, nearly full width)
        int panelWidth = getWidth() - 40;
        int panelHeight = juce::jmin(420, getHeight() - 70);
        trackSettingsPanel->setBounds(20, 55, panelWidth, panelHeight);
        
        addAndMakeVisible(trackSettingsPanel.get());
        settingsPanelVisible = true;
    }
}

void NewProjectAudioProcessorEditor::saveMidiButtonClicked()
{
    if (!audioProcessor.isFileLoaded())
        return;
    
    // Erstelle Auswahl-Dialog: Aktuelle Spur oder alle Spuren
    juce::PopupMenu menu;
    
    // Option 1: Nur aktuelle Spur als Einkanal-MIDI
    int currentTrack = audioProcessor.getSelectedTrack();
    const auto& tracks = audioProcessor.getActiveTracks();
    juce::String currentTrackName = (currentTrack >= 0 && currentTrack < tracks.size()) 
        ? tracks[currentTrack].name 
        : "Current Track";
    
    menu.addItem(1, "Current Track: " + currentTrackName + " (Single Channel)");
    menu.addSeparator();
    menu.addItem(2, "All Tracks (Multi-Channel MIDI)");
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&saveMidiButton),
        [this, currentTrack](int result)
        {
            if (result == 0)
                return;  // Menu was dismissed
            
            bool exportAllTracks = (result == 2);
            
            // Bestimme Standard-Dateiname basierend auf Song-Info
            const auto& info = audioProcessor.getActiveSongInfo();
            juce::String defaultFileName = info.title.isNotEmpty() ? info.title : "Exported";
            if (exportAllTracks)
                defaultFileName += "_AllTracks";
            else
                defaultFileName += "_Track" + juce::String(currentTrack + 1);
            defaultFileName += ".mid";
            
            // Datei-Auswahl-Dialog
            midiFileChooser = std::make_unique<juce::FileChooser>(
                exportAllTracks ? "Save All Tracks as MIDI..." : "Save Track as MIDI...",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(defaultFileName),
                "*.mid");
            
            auto chooserFlags = juce::FileBrowserComponent::saveMode 
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting;
            
            midiFileChooser->launchAsync(chooserFlags, 
                [this, exportAllTracks, currentTrack](const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    
                    if (file != juce::File{})
                    {
                        // Stelle sicher, dass die Datei mit .mid endet
                        if (!file.hasFileExtension(".mid"))
                            file = file.withFileExtension(".mid");
                        
                        bool success;
                        if (exportAllTracks)
                            success = audioProcessor.exportAllTracksToMidi(file);
                        else
                            success = audioProcessor.exportTrackToMidi(currentTrack, file);
                        
                        if (success)
                        {
                            infoLabel.setText("MIDI exported: " + file.getFileName(), juce::dontSendNotification);
                            DBG("MIDI exported successfully to: " << file.getFullPathName());
                        }
                        else
                        {
                            infoLabel.setText("Error exporting MIDI file!", juce::dontSendNotification);
                            DBG("Error exporting MIDI file");
                        }
                    }
                });
        });
}