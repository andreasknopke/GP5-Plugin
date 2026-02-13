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
    
    // Unified Save Button (shows format menu: MIDI or GP5)
    addAndMakeVisible (saveButton);
    saveButton.onClick = [this] { saveButtonClicked(); };
    saveButton.setVisible(false);  // Nur sichtbar wenn Noten vorhanden
    
    // Info Label (Header links - Song-Infos)
    addAndMakeVisible (infoLabel);
    infoLabel.setFont (juce::FontOptions(13.0f, juce::Font::bold));
    infoLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    infoLabel.setColour (juce::Label::backgroundColourId, juce::Colour(0xFF3C3C3C));
    infoLabel.setText ("Load a GuitarPro file to see the tablature", juce::dontSendNotification);
    infoLabel.setJustificationType(juce::Justification::centredLeft);
    
    // Transport Label (Header rechts - Transport-Infos)
    addAndMakeVisible (transportLabel);
    transportLabel.setFont (juce::FontOptions(12.0f));
    transportLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
    transportLabel.setColour (juce::Label::backgroundColourId, juce::Colour(0xFF3C3C3C));
    transportLabel.setText ("Stopped", juce::dontSendNotification);
    transportLabel.setJustificationType(juce::Justification::centredRight);
    
    // Auto-Scroll Toggle
    addAndMakeVisible (autoScrollButton);
    autoScrollButton.setToggleState (true, juce::dontSendNotification);
    autoScrollButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    autoScrollButton.setColour (juce::ToggleButton::tickColourId, juce::Colours::lightgreen);
    
    // Recording Button (Editor Mode only) - toggles recording, also syncs with DAW record status
    addAndMakeVisible (recordButton);
    recordButton.setColour (juce::ToggleButton::textColourId, juce::Colours::red);
    recordButton.setColour (juce::ToggleButton::tickColourId, juce::Colours::red);
    recordButton.onClick = [this] {
        audioProcessor.setRecordingEnabled(recordButton.getToggleState());
    };
    recordButton.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Clear Recording Button (im Editor: löscht Aufnahme, im Player: entlädt Datei)
    addAndMakeVisible (clearRecordingButton);
    clearRecordingButton.onClick = [this] { 
        if (audioProcessor.isFileLoaded())
        {
            // Im Player-Modus: Datei entladen und zum Editor wechseln
            auto options = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Clear")
                .withMessage("Unload the current file?")
                .withButton("Yes")
                .withButton("No")
                .withAssociatedComponent(this);
            juce::AlertWindow::showAsync(options, [this](int result) {
                if (result == 1) // Yes
                {
                    audioProcessor.unloadFile();
                    tabView.resetScrollPosition();
                    refreshFromProcessor();
                }
            });
        }
        else if (audioProcessor.hasRecordedNotes() || audioProcessor.isAudioRecording() || audioProcessor.isAudioTranscribing())
        {
            // Im Editor-Modus: Nachfragen bevor Aufnahme gelöscht wird
            auto options = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Clear")
                .withMessage("Clear the entire recording?")
                .withButton("Yes")
                .withButton("No")
                .withAssociatedComponent(this);
            juce::AlertWindow::showAsync(options, [this](int result) {
                if (result == 1) // Yes
                {
                    // Disable recording toggle
                    recordButton.setToggleState(false, juce::dontSendNotification);
                    audioProcessor.setRecordingEnabled(false);
                    
                    // Full clear
                    audioProcessor.clearRecording();
                    
                    // Force empty track in UI
                    TabTrack emptyTrack = audioProcessor.getEmptyTabTrack();
                    tabView.setTrack(emptyTrack);
                    tabView.setEditorMode(true);
                    tabView.setOverlayMessage({});
                    tabView.setLiveNotes({});
                    tabView.setLiveChordName({});
                    tabView.resetScrollPosition();
                    
                    // Reset tracking flags
                    hadRecordedNotes = false;
                    wasRecording = false;
                    
                    updateModeDisplay();
                }
            });
        }
    };
    clearRecordingButton.setVisible(false);  // Wird in updateModeDisplay() gesteuert
    
    // (saveGpButton removed - merged into unified saveButton)
    
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
    fretPositionSelector.setSelectedId(2, juce::dontSendNotification);  // Default: Mid
    fretPositionSelector.onChange = [this] {
        int selectedId = fretPositionSelector.getSelectedId();
        if (selectedId == 1)
            audioProcessor.setFretPosition(NewProjectAudioProcessor::FretPosition::Low);
        else if (selectedId == 2)
            audioProcessor.setFretPosition(NewProjectAudioProcessor::FretPosition::Mid);
        else if (selectedId == 3)
            audioProcessor.setFretPosition(NewProjectAudioProcessor::FretPosition::High);
        
        // Mark settings as pending (apply with Apply button)
        markSettingsPending();
    };
    fretPositionSelector.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Legato Quantization Selector (Editor Mode only)
    addAndMakeVisible (legatoQuantizeLabel);
    legatoQuantizeLabel.setText("Legato:", juce::dontSendNotification);
    legatoQuantizeLabel.setFont(juce::FontOptions(11.0f));
    legatoQuantizeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    legatoQuantizeLabel.setVisible(false);
    
    addAndMakeVisible (legatoQuantizeSelector);
    legatoQuantizeSelector.addItem("Off", 1);
    legatoQuantizeSelector.addItem("1/32", 2);      // 0.125 beats
    legatoQuantizeSelector.addItem("1/16", 3);      // 0.25 beats (default)
    legatoQuantizeSelector.addItem("1/8", 4);       // 0.5 beats
    legatoQuantizeSelector.addItem("1/4", 5);       // 1.0 beats
    legatoQuantizeSelector.setSelectedId(3, juce::dontSendNotification);  // Default: 1/16
    legatoQuantizeSelector.onChange = [this] {
        int selectedId = legatoQuantizeSelector.getSelectedId();
        double threshold = 0.0;
        if (selectedId == 2) threshold = 0.125;      // 1/32
        else if (selectedId == 3) threshold = 0.25;  // 1/16
        else if (selectedId == 4) threshold = 0.5;   // 1/8
        else if (selectedId == 5) threshold = 1.0;   // 1/4
        audioProcessor.setLegatoQuantization(threshold);
        
        // Mark settings as pending (apply with Apply button)
        markSettingsPending();
    };
    legatoQuantizeSelector.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Position Lookahead Selector (Editor Mode only)
    addAndMakeVisible (posLookaheadLabel);
    posLookaheadLabel.setText("Pos:", juce::dontSendNotification);
    posLookaheadLabel.setFont(juce::FontOptions(11.0f));
    posLookaheadLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    posLookaheadLabel.setVisible(false);
    
    addAndMakeVisible (posLookaheadSelector);
    posLookaheadSelector.addItem("1", 1);   // Update every note
    posLookaheadSelector.addItem("2", 2);   // Update every 2nd note
    posLookaheadSelector.addItem("3", 3);   // Update every 3rd note
    posLookaheadSelector.addItem("4", 4);   // Update every 4th note
    posLookaheadSelector.setSelectedId(4, juce::dontSendNotification);  // Default: 4 notes
    posLookaheadSelector.onChange = [this] {
        int selectedId = posLookaheadSelector.getSelectedId();
        audioProcessor.setPositionLookahead(selectedId);
        
        // Mark settings as pending (apply with Apply button)
        markSettingsPending();
    };
    posLookaheadSelector.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // All Tracks Checkbox (Editor Mode only)
    addAndMakeVisible (allTracksCheckbox);
    allTracksCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    allTracksCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::cyan);
    allTracksCheckbox.setToggleState(true, juce::dontSendNotification);  // Default: apply to all tracks
    allTracksCheckbox.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Measure Quantization Toggle (Editor Mode only)
    addAndMakeVisible (measureQuantizeButton);
    measureQuantizeButton.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    measureQuantizeButton.setColour (juce::ToggleButton::tickColourId, juce::Colours::orange);
    measureQuantizeButton.setToggleState(audioProcessor.isMeasureQuantizationEnabled(), juce::dontSendNotification);
    measureQuantizeButton.onClick = [this] {
        audioProcessor.setMeasureQuantizationEnabled(measureQuantizeButton.getToggleState());
        // Mark settings as pending (apply with Apply button)
        markSettingsPending();
    };
    measureQuantizeButton.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Finger Numbers Toggle (Editor Mode)
    addAndMakeVisible (fingerNumbersButton);
    fingerNumbersButton.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    fingerNumbersButton.setColour (juce::ToggleButton::tickColourId, juce::Colour(0xFF0077CC));
    fingerNumbersButton.setToggleState(audioProcessor.getShowFingerNumbers(), juce::dontSendNotification);
    tabView.setShowFingerNumbers(audioProcessor.getShowFingerNumbers());  // Sync initial state
    fingerNumbersButton.onClick = [this] {
        bool show = fingerNumbersButton.getToggleState();
        audioProcessor.setShowFingerNumbers(show);
        tabView.setShowFingerNumbers(show);
    };
    fingerNumbersButton.setVisible(false);  // Nur im Editor-Modus sichtbar
    
    // Note Edit Toggle Button (Player Mode only)
    addAndMakeVisible (noteEditButton);
    noteEditButton.setColour (juce::ToggleButton::textColourId, juce::Colours::cyan);
    noteEditButton.setColour (juce::ToggleButton::tickColourId, juce::Colours::cyan);
    noteEditButton.onClick = [this] { noteEditToggled(); };
    noteEditButton.setVisible(false);  // Nur im Player-Modus mit geladenem File sichtbar
    
    // Apply Button (deferred apply for bottom bar settings)
    addAndMakeVisible (applyButton);
    applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4CAF50));  // Green
    applyButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    applyButton.onClick = [this] { applyPendingSettings(); };
    applyButton.setVisible(false);
    applyButton.setEnabled(false);
    
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
    
    // Note-Position-Changed Callback
    tabView.onNotePositionChanged = [this](int measureIdx, int beatIdx, int oldString, int newString, int newFret) {
        DBG("Note geändert: Takt " << (measureIdx + 1) << ", Beat " << (beatIdx + 1) 
            << ", Saite " << (oldString + 1) << " -> Saite " << (newString + 1) << ", Bund " << newFret);
        
        // Änderung an den Processor weiterleiten für Speicherung und Export
        audioProcessor.updateRecordedNotePosition(measureIdx, beatIdx, oldString, newString, newFret);
        
        // Speichere den aktuellen Track-Zustand im Processor
        audioProcessor.setEditedTrack(audioProcessor.getSelectedTrack(), tabView.getTrack());
    };
    
    // Note-Deleted Callback
    tabView.onNoteDeleted = [this](int measureIdx, int beatIdx, int stringIndex) {
        DBG("Note gelöscht: Takt " << (measureIdx + 1) << ", Beat " << (beatIdx + 1) 
            << ", Saite " << (stringIndex + 1));
        
        audioProcessor.deleteRecordedNote(measureIdx, beatIdx, stringIndex);
        audioProcessor.setEditedTrack(audioProcessor.getSelectedTrack(), tabView.getTrack());
    };
    
    // Beat-Duration-Changed Callback
    tabView.onBeatDurationChanged = [this](int measureIdx, int beatIdx, int newDurationValue, bool isDotted) {
        DBG("Beat-Dauer geändert: Takt " << (measureIdx + 1) << ", Beat " << (beatIdx + 1) 
            << ", Dauer " << newDurationValue << (isDotted ? " (dotted)" : ""));
        
        audioProcessor.updateRecordedNoteDuration(measureIdx, beatIdx, newDurationValue, isDotted);
        audioProcessor.setEditedTrack(audioProcessor.getSelectedTrack(), tabView.getTrack());
    };

    // Fenstergröße setzen (größer für die Tab-Ansicht + Header)
    setSize (900, 480);
    setResizable (true, true);
    setResizeLimits (700, 380, 1920, 1080);
    
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
    
    // Bottom Bar Hintergrund (im Editor-Modus oder Note Edit Modus)
    if (isBottomBarVisible())
    {
        auto bottomBarArea = getLocalBounds().removeFromBottom(30);
        g.setColour (juce::Colour(0xFF3C3C3C));
        g.fillRect (bottomBarArea);
        g.setColour (juce::Colour(0xFF555555));
        g.drawLine ((float)bottomBarArea.getX(), (float)bottomBarArea.getY(), 
                    (float)bottomBarArea.getRight(), (float)bottomBarArea.getY(), 1.0f);
    }
}

void NewProjectAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Header-Bereich oben (25px hoch) - Song-Info und Transport
    auto header = bounds.removeFromTop(25);
    header = header.reduced(5, 2);
    
    // Info Label (Song-Titel, Artist, BPM, Tracks) - linke Hälfte
    infoLabel.setBounds (header.removeFromLeft(header.getWidth() / 2));
    
    // Transport Label (Position, Tempo, Taktart-Warnung) - rechte Hälfte
    transportLabel.setBounds (header);
    
    // Bottom Bar (30px hoch) - Editor-Modus Selektoren (Fret, Legato, Pos)
    // Sichtbar im Editor-Modus UND im Note Edit Modus (auch mit geladenem File)
    bool showBottomBar = isBottomBarVisible();
    if (showBottomBar)
    {
        auto bottomBar = bounds.removeFromBottom(30);
        bottomBar = bottomBar.reduced(5, 2);
        
        // Fret Position Selector
        fretPositionLabel.setBounds (bottomBar.removeFromLeft(30));
        fretPositionSelector.setBounds (bottomBar.removeFromLeft(90));
        bottomBar.removeFromLeft(20); // Spacer
        
        // Legato Quantization Selector
        legatoQuantizeLabel.setBounds (bottomBar.removeFromLeft(50));
        legatoQuantizeSelector.setBounds (bottomBar.removeFromLeft(70));
        bottomBar.removeFromLeft(20); // Spacer
        
        // Position Lookahead Selector
        posLookaheadLabel.setBounds (bottomBar.removeFromLeft(30));
        posLookaheadSelector.setBounds (bottomBar.removeFromLeft(55));
        bottomBar.removeFromLeft(30); // Spacer
        
        // All Tracks Checkbox
        allTracksCheckbox.setBounds (bottomBar.removeFromLeft(90));
        // Disable checkbox when recording is active (settings apply to all during recording)
        allTracksCheckbox.setEnabled(!audioProcessor.isRecording());
        bottomBar.removeFromLeft(20); // Spacer
        
        // Measure Quantization Toggle
        measureQuantizeButton.setBounds (bottomBar.removeFromLeft(110));
        bottomBar.removeFromLeft(10); // Spacer
        
        // Finger Numbers Toggle
        fingerNumbersButton.setBounds (bottomBar.removeFromLeft(80));
        bottomBar.removeFromLeft(20); // Spacer
        
        // Apply Button (rechtsbündig)
        applyButton.setBounds (bottomBar.removeFromRight(70));
    }
    else
    {
        // Player-Modus (ohne Note Edit) - Bottom Bar Elemente nicht sichtbar
        fretPositionLabel.setBounds (0, 0, 0, 0);
        fretPositionSelector.setBounds (0, 0, 0, 0);
        legatoQuantizeLabel.setBounds (0, 0, 0, 0);
        legatoQuantizeSelector.setBounds (0, 0, 0, 0);
        posLookaheadLabel.setBounds (0, 0, 0, 0);
        posLookaheadSelector.setBounds (0, 0, 0, 0);
        allTracksCheckbox.setBounds (0, 0, 0, 0);
        measureQuantizeButton.setBounds (0, 0, 0, 0);
        fingerNumbersButton.setBounds (0, 0, 0, 0);
        applyButton.setBounds (0, 0, 0, 0);
    }
    
    // Toolbar (45px hoch) - Buttons
    auto toolbar = bounds.removeFromTop(45);
    toolbar = toolbar.reduced(5);
    
    // Load Button
    loadButton.setBounds (toolbar.removeFromLeft(100));
    // Unload Button (direkt neben Load Button)
    unloadButton.setBounds (toolbar.removeFromLeft(25));
    toolbar.removeFromLeft(5); // Spacer
    
    // Zoom Buttons
    zoomOutButton.setBounds (toolbar.removeFromLeft(30));
    toolbar.removeFromLeft(5);
    zoomInButton.setBounds (toolbar.removeFromLeft(30));
    toolbar.removeFromLeft(15); // Spacer
    
    // Track Selector (Player-Modus oder Editor-Modus mit Aufnahmen)
    trackLabel.setBounds (toolbar.removeFromLeft(40));
    auto trackSelectorArea = toolbar.removeFromLeft(120);
    trackSelector.setBounds (trackSelectorArea);
    
    toolbar.removeFromLeft(5); // Spacer
    
    // Settings Button
    settingsButton.setBounds (toolbar.removeFromLeft(70));
    toolbar.removeFromLeft(5);
    
    // Note Edit Button (nur im Player-Modus)
    noteEditButton.setBounds (toolbar.removeFromLeft(90));
    
    // Rechtsbündige Controls (von rechts nach links platziert)
    // Clear Button (in beiden Modi sichtbar)
    clearRecordingButton.setBounds (toolbar.removeFromRight(45));
    toolbar.removeFromRight(5);
    
    // Unified Save Button (zeigt Format-Menü)
    auto saveButtonArea = toolbar.removeFromRight(60);
    saveButton.setBounds (saveButtonArea);
    toolbar.removeFromRight(5);
    
    // Recording Button (nur im Editor-Modus)
    recordButton.setBounds (toolbar.removeFromRight(55));
    toolbar.removeFromRight(10); // Spacer
    
    // Auto-Scroll Toggle (in beiden Modi)
    autoScrollButton.setBounds (toolbar.removeFromRight(100));
    
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
                
                // Note-Editing-Zustand synchronisieren (falls vorher aktiv war)
                bool editingEnabled = noteEditButton.getToggleState();
                tabView.setNoteEditingEnabled(editingEnabled);
                
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
    bool hasRecordings = audioProcessor.hasRecordedNotes();
    
    if (audioProcessor.isFileLoaded())
    {
        // Player-Modus (File geladen)
        unloadButton.setVisible(true);
        saveButton.setVisible(true);  // Save Button im Player-Modus immer sichtbar (Datei geladen = Noten vorhanden)
        
        // Track-Auswahl und Settings im Player-Modus
        trackLabel.setVisible(true);
        trackSelector.setVisible(true);
        settingsButton.setVisible(true);
        noteEditButton.setVisible(true);  // Note Editing im Player-Modus verfügbar
        
        // Recording-Controls ausblenden im Player-Modus
        recordButton.setVisible(false);
        clearRecordingButton.setVisible(true);  // Sichtbar für Wechsel zu Editor-Modus
        
        // Bottom Bar Controls: sichtbar wenn Note Edit aktiv
        bool noteEditActive = noteEditButton.getToggleState();
        fretPositionLabel.setVisible(noteEditActive);
        fretPositionSelector.setVisible(noteEditActive);
        legatoQuantizeLabel.setVisible(noteEditActive);
        legatoQuantizeSelector.setVisible(noteEditActive);
        posLookaheadLabel.setVisible(noteEditActive);
        posLookaheadSelector.setVisible(noteEditActive);
        allTracksCheckbox.setVisible(noteEditActive);
        measureQuantizeButton.setVisible(noteEditActive);
        fingerNumbersButton.setVisible(noteEditActive);
        applyButton.setVisible(noteEditActive);
        applyButton.setEnabled(pendingSettingsChange);
    }
    else if (hasRecordings)
    {
        // Editor-Modus MIT Aufnahmen - hybride Ansicht
        unloadButton.setVisible(false);
        saveButton.setVisible(true);  // Save Button sichtbar wenn Aufnahmen vorhanden
        
        // Track-Auswahl und Settings auch im Editor-Modus wenn Aufnahmen vorhanden
        // Dies ermöglicht Playback und Track-Konfiguration nach der Aufnahme
        trackLabel.setVisible(true);
        trackSelector.setVisible(true);
        settingsButton.setVisible(true);
        noteEditButton.setVisible(true);  // Note Editing auch nach Recording verfügbar
        
        // Recording und Editor-Selektoren bleiben sichtbar
        recordButton.setVisible(true);
        clearRecordingButton.setVisible(true);
        fretPositionLabel.setVisible(true);
        fretPositionSelector.setVisible(true);
        legatoQuantizeLabel.setVisible(true);
        legatoQuantizeSelector.setVisible(true);
        posLookaheadLabel.setVisible(true);
        posLookaheadSelector.setVisible(true);
        allTracksCheckbox.setVisible(true);
        measureQuantizeButton.setVisible(true);
        fingerNumbersButton.setVisible(true);
        applyButton.setVisible(true);
        applyButton.setEnabled(pendingSettingsChange);
        
        // Track-Selector mit aufgezeichneten Tracks aktualisieren
        updateTrackSelectorForRecording();
    }
    else
    {
        // Editor-Modus OHNE Aufnahmen - nur Recording-Features
        unloadButton.setVisible(false);
        saveButton.setVisible(false);  // Kein Save Button ohne Noten
        
        // Keine Track-Auswahl ohne Aufnahmen
        trackLabel.setVisible(false);
        trackSelector.setVisible(false);
        settingsButton.setVisible(false);
        noteEditButton.setVisible(false);  // Kein Note Editing im Editor-Modus ohne Aufnahmen
        tabView.setNoteEditingEnabled(false);
        
        // Recording und Fret-Selector verfügbar
        recordButton.setVisible(true);
        clearRecordingButton.setVisible(true);
        fretPositionLabel.setVisible(true);
        fretPositionSelector.setVisible(true);
        legatoQuantizeLabel.setVisible(true);
        legatoQuantizeSelector.setVisible(true);
        posLookaheadLabel.setVisible(true);
        posLookaheadSelector.setVisible(true);
        allTracksCheckbox.setVisible(false);  // Keine Checkbox ohne mehrere Tracks
        measureQuantizeButton.setVisible(true);
        fingerNumbersButton.setVisible(true);
        applyButton.setVisible(true);
        applyButton.setEnabled(pendingSettingsChange);
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

void NewProjectAudioProcessorEditor::updateTrackSelectorForRecording()
{
    trackSelector.clear(juce::dontSendNotification);
    
    // Hole Tracks aus aufgezeichneten Noten (gruppiert nach MIDI-Kanal)
    auto tracks = audioProcessor.getDisplayTracks();
    
    DBG("updateTrackSelectorForRecording: " << tracks.size() << " recorded tracks found");
    
    for (int i = 0; i < tracks.size(); ++i)
    {
        const auto& track = tracks[i];
        juce::String itemText = juce::String(i + 1) + ": " + track.name;
        
        // MIDI-Kanal-Info hinzufügen
        itemText += " (Ch " + juce::String(track.midiChannel) + ")";
        
        DBG("  Adding recorded track: " << itemText);
        trackSelector.addItem(itemText, i + 1);  // ID ist 1-basiert
    }
    
    // Ersten Track auswählen und Tab-Ansicht aktualisieren
    if (tracks.size() > 0)
    {
        trackSelector.setSelectedId(1, juce::dontSendNotification);
        audioProcessor.setSelectedTrack(0);
        
        // Tab-Ansicht mit erstem Track aktualisieren
        trackSelectionChanged();
    }
    
    DBG("Track-Selector für Aufnahme mit " << tracks.size() << " Tracks aktualisiert");
}

void NewProjectAudioProcessorEditor::trackSelectionChanged()
{
    int selectedId = trackSelector.getSelectedId();
    if (selectedId <= 0)
        return;
    
    int trackIndex = selectedId - 1;  // Zurück zu 0-basiert
    
    // Unterscheide zwischen Player-Modus (Datei geladen) und Editor-Modus (Aufnahmen)
    if (audioProcessor.isFileLoaded())
    {
        // Player-Modus: Verwende geladene Datei
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
    else if (audioProcessor.hasRecordedNotes())
    {
        // Editor-Modus mit Aufnahmen: Verwende editierten Track wenn vorhanden
        if (audioProcessor.hasEditedTrack(trackIndex))
        {
            tabView.setTrack(audioProcessor.getEditedTrack(trackIndex));
            tabView.setEditorMode(true);
            audioProcessor.setSelectedTrack(trackIndex);
            DBG("Aufgezeichneter Track " << (trackIndex + 1) << " ausgewählt (edited)");
        }
        else
        {
            auto recordedTracks = audioProcessor.getRecordedTabTracks();
            int trackCount = (int)recordedTracks.size();
            if (trackIndex >= 0 && trackIndex < trackCount)
            {
                tabView.setTrack(recordedTracks[trackIndex]);
                tabView.setEditorMode(true);
                audioProcessor.setSelectedTrack(trackIndex);
                DBG("Aufgezeichneter Track " << (trackIndex + 1) << " ausgewählt: " 
                    << recordedTracks[trackIndex].name);
            }
        }
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
        bool isRecordEnabled = audioProcessor.isRecordingEnabled();
        bool hasRecordings = audioProcessor.hasRecordedNotes();
        
        // UI-Modus aktualisieren wenn:
        // 1. Recording gerade beendet wurde (wasRecording -> !isRecording)
        // 2. Erste Aufnahmen hinzugekommen sind (!hadRecordedNotes -> hasRecordings)
        // 3. Playback gestoppt wurde nach Recording (wasPlaying && !isPlaying && hasRecordings)
        bool shouldUpdateUI = (wasRecording && !isRecording) || 
                              (!hadRecordedNotes && hasRecordings) ||
                              (wasPlaying && !isPlaying && hasRecordings);
        
        if (shouldUpdateUI)
        {
            // Recording beendet oder erste Aufnahmen vorhanden - UI aktualisieren
            updateModeDisplay();
        }
        
        // Recording-Button nach Stop automatisch deaktivieren wenn Aufnahmen vorhanden
        // Dies ermöglicht reines Playback der Aufnahme beim nächsten Play
        if (wasPlaying && !isPlaying && hasRecordings && recordButton.getToggleState())
        {
            recordButton.setToggleState(false, juce::dontSendNotification);
            audioProcessor.setRecordingEnabled(false);
            DBG("Recording nach Stop automatisch deaktiviert - Playback-Modus aktiv");
        }
        
        // Track state für nächsten Durchlauf
        wasRecording = isRecording;
        hadRecordedNotes = hasRecordings;
        
        // Sync Recording-Button mit DAW Record-Status
        // Wenn DAW Record aktiviert, Button auch aktivieren
        if (audioProcessor.isHostRecording() && !recordButton.getToggleState())
        {
            recordButton.setToggleState(true, juce::dontSendNotification);
            audioProcessor.setRecordingEnabled(true);
        }
        // Wenn DAW Record deaktiviert UND nicht mehr playing, Button deaktivieren
        else if (!audioProcessor.isHostRecording() && !isPlaying && recordButton.getToggleState())
        {
            recordButton.setToggleState(false, juce::dontSendNotification);
            audioProcessor.setRecordingEnabled(false);
        }
        
        // Update Recording-Button Farbe basierend auf Status
        if (isRecording)
        {
            recordButton.setColour(juce::ToggleButton::textColourId, juce::Colours::red);
            recordButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::red);
        }
        else if (isRecordEnabled)
        {
            // Record enabled aber nicht playing
            recordButton.setColour(juce::ToggleButton::textColourId, juce::Colours::darkred);
            recordButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::darkred);
        }
        else
        {
            recordButton.setColour(juce::ToggleButton::textColourId, juce::Colours::grey);
            recordButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::grey);
        }
        
        // =====================================================================
        // Audio-to-MIDI Overlay: Zeige Status-Nachricht statt Live-Noten
        // =====================================================================
        bool audioRecordingActive = audioProcessor.isAudioRecording();
        bool audioTranscribing = audioProcessor.isAudioTranscribing();
        
        if (audioRecordingActive)
        {
            tabView.setOverlayMessage(juce::CharPointer_UTF8("🎙 Audio-to-MIDI Recording..."));
            tabView.setLiveNotes({});
            tabView.setLiveChordName({});
        }
        else if (audioTranscribing)
        {
            tabView.setOverlayMessage(juce::CharPointer_UTF8("⏳ Audio-to-MIDI Processing. Please wait..."));
            tabView.setLiveNotes({});
            tabView.setLiveChordName({});
        }
        else
        {
            // Kein Audio-Recording/Transcription - Overlay entfernen
            tabView.setOverlayMessage({});
        }
        
        // Zeige aufgezeichnete Noten wenn Recording aktiv oder Aufnahmen vorhanden
        auto recordedNotes = audioProcessor.getRecordedNotes();
        if (!recordedNotes.empty())
        {
            // Während Recording: Zeige kombinierte Live-Ansicht
            // Nach Recording (Record deaktiviert): Zeige gewählten Track
            if (isRecording || isRecordEnabled)
            {
                // Im Audio-Modus während REC: Kein Live-Tab-Update (Overlay wird angezeigt)
                if (!audioRecordingActive)
                {
                    // MIDI-Recording: zeige kombinierte Live-Aufnahme
                    TabTrack recordedTrack = audioProcessor.getRecordedTabTrack();
                    tabView.setTrack(recordedTrack);
                    tabView.setEditorMode(true);
                }
            }
            // Sonst: Der gewählte Track wird bereits durch trackSelectionChanged() gesetzt
            // Timer soll nicht überschreiben!
        }
        else
        {
            // Keine aufgenommenen Noten - leeren Track anzeigen
            if (!tabView.isEditorMode())
            {
                TabTrack emptyTrack = audioProcessor.getEmptyTabTrack();
                tabView.setTrack(emptyTrack);
                tabView.setEditorMode(true);
            }
        }
        
        // Live-MIDI-Noten aktualisieren (nur wenn kein Audio-Recording-Overlay aktiv)
        if (!audioRecordingActive && !audioTranscribing)
        {
            auto midiNotes = audioProcessor.getLiveMidiNotes();
            std::vector<TabViewComponent::LiveNote> liveNotes;
            for (const auto& note : midiNotes)
            {
                TabViewComponent::LiveNote ln;
                ln.string = note.string;
                ln.fret = note.fret;
                ln.velocity = note.velocity;
                ln.fingerNumber = note.fingerNumber;
                liveNotes.push_back(ln);
            }
            tabView.setLiveNotes(liveNotes);
            tabView.setLiveMutedStrings(audioProcessor.getLiveMutedStrings());
            
            // Erkannten Akkordnamen anzeigen
            tabView.setLiveChordName(audioProcessor.getDetectedChordName());
        }
        
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
        // Auto-Scroll während Playback/Recording im Editor-Modus
        else if (isPlaying && autoScrollButton.getToggleState())
        {
            tabView.scrollToMeasure(currentMeasure);
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

void NewProjectAudioProcessorEditor::saveButtonClicked()
{
    // Popup-Menü mit Format-Auswahl
    juce::PopupMenu menu;
    
    menu.addItem(1, "Save as MIDI...");
    menu.addItem(2, "Save as GuitarPro (.gp5)...");
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&saveButton),
        [this](int result)
        {
            if (result == 1)
                doSaveMidi();
            else if (result == 2)
                doSaveGp5();
        });
}

void NewProjectAudioProcessorEditor::doSaveMidi()
{
    // Prüfe ob Noten vorhanden sind (entweder geladene Datei oder Aufnahmen)
    bool hasNotes = audioProcessor.isFileLoaded() || audioProcessor.hasRecordedNotes();
    if (!hasNotes)
    {
        infoLabel.setText("No notes to save!", juce::dontSendNotification);
        return;
    }
    
    // Erstelle Auswahl-Dialog: Aktuelle Spur oder alle Spuren
    juce::PopupMenu menu;
    
    // Option 1: Nur aktuelle Spur als Einkanal-MIDI
    int currentTrack = audioProcessor.getSelectedTrack();
    juce::String currentTrackName;
    
    if (audioProcessor.isFileLoaded())
    {
        const auto& tracks = audioProcessor.getActiveTracks();
        currentTrackName = (currentTrack >= 0 && currentTrack < tracks.size()) 
            ? tracks[currentTrack].name 
            : "Current Track";
    }
    else
    {
        // Audio-to-Tab Modus: Verwende aufgenommene TabTracks
        auto recordedTracks = audioProcessor.getRecordedTabTracks();
        currentTrackName = (currentTrack >= 0 && currentTrack < (int)recordedTracks.size()) 
            ? recordedTracks[currentTrack].name 
            : "Current Track";
    }
    
    menu.addItem(1, "Current Track: " + currentTrackName + " (Single Channel)");
    menu.addSeparator();
    menu.addItem(2, "All Tracks (Multi-Channel MIDI)");
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&saveButton),
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

void NewProjectAudioProcessorEditor::doSaveGp5()
{
    // Prüfe ob Noten vorhanden sind
    bool hasNotes = audioProcessor.isFileLoaded() || audioProcessor.hasRecordedNotes();
    if (!hasNotes)
    {
        infoLabel.setText("No notes to save!", juce::dontSendNotification);
        return;
    }
    
    // Show export panel for metadata entry
    showExportPanel();
}

void NewProjectAudioProcessorEditor::showExportPanel()
{
    // Get tracks - entweder von geladener Datei oder von Aufnahmen
    std::vector<TabTrack> tracks;
    juce::String defaultTitle = "Untitled";
    
    if (audioProcessor.isFileLoaded())
    {
        // Player-Modus: Konvertiere geladene GP5Tracks zu TabTracks
        const auto& loadedTracks = audioProcessor.getActiveTracks();
        for (size_t i = 0; i < loadedTracks.size(); ++i)
        {
            tracks.push_back(audioProcessor.getGP5Parser().convertToTabTrack(static_cast<int>(i)));
        }
        defaultTitle = audioProcessor.getActiveSongInfo().title;
        if (defaultTitle.isEmpty())
            defaultTitle = "Untitled";
    }
    else
    {
        // Editor-Modus: Verwende editierte Tracks wenn vorhanden, sonst aufgezeichnete
        auto baseTracks = audioProcessor.getRecordedTabTracks();
        for (int i = 0; i < (int)baseTracks.size(); ++i)
        {
            if (audioProcessor.hasEditedTrack(i))
                tracks.push_back(audioProcessor.getEditedTrack(i));
            else
                tracks.push_back(baseTracks[i]);
        }
    }
    
    if (tracks.empty())
    {
        infoLabel.setText("No tracks to export!", juce::dontSendNotification);
        return;
    }
    
    // Create export panel
    exportPanel = std::make_unique<ExportPanelComponent>(
        defaultTitle,
        tracks,
        // Export callback
        [this](const juce::String& title, const std::vector<std::pair<juce::String, int>>& trackData) {
            doExportWithMetadata(title, trackData);
        },
        // Cancel callback
        [this]() {
            hideExportPanel();
        }
    );
    
    // Center the panel - height is calculated by the component itself
    int panelWidth = 600;
    // Use component's preferred height: header(50) + title(35) + tracksLabel(30) + tracks + buttons(60)
    int tracksHeight = juce::jmin((int)tracks.size() * 35, 300);  // Max 300px for tracks
    int panelHeight = 50 + 35 + 30 + tracksHeight + 60;
    exportPanel->setBounds(
        (getWidth() - panelWidth) / 2,
        (getHeight() - panelHeight) / 2,
        panelWidth,
        panelHeight
    );
    
    addAndMakeVisible(*exportPanel);
    exportPanelVisible = true;
    repaint();
}

void NewProjectAudioProcessorEditor::hideExportPanel()
{
    if (exportPanel)
    {
        removeChildComponent(exportPanel.get());
        exportPanel.reset();
    }
    exportPanelVisible = false;
    repaint();
}

void NewProjectAudioProcessorEditor::doExportWithMetadata(const juce::String& title, 
    const std::vector<std::pair<juce::String, int>>& trackData)
{
    hideExportPanel();
    
    // Create file chooser for GP5 save
    midiFileChooser = std::make_unique<juce::FileChooser>(
        "Save as Guitar Pro 5...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(title + ".gp5"),
        "*.gp5");
    
    auto chooserFlags = juce::FileBrowserComponent::saveMode 
                      | juce::FileBrowserComponent::canSelectFiles
                      | juce::FileBrowserComponent::warnAboutOverwriting;
    
    // Store title and track data for use in callback
    juce::String savedTitle = title;
    std::vector<std::pair<juce::String, int>> savedTrackData = trackData;
    
    midiFileChooser->launchAsync(chooserFlags, 
        [this, savedTitle, savedTrackData](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            
            if (file != juce::File{})
            {
                // Ensure file has .gp5 extension
                if (!file.hasFileExtension(".gp5"))
                    file = file.withFileExtension(".gp5");
                
                bool success = audioProcessor.exportRecordingToGP5WithMetadata(
                    file, savedTitle, savedTrackData);
                
                if (success)
                {
                    infoLabel.setText("GP5 saved: " + file.getFileName(), juce::dontSendNotification);
                }
                else
                {
                    infoLabel.setText("Error saving GP5 file!", juce::dontSendNotification);
                }
            }
        });
}

void NewProjectAudioProcessorEditor::noteEditToggled()
{
    bool editingEnabled = noteEditButton.getToggleState();
    tabView.setNoteEditingEnabled(editingEnabled);
    
    // Update bottom bar visibility (controls shown when Note Edit is active)
    updateModeDisplay();
    resized();
    repaint();
    
    if (editingEnabled)
    {
        infoLabel.setText("Note Editing: Click on a note to change its fret/string position", juce::dontSendNotification);
    }
    else
    {
        // Nur Info-Label aktualisieren, NICHT refreshFromProcessor() aufrufen!
        // Das wuerde die aufgezeichneten Noten ueberschreiben.
        if (audioProcessor.isFileLoaded())
        {
            const auto& info = audioProcessor.getActiveSongInfo();
            int trackCount = audioProcessor.getActiveTracks().size();
            int measureCount = audioProcessor.getActiveMeasureHeaders().size();
            juce::String infoText = info.title;
            if (info.artist.isNotEmpty())
                infoText += " - " + info.artist;
            infoText += " | " + juce::String(info.tempo) + " BPM";
            infoText += " | " + juce::String(trackCount) + " Tracks";
            infoText += " | " + juce::String(measureCount) + " Measures";
            infoLabel.setText(infoText, juce::dontSendNotification);
        }
        else if (audioProcessor.hasRecordedNotes())
        {
            infoLabel.setText("Recorded notes - Use Save to export", juce::dontSendNotification);
        }
        else
        {
            infoLabel.setText("No file loaded - Play MIDI to see notes on tab", juce::dontSendNotification);
        }
    }
}

void NewProjectAudioProcessorEditor::reoptimizeAndRefreshNotes()
{
    // Only reoptimize if there are recorded notes
    if (!audioProcessor.hasRecordedNotes())
        return;
    
    // Remember currently selected track
    int currentTrackId = trackSelector.getSelectedId();
    int currentTrackIndex = currentTrackId - 1;  // 0-based
    
    // Check if "All Tracks" is enabled
    bool applyToAllTracks = allTracksCheckbox.getToggleState();
    
    if (applyToAllTracks)
    {
        // Reoptimize all tracks (all MIDI channels)
        audioProcessor.reoptimizeRecordedNotes(-1);
    }
    else
    {
        // Reoptimize only the current track (specific MIDI channel)
        int midiChannel = audioProcessor.getRecordedTrackMidiChannel(currentTrackIndex);
        if (midiChannel > 0)
        {
            audioProcessor.reoptimizeRecordedNotes(midiChannel);
        }
    }
    
    // Re-trigger track selection to refresh the view with recalculated notes
    // This preserves the currently selected track
    if (currentTrackId > 0)
    {
        trackSelectionChanged();
    }
}

//==============================================================================
bool NewProjectAudioProcessorEditor::isBottomBarVisible() const
{
    // Bottom bar is visible when:
    // 1. Editor mode (no file loaded) - always
    // 2. Note Edit mode is active (even with a loaded file)
    if (!audioProcessor.isFileLoaded())
        return true;
    return noteEditButton.getToggleState();
}

void NewProjectAudioProcessorEditor::markSettingsPending()
{
    pendingSettingsChange = true;
    applyButton.setEnabled(true);
    applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFF9800));  // Orange = pending
    repaint();
}

void NewProjectAudioProcessorEditor::applyPendingSettings()
{
    if (!pendingSettingsChange)
        return;
    
    // Determine scope text
    bool applyToAll = allTracksCheckbox.getToggleState();
    juce::String scopeText = applyToAll 
        ? "the ENTIRE song (all tracks)" 
        : "the currently active track";
    
    // Show warning dialog
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Apply Settings")
        .withMessage("This will recalculate " + scopeText + ".\n\n"
                     "All note positions (fret/string assignments) will be re-optimized "
                     "based on the new settings.\n\n"
                     "Do you want to continue?")
        .withButton("Apply")
        .withButton("Cancel");
    
    juce::AlertWindow::showAsync(options, [this](int result) {
        if (result == 1)  // "Apply" clicked
        {
            // Apply the settings
            reoptimizeAndRefreshNotes();
            
            // Reset pending state
            pendingSettingsChange = false;
            applyButton.setEnabled(false);
            applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4CAF50));  // Green = no pending
            repaint();
        }
    });
}