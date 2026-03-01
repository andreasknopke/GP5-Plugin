/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GP5Writer.h"
#include <limits>
#include <algorithm>
#include <map>
#include <set>
#include <functional>

//==============================================================================
// GM Instrument names for track naming (must be defined before use in processBlock)
//==============================================================================
static const char* gmInstrumentNames[] = {
    "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
    "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavi",
    "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
    "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
    "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
    "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
    "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
    "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
    "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
    "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
    "Violin", "Viola", "Cello", "Contrabass",
    "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
    "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
    "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
    "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
    "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
    "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
    "Oboe", "English Horn", "Bassoon", "Clarinet",
    "Piccolo", "Flute", "Recorder", "Pan Flute",
    "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
    "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
    "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
    "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
    "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
    "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
    "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
    "Sitar", "Banjo", "Shamisen", "Koto",
    "Kalimba", "Bag pipe", "Fiddle", "Shanai",
    "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
    "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
    "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
    "Telephone Ring", "Helicopter", "Applause", "Gunshot"
};

//==============================================================================
// Helper: Fix MIDI file format (JUCE writes Format 1 even for single track)
// This corrects the format byte to 0 for single-track MIDI files
//==============================================================================
static bool fixMidiFileFormat(const juce::File& midiFile)
{
    juce::FileInputStream input(midiFile);
    if (!input.openedOk())
        return false;
    
    // Read the file
    juce::MemoryBlock data;
    input.readIntoMemoryBlock(data);
    
    if (data.getSize() < 14)
        return false;
    
    auto* bytes = static_cast<juce::uint8*>(data.getData());
    
    // Verify MIDI header "MThd"
    if (bytes[0] != 'M' || bytes[1] != 'T' || bytes[2] != 'h' || bytes[3] != 'd')
        return false;
    
    // Read format (bytes 8-9, big-endian)
    int format = (bytes[8] << 8) | bytes[9];
    // Read number of tracks (bytes 10-11, big-endian)
    int numTracks = (bytes[10] << 8) | bytes[11];
    
    // If Format 1 with only 1 track, change to Format 0
    if (format == 1 && numTracks == 1)
    {
        DBG("Fixing MIDI format: Format 1 with 1 track -> Format 0");
        bytes[8] = 0;
        bytes[9] = 0;
        
        // Write back to file
        midiFile.deleteFile();
        juce::FileOutputStream output(midiFile);
        if (!output.openedOk())
            return false;
        
        output.write(data.getData(), data.getSize());
        return true;
    }
    
    return true;  // No fix needed
}

//==============================================================================
// Helper: Berechnet die Dauer eines GP5-Beats in Viertelnoten (Quarter Notes)
//==============================================================================
static double getGP5BeatDurationInQuarters(const GP5Beat& beat)
{
    // GP5 duration: -2=whole, -1=half, 0=quarter, 1=eighth, 2=sixteenth, 3=32nd
    // Formel: Dauer in Vierteln = 4 / (2^(duration + 2))
    // -2 -> 4/1 = 4.0 (ganze Note)
    // -1 -> 4/2 = 2.0 (halbe Note)
    //  0 -> 4/4 = 1.0 (Viertelnote)
    //  1 -> 4/8 = 0.5 (Achtelnote)
    //  2 -> 4/16= 0.25 (Sechzehntelnote)
    //  3 -> 4/32= 0.125 (Zweiunddreißigstelnote)
    
    double baseDuration = 4.0 / std::pow(2.0, beat.duration + 2);
    
    // Punktierung: +50% der Dauer
    if (beat.isDotted)
        baseDuration *= 1.5;
    
    // Tuplet (z.B. Triole: 3 Noten in der Zeit von 2)
    if (beat.tupletN > 0)
    {
        // Typische Tuplets: 3 (Triole), 5, 6, 7, etc.
        // Eine Triole bedeutet: 3 Noten in der Zeit von 2
        // Allgemein: N Noten in der Zeit von floor(N * 2/3) für ungerade N
        // Für Standardfälle:
        switch (beat.tupletN)
        {
            case 3:  baseDuration *= (2.0 / 3.0); break;  // Triole: 3 in 2
            case 5:  baseDuration *= (4.0 / 5.0); break;  // Quintole: 5 in 4
            case 6:  baseDuration *= (4.0 / 6.0); break;  // Sextole: 6 in 4
            case 7:  baseDuration *= (4.0 / 7.0); break;  // Septole: 7 in 4
            case 9:  baseDuration *= (8.0 / 9.0); break;  // 9 in 8
            case 10: baseDuration *= (8.0 / 10.0); break; // 10 in 8
            case 11: baseDuration *= (8.0 / 11.0); break; // 11 in 8
            case 12: baseDuration *= (8.0 / 12.0); break; // 12 in 8
            case 13: baseDuration *= (8.0 / 13.0); break; // 13 in 8
            default: break;  // Kein Tuplet oder unbekannt
        }
    }
    
    return baseDuration;
}

//==============================================================================
// Helper: Findet den Beat-Index und die relative Position für eine Beat-Position im Takt
// Gibt den Index des Beats zurück, der bei beatInMeasure aktiv ist
// beatStartTime wird auf die Startzeit des gefundenen Beats gesetzt
//==============================================================================
static int findBeatAtPosition(const juce::Array<GP5Beat>& beats, double beatInMeasure, double& beatStartTime)
{
    double cumulativeTime = 0.0;
    
    for (int i = 0; i < beats.size(); ++i)
    {
        double beatDuration = getGP5BeatDurationInQuarters(beats[i]);
        
        if (beatInMeasure < cumulativeTime + beatDuration)
        {
            beatStartTime = cumulativeTime;
            return i;
        }
        
        cumulativeTime += beatDuration;
    }
    
    // Falls wir über das Ende hinaus sind, letzten Beat zurückgeben
    if (beats.size() > 0)
    {
        beatStartTime = cumulativeTime - getGP5BeatDurationInQuarters(beats[beats.size() - 1]);
        return beats.size() - 1;
    }
    
    beatStartTime = 0.0;
    return 0;
}

//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       // Dummy Main Input (Bus 0 = kMain in VST3) - bleibt deaktiviert
                       // Wird benötigt damit der Sidechain als kAux (Bus 1) registriert wird
                       .withInput ("Input", juce::AudioChannelSet::stereo(), false)
                       // Sidechain Input (Bus 1 = kAux in VST3) - Cubase zeigt nur kAux als Sidechain!
                       .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                     #endif
                       )
#endif
{
    // Initialize all track settings with defaults
    for (int i = 0; i < maxTracks; ++i)
    {
        trackMidiChannels[i].store(i + 1);  // Track 0 -> Channel 1, Track 1 -> Channel 2, etc.
        trackMuted[i].store(false);
        trackSolo[i].store(false);
        trackVolume[i].store(100);  // Default volume
        trackPan[i].store(64);      // Center pan
    }
    
    // Initialize per-track beat tracking
    lastProcessedBeatPerTrack.resize(maxTracks, -1);
    lastProcessedMeasurePerTrack.resize(maxTracks, -1);
    
    // Load chord finger database from embedded BinaryData
    {
        chordFingerDB.loadFromBinaryData(BinaryData::chordfingers_csv, BinaryData::chordfingers_csvSize);
        DBG("ChordFingerDB loaded: " << chordFingerDB.getEntryCount() << " entries from BinaryData");
    }
}

NewProjectAudioProcessor::~NewProjectAudioProcessor()
{
}

//==============================================================================
const juce::String NewProjectAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NewProjectAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NewProjectAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NewProjectAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NewProjectAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NewProjectAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NewProjectAudioProcessor::getProgramName (int index)
{
    return {};
}

void NewProjectAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void NewProjectAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    // Inline MIDI-Generierung - keine externe Engine
    
    // Audio-to-MIDI Processor vorbereiten
    audioToMidiProcessor.prepare(sampleRate, samplesPerBlock);
    
    // Polyphonic Audio Transcriber (BasicPitch) vorbereiten
    audioTranscriber.prepare(sampleRate, samplesPerBlock);
}

void NewProjectAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Output muss Mono oder Stereo sein
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Main Input: Bei Instrumenten (isSynth=true) ist der Main Input oft disabled.
    // Wir erlauben disabled, mono oder stereo.
    auto mainInputSet = layouts.getMainInputChannelSet();
    if (!mainInputSet.isDisabled() 
        && mainInputSet != juce::AudioChannelSet::mono()
        && mainInputSet != juce::AudioChannelSet::stereo())
        return false;

    // Sidechain Input: Prüfe alle Input-Busse (Index > 0 sind Sidechain/Aux)
    // Der Host kann den Sidechain aktivieren oder deaktivieren
    for (int i = 0; i < layouts.inputBuses.size(); ++i)
    {
        auto busSet = layouts.inputBuses[i];
        // Erlaube disabled, mono oder stereo für alle Input-Busse
        if (!busSet.isDisabled() 
            && busSet != juce::AudioChannelSet::mono()
            && busSet != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
  #endif
}
#endif

void NewProjectAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Temporärer Buffer für neue MIDI-Events
    juce::MidiBuffer generatedMidi;

    // =========================================================================
    // DAW Synchronisation - Hole Position vom Host
    // =========================================================================
    if (auto* playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            // Play/Stop Status
            hostIsPlaying.store(posInfo->getIsPlaying());
            
            // Record Status (track record-armed in DAW)
            // Note: getIsRecording() returns bool directly
            hostIsRecording.store(posInfo->getIsRecording());
            
            // Tempo
            if (auto bpm = posInfo->getBpm())
                hostTempo.store(*bpm);
            
            // Position in Beats (Quarter Notes)
            if (auto ppqPosition = posInfo->getPpqPosition())
                hostPositionBeats.store(*ppqPosition);
            
            // Position in Seconds
            if (auto timeInSeconds = posInfo->getTimeInSeconds())
                hostPositionSeconds.store(*timeInSeconds);
            
            // Time Signature
            if (auto timeSig = posInfo->getTimeSignature())
            {
                hostTimeSigNumerator.store(timeSig->numerator);
                hostTimeSigDenominator.store(timeSig->denominator);
            }
        }
    }

    // =========================================================================
    // Auto-detect Input Mode: Sidechain aktiv → Audio, sonst MIDI/Player
    // =========================================================================
    {
        auto* scBus = getBus(true, 1);  // Bus 1 = kAux = Sidechain
        bool sidechainActive = (scBus != nullptr && scBus->isEnabled());
        
        if (sidechainActive)
            inputMode.store(static_cast<int>(InputMode::Audio));
        else if (hasMidiInputActivity)
            inputMode.store(static_cast<int>(InputMode::MIDI));
        else
            inputMode.store(static_cast<int>(InputMode::Player));
    }
    
    // =========================================================================
    // Audio-to-MIDI - Wenn Sidechain aktiv, generiere MIDI aus Sidechain Audio
    // =========================================================================
    if (inputMode.load() == static_cast<int>(InputMode::Audio))
    {
        bool isPlaying = hostIsPlaying.load();
        bool isRecArmed = hostIsRecording.load();
        bool isManualRec = recordingEnabled.load();
        double currentBeat = hostPositionBeats.load();
        bool shouldRecordAudio = isPlaying && currentBeat >= 0.0 && (isRecArmed || isManualRec);
        
        // Sidechain Audio holen
        // Bus 0 = kMain (Dummy, disabled), Bus 1 = kAux (Sidechain)
        auto* sidechainBus = getBus(true, 1);  // Input Bus 1 = kAux = Sidechain
        
        if (sidechainBus != nullptr && sidechainBus->isEnabled())
        {
            auto sidechainBuffer = sidechainBus->getBusBuffer(buffer);
            
            if (sidechainBuffer.getNumChannels() > 0 && sidechainBuffer.getNumSamples() > 0)
            {
                // Monophonic real-time pitch detection (YIN) - nur wenn NICHT im Audio-Recording
                // Während REC+Play wird NUR BasicPitch verwendet (polyphon, nach Stop)
                // Die monophone Live-Erkennung würde unvollständige Noten ins Tab schreiben
                
                // --- Recording START transition: YIN sauber beenden ---
                // Wenn Recording gerade beginnt, aktive YIN-Note per NoteOff beenden
                // und den Detektor resetten, damit keine Ghost-Notes in liveMidiNotes bleiben
                if (shouldRecordAudio && !wasRecordingAudio)
                {
                    if (audioToMidiProcessor.isNoteActive())
                    {
                        midiMessages.addEvent(
                            juce::MidiMessage::noteOff(audioToMidiProcessor.getMidiChannel(),
                                                        audioToMidiProcessor.getCurrentNote()), 0);
                        DBG("YIN: Injected noteOff for active note " << audioToMidiProcessor.getCurrentNote()
                            << " at recording start");
                    }
                    audioToMidiProcessor.reset();
                }
                
                if (!shouldRecordAudio)
                    audioToMidiProcessor.processBlock(sidechainBuffer, midiMessages);
                
                // Polyphonic transcriber: nur bei REC+Play Audio akkumulieren
                if (shouldRecordAudio)
                {
                    // Beim Start der Audio-Aufnahme: Start-Beat merken
                    if (!audioRecordingStartSet)
                    {
                        int numerator = hostTimeSigNumerator.load();
                        int denominator = hostTimeSigDenominator.load();
                        double beatsPerMeasure = numerator * (4.0 / denominator);
                        int currentMeasure = static_cast<int>(currentBeat / beatsPerMeasure);
                        audioRecordingStartBeat = currentMeasure * beatsPerMeasure;
                        audioRecordingStartSet = true;
                        
                        // Auch recordingStartBeat setzen (für Tab-Anzeige)
                        if (!recordingStartSet)
                        {
                            recordingStartBeat = audioRecordingStartBeat;
                            recordingStartSet = true;
                            recordingFretPosition = getFretPosition();
                        }
                        
                        // Vorherige Aufnahme löschen
                        audioTranscriber.clearRecording();
                        
                        DBG("Audio recording started at beat " << currentBeat 
                            << ", measure start: " << audioRecordingStartBeat);
                    }
                    
                    audioTranscriber.pushAudioBlock(sidechainBuffer);
                }
            }
        }
        
        // Stop-Erkennung: Recording war aktiv und jetzt nicht mehr
        // → Alte Live-Noten löschen und BasicPitch Transkription starten
        if (wasRecordingAudio && !shouldRecordAudio)
        {
            // --- Recording STOP transition: YIN-Detektor resetten ---
            // Stale Puffer und State löschen, damit bei Neustart keine falschen Noten kommen
            audioToMidiProcessor.reset();
            DBG("YIN: Reset at recording stop");
            
            if (audioTranscriber.getRecordedDurationSeconds() > 0.1)
            {
                // Vorher aufgenommene Noten löschen (könnten Reste von vorher sein)
                {
                    std::lock_guard<std::mutex> recLock(recordingMutex);
                    recordedNotes.clear();
                    activeRecordingNotes.clear();
                }
                
                DBG("Audio recording stopped - starting BasicPitch transcription ("
                    << juce::String(audioTranscriber.getRecordedDurationSeconds(), 1) << "s audio)");
                audioTranscriber.startTranscription();
            }
            audioRecordingStartSet = false;
        }
        
        wasRecordingAudio = shouldRecordAudio;
        
        // Poll: Wenn Transkription fertig → Noten in Tab einfügen
        if (audioTranscriber.hasResults())
        {
            insertTranscribedNotesIntoTab();
        }
    }

    // =========================================================================
    // MIDI Input - Verarbeite eingehende MIDI-Noten für Tab-Anzeige
    // =========================================================================
    {
        std::lock_guard<std::mutex> lock(liveMidiMutex);
        
        double currentBeat = hostPositionBeats.load();
        bool isPlaying = hostIsPlaying.load();
        bool isRecArmed = hostIsRecording.load();
        bool isManualRec = recordingEnabled.load();
        // Recording: wenn DAW spielt UND (DAW Record-armed ODER manual Record enabled)
        bool shouldRecord = isPlaying && currentBeat >= 0.0 && (isRecArmed || isManualRec);
        int numerator = hostTimeSigNumerator.load();
        int denominator = hostTimeSigDenominator.load();
        
        for (const auto metadata : midiMessages)
        {
            const auto msg = metadata.getMessage();
            
            if (msg.isNoteOn())
            {
                int midiNote = msg.getNoteNumber();
                int velocity = msg.getVelocity();
                
                // Track MIDI input activity for auto-mode detection
                hasMidiInputActivity = true;
                
                LiveMidiNote tabNote = midiNoteToTab(midiNote, velocity);
                liveMidiNotes[midiNote] = tabNote;
                
                // Recording: Start einer neuen Note
                if (shouldRecord)
                {
                    std::lock_guard<std::mutex> recLock(recordingMutex);
                    
                    // Beim ersten Note speichern wir den Start-Beat für die Bar-Synchronisation
                    // Wir runden auf den Anfang des aktuellen Takts
                    if (!recordingStartSet)
                    {
                        double beatsPerMeasure = numerator * (4.0 / denominator);
                        // Finde den Anfang des aktuellen Takts
                        int currentMeasure = static_cast<int>(currentBeat / beatsPerMeasure);
                        recordingStartBeat = currentMeasure * beatsPerMeasure;
                        recordingStartSet = true;
                        // Speichere die aktuelle FretPosition für die Aufnahme
                        recordingFretPosition = getFretPosition();
                        DBG("Recording started at beat " << currentBeat << ", measure start: " << recordingStartBeat << ", fret position: " << static_cast<int>(recordingFretPosition));
                    }
                    
                    RecordedNote recNote;
                    recNote.midiNote = midiNote;
                    recNote.midiChannel = msg.getChannel();
                    recNote.velocity = velocity;
                    recNote.string = tabNote.string;
                    recNote.fret = tabNote.fret;
                    // Quantisiere startBeat auf 1/64-Noten (0.0625 Beats) um Floating-Point-Fehler zu beheben
                    // 7.99887 wird zu 8.0, 8.51234 bleibt ca. 8.5
                    double quantizeGrid = 0.0625;  // 1/64 Note
                    double rawBeat = currentBeat;
                    recNote.startBeat = std::round(currentBeat / quantizeGrid) * quantizeGrid;
                    
                    // Note: We intentionally allow quantization to push notes to the next
                    // bar boundary (e.g. 7.97 -> 8.0). The measure quantization in
                    // getRecordedTabTrack() will handle the bar assignment correctly.
                    // Only prevent pushing BACKWARDS across a bar boundary.
                    {
                        double beatsPerBar = numerator * (4.0 / denominator);
                        if (beatsPerBar > 0.0)
                        {
                            int rawBar = static_cast<int>(rawBeat / beatsPerBar);
                            int quantizedBar = static_cast<int>(recNote.startBeat / beatsPerBar);
                            // If quantization moved the note to a PREVIOUS bar, undo it
                            if (quantizedBar < rawBar)
                                recNote.startBeat += quantizeGrid;
                        }
                    }
                    recNote.endBeat = recNote.startBeat;  // Wird beim Note-Off aktualisiert
                    recNote.isActive = true;
                    
                    // Assign finger number for single notes (Paper: Distance Rule, Little Finger Rule)
                    recNote.fingerNumber = ChordFingerDB::calculateFingerForNote(
                        recNote.fret, recNote.string,
                        lastPlayedFret, lastFingerUsed, lastFingerString);
                    lastFingerUsed = recNote.fingerNumber;
                    lastFingerString = recNote.string;
                    
                    recordedNotes.push_back(recNote);
                    activeRecordingNotes[midiNote] = recordedNotes.size() - 1;
                }
            }
            else if (msg.isNoteOff())
            {
                int midiNote = msg.getNoteNumber();
                liveMidiNotes.erase(midiNote);
                
                // Reset last played position wenn keine Noten mehr gehalten werden
                if (liveMidiNotes.empty())
                {
                    // Statt hartem Reset speichern wir die Zeit
                    lastNoteOffTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                }
                
                // Recording: Note beenden
                if (hostIsRecording.load())
                {
                    std::lock_guard<std::mutex> recLock(recordingMutex);
                    auto it = activeRecordingNotes.find(midiNote);
                    if (it != activeRecordingNotes.end())
                    {
                        if (it->second < recordedNotes.size())
                        {
                            recordedNotes[it->second].endBeat = currentBeat;
                            recordedNotes[it->second].isActive = false;
                        }
                        activeRecordingNotes.erase(it);
                    }
                }
            }
            else if (msg.isPitchWheel())
            {
                if (shouldRecord)
                {
                    std::lock_guard<std::mutex> recLock(recordingMutex);
                    int channel = msg.getChannel();
                    int wheelValue = msg.getPitchWheelValue();
                    
                    // Convert to GP5 1/100 semitones (assume +/- 2 semitone range)
                    // Range 0-16383, center 8192 => +/- 8192 units = +/- 200 cents
                    int bendVal = (int)((wheelValue - 8192.0) / 8192.0 * 200.0);
                    if (std::abs(bendVal) < 10) bendVal = 0; // Noise threshold (10 cents)
                    
                    for (auto& [note, idx] : activeRecordingNotes)
                    {
                        if (idx < recordedNotes.size())
                        {
                            auto& recNote = recordedNotes[idx];
                            if (recNote.isActive && recNote.midiChannel == channel)
                            {
                                // Only store non-zero bend events to keep curve clean
                                if (bendVal != 0 || !recNote.rawBendEvents.empty())
                                    recNote.rawBendEvents.push_back({currentBeat, bendVal});
                                float valSemis = std::abs(bendVal) / 100.0f;
                                if (valSemis > recNote.maxBendValue)
                                    recNote.maxBendValue = valSemis;
                            }
                        }
                    }
                }
            }
            else if (msg.isController())
            {
               if (shouldRecord && msg.getControllerNumber() == 1) // Modulation Wheel
               {
                    std::lock_guard<std::mutex> recLock(recordingMutex);
                    int channel = msg.getChannel();
                    int val = msg.getControllerValue();
                    
                    if (val > 20) // Threshold for Vibrato
                    {
                         for (auto& [note, idx] : activeRecordingNotes)
                        {
                            if (idx < recordedNotes.size())
                            {
                                auto& recNote = recordedNotes[idx];
                                if (recNote.isActive && recNote.midiChannel == channel)
                                   recNote.hasVibrato = true;
                            }
                        }
                    }
               }
            }
            else if (msg.isProgramChange())
            {
                // Store instrument for this channel (Program Change: 0xC0-0xCF)
                // This is processed ALWAYS (not just during playback) because DAWs
                // often send Program Changes when loading a project, before Play
                int channel = msg.getChannel() - 1;  // JUCE channels are 1-based, we use 0-based
                if (channel >= 0 && channel < 16)
                {
                    int instrument = msg.getProgramChangeNumber();
                    channelInstruments[channel] = instrument;
                }
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                liveMidiNotes.clear();
                
                // Reset last played position für Kostenfunktion
                lastNoteOffTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                
                // Recording: Alle aktiven Noten beenden
                if (hostIsRecording.load())
                {
                    std::lock_guard<std::mutex> recLock(recordingMutex);
                    for (auto& [note, idx] : activeRecordingNotes)
                    {
                        if (idx < recordedNotes.size())
                        {
                            recordedNotes[idx].endBeat = currentBeat;
                            recordedNotes[idx].isActive = false;
                        }
                    }
                    activeRecordingNotes.clear();
                }
            }
        }
        
        // =====================================================================
        // Recording: Die string/fret Werte werden bereits korrekt von midiNoteToTab()
        // berechnet und in recordedNotes gespeichert. Keine weitere Optimierung nötig.
        // =====================================================================
    }
    
    // =========================================================================
    // MIDI Output - Mit Echtzeit-Bend-Interpolation
    // =========================================================================
    if (fileLoaded && midiOutputEnabled.load())
    {
        bool isPlaying = hostIsPlaying.load();
        double currentBeat = hostPositionBeats.load();
        
        const auto& tracks = getActiveTracks();
        
        // Stop-Erkennung: Wenn Playback stoppt, alle Noten und Bends beenden
        if (!isPlaying && wasPlaying)
        {
            for (auto& [channel, notes] : activeNotesPerChannel)
            {
                for (int note : notes)
                {
                    generatedMidi.addEvent(juce::MidiMessage::noteOff(channel, note), 0);
                }
                // Reset Pitch Bend auf neutral
                generatedMidi.addEvent(juce::MidiMessage::pitchWheel(channel, 8192), 0);
                notes.clear();
            }
            activeNotesPerChannel.clear();
            activeBendCount = 0;  // Clear all active bends
            
            for (int i = 0; i < maxTracks; ++i)
            {
                lastProcessedBeatPerTrack[i] = -1;
                lastProcessedMeasurePerTrack[i] = -1;
            }
        }
        
        if (isPlaying)
        {
            // =====================================================================
            // STEP 1: Update ALL active bends - real-time pitch bend interpolation
            // This runs on EVERY processBlock call, not just on beat changes!
            // =====================================================================
            for (int b = 0; b < activeBendCount; ++b)
            {
                ActiveBend& bend = activeBends[b];
                
                // Calculate position in bend (0.0 to 1.0)
                double elapsed = currentBeat - bend.startBeat;
                double progress = (bend.durationBeats > 0) ? (elapsed / bend.durationBeats) : 1.0;
                
                // Remove bend if note duration exceeded
                if (progress >= 1.0)
                {
                    // Reset pitch wheel at end of bend
                    generatedMidi.addEvent(juce::MidiMessage::pitchWheel(bend.midiChannel, 8192), 0);
                    // Remove by swapping with last
                    activeBends[b] = activeBends[activeBendCount - 1];
                    activeBendCount--;
                    b--;  // Re-check this index
                    continue;
                }
                
                progress = juce::jlimit(0.0, 1.0, progress);
                
                // Convert progress (0-1) to GP5 position scale (0-60)
                int positionInBend = (int)(progress * 60.0);
                
                // Interpolate bend value from bend points
                int bendValue = 0;  // Value in 1/100 semitones
                int vibratoValue = 0; // 0=none, 1=fast, 2=avg, 3=slow
                
                if (bend.pointCount >= 2)
                {
                    // Find the two surrounding points
                    int prevIdx = 0;
                    int nextIdx = bend.pointCount - 1;
                    
                    for (int i = 0; i < bend.pointCount; ++i)
                    {
                        if (bend.points[i].position <= positionInBend)
                            prevIdx = i;
                    }
                    for (int i = bend.pointCount - 1; i >= 0; --i)
                    {
                        if (bend.points[i].position >= positionInBend)
                            nextIdx = i;
                    }
                    
                    if (nextIdx < prevIdx) nextIdx = prevIdx;
                    
                    const auto& prevPoint = bend.points[prevIdx];
                    const auto& nextPoint = bend.points[nextIdx];
                    
                    // Use vibrato from the starting point of the segment
                    vibratoValue = prevPoint.vibrato;
                    
                    if (prevPoint.position == nextPoint.position)
                    {
                        bendValue = prevPoint.value;
                    }
                    else
                    {
                        // Linear interpolation between points
                        double t = (double)(positionInBend - prevPoint.position) / 
                                   (double)(nextPoint.position - prevPoint.position);
                        t = juce::jlimit(0.0, 1.0, t);
                        bendValue = (int)(prevPoint.value + t * (nextPoint.value - prevPoint.value));
                    }
                }
                else if (bend.pointCount == 1)
                {
                    vibratoValue = bend.points[0].vibrato;
                    
                    // Single point - use progress to interpolate based on bend type
                    if (bend.bendType == 1)  // Normal bend: 0 -> target
                        bendValue = (int)(bend.points[0].value * progress);
                    else if (bend.bendType == 2)  // Bend+Release: 0 -> target -> 0
                    {
                        if (progress < 0.5)
                            bendValue = (int)(bend.points[0].value * (progress * 2.0));
                        else
                            bendValue = (int)(bend.points[0].value * ((1.0 - progress) * 2.0));
                    }
                    else if (bend.bendType == 3 || bend.bendType == 5)  // Release: target -> 0
                        bendValue = (int)(bend.points[0].value * (1.0 - progress));
                    else
                        bendValue = bend.points[0].value;
                }
                else
                {
                    // No points but has bend - use simple interpolation based on type
                    if (bend.bendType == 1)  // Normal bend: 0 -> target
                        bendValue = (int)(bend.maxBendValue * progress);
                    else if (bend.bendType == 2)  // Bend+Release: 0 -> target -> 0
                    {
                        if (progress < 0.5)
                            bendValue = (int)(bend.maxBendValue * (progress * 2.0));
                        else
                            bendValue = (int)(bend.maxBendValue * ((1.0 - progress) * 2.0));
                    }
                    else if (bend.bendType == 3 || bend.bendType == 5)  // Release: target -> 0
                        bendValue = (int)(bend.maxBendValue * (1.0 - progress));
                    else
                        bendValue = bend.maxBendValue;
                }
                
                // === Apply Vibrato Modulation ===
                if (vibratoValue > 0)
                {
                   // Approximate 5Hz vibrato
                    double vibDepth = 0.0; // In 1/100 semitones
                    if (vibratoValue == 1) vibDepth = 25.0;  // +/- 0.25 semi (Fast)
                    else if (vibratoValue == 2) vibDepth = 50.0;  // +/- 0.50 semi (Average)
                    else if (vibratoValue == 3) vibDepth = 100.0; // +/- 1.00 semi (Slow/Wide)
                    
                    // Use measure position for phase to keep it continuous
                    double phase = currentBeat * 30.0; // Approx speed
                    bendValue += (int)(std::sin(phase) * vibDepth);
                }

                // Convert bend value to MIDI pitch wheel
                // ±2 semitone range: 4096 per semitone
                constexpr double unitsPerSemitone = 8192.0 / 2.0;
                int pitchBend = 8192 + (int)((bendValue / 100.0) * unitsPerSemitone);
                pitchBend = juce::jlimit(0, 16383, pitchBend);
                
                // Send if changed (lower threshold for smoother bends)
                if (std::abs(pitchBend - bend.lastSentPitchBend) > 50)
                {
                    generatedMidi.addEvent(juce::MidiMessage::pitchWheel(bend.midiChannel, pitchBend), 0);
                    bend.lastSentPitchBend = pitchBend;
                }
            }
            
            // =====================================================================
            // STEP 2: Process new notes and send MIDI
            // =====================================================================
            
            // Während der Vorzähl-Pause (negative Beats) keine neuen Noten ausgeben
            if (currentBeat < 0.0)
            {
                // Skip note processing during count-in
            }
            else
            {
            bool anySoloActive = hasAnySolo();
            const auto& measureHeaders = getActiveMeasureHeaders();
            
            // Berechne aktuellen Takt
            int measureIndex = 0;
            double measureStartBeat = 0.0;
            double cumulativeBeat = 0.0;
            
            for (int m = 0; m < measureHeaders.size(); ++m)
            {
                double measureLength = measureHeaders[m].numerator * (4.0 / measureHeaders[m].denominator);
                if (currentBeat < cumulativeBeat + measureLength)
                {
                    measureIndex = m;
                    measureStartBeat = cumulativeBeat;
                    break;
                }
                cumulativeBeat += measureLength;
                measureIndex = m;
                measureStartBeat = cumulativeBeat;
            }
            
            double beatInMeasure = currentBeat - measureStartBeat;
            
            // Iteriere über Tracks
            int numTracks = juce::jmin((int)tracks.size(), maxTracks);
            
            for (int trackIdx = 0; trackIdx < numTracks; ++trackIdx)
            {
                bool isMuted = isTrackMuted(trackIdx);
                bool isSolo = isTrackSolo(trackIdx);
                
                if (isMuted || (anySoloActive && !isSolo))
                    continue;
                
                // =============================================================
                // CHECK FOR EDITED TRACK: If the user edited notes (pitch, 
                // duration, position, delete), use the TabTrack data instead
                // of the original parser data for MIDI output.
                // =============================================================
                if (hasEditedTrack(trackIdx))
                {
                    const auto& editTrack = getEditedTrack(trackIdx);
                    int midiChannel = getTrackMidiChannel(trackIdx);
                    int volumeScale = getTrackVolume(trackIdx);
                    int pan = getTrackPan(trackIdx);
                    
                    // Find the measure at the current position
                    // For loaded files, measureIndex is sequential (0-based)
                    if (measureIndex < 0 || measureIndex >= editTrack.measures.size())
                        continue;
                    
                    const auto& etMeasure = editTrack.measures[measureIndex];
                    const auto& etBeats = etMeasure.beats;
                    
                    if (etBeats.size() == 0)
                        continue;
                    
                    // Find current beat using TabBeat duration
                    double etBeatStartTime = 0.0;
                    int etBeatIndex = 0;
                    for (int b = 0; b < etBeats.size(); ++b)
                    {
                        double dur = etBeats[b].getDurationInQuarters();
                        if (beatInMeasure < etBeatStartTime + dur)
                        {
                            etBeatIndex = b;
                            break;
                        }
                        etBeatStartTime += dur;
                        etBeatIndex = b;
                    }
                    
                    if (measureIndex != lastProcessedMeasurePerTrack[trackIdx] ||
                        etBeatIndex != lastProcessedBeatPerTrack[trackIdx])
                    {
                        // Stop all notes on this channel
                        if (activeNotesPerChannel.count(midiChannel))
                        {
                            for (int note : activeNotesPerChannel[midiChannel])
                                generatedMidi.addEvent(juce::MidiMessage::noteOff(midiChannel, note), 0);
                            activeNotesPerChannel[midiChannel].clear();
                        }
                        
                        // Reset pitch wheel if no active bends
                        bool hasActiveBendOnChannel = false;
                        for (int b = 0; b < activeBendCount; ++b)
                        {
                            if (activeBends[b].midiChannel == midiChannel)
                            {
                                hasActiveBendOnChannel = true;
                                break;
                            }
                        }
                        if (!hasActiveBendOnChannel)
                            generatedMidi.addEvent(juce::MidiMessage::pitchWheel(midiChannel, 8192), 0);
                        
                        const auto& etBeat = etBeats[etBeatIndex];
                        
                        if (!etBeat.isRest)
                        {
                            generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 10, pan), 0);
                            
                            // Calculate beat duration for bend/timing
                            double etBeatDuration = etBeat.getDurationInQuarters();
                            
                            for (const auto& tabNote : etBeat.notes)
                            {
                                if (tabNote.fret < 0 || tabNote.isTied)
                                    continue;
                                
                                // Calculate MIDI note
                                int midiNote = 0;
                                if (tabNote.midiNote > 0)
                                    midiNote = tabNote.midiNote;
                                else if (tabNote.string >= 0 && tabNote.string < editTrack.tuning.size())
                                    midiNote = editTrack.tuning[tabNote.string] + tabNote.fret;
                                else if (tabNote.string < 6)
                                {
                                    const int defaultTuning[] = { 64, 59, 55, 50, 45, 40 };
                                    midiNote = defaultTuning[tabNote.string] + tabNote.fret;
                                }
                                
                                if (midiNote <= 0 || midiNote >= 128)
                                    continue;
                                
                                // Velocity with effects
                                int velocity = tabNote.velocity > 0 ? tabNote.velocity : 95;
                                if (tabNote.effects.ghostNote) velocity = 50;
                                if (tabNote.effects.accentuatedNote) velocity = 115;
                                if (tabNote.effects.heavyAccentuatedNote) velocity = 127;
                                if (tabNote.effects.hammerOn) velocity = juce::jmax(50, velocity - 15);
                                velocity = (velocity * volumeScale) / 100;
                                velocity = juce::jlimit(1, 127, velocity);
                                
                                // Expression controllers
                                if (tabNote.effects.vibrato || tabNote.effects.wideVibrato)
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 1, 80), 0);
                                if (tabNote.effects.hammerOn)
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 68, 127), 0);
                                if (tabNote.effects.slideType != SlideType::None)
                                {
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 65, 127), 0);
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 5, 64), 0);
                                }
                                
                                // Bend handling
                                if (tabNote.effects.bend && tabNote.effects.bendValue != 0.0f)
                                {
                                    int bendVal100 = (int)(tabNote.effects.bendValue * 100.0f);
                                    constexpr double unitsPerSemitone = 8192.0 / 2.0;
                                    int maxPitchBend = 8192 + (int)((bendVal100 / 100.0) * unitsPerSemitone);
                                    maxPitchBend = juce::jlimit(0, 16383, maxPitchBend);
                                    int initialPitchBend = 8192;
                                    
                                    switch (tabNote.effects.bendType)
                                    {
                                        case 1: case 2: initialPitchBend = 8192; break;
                                        case 3: case 5: initialPitchBend = maxPitchBend; break;
                                        case 4: initialPitchBend = maxPitchBend; break;
                                        default: initialPitchBend = 8192; break;
                                    }
                                    
                                    generatedMidi.addEvent(juce::MidiMessage::pitchWheel(midiChannel, initialPitchBend), 0);
                                    
                                    if (tabNote.effects.bendType != 4 && activeBendCount < maxActiveBends)
                                    {
                                        ActiveBend& newBend = activeBends[activeBendCount++];
                                        newBend.midiChannel = midiChannel;
                                        newBend.midiNote = midiNote;
                                        newBend.startBeat = currentBeat;
                                        newBend.durationBeats = etBeatDuration;
                                        newBend.bendType = tabNote.effects.bendType;
                                        newBend.maxBendValue = bendVal100;
                                        newBend.pointCount = std::min((int)tabNote.effects.bendPoints.size(), 16);
                                        for (int i = 0; i < newBend.pointCount; ++i)
                                        {
                                            newBend.points[i].position = tabNote.effects.bendPoints[i].position;
                                            newBend.points[i].value = tabNote.effects.bendPoints[i].value;
                                            newBend.points[i].vibrato = tabNote.effects.bendPoints[i].vibrato;
                                        }
                                        newBend.lastSentPitchBend = initialPitchBend;
                                    }
                                }
                                
                                // Note On
                                generatedMidi.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity), 0);
                                activeNotesPerChannel[midiChannel].insert(midiNote);
                                
                                // Track note end time
                                double tempo = hostTempo.load();
                                if (tempo <= 0.0) tempo = 120.0;
                                double noteDurationMs = etBeatDuration * 60000.0 / tempo;
                                double noteEndTime = juce::Time::getMillisecondCounterHiRes() + noteDurationMs;
                                trackNoteEndTime[trackIdx].store(noteEndTime);
                            }
                        }
                        
                        lastProcessedMeasurePerTrack[trackIdx] = measureIndex;
                        lastProcessedBeatPerTrack[trackIdx] = etBeatIndex;
                    }
                    
                    continue;  // Skip original GP5Track processing for this track
                }
                
                const auto& track = tracks[trackIdx];
                int midiChannel = getTrackMidiChannel(trackIdx);
                int volumeScale = getTrackVolume(trackIdx);
                int pan = getTrackPan(trackIdx);
                
                if (measureIndex < 0 || measureIndex >= (int)track.measures.size())
                    continue;
                
                const auto& measure = track.measures[measureIndex];
                const auto& beats = measure.voice1;
                
                if (beats.size() == 0)
                    continue;
                
                double beatStartTime = 0.0;
                int beatIndex = findBeatAtPosition(beats, beatInMeasure, beatStartTime);
                beatIndex = juce::jlimit(0, (int)beats.size() - 1, beatIndex);
                
                if (measureIndex != lastProcessedMeasurePerTrack[trackIdx] || 
                    beatIndex != lastProcessedBeatPerTrack[trackIdx])
                {
                    // Alle Noten auf diesem Kanal stoppen
                    if (activeNotesPerChannel.count(midiChannel))
                    {
                        for (int note : activeNotesPerChannel[midiChannel])
                        {
                            generatedMidi.addEvent(juce::MidiMessage::noteOff(midiChannel, note), 0);
                        }
                        activeNotesPerChannel[midiChannel].clear();
                    }
                    
                    // Reset pitch wheel before new notes (only if no active bends for this channel)
                    bool hasActiveBendOnChannel = false;
                    for (int b = 0; b < activeBendCount; ++b)
                    {
                        if (activeBends[b].midiChannel == midiChannel)
                        {
                            hasActiveBendOnChannel = true;
                            break;
                        }
                    }
                    if (!hasActiveBendOnChannel)
                    {
                        generatedMidi.addEvent(juce::MidiMessage::pitchWheel(midiChannel, 8192), 0);
                    }
                    
                    const auto& beat = beats[beatIndex];
                    
                    // Calculate beat duration in quarter notes
                    double beatDurationBeats = 4.0 / std::pow(2.0, beat.duration + 2);
                    if (beat.isDotted) beatDurationBeats *= 1.5;
                    if (beat.tupletN > 0)
                    {
                        int tupletDenom = (beat.tupletN == 3) ? 2 : (beat.tupletN == 5 || beat.tupletN == 6) ? 4 : beat.tupletN - 1;
                        beatDurationBeats = beatDurationBeats * tupletDenom / beat.tupletN;
                    }
                    
                    if (!beat.isRest)
                    {
                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 10, pan), 0);
                        
                        // Iteriere sicher über die Noten
                        for (auto it = beat.notes.begin(); it != beat.notes.end(); ++it)
                        {
                            int stringIndex = it->first;
                            const auto& gpNote = it->second;
                            
                            if (gpNote.isDead || gpNote.isTied)
                                continue;
                            
                            if (stringIndex < 0 || stringIndex >= 12)
                                continue;
                            
                            // MIDI-Note berechnen
                            int midiNote = 0;
                            int tuningSize = track.tuning.size();
                            if (tuningSize > 0 && stringIndex < tuningSize)
                            {
                                midiNote = track.tuning[stringIndex] + gpNote.fret;
                            }
                            else if (stringIndex < 6)
                            {
                                const int defaultTuning[] = { 64, 59, 55, 50, 45, 40 };
                                midiNote = defaultTuning[stringIndex] + gpNote.fret;
                            }
                            
                            if (midiNote <= 0 || midiNote >= 128)
                                continue;
                            
                            // Velocity
                            int velocity = gpNote.velocity > 0 ? gpNote.velocity : 95;
                            if (gpNote.isGhost) velocity = 50;
                            if (gpNote.hasAccent) velocity = 115;
                            if (gpNote.hasHeavyAccent) velocity = 127;
                            if (gpNote.hasHammerOn) velocity = juce::jmax(50, velocity - 15);
                            velocity = (velocity * volumeScale) / 100;
                            velocity = juce::jlimit(1, 127, velocity);
                            
                            // Expression Controllers
                            if (gpNote.hasVibrato)
                                generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 1, 80), 0);
                            
                            if (gpNote.hasHammerOn)
                                generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 68, 127), 0);
                            
                            if (gpNote.hasSlide)
                            {
                                generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 65, 127), 0);
                                generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 5, 64), 0);
                            }
                            
                            // =========================================================
                            // BEND HANDLING - Create ActiveBend for real-time interpolation
                            // =========================================================
                            if (gpNote.hasBend && gpNote.bendValue != 0)
                            {
                                // Calculate initial pitch based on bend type
                                int initialPitchBend = 8192;
                                constexpr double unitsPerSemitone = 8192.0 / 2.0;  // ±2 semitone range
                                int maxPitchBend = 8192 + (int)((gpNote.bendValue / 100.0) * unitsPerSemitone);
                                maxPitchBend = juce::jlimit(0, 16383, maxPitchBend);
                                
                                switch (gpNote.bendType)
                                {
                                    case 1:  // Normal Bend: Start at 0, bend UP to target
                                        initialPitchBend = 8192;  // Start at original pitch
                                        break;
                                        
                                    case 2:  // Bend and Release: Start at 0, bend up, back to 0
                                        initialPitchBend = 8192;  // Start at original pitch
                                        break;
                                        
                                    case 3:  // Release: Start at target, release to 0
                                    case 5:  // Pre-Bend and Release: Start at target, release to 0
                                        initialPitchBend = maxPitchBend;  // Start bent
                                        break;
                                        
                                    case 4:  // Pre-Bend: Static at target pitch (no interpolation needed)
                                        initialPitchBend = maxPitchBend;
                                        break;
                                        
                                    default:
                                        initialPitchBend = 8192;
                                        break;
                                }
                                
                                // Send initial pitch bend
                                generatedMidi.addEvent(juce::MidiMessage::pitchWheel(midiChannel, initialPitchBend), 0);
                                
                                // Add to active bends for real-time interpolation (not for static pre-bend)
                                if (gpNote.bendType != 4 && activeBendCount < maxActiveBends)
                                {
                                    ActiveBend& newBend = activeBends[activeBendCount++];
                                    newBend.midiChannel = midiChannel;
                                    newBend.midiNote = midiNote;
                                    newBend.startBeat = currentBeat;
                                    newBend.durationBeats = beatDurationBeats;
                                    newBend.bendType = gpNote.bendType;
                                    newBend.maxBendValue = gpNote.bendValue;
                                    
                                    // Manual copy to fixed array for thread safety
                                    newBend.pointCount = std::min((int)gpNote.bendPoints.size(), 16);
                                    for (int i = 0; i < newBend.pointCount; ++i)
                                    {
                                        newBend.points[i] = gpNote.bendPoints[i];
                                    }
                                    
                                    newBend.lastSentPitchBend = initialPitchBend;
                                }
                            }
                            
                            // Note On
                            generatedMidi.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity), 0);
                            activeNotesPerChannel[midiChannel].insert(midiNote);
                            
                            // Mark track as playing - calculate note end time in milliseconds
                            double tempo = hostTempo.load();
                            if (tempo <= 0.0) tempo = 120.0;  // Fallback
                            double noteDurationMs = beatDurationBeats * 60000.0 / tempo;
                            double noteEndTime = juce::Time::getMillisecondCounterHiRes() + noteDurationMs;
                            trackNoteEndTime[trackIdx].store(noteEndTime);
                        }
                    }
                    
                    lastProcessedMeasurePerTrack[trackIdx] = measureIndex;
                    lastProcessedBeatPerTrack[trackIdx] = beatIndex;
                }
            }
            }  // Ende des else-Blocks für currentBeat >= 0
        }
        
        wasPlaying = isPlaying;
        lastProcessedBeat = currentBeat;
    }
    // =========================================================================
    // MIDI Output for Recorded Notes (Editor Mode - no file loaded)
    // =========================================================================
    else if (!fileLoaded && midiOutputEnabled.load())
    {
        bool isPlaying = hostIsPlaying.load();
        double currentBeat = hostPositionBeats.load();
        
        // Stop-Erkennung: Wenn Playback stoppt, alle Noten beenden
        if (!isPlaying && wasPlaying)
        {
            for (int note : activePlaybackNotes)
            {
                generatedMidi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
            }
            activePlaybackNotes.clear();
            lastPlaybackBeat = -1.0;
            lastProcessedRecMeasure = -1;
            lastProcessedRecBeat = -1;
        }
        
        if (isPlaying && currentBeat >= 0.0)
        {
            int selTrack = selectedTrackIndex.load();
            bool useEditedTrack = hasEditedTrack(selTrack);
            
            if (useEditedTrack)
            {
                // =============================================================
                // EDITED TRACK PLAYBACK: Walk through TabTrack measures/beats
                // This respects rest deletion and duration edits!
                // =============================================================
                const auto& editTrack = getEditedTrack(selTrack);
                
                int numerator = hostTimeSigNumerator.load();
                int denominator = hostTimeSigDenominator.load();
                double beatsPerMeasure = numerator * (4.0 / denominator);
                
                // Calculate current measure index and beat-in-measure
                // The TabTrack measures use measureNumber (1-based DAW bar numbers)
                // and ppqPosition maps to bar as: bar = floor(ppq / beatsPerMeasure) + 1
                int currentBar = (int)(currentBeat / beatsPerMeasure) + 1;
                double beatInMeasure = currentBeat - (currentBar - 1) * beatsPerMeasure;
                
                // Find the measure in the editedTrack that matches the current bar
                int measureIdx = -1;
                for (int m = 0; m < editTrack.measures.size(); ++m)
                {
                    if (editTrack.measures[m].measureNumber == currentBar)
                    {
                        measureIdx = m;
                        break;
                    }
                }
                
                if (measureIdx >= 0 && measureIdx < editTrack.measures.size())
                {
                    const auto& measure = editTrack.measures[measureIdx];
                    const auto& beats = measure.beats;
                    
                    if (beats.size() > 0)
                    {
                        // Find which beat we're at based on beat-in-measure position
                        double beatStartTime = 0.0;
                        int beatIndex = 0;
                        for (int b = 0; b < beats.size(); ++b)
                        {
                            double dur = beats[b].getDurationInQuarters();
                            if (beatInMeasure < beatStartTime + dur)
                            {
                                beatIndex = b;
                                break;
                            }
                            beatStartTime += dur;
                            beatIndex = b;
                        }
                        
                        // Only process when beat changes (same logic as file-based playback)
                        if (measureIdx != lastProcessedRecMeasure || beatIndex != lastProcessedRecBeat)
                        {
                            // Stop all currently playing notes
                            for (int note : activePlaybackNotes)
                            {
                                generatedMidi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
                            }
                            activePlaybackNotes.clear();
                            
                            const auto& beat = beats[beatIndex];
                            
                            if (!beat.isRest)
                            {
                                // Play all notes in this beat
                                for (const auto& tabNote : beat.notes)
                                {
                                    if (tabNote.fret < 0)
                                        continue;  // Empty string slot
                                    
                                    if (tabNote.isTied)
                                        continue;
                                    
                                    // Calculate MIDI note from string + fret + tuning
                                    int midiNote = 0;
                                    if (tabNote.midiNote > 0)
                                    {
                                        midiNote = tabNote.midiNote;
                                    }
                                    else if (tabNote.string >= 0 && tabNote.string < editTrack.tuning.size())
                                    {
                                        midiNote = editTrack.tuning[tabNote.string] + tabNote.fret;
                                    }
                                    else if (tabNote.string < 6)
                                    {
                                        const int defaultTuning[] = { 64, 59, 55, 50, 45, 40 };
                                        midiNote = defaultTuning[tabNote.string] + tabNote.fret;
                                    }
                                    
                                    if (midiNote <= 0 || midiNote >= 128)
                                        continue;
                                    
                                    int velocity = juce::jlimit(1, 127, tabNote.velocity > 0 ? tabNote.velocity : 95);
                                    generatedMidi.addEvent(juce::MidiMessage::noteOn(1, midiNote, (juce::uint8)velocity), 0);
                                    activePlaybackNotes.insert(midiNote);
                                }
                            }
                            // If beat.isRest, we just stopped old notes and don't start new ones
                            
                            lastProcessedRecMeasure = measureIdx;
                            lastProcessedRecBeat = beatIndex;
                        }
                    }
                }
                
                lastPlaybackBeat = currentBeat - recordingStartBeat;
            }
            else
            {
                // =============================================================
                // RAW RECORDED NOTES PLAYBACK (fallback, no edits applied)
                // =============================================================
                std::lock_guard<std::mutex> lock(recordingMutex);
                
                if (!recordedNotes.empty() && recordingStartSet)
                {
                    // Berechne relative Position zu Recording-Start
                    double relativeBeat = currentBeat - recordingStartBeat;
                    
                    // Prüfe welche Noten starten oder enden sollen
                    for (const auto& note : recordedNotes)
                    {
                        if (note.isActive)
                            continue;  // Noch nicht beendet
                        
                        double noteStartRel = note.startBeat - recordingStartBeat;
                        double noteEndRel = note.endBeat - recordingStartBeat;
                        
                        // Note sollte starten
                        if (relativeBeat >= noteStartRel && lastPlaybackBeat < noteStartRel)
                        {
                            if (activePlaybackNotes.find(note.midiNote) == activePlaybackNotes.end())
                            {
                                int velocity = juce::jlimit(1, 127, note.velocity);
                                generatedMidi.addEvent(juce::MidiMessage::noteOn(1, note.midiNote, (juce::uint8)velocity), 0);
                                activePlaybackNotes.insert(note.midiNote);
                            }
                        }
                        
                        // Note sollte enden
                        if (relativeBeat >= noteEndRel && lastPlaybackBeat < noteEndRel)
                        {
                            if (activePlaybackNotes.find(note.midiNote) != activePlaybackNotes.end())
                            {
                                generatedMidi.addEvent(juce::MidiMessage::noteOff(1, note.midiNote), 0);
                                activePlaybackNotes.erase(note.midiNote);
                            }
                        }
                    }
                    
                    lastPlaybackBeat = relativeBeat;
                }
            }
        }
        
        wasPlaying = isPlaying;
    }
    
    // Generierte MIDI-Events zum Output hinzufügen
    midiMessages.addEvents(generatedMidi, 0, buffer.getNumSamples(), 0);

    // Dieses Plugin ist ein MIDI-Generator (Synth/Instrument).
    // Es soll KEIN Audio durchleiten - weder vom Main Input noch vom Sidechain.
    // Ohne diesen Clear würde das Sidechain-Audio (z.B. Gitarren-Recording)
    // im Output-Buffer verbleiben und zusammen mit dem MIDI-Output abgespielt werden,
    // was zu einem "doppelten" Klang führt.
    for (auto i = 0; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
}

//==============================================================================
bool NewProjectAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor()
{
    return new NewProjectAudioProcessorEditor (*this);
}

//==============================================================================
void NewProjectAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Speichere den Dateipfad der geladenen GP5-Datei und UI-Zustand
    juce::ValueTree state ("GP5PluginState");
    state.setProperty ("filePath", loadedFilePath, nullptr);
    state.setProperty ("selectedTrack", selectedTrackIndex.load(), nullptr);
    state.setProperty ("autoScroll", autoScrollEnabled.load(), nullptr);
    state.setProperty ("fretPosition", static_cast<int>(fretPosition.load()), nullptr);
    state.setProperty ("positionLookahead", positionLookahead.load(), nullptr);
    
    // Speichere Track-MIDI-Einstellungen
    juce::ValueTree trackSettings ("TrackSettings");
    for (int i = 0; i < maxTracks; ++i)
    {
        juce::ValueTree track ("Track");
        track.setProperty ("index", i, nullptr);
        track.setProperty ("midiChannel", trackMidiChannels[i].load(), nullptr);
        track.setProperty ("muted", trackMuted[i].load(), nullptr);
        track.setProperty ("solo", trackSolo[i].load(), nullptr);
        track.setProperty ("volume", trackVolume[i].load(), nullptr);
        track.setProperty ("pan", trackPan[i].load(), nullptr);
        trackSettings.appendChild (track, nullptr);
    }
    state.appendChild (trackSettings, nullptr);
    
    // Speichere aufgezeichnete Noten
    {
        std::lock_guard<std::mutex> lock(recordingMutex);
        if (!recordedNotes.empty())
        {
            juce::ValueTree recNotesTree ("RecordedNotes");
            recNotesTree.setProperty ("startBeat", recordingStartBeat, nullptr);
            recNotesTree.setProperty ("startSet", recordingStartSet, nullptr);
            
            for (const auto& note : recordedNotes)
            {
                juce::ValueTree noteTree ("Note");
                noteTree.setProperty ("midiNote", note.midiNote, nullptr);
                noteTree.setProperty ("velocity", note.velocity, nullptr);
                noteTree.setProperty ("string", note.string, nullptr);
                noteTree.setProperty ("fret", note.fret, nullptr);
                noteTree.setProperty ("startBeat", note.startBeat, nullptr);
                noteTree.setProperty ("endBeat", note.endBeat, nullptr);
                noteTree.setProperty ("midiChannel", note.midiChannel, nullptr);
                recNotesTree.appendChild (noteTree, nullptr);
            }
            state.appendChild (recNotesTree, nullptr);
        }
    }
    
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void NewProjectAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Lade den Dateipfad und parse die Datei erneut
    juce::ValueTree state = juce::ValueTree::readFromData (data, static_cast<size_t>(sizeInBytes));
    
    if (state.isValid() && state.hasType ("GP5PluginState"))
    {
        juce::String filePath = state.getProperty ("filePath", "").toString();
        
        if (filePath.isNotEmpty())
        {
            juce::File file (filePath);
            if (file.existsAsFile())
            {
                loadGP5File (file);
            }
        }
        
        // Lade UI-Zustand
        savedSelectedTrack = state.getProperty ("selectedTrack", 0);
        autoScrollEnabled.store(state.getProperty ("autoScroll", true));
        
        // Lade Fret Position
        int fretPosInt = state.getProperty ("fretPosition", 1);  // Default: Mid
        fretPosition.store(fretPosInt);
        
        // Lade Position Lookahead
        int posLookahead = state.getProperty ("positionLookahead", 4);  // Default: 4
        positionLookahead.store(posLookahead);
        
        // Lade Track-MIDI-Einstellungen
        juce::ValueTree trackSettings = state.getChildWithName ("TrackSettings");
        if (trackSettings.isValid())
        {
            for (int i = 0; i < trackSettings.getNumChildren(); ++i)
            {
                juce::ValueTree track = trackSettings.getChild (i);
                int trackIndex = track.getProperty ("index", -1);
                
                if (trackIndex >= 0 && trackIndex < maxTracks)
                {
                    trackMidiChannels[trackIndex].store (track.getProperty ("midiChannel", trackIndex + 1));
                    trackMuted[trackIndex].store (track.getProperty ("muted", false));
                    trackSolo[trackIndex].store (track.getProperty ("solo", false));
                    trackVolume[trackIndex].store (track.getProperty ("volume", 100));
                    trackPan[trackIndex].store (track.getProperty ("pan", 64));
                }
            }
        }
        
        // Lade aufgezeichnete Noten
        juce::ValueTree recNotesTree = state.getChildWithName ("RecordedNotes");
        if (recNotesTree.isValid())
        {
            std::lock_guard<std::mutex> lock(recordingMutex);
            recordedNotes.clear();
            
            recordingStartBeat = recNotesTree.getProperty ("startBeat", 0.0);
            recordingStartSet = recNotesTree.getProperty ("startSet", false);
            
            for (int i = 0; i < recNotesTree.getNumChildren(); ++i)
            {
                juce::ValueTree noteTree = recNotesTree.getChild (i);
                RecordedNote note;
                note.midiNote = noteTree.getProperty ("midiNote", 0);
                note.velocity = noteTree.getProperty ("velocity", 100);
                note.string = noteTree.getProperty ("string", 0);
                note.fret = noteTree.getProperty ("fret", 0);
                note.startBeat = noteTree.getProperty ("startBeat", 0.0);
                note.endBeat = noteTree.getProperty ("endBeat", 0.0);
                note.midiChannel = noteTree.getProperty ("midiChannel", 1);
                note.isActive = false;
                recordedNotes.push_back(note);
            }
            
            DBG("Loaded " << recordedNotes.size() << " recorded notes from state");
        }
    }
}

void NewProjectAudioProcessor::unloadFile()
{
    fileLoaded = false;
    loadedFilePath = "";
    usingGP7Parser = false;
    usingPTBParser = false;
    usingMidiImporter = false;
    
    // Reset all track settings
    for (int i = 0; i < maxTracks; ++i)
    {
        lastProcessedBeatPerTrack[i] = -1;
        lastProcessedMeasurePerTrack[i] = -1;
        trackMuted[i].store(false);
        trackSolo[i].store(false);
    }
    
    // Clear active notes and bends
    activeBendCount = 0;
    activeNotesPerChannel.clear();
    
    // Clear seek position
    clearSeekPosition();
    
    // Clear edited tracks
    editedTracks.clear();
    
    DBG("Processor: File unloaded");
}

bool NewProjectAudioProcessor::loadGP5File(const juce::File& file)
{
    // Check file extension to determine which parser to use
    auto extension = file.getFileExtension().toLowerCase();
    
    // MIDI file import
    if (extension == ".mid" || extension == ".midi")
    {
        if (midiImporter.parseFile(file))
        {
            loadedFilePath = file.getFullPathName();
            fileLoaded = true;
            usingGP7Parser = false;
            usingPTBParser = false;
            usingMidiImporter = true;
            
            // Initialize track settings based on imported MIDI
            initializeTrackSettings();
            
            DBG("Processor: MIDI file loaded successfully: " << loadedFilePath);
            return true;
        }
        else
        {
            fileLoaded = false;
            DBG("Processor: Failed to load MIDI file: " << midiImporter.getLastError());
            return false;
        }
    }
    
    // Try GP7/8 parser for .gp files (ZIP-based format)
    if (extension == ".gp")
    {
        if (gp7Parser.parseFile(file))
        {
            loadedFilePath = file.getFullPathName();
            fileLoaded = true;
            usingGP7Parser = true;
            usingPTBParser = false;
            usingMidiImporter = false;
            
            // Initialize track settings based on loaded file
            initializeTrackSettings();
            
            DBG("Processor: GP7/8 file loaded successfully: " << loadedFilePath);
            return true;
        }
        else
        {
            fileLoaded = false;
            DBG("Processor: Failed to load GP7/8 file: " << gp7Parser.getLastError());
            return false;
        }
    }
    
    // Use GP5 parser for .gp3, .gp4, .gp5, .gpx files
    if (extension == ".ptb")
    {
        if (ptbParser.parse(file))
        {
            loadedFilePath = file.getFullPathName();
            fileLoaded = true;
            usingGP7Parser = false;
            usingPTBParser = true;
            usingMidiImporter = false;
            
            // Initialize track settings based on loaded file
            initializeTrackSettings();
            
            DBG("Processor: PTB file loaded successfully: " << loadedFilePath);
            return true;
        }
        else
        {
            fileLoaded = false;
            DBG("Processor: Failed to load PTB file: " << ptbParser.getLastError());
            return false;
        }
    }
    
    // Use GP5 parser for .gp3, .gp4, .gp5, .gpx files
    if (gp5Parser.parse(file))
    {
        loadedFilePath = file.getFullPathName();
        fileLoaded = true;
        usingGP7Parser = false;
        usingPTBParser = false;
        usingMidiImporter = false;
        
        // Initialize track settings based on loaded file
        initializeTrackSettings();
        
        DBG("Processor: GP5 file loaded successfully: " << loadedFilePath);
        return true;
    }
    else
    {
        fileLoaded = false;
        DBG("Processor: Failed to load GP5 file: " << gp5Parser.getLastError());
        return false;
    }
}

void NewProjectAudioProcessor::initializeTrackSettings()
{
    const auto& tracks = getActiveTracks();
    
    for (int i = 0; i < juce::jmin((int)tracks.size(), maxTracks); ++i)
    {
        const auto& track = tracks[i];
        
        // Use MIDI channel from GP5 file, or assign sequentially
        int channel = track.midiChannel;
        if (channel < 1 || channel > 16)
            channel = (i % 16) + 1;
        
        // Drums typically use channel 10
        if (track.isPercussion)
            channel = 10;
        
        trackMidiChannels[i].store(channel);
        trackMuted[i].store(false);
        trackSolo[i].store(false);
        trackVolume[i].store(track.volume > 0 ? track.volume : 100);
        trackPan[i].store(track.pan >= 0 ? track.pan : 64);
    }
    
    // Reset beat tracking
    for (int i = 0; i < maxTracks; ++i)
    {
        lastProcessedBeatPerTrack[i] = -1;
        lastProcessedMeasurePerTrack[i] = -1;
    }
    
    // Clear all active notes
    activeNotesPerChannel.clear();
    activeNotes.clear();
    
    DBG("Track settings initialized for " << tracks.size() << " tracks");
}

int NewProjectAudioProcessor::getCurrentMeasureIndex() const
{
    if (!fileLoaded)
        return 0;
    
    double positionInBeats = hostPositionBeats.load();
    
    // Bei negativen Beats (Vorzähl-Pause / Count-in) immer Takt 0 zurückgeben
    if (positionInBeats < 0.0)
        return 0;
    
    // Verwende GP-Taktstruktur für konsistente Anzeige mit MIDI-Ausgabe
    const auto& measureHeaders = getActiveMeasureHeaders();
    
    if (measureHeaders.isEmpty())
        return 0;
    
    // Iteriere durch alle Takte und finde den aktuellen basierend auf kumulativen Beats
    double cumulativeBeat = 0.0;
    
    for (int m = 0; m < (int)measureHeaders.size(); ++m)
    {
        // Taktlänge in Viertelnoten: numerator * (4.0 / denominator)
        double measureLength = measureHeaders[m].numerator * (4.0 / measureHeaders[m].denominator);
        
        if (positionInBeats < cumulativeBeat + measureLength)
        {
            return m;
        }
        cumulativeBeat += measureLength;
    }
    
    // Falls wir am Ende sind, letzten Takt zurückgeben
    return juce::jmax(0, (int)measureHeaders.size() - 1);
}

double NewProjectAudioProcessor::getPositionInCurrentMeasure() const
{
    if (!fileLoaded)
        return 0.0;
    
    double positionInBeats = hostPositionBeats.load();
    
    // Bei negativen Beats (Vorzähl-Pause / Count-in) immer Position 0.0 zurückgeben
    if (positionInBeats < 0.0)
        return 0.0;
    
    // Verwende GP-Taktstruktur für konsistente Anzeige
    const auto& measureHeaders = getActiveMeasureHeaders();
    
    if (measureHeaders.isEmpty())
        return 0.0;
    
    // Finde den aktuellen Takt und berechne Position darin
    double cumulativeBeat = 0.0;
    
    for (int m = 0; m < (int)measureHeaders.size(); ++m)
    {
        double measureLength = measureHeaders[m].numerator * (4.0 / measureHeaders[m].denominator);
        
        if (positionInBeats < cumulativeBeat + measureLength)
        {
            // Position innerhalb dieses Taktes (0.0 - 1.0)
            double beatInMeasure = positionInBeats - cumulativeBeat;
            return juce::jlimit(0.0, 1.0, beatInMeasure / measureLength);
        }
        cumulativeBeat += measureLength;
    }
    
    return 1.0;  // Am Ende
}

std::pair<int, int> NewProjectAudioProcessor::getGP5TimeSignature(int measureIndex) const
{
    const auto& measureHeaders = getActiveMeasureHeaders();
    
    if (measureIndex >= 0 && measureIndex < (int)measureHeaders.size())
    {
        return { measureHeaders[measureIndex].numerator, measureHeaders[measureIndex].denominator };
    }
    
    // Default: 4/4
    return { 4, 4 };
}

int NewProjectAudioProcessor::getGP5Tempo() const
{
    return getActiveSongInfo().tempo;
}

bool NewProjectAudioProcessor::isTimeSignatureMatching() const
{
    if (!fileLoaded)
        return true;
    
    int currentMeasure = getCurrentMeasureIndex();
    auto [gp5Num, gp5Den] = getGP5TimeSignature(currentMeasure);
    
    int dawNum = hostTimeSigNumerator.load();
    int dawDen = hostTimeSigDenominator.load();
    
    return (gp5Num == dawNum && gp5Den == dawDen);
}

void NewProjectAudioProcessor::setSeekPosition(int measureIndex, double positionInMeasure)
{
    if (!fileLoaded || measureIndex < 0)
        return;
        
    const auto& headers = getActiveMeasureHeaders();
    if (measureIndex >= headers.size())
        return;
    
    // Calculate beat position by summing up beats from previous measures
    double totalBeats = 0.0;
    for (int m = 0; m < measureIndex; ++m)
    {
        const auto& header = headers[m];
        // Beats per measure = numerator * (4 / denominator)
        // e.g., 4/4 = 4 beats, 6/8 = 3 beats, 3/4 = 3 beats
        double beatsInMeasure = header.numerator * (4.0 / header.denominator);
        totalBeats += beatsInMeasure;
    }
    
    // Add position within current measure
    const auto& currentHeader = headers[measureIndex];
    double beatsInCurrentMeasure = currentHeader.numerator * (4.0 / currentHeader.denominator);
    totalBeats += positionInMeasure * beatsInCurrentMeasure;
    
    // Store the seek position
    seekMeasureIndex.store(measureIndex);
    seekPositionInMeasure.store(positionInMeasure);
    seekPositionInBeats.store(totalBeats);
    seekPositionValid.store(true);
    
    DBG("Seek to: Measure " << (measureIndex + 1) << ", Position " << positionInMeasure 
        << " = " << totalBeats << " beats");
}

//==============================================================================
// MIDI Input -> Tab Display (Editor Mode)
//==============================================================================

std::vector<NewProjectAudioProcessor::GuitarPosition> NewProjectAudioProcessor::getPossiblePositions(int midiNote) const
{
    std::vector<GuitarPosition> positions;
    for (int str = 0; str < 6; ++str)
    {
        int fret = midiNote - standardTuning[str];
        // Prüfen ob im spielbaren Bereich (0 bis 24 Bünde)
        if (fret >= 0 && fret <= 24)
        {
            positions.push_back({str, fret});
        }
    }
    return positions;
}

float NewProjectAudioProcessor::calculatePositionCost(const GuitarPosition& current, const GuitarPosition& previous) const
{
    float cost = 0.0f;

    // 0. Fret-Position-Präferenz (Low/Mid/High) - sanfte Tendenz zur bevorzugten Region
    FretPosition pos = getFretPosition();
    int preferredMinFret, preferredMaxFret;
    switch (pos)
    {
        case FretPosition::Mid:
            preferredMinFret = 5;
            preferredMaxFret = 8;
            break;
        case FretPosition::High:
            preferredMinFret = 9;
            preferredMaxFret = 12;
            break;
        case FretPosition::Low:
        default:
            preferredMinFret = 0;
            preferredMaxFret = 4;
            break;
    }
    
    // Bonus wenn in bevorzugter Region, STARKE Strafe wenn außerhalb
    if (current.fret >= preferredMinFret && current.fret <= preferredMaxFret)
    {
        cost -= 10.0f;  // Starker Bonus für bevorzugte Region
    }
    else
    {
        int distFromPreferred = 0;
        if (current.fret < preferredMinFret)
            distFromPreferred = preferredMinFret - current.fret;
        else
            distFromPreferred = current.fret - preferredMaxFret;
        cost += distFromPreferred * 3.0f;  // Deutliche Strafe pro Bund Abstand
    }

    // 1. Fret-Distanz (Horizontal) - Bewegung entlang des Halses
    int fretDiff = std::abs(current.fret - previous.fret);
    cost += fretDiff * 1.0f;  // Reduziert von 1.5
    
    // Zusätzliche quadratische Komponente für große Sprünge (nur bei sehr großen)
    if (fretDiff > 4)
    {
        cost += (fretDiff - 4) * (fretDiff - 4) * 0.3f;
    }

    // 2. String-Distanz (Vertikal) - Saitenwechsel
    int stringDiff = std::abs(current.stringIndex - previous.stringIndex);
    cost += stringDiff * 0.5f;  // Reduziert von 0.8

    // 3. Hand-Spanne (Stretch Penalty) - NUR wenn NICHT in bevorzugter Region
    // Wenn wir in Richtung bevorzugte Region gehen, keine Strafe!
    bool movingTowardsPreferred = (current.fret >= preferredMinFret && current.fret <= preferredMaxFret) ||
                                   (current.fret < previous.fret && previous.fret > preferredMaxFret) ||
                                   (current.fret > previous.fret && previous.fret < preferredMinFret);
    
    if (fretDiff > 4 && current.fret != 0 && previous.fret != 0 && !movingTowardsPreferred)
    {
        cost += 8.0f;  // Reduziert von 15.0
        cost += (fretDiff - 4) * 1.5f;  // Reduziert von 3.0
    }
    
    // 4. "Handposition-Trägheit" - DEAKTIVIERT wenn außerhalb bevorzugter Region
    // Dies ermöglicht das Verlassen einer hohen Position wenn Low bevorzugt ist
    bool previousInPreferred = (previous.fret >= preferredMinFret && previous.fret <= preferredMaxFret);
    if (previousInPreferred && previous.fret >= 5 && current.fret != 0)
    {
        // Nur Trägheit anwenden wenn vorherige Position bereits im bevorzugten Bereich war
        int centerFret = previous.fret;
        int distFromCenter = std::abs(current.fret - centerFret);
        
        if (distFromCenter > 3)
        {
            cost += (distFromCenter - 3) * 2.0f;  // Reduziert von 4.0
        }
    }

    // 5. Offene Saiten sind IMMER einfacher zu spielen (starker Bonus)
    if (current.fret == 0)
    {
        cost -= 8.0f;  // Starker Bonus für offene Saiten - immer einfacher zu greifen
        if (pos == FretPosition::Low)
            cost -= 4.0f;  // Extra Bonus bei Low-Präferenz
        else if (pos == FretPosition::High)
            cost += 3.0f;  // Leichte Reduzierung des Bonus bei High-Präferenz
    }
    
    // 6. Kleine Präferenz für höhere Saiten bei Melodien (dünnere Saiten)
    cost -= current.stringIndex * 0.15f;
    
    // 7. Bonus für das Bleiben auf derselben Saite (fließenderes Spiel)
    if (current.stringIndex == previous.stringIndex && fretDiff <= 4)
    {
        cost -= 1.5f;
    }

    // 8. Hohe Strafe für große Fret-Sprünge (>3) bei schnellen Passagen
    //    Im Live-Modus nutzen wir die Zeit seit dem letzten Note-On.
    //    Achtel bei 120 BPM = 0.25s, bei 200 BPM = 0.15s.
    //    Threshold: 0.3s deckt Achtel und schneller ab.
    if (fretDiff > 3 && current.fret != 0 && previous.fret != 0)
    {
        double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double timeSinceLastNote = now - lastNoteOnTime;
        if (lastNoteOnTime > 0.0 && timeSinceLastNote < 0.3)
        {
            // Je schneller die Passage, desto teurer der Sprung
            float speedFactor = static_cast<float>(1.0 - (timeSinceLastNote / 0.3));  // 0..1
            cost += (fretDiff - 3) * 50.0f * speedFactor;  // Sehr hohe Strafe
        }
    }

    // 9. Ascending/Descending Direction Penalty (KTH Paper 6.2.9)
    //    Wenn die Melodie sich in eine Richtung bewegt (aufsteigend/absteigend),
    //    bevorzuge Positionen die diese Richtung beibehalten.
    //    Richtungswechsel sind teurer, besonders bei schnellen Passagen.
    if (lastFretDirection != 0 && current.fret != 0 && previous.fret != 0 && fretDiff > 0)
    {
        int currentDirection = (current.fret > previous.fret) ? 1 : -1;
        if (currentDirection != lastFretDirection)
        {
            // Richtungswechsel - moderate Strafe
            cost += 2.0f;
        }
        else
        {
            // Gleiche Richtung - kleiner Bonus (flüssigere Bewegung)
            cost -= 1.0f;
        }
    }

    // 10. String Change to Same Fret Penalty (KTH Paper 6.3.3)
    //     Saitenwechsel zum gleichen Bund ist auf der Gitarre schwieriger als
    //     man denkt - erfordert Barré oder schnellen Fingerwechsel.
    //     Ausnahme: Leersaiten (fret 0) und adjacent strings (nur 1 Saite entfernt)
    if (current.stringIndex != previous.stringIndex &&
        current.fret == previous.fret && current.fret > 0)
    {
        int stringDist = std::abs(current.stringIndex - previous.stringIndex);
        if (stringDist == 1)
        {
            // Benachbarte Saiten, gleicher Bund: Barré möglich, leichte Strafe
            cost += 1.5f;
        }
        else
        {
            // Nicht-benachbarte Saiten, gleicher Bund: sehr schwierig
            cost += 4.0f + stringDist * 1.0f;
        }
    }

    // 11. Neck Position Scaling (KTH Paper 6.2.7)
    //     Auf höheren Bünden (>12) sind die Abstände physisch kleiner,
    //     daher sind größere Fret-Sprünge dort einfacher.
    //     Reduziere die Fret-Distanz-Strafe proportional.
    if (current.fret > 12 && previous.fret > 12 && fretDiff > 2)
    {
        // Durchschnittliche Position auf dem Hals
        float avgFret = (current.fret + previous.fret) / 2.0f;
        // Reduktionsfaktor: bei Fret 12 = 1.0 (kein Rabatt), bei Fret 24 = 0.6
        float reductionFactor = 1.0f - (avgFret - 12.0f) * 0.033f;
        reductionFactor = juce::jmax(0.6f, reductionFactor);
        // Die Fret-Distanz-Kosten aus Punkt 1 teilweise zurückgeben
        float fretCostReduction = fretDiff * 1.0f * (1.0f - reductionFactor);
        cost -= fretCostReduction;
    }

    return cost;
}

NewProjectAudioProcessor::GuitarPosition NewProjectAudioProcessor::findBestPosition(int midiNote, int previousString, int previousFret) const
{
    auto candidates = getPossiblePositions(midiNote);
    
    if (candidates.empty())
    {
        // Fallback: Note nicht spielbar
        return {0, juce::jmax(0, midiNote - standardTuning[5])}; 
    }

    // Wenn keine vorherige Position, nutze Fret-Position-Präferenz
    if (previousString < 0 || previousFret < 0)
    {
        FretPosition pos = getFretPosition();
        int preferredMinFret, preferredMaxFret;
        switch (pos)
        {
            case FretPosition::Mid:
                preferredMinFret = 5;
                preferredMaxFret = 8;
                break;
            case FretPosition::High:
                preferredMinFret = 9;
                preferredMaxFret = 12;
                break;
            case FretPosition::Low:
            default:
                preferredMinFret = 0;
                preferredMaxFret = 4;
                break;
        }
        
        // Finde beste Position im bevorzugten Bereich
        GuitarPosition bestPos = candidates[0];
        int bestScore = -1000;
        
        for (const auto& cand : candidates)
        {
            int score = 0;
            
            if (cand.fret >= preferredMinFret && cand.fret <= preferredMaxFret)
            {
                score += 100;
            }
            else
            {
                int distFromRange = 0;
                if (cand.fret < preferredMinFret)
                    distFromRange = preferredMinFret - cand.fret;
                else
                    distFromRange = cand.fret - preferredMaxFret;
                score -= distFromRange * 5;
            }
            
            // Präferenz für höhere Saiten
            score += cand.stringIndex * 2;
            
            if (score > bestScore)
            {
                bestScore = score;
                bestPos = cand;
            }
        }
        return bestPos;
    }

    // Mit vorheriger Position: Nutze Kostenfunktion
    GuitarPosition previousPos = {previousString, previousFret};
    GuitarPosition bestPos = candidates[0];
    float minCost = std::numeric_limits<float>::max();

    for (const auto& cand : candidates)
    {
        float currentCost = calculatePositionCost(cand, previousPos);
        if (currentCost < minCost)
        {
            minCost = currentCost;
            bestPos = cand;
        }
    }
    return bestPos;
}

NewProjectAudioProcessor::LiveMidiNote NewProjectAudioProcessor::midiNoteToTab(int midiNote, int velocity) const
{
    LiveMidiNote result;
    result.midiNote = midiNote;
    result.velocity = velocity;
    
    // Check if context is stale (e.g. 2s pause), unless we are potentially in a recording session
    // For simplicity, we just use a 2-second timeout for resetting hand position logic
    // But if we are legally recording, we shouldn't timeout to preserve phrase
    bool isRecordingActive = recordingEnabled.load() || hostIsRecording.load();

    if (!isRecordingActive && lastPlayedString != -1)
    {
        double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        if (now - lastNoteOffTime > 2.0)
        {
            lastPlayedString = -1;
            lastPlayedFret = -1;
            lastFretDirection = 0;  // Reset direction on timeout
            lastFingerUsed = -1;    // Reset finger tracking on timeout
            lastFingerString = -1;
            positionLookaheadCounter = 0;  // Reset counter on timeout
        }
    }

    // Finde beste Position mit Kostenfunktion
    GuitarPosition bestPos = findBestPosition(midiNote, lastPlayedString, lastPlayedFret);
    
    // Track note-on time for fast-passage detection in calculatePositionCost
    lastNoteOnTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    
    // Track fret movement direction for ascending/descending sequence awareness (Paper 6.2.9)
    if (lastPlayedFret >= 0 && bestPos.fret > 0 && lastPlayedFret > 0)
    {
        int fretDelta = bestPos.fret - lastPlayedFret;
        if (fretDelta > 0)
            lastFretDirection = 1;   // ascending
        else if (fretDelta < 0)
            lastFretDirection = -1;  // descending
        // fretDelta == 0: keep previous direction
    }
    
    // Position Lookahead: Update last played position only every N notes
    // This helps the algorithm stay in one fret region during runs
    int lookahead = positionLookahead.load();
    positionLookaheadCounter++;
    
    if (positionLookaheadCounter >= lookahead)
    {
        // Update last played position for next note group
        lastPlayedString = bestPos.stringIndex;
        lastPlayedFret = bestPos.fret;
        positionLookaheadCounter = 0;
    }
    // If lookahead > 1 and counter not reached, keep using the old reference position
    
    // standardTuning[0]=E4 (highest, top line), [5]=E2 (lowest, bottom line)
    // This already matches display convention: 0=top, 5=bottom
    result.string = bestPos.stringIndex;
    result.fret = bestPos.fret;
    
    return result;
}

std::vector<NewProjectAudioProcessor::LiveMidiNote> NewProjectAudioProcessor::getLiveMidiNotes() const
{
    std::lock_guard<std::mutex> lock(liveMidiMutex);
    
    if (liveMidiNotes.empty())
    {
        detectedChordName = "";  // Kein Akkord wenn keine Noten
        liveMutedStrings = { false, false, false, false, false, false };
        return {};
    }
    
    // Sammle alle aktiven MIDI-Noten
    std::vector<int> midiNoteNumbers;
    std::map<int, int> noteVelocities;
    for (const auto& [note, liveNote] : liveMidiNotes)
    {
        midiNoteNumbers.push_back(note);
        noteVelocities[note] = liveNote.velocity;
    }
    std::sort(midiNoteNumbers.begin(), midiNoteNumbers.end());
    
    // =========================================================================
    // CHORD MATCHING: Versuche zuerst, einen bekannten Akkord zu finden
    // =========================================================================
    if (midiNoteNumbers.size() >= 3)  // Mindestens 3 Noten für Akkord-Matching
    {
        // Berechne aktuelle Handposition aus lastPlayedFret
        int currentFretPosition = (lastPlayedFret >= 0) ? lastPlayedFret : 0;
        
        // Hole Fret-Präferenz
        FretPosition pos = getFretPosition();
        int preferredMinFret, preferredMaxFret;
        switch (pos)
        {
            case FretPosition::Mid:
                preferredMinFret = 5;
                preferredMaxFret = 8;
                break;
            case FretPosition::High:
                preferredMinFret = 9;
                preferredMaxFret = 12;
                break;
            case FretPosition::Low:
            default:
                preferredMinFret = 0;
                preferredMaxFret = 4;
                break;
        }
        
        // Versuche erst mit exaktem Bass-Matching
        auto chordResult = chordMatcher.findBestChord(midiNoteNumbers, currentFretPosition, true, preferredMinFret, preferredMaxFret);
        
        // Falls kein Match mit exaktem Bass, versuche ohne Bass-Constraint
        if (!chordResult.isMatch)
        {
            chordResult = chordMatcher.findBestChord(midiNoteNumbers, currentFretPosition, false, preferredMinFret, preferredMaxFret);
        }
        
        if (chordResult.isMatch && chordResult.shape != nullptr)
        {
            // Akkord gefunden! Verwende das Shape direkt
            std::vector<LiveMidiNote> result;
            const auto& shape = *chordResult.shape;
            
            // Speichere erkannten Akkordnamen für die UI
            detectedChordName = shape.name;
            
            DBG("Chord matched: " << shape.name << " (cost: " << chordResult.totalCost << ")");
            
            // Determine muted strings from the chord shape
            // ChordShape.frets: index 0=E2(lowest), 5=E4(highest)
            // Display/standardTuning: index 0=E4(highest), 5=E2(lowest)
            // So we need to reverse: display string s corresponds to shape string (5-s)
            liveMutedStrings = { false, false, false, false, false, false };
            for (int s = 0; s < 6; ++s)
            {
                if (shape.frets[5 - s] < 0)  // -1 = gedämpft (x)
                    liveMutedStrings[s] = true;
            }
            
            for (int s = 0; s < 6; ++s)
            {
                int shapeString = 5 - s;  // Reverse: display s=0(E4) -> shape 5(E4)
                if (shape.frets[shapeString] >= 0)  // Nicht gedämpft
                {
                    int midiNote = standardTuning[s] + shape.frets[shapeString];
                    
                    // Finde die entsprechende Velocity (oder Standard)
                    int velocity = 100;
                    int midiPitchClass = midiNote % 12;
                    for (const auto& [inputNote, vel] : noteVelocities)
                    {
                        if (inputNote % 12 == midiPitchClass)
                        {
                            velocity = vel;
                            break;
                        }
                    }
                    
                    LiveMidiNote ln;
                    ln.midiNote = midiNote;
                    ln.velocity = velocity;
                    ln.string = s;  // Display string: 0=E4(top), 5=E2(bottom)
                    ln.fret = shape.frets[shapeString];
                    result.push_back(ln);
                }
            }
            
            // Update lastPlayedFret für nächsten Akkord
            lastPlayedFret = shape.baseFret;
            
            // === Assign finger numbers for matched chord ===
            {
                std::array<int, 6> chordFrets = { -1, -1, -1, -1, -1, -1 };
                for (const auto& ln : result)
                    if (ln.string >= 0 && ln.string < 6)
                        chordFrets[ln.string] = ln.fret;
                
                // Try database lookup first
                std::array<int, 6> fingers = { -1, -1, -1, -1, -1, -1 };
                if (chordFingerDB.isLoaded())
                    fingers = chordFingerDB.findFingers(shape.name, chordFrets, standardTuning);
                
                // Fallback: algorithmic
                bool hasDBFingers = false;
                for (int f : fingers)
                    if (f >= 0) { hasDBFingers = true; break; }
                if (!hasDBFingers)
                    fingers = ChordFingerDB::calculateFingersForChord(chordFrets);
                
                // Apply to result
                for (auto& ln : result)
                    if (ln.string >= 0 && ln.string < 6)
                        ln.fingerNumber = fingers[ln.string];
            }
            
            return result;
        }
    }
    
    // =========================================================================
    // FALLBACK: Kein Akkord erkannt - verwende bestehenden Algorithmus
    // =========================================================================
    detectedChordName = "";  // Kein bekannter Akkord
    liveMutedStrings = { false, false, false, false, false, false };  // No muted strings in fallback
    
    // Hole bevorzugten Fret-Bereich
    FretPosition pos = getFretPosition();
    int preferredMinFret, preferredMaxFret;
    switch (pos)
    {
        case FretPosition::Mid:
            preferredMinFret = 5;
            preferredMaxFret = 8;
            break;
        case FretPosition::High:
            preferredMinFret = 9;
            preferredMaxFret = 12;
            break;
        case FretPosition::Low:
        default:
            preferredMinFret = 0;
            preferredMaxFret = 4;
            break;
    }
    
    // Sammle alle aktiven Noten und sortiere sie nach Tonhöhe (niedrig zu hoch)
    std::vector<std::pair<int, int>> notesWithVelocity;  // midiNote, velocity
    for (const auto& [note, liveNote] : liveMidiNotes)
    {
        notesWithVelocity.push_back({note, liveNote.velocity});
    }
    std::sort(notesWithVelocity.begin(), notesWithVelocity.end());
    
    // Für jede Note: Sammle alle möglichen Saite/Bund-Kombinationen
    struct NoteOption {
        int string;
        int fret;
        int score;  // Score für diese Option
    };
    std::vector<std::vector<NoteOption>> allOptions;
    
    for (const auto& [midiNote, velocity] : notesWithVelocity)
    {
        std::vector<NoteOption> options;
        for (int s = 0; s < 6; ++s)
        {
            int fret = midiNote - standardTuning[s];
            if (fret >= 0 && fret <= 24)
            {
                // Berechne Score für diese Option
                int score = 0;
                
                // Bonus für Frets im bevorzugten Bereich
                if (fret >= preferredMinFret && fret <= preferredMaxFret)
                {
                    score += 100;
                }
                else
                {
                    // Strafe basierend auf Distanz zum bevorzugten Bereich
                    // Progressive Strafe - je weiter weg, desto teurer
                    int dist = 0;
                    if (fret < preferredMinFret)
                        dist = preferredMinFret - fret;
                    else
                        dist = fret - preferredMaxFret;
                    
                    // Quadratische Strafe für große Distanzen
                    score -= dist * 15;  // Erhöht von 10
                    if (dist > 3)
                        score -= (dist - 3) * (dist - 3) * 5;
                }
                
                // Bonus für Nähe zur letzten Position (Trägheit)
                if (lastPlayedFret >= 0 && lastPlayedFret >= 7)
                {
                    // Wir sind in einer hohen Position - bevorzuge diese beizubehalten
                    int distFromLast = std::abs(fret - lastPlayedFret);
                    if (distFromLast <= 3)
                        score += 30;  // Bonus für nahes Bleiben
                    else if (distFromLast > 5)
                        score -= (distFromLast - 5) * 8;  // Strafe für weites Springen
                }
                
                // Leichte Präferenz für höhere Saiten (Melodie)
                score += s * 2;
                
                options.push_back({s, fret, score});
            }
        }
        // Sortiere Optionen nach Score (höchster zuerst)
        std::sort(options.begin(), options.end(), [](const NoteOption& a, const NoteOption& b) {
            return a.score > b.score;
        });
        allOptions.push_back(options);
    }
    
    // Finde die beste Kombination mit minimaler Bund-Spannweite
    // Progressive Relaxierung: erst 3 Bünde, dann 4, 5, ... bis max 7
    std::vector<LiveMidiNote> bestResult;
    int bestScore = -10000;
    
    auto runLiveBacktracking = [&](int allowedSpan) {
        std::function<void(int, std::vector<NoteOption>&, std::set<int>&)> findBest;
        findBest = [&](int noteIdx, std::vector<NoteOption>& current, std::set<int>& usedStrings) {
            if (noteIdx >= (int)allOptions.size())
            {
                int minFret = 100, maxFret = 0;
                for (const auto& opt : current)
                {
                    if (opt.fret > 0)
                    {
                        minFret = std::min(minFret, opt.fret);
                        maxFret = std::max(maxFret, opt.fret);
                    }
                }
                if (minFret > maxFret) minFret = maxFret = 0;
                
                int fretSpan = maxFret - minFret;
                if (fretSpan > allowedSpan)
                    return;
                
                int score = 0;
                for (const auto& opt : current)
                    score += opt.score;
                
                // Schwere quadratische Strafe für Spannweite
                score -= fretSpan * fretSpan * 15;
                if (fretSpan <= 2) score += 20;
                else if (fretSpan <= 3) score += 10;
                
                if (lastPlayedFret >= 7 && maxFret > 0)
                {
                    int centerOfCurrent = (minFret + maxFret) / 2;
                    int distFromLastPos = std::abs(centerOfCurrent - lastPlayedFret);
                    if (distFromLastPos <= 3)
                        score += 25;
                    else
                        score -= distFromLastPos * 5;
                }
                
                if (score > bestScore)
                {
                    bestScore = score;
                    bestResult.clear();
                    for (int i = 0; i < (int)current.size(); ++i)
                    {
                        LiveMidiNote ln;
                        ln.midiNote = notesWithVelocity[i].first;
                        ln.velocity = notesWithVelocity[i].second;
                        ln.string = current[i].string;
                        ln.fret = current[i].fret;
                        bestResult.push_back(ln);
                    }
                }
                return;
            }
            
            for (const auto& opt : allOptions[noteIdx])
            {
                if (usedStrings.count(opt.string) > 0)
                    continue;
                
                int minFret = 100, maxFret = 0;
                for (const auto& prev : current)
                {
                    if (prev.fret > 0)
                    {
                        minFret = std::min(minFret, prev.fret);
                        maxFret = std::max(maxFret, prev.fret);
                    }
                }
                if (opt.fret > 0)
                {
                    int newMin = std::min(minFret, opt.fret);
                    int newMax = std::max(maxFret, opt.fret);
                    if (newMin <= newMax && newMax - newMin > allowedSpan)
                        continue;
                }
                
                current.push_back(opt);
                usedStrings.insert(opt.string);
                findBest(noteIdx + 1, current, usedStrings);
                usedStrings.erase(opt.string);
                current.pop_back();
            }
        };
        
        std::vector<NoteOption> current;
        std::set<int> usedStrings;
        findBest(0, current, usedStrings);
    };
    
    // Progressive Relaxierung: erst 3 Bünde, dann 4, 5, ... bis max 7
    for (int trySpan = 3; trySpan <= 7; ++trySpan)
    {
        runLiveBacktracking(trySpan);
        if (!bestResult.empty())
            break;
    }
    
    // Fallback: Wenn immer noch keine gültige Kombination, zeige einzelne Noten
    if (bestResult.empty())
    {
        for (const auto& [midiNote, velocity] : notesWithVelocity)
        {
            LiveMidiNote ln = midiNoteToTab(midiNote, velocity);
            bestResult.push_back(ln);
        }
    }
    
    // Assign finger numbers to fallback bestResult (backtracking or single-note path)
    if (!bestResult.empty() && bestResult[0].fingerNumber < 0)
    {
        if (bestResult.size() > 1)
        {
            // Multi-note: use algorithmic chord finger calculation
            std::array<int,6> chordFrets = {-1,-1,-1,-1,-1,-1};
            for (const auto& n : bestResult)
            {
                if (n.string >= 0 && n.string < 6)
                    chordFrets[n.string] = n.fret;
            }
            auto fingers = ChordFingerDB::calculateFingersForChord(chordFrets);
            for (auto& n : bestResult)
            {
                if (n.string >= 0 && n.string < 6)
                    n.fingerNumber = fingers[n.string];
            }
        }
        else
        {
            // Single note: use single-note finger calculation
            for (auto& n : bestResult)
            {
                if (n.string >= 0 && n.string < 6)
                {
                    std::array<int,6> singleFrets = {-1,-1,-1,-1,-1,-1};
                    singleFrets[n.string] = n.fret;
                    auto fingers = ChordFingerDB::calculateFingersForChord(singleFrets);
                    n.fingerNumber = fingers[n.string];
                }
            }
        }
    }

    // Update lastPlayedFret for next call (inertia)
    if (!bestResult.empty())
    {
        int maxFret = 0;
        for (const auto& note : bestResult)
        {
            if (note.fret > maxFret)
                maxFret = note.fret;
        }
        if (maxFret > 0)
            lastPlayedFret = maxFret;
    }
    
    return bestResult;
}

TabTrack NewProjectAudioProcessor::getEmptyTabTrack() const
{
    TabTrack track;
    track.name = "MIDI Input";
    track.stringCount = 6;
    track.tuning = { 64, 59, 55, 50, 45, 40 };  // E-Standard (High to Low)
    track.colour = juce::Colours::blue;
    
    // Create measures based on DAW time signature
    int numerator = hostTimeSigNumerator.load();
    int denominator = hostTimeSigDenominator.load();
    
    // Create empty measures for display - mehr Takte für bessere Übersicht
    for (int m = 0; m < 16; ++m)
    {
        TabMeasure measure;
        measure.measureNumber = m + 1;  // 1-basierte Anzeige (entspricht DAW Takt 1, 2, 3, ...)
        measure.timeSignatureNumerator = numerator;
        measure.timeSignatureDenominator = denominator;
        
        // Keine Beats hinzufügen - Takte bleiben wirklich leer
        
        track.measures.add(measure);
    }
    
    return track;
}

//==============================================================================
// Recording functionality (automatic based on DAW track record status)
//==============================================================================

void NewProjectAudioProcessor::insertTranscribedNotesIntoTab()
{
    // Get transcribed note events from BasicPitch
    auto noteEvents = audioTranscriber.getNoteEvents();
    
    if (noteEvents.empty())
    {
        DBG("AudioTranscriber: No notes detected in transcription");
        return;
    }
    
    DBG("AudioTranscriber: Inserting " << noteEvents.size() << " transcribed notes into tab");
    
    double tempo = hostTempo.load();
    if (tempo <= 0.0)
        tempo = 120.0;
    
    // Beats per second = BPM / 60
    double beatsPerSecond = tempo / 60.0;
    
    std::lock_guard<std::mutex> recLock(recordingMutex);
    
    // Setze recordingStartBeat falls nicht gesetzt
    if (!recordingStartSet)
    {
        recordingStartBeat = audioRecordingStartBeat;
        recordingStartSet = true;
    }
    
    for (const auto& event : noteEvents)
    {
        // BasicPitch event times are in seconds (relative to start of recording at 22050 Hz)
        // Convert to beats relative to DAW timeline
        double startBeat = audioRecordingStartBeat + (event.startTime * beatsPerSecond);
        double endBeat = audioRecordingStartBeat + (event.endTime * beatsPerSecond);
        
        // Quantize to 1/64 note grid (like MIDI recording does)
        double quantizeGrid = 0.0625;
        startBeat = std::round(startBeat / quantizeGrid) * quantizeGrid;
        endBeat = std::round(endBeat / quantizeGrid) * quantizeGrid;
        
        // Ensure minimum duration
        if (endBeat <= startBeat)
            endBeat = startBeat + quantizeGrid;
        
        // MIDI note from BasicPitch (pitch is already MIDI note number)
        int midiNote = juce::jlimit(0, 127, event.pitch);
        
        // Velocity from amplitude
        int velocity = juce::jlimit(1, 127, static_cast<int>(event.amplitude * 127.0));
        
        // Convert to tab position using the same logic as MIDI recording
        LiveMidiNote tabNote = midiNoteToTab(midiNote, velocity);
        
        RecordedNote recNote;
        recNote.midiNote = midiNote;
        recNote.midiChannel = 1;  // Audio input = channel 1
        recNote.velocity = velocity;
        recNote.string = tabNote.string;
        recNote.fret = tabNote.fret;
        recNote.startBeat = startBeat;
        recNote.endBeat = endBeat;
        recNote.isActive = false;  // Already complete
        
        // Pitch bend data from BasicPitch contour
        // Im Audio-to-MIDI Modus: Bends unter 0.5 Halbtöne ignorieren!
        // Kleine Pitch-Schwankungen sind keine echten Bends sondern Intonation/Vibrato
        if (!event.bends.empty())
        {
            recNote.maxBendValue = 0.0f;
            for (int bend : event.bends)
            {
                // BasicPitch bends are in 1/3 semitones → convert to 1/100 semitones
                int bendCents = static_cast<int>(bend * (100.0 / 3.0));
                float bendSemis = std::abs(bendCents) / 100.0f;
                if (bendSemis > recNote.maxBendValue)
                    recNote.maxBendValue = bendSemis;
            }
            
            // Bends unter 2.0 Halbtöne verwerfen - das sind keine echten Bends
            // (Ein echter Guitar-Bend ist mindestens ein Ganzton / whole step)
            // Kleinere Werte sind Intonation, Vibrato oder Oberton-Artefakte
            if (recNote.maxBendValue < 2.0f)
                recNote.maxBendValue = 0.0f;
        }
        
        // Assign finger number for audio-transcribed notes
        recNote.fingerNumber = ChordFingerDB::calculateFingerForNote(
            recNote.fret, recNote.string,
            lastPlayedFret, lastFingerUsed, lastFingerString);
        lastFingerUsed = recNote.fingerNumber;
        lastFingerString = recNote.string;
        
        recordedNotes.push_back(recNote);
    }
    
    DBG("AudioTranscriber: " << noteEvents.size() << " notes inserted, total recorded: " << recordedNotes.size());
    
    // Mark results as consumed so we don't insert again
    audioTranscriber.clearResults();
}

void NewProjectAudioProcessor::clearRecording()
{
    {
        std::lock_guard<std::mutex> lock(recordingMutex);
        recordedNotes.clear();
        activeRecordingNotes.clear();
        recordingStartBeat = 0.0;
        recordingStartSet = false;
    }
    
    // Reset playback state
    activePlaybackNotes.clear();
    lastPlaybackBeat = -1.0;
    lastProcessedRecMeasure = -1;
    lastProcessedRecBeat = -1;
    
    // Reset position tracking
    positionLookaheadCounter = 0;
    
    // Reset audio transcriber state completely
    audioTranscriber.clearRecording();
    audioTranscriber.clearResults();
    audioRecordingStartBeat = 0.0;
    audioRecordingStartSet = false;
    wasRecordingAudio = false;
    
    // Clear live MIDI notes (from YIN or MIDI input)
    {
        std::lock_guard<std::mutex> lock(liveMidiMutex);
        liveMidiNotes.clear();
    }
    
    // Reset YIN monophonic pitch detector
    audioToMidiProcessor.reset();
    
    // Clear edited tracks
    editedTracks.clear();
    
    // Reset recording enabled flag
    recordingEnabled.store(false);
    
    DBG("Recording fully cleared");
}

void NewProjectAudioProcessor::reoptimizeRecordedNotes(int midiChannelFilter)
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    if (recordedNotes.empty())
        return;
    
    // Get current settings
    int lookahead = positionLookahead.load();
    FretPosition pos = getFretPosition();
    int preferredMinFret, preferredMaxFret;
    switch (pos)
    {
        case FretPosition::Mid:
            preferredMinFret = 5;
            preferredMaxFret = 8;
            break;
        case FretPosition::High:
            preferredMinFret = 9;
            preferredMaxFret = 12;
            break;
        case FretPosition::Low:
        default:
            preferredMinFret = 0;
            preferredMaxFret = 4;
            break;
    }
    
    // Collect indices to process (filtered by channel if specified)
    std::vector<size_t> sortedIndices;
    for (size_t i = 0; i < recordedNotes.size(); ++i)
    {
        if (midiChannelFilter < 0 || recordedNotes[i].midiChannel == midiChannelFilter)
            sortedIndices.push_back(i);
    }
    
    if (sortedIndices.empty())
        return;
    
    // Sortiere Noten nach Startzeit für sequentielle Verarbeitung
    std::sort(sortedIndices.begin(), sortedIndices.end(), [this](size_t a, size_t b) {
        return recordedNotes[a].startBeat < recordedNotes[b].startBeat;
    });
    
    // Reset last played position
    lastPlayedString = -1;
    lastPlayedFret = -1;
    positionLookaheadCounter = 0;  // Reset counter for lookahead
    
    // Tracking variables for lookahead - separate from the mutable class members
    int referenceString = -1;
    int referenceFret = -1;
    int noteCounter = 0;
    
    // Gruppiere Noten nach Beat (simultane Noten = Akkord)
    double currentBeat = -1.0;
    std::vector<size_t> currentGroup;
    
    auto processGroup = [this, &referenceString, &referenceFret, &noteCounter, lookahead, preferredMinFret, preferredMaxFret](std::vector<size_t>& group) {
        if (group.empty()) return;
        
        if (group.size() == 1)
        {
            // Einzelne Note - custom lookahead logic
            size_t idx = group[0];
            int midiNote = recordedNotes[idx].midiNote;
            
            // Find best position based on reference position
            auto candidates = getPossiblePositions(midiNote);
            if (candidates.empty())
            {
                recordedNotes[idx].string = 0;
                recordedNotes[idx].fret = juce::jmax(0, midiNote - standardTuning[5]);
            }
            else
            {
                GuitarPosition bestPos = candidates[0];
                
                // Check if fret 0 is available as an option
                bool hasFret0Option = false;
                int fret0String = -1;
                for (const auto& cand : candidates)
                {
                    if (cand.fret == 0)
                    {
                        hasFret0Option = true;
                        fret0String = cand.stringIndex;
                        break;
                    }
                }
                
                // Store scores for debug output
                std::vector<std::pair<GuitarPosition, int>> debugScores;
                
                if (referenceString < 0 || referenceFret < 0)
                {
                    // No reference - use fret position preference
                    int bestScore = -1000;
                    for (const auto& cand : candidates)
                    {
                        int score = 0;
                        
                        // Open string (fret 0) gets a bonus to be preferred
                        if (cand.fret == 0)
                        {
                            score += 115;  // Bonus for open strings
                        }
                        else if (cand.fret >= preferredMinFret && cand.fret <= preferredMaxFret)
                        {
                            score += 100;
                        }
                        else
                        {
                            int dist = (cand.fret < preferredMinFret) ? (preferredMinFret - cand.fret) : (cand.fret - preferredMaxFret);
                            score -= dist * 5;
                        }
                        score += cand.stringIndex * 2;  // Slight preference for higher strings
                        
                        debugScores.push_back({cand, score});
                        
                        if (score > bestScore)
                        {
                            bestScore = score;
                            bestPos = cand;
                        }
                    }
                    
                    // Debug: Why was fret 0 not chosen?
                    if (hasFret0Option && bestPos.fret != 0)
                    {
                        DBG("=== FRET 0 NOT CHOSEN (no ref) for MIDI " << midiNote << " ===");
                        DBG("  Chosen: string=" << bestPos.stringIndex << " fret=" << bestPos.fret);
                        DBG("  preferredMinFret=" << preferredMinFret << " preferredMaxFret=" << preferredMaxFret);
                        for (const auto& [pos, score] : debugScores)
                        {
                            DBG("    Option: string=" << pos.stringIndex << " fret=" << pos.fret << " -> score=" << score);
                        }
                    }
                }
                else
                {
                    // Have reference - use reference position as dynamic center
                    // The reference position becomes the new "preferred" position
                    float bestCost = 99999.0f;
                    std::vector<std::pair<GuitarPosition, float>> debugCosts;
                    
                    for (const auto& cand : candidates)
                    {
                        float cost = 0.0f;
                        
                        // Open string (fret 0) gets a bonus - subtract from cost
                        if (cand.fret == 0)
                        {
                            cost -= 10.0f;  // Bonus for open strings
                        }
                        else
                        {
                            // Distance from reference position
                            int fretDiff = std::abs(cand.fret - referenceFret);
                            cost += fretDiff * 5.0f;  // Strong penalty for moving away from reference
                        }
                        
                        // String jump penalty
                        int stringDiff = std::abs(cand.stringIndex - referenceString);
                        cost += stringDiff * 3.0f;
                        
                        // Small preference for staying in original fret position range
                        if (cand.fret > 0 && cand.fret >= preferredMinFret && cand.fret <= preferredMaxFret)
                            cost -= 2.0f;
                        
                        // Hohe Strafe für große Fret-Sprünge (>3) bei schnellen Noten
                        // Achtel = 0.5 Beats, Sechzehntel = 0.25, 32stel = 0.125
                        if (cand.fret != 0 && referenceFret > 0)
                        {
                            int fretJump = std::abs(cand.fret - referenceFret);
                            if (fretJump > 3)
                            {
                                double noteDuration = recordedNotes[idx].endBeat - recordedNotes[idx].startBeat;
                                if (noteDuration <= 0.5 + 0.01)  // Achtel oder kürzer (+ kleine Toleranz)
                                {
                                    // Je kürzer die Note, desto teurer der Sprung
                                    float speedFactor = (noteDuration <= 0.125) ? 3.0f :   // 32stel
                                                        (noteDuration <= 0.25)  ? 2.0f :   // 16tel
                                                                                   1.0f;    // Achtel
                                    cost += (fretJump - 3) * 50.0f * speedFactor;
                                }
                            }
                        }
                        
                        debugCosts.push_back({cand, cost});
                        
                        if (cost < bestCost)
                        {
                            bestCost = cost;
                            bestPos = cand;
                        }
                    }
                    
                    // Debug: Why was fret 0 not chosen?
                    if (hasFret0Option && bestPos.fret != 0)
                    {
                        DBG("=== FRET 0 NOT CHOSEN (with ref) for MIDI " << midiNote << " ===");
                        DBG("  Chosen: string=" << bestPos.stringIndex << " fret=" << bestPos.fret);
                        DBG("  refString=" << referenceString << " refFret=" << referenceFret);
                        for (const auto& [pos, cost] : debugCosts)
                        {
                            DBG("    Option: string=" << pos.stringIndex << " fret=" << pos.fret << " -> cost=" << cost);
                        }
                    }
                }
                
                recordedNotes[idx].string = bestPos.stringIndex;
                recordedNotes[idx].fret = bestPos.fret;
                
                // Update reference only every N notes (lookahead)
                noteCounter++;
                if (noteCounter >= lookahead)
                {
                    referenceString = bestPos.stringIndex;
                    referenceFret = bestPos.fret;
                    noteCounter = 0;
                }
            }
        }
        else
        {
            // Mehrere Noten gleichzeitig - verwende getLiveMidiNotes-Logik
            // Baue temporäre liveMidiNotes auf
            std::map<int, LiveMidiNote> tempLiveMidi;
            for (size_t idx : group)
            {
                LiveMidiNote ln;
                ln.midiNote = recordedNotes[idx].midiNote;
                ln.velocity = recordedNotes[idx].velocity;
                tempLiveMidi[ln.midiNote] = ln;
            }
            
            // Sammle alle Noten und sortiere nach Tonhöhe
            std::vector<std::pair<int, size_t>> notesWithIdx;  // midiNote, index in group
            for (size_t i = 0; i < group.size(); ++i)
            {
                notesWithIdx.push_back({recordedNotes[group[i]].midiNote, i});
            }
            std::sort(notesWithIdx.begin(), notesWithIdx.end());
            
            // Für jede Note: finde alle möglichen Positionen
            struct NoteOption { int string; int fret; int score; };
            std::vector<std::vector<NoteOption>> allOptions;
            
            for (const auto& [midiNote, _] : notesWithIdx)
            {
                std::vector<NoteOption> options;
                for (int s = 0; s < 6; ++s)
                {
                    int fret = midiNote - standardTuning[s];
                    if (fret >= 0 && fret <= 24)
                    {
                        int score = 0;
                        
                        // Open string (fret 0) is neutral - valid in any fret position range
                        if (fret == 0 || (fret >= preferredMinFret && fret <= preferredMaxFret))
                        {
                            score += 100;
                        }
                        else
                        {
                            int dist = (fret < preferredMinFret) ? (preferredMinFret - fret) : (fret - preferredMaxFret);
                            score -= dist * 15;
                            if (dist > 3)
                                score -= (dist - 3) * (dist - 3) * 5;
                        }
                        
                        // Use reference fret for lookahead consistency (only for fretted notes)
                        if (fret > 0 && referenceFret >= 7)
                        {
                            int distFromRef = std::abs(fret - referenceFret);
                            if (distFromRef <= 3)
                                score += 30;
                            else if (distFromRef > 5)
                                score -= (distFromRef - 5) * 8;
                        }
                        
                        score += s * 2;
                        options.push_back({s, fret, score});
                    }
                }
                std::sort(options.begin(), options.end(), [](const NoteOption& a, const NoteOption& b) {
                    return a.score > b.score;
                });
                allOptions.push_back(options);
            }
            
            // Backtracking-Suche mit progressiver Relaxierung:
            // Erst versuche max 3 Bünde Spannweite, dann 4, 5, ... bis Lösung gefunden.
            // Die Spannweite fließt als SCHWERE Strafe in den Score ein.
            std::vector<NoteOption> bestAssignment(allOptions.size());
            int bestTotalScore = -100000;
            int usedMaxSpan = 0;
            
            auto runBacktracking = [&](int allowedSpan) {
                std::function<void(size_t, std::vector<NoteOption>&, std::set<int>&)> findBestChord;
                findBestChord = [&](size_t noteIdx, std::vector<NoteOption>& current, std::set<int>& usedStrings) {
                    if (noteIdx >= allOptions.size())
                    {
                        // Prüfe Fret-Spannweite (Leersaiten ausgenommen)
                        int minF = 100, maxF = 0;
                        for (const auto& opt : current)
                        {
                            if (opt.fret > 0)
                            {
                                minF = std::min(minF, opt.fret);
                                maxF = std::max(maxF, opt.fret);
                            }
                        }
                        int span = (minF <= maxF) ? (maxF - minF) : 0;
                        if (span > allowedSpan)
                            return;  // Spannweite zu groß für diese Runde
                        
                        int totalScore = 0;
                        for (const auto& opt : current)
                            totalScore += opt.score;
                        // Schwere Strafe für Spannweite - je größer, desto schlimmer
                        totalScore -= span * span * 15;
                        if (span <= 2) totalScore += 20;
                        else if (span <= 3) totalScore += 10;
                        
                        if (totalScore > bestTotalScore)
                        {
                            bestTotalScore = totalScore;
                            bestAssignment = current;
                            usedMaxSpan = span;
                        }
                        return;
                    }
                    
                    for (const auto& opt : allOptions[noteIdx])
                    {
                        // Saite darf NICHT doppelt belegt sein
                        if (usedStrings.count(opt.string) > 0)
                            continue;
                        
                        // Frühe Prüfung: Fret-Spannweite
                        int minF = 100, maxF = 0;
                        for (const auto& prev : current)
                        {
                            if (prev.fret > 0) { minF = std::min(minF, prev.fret); maxF = std::max(maxF, prev.fret); }
                        }
                        if (opt.fret > 0)
                        {
                            int newMin = std::min(minF, opt.fret);
                            int newMax = std::max(maxF, opt.fret);
                            if (newMin <= newMax && newMax - newMin > allowedSpan)
                                continue;  // Würde erlaubte Spannweite überschreiten
                        }
                        
                        current.push_back(opt);
                        usedStrings.insert(opt.string);
                        findBestChord(noteIdx + 1, current, usedStrings);
                        usedStrings.erase(opt.string);
                        current.pop_back();
                    }
                };
                
                std::vector<NoteOption> current;
                std::set<int> usedStrings;
                findBestChord(0, current, usedStrings);
            };
            
            // Progressive Relaxierung: erst 3 Bünde, dann 4, 5, ... bis max 7
            for (int trySpan = 3; trySpan <= 7; ++trySpan)
            {
                runBacktracking(trySpan);
                if (bestTotalScore > -100000)
                    break;  // Gültige Lösung gefunden!
            }
            
            // Absoluter Notfall: Kein gültiges Ergebnis bis Spannweite 7.
            // Nimm die beste Option pro Note mit unique Strings.
            if (bestTotalScore <= -100000)
            {
                std::set<int> usedStrings;
                for (size_t i = 0; i < allOptions.size(); ++i)
                {
                    bool assigned = false;
                    for (const auto& opt : allOptions[i])
                    {
                        if (usedStrings.count(opt.string) == 0)
                        {
                            bestAssignment[i] = opt;
                            usedStrings.insert(opt.string);
                            assigned = true;
                            break;
                        }
                    }
                    if (!assigned && !allOptions[i].empty())
                        bestAssignment[i] = allOptions[i][0];
                }
            }
            
            // Weise die Ergebnisse zu
            int avgFret = 0;
            int fretCount = 0;
            for (size_t i = 0; i < notesWithIdx.size(); ++i)
            {
                size_t groupIdx = notesWithIdx[i].second;
                size_t recIdx = group[groupIdx];
                // standardTuning[0]=E4(top), [5]=E2(bottom) - matches display convention
                recordedNotes[recIdx].string = bestAssignment[i].string;
                recordedNotes[recIdx].fret = bestAssignment[i].fret;
                
                // Track average fret for chord position reference
                if (bestAssignment[i].fret > 0)
                {
                    avgFret += bestAssignment[i].fret;
                    fretCount++;
                }
            }
            
            // === Chord finger assignment ===
            // For groups with 2+ notes, assign fingers using the DB or algorithmic method
            if (group.size() >= 2)
            {
                // Build fret array for all 6 strings
                std::array<int, 6> chordFrets = { -1, -1, -1, -1, -1, -1 };
                for (size_t idx : group)
                {
                    int s = recordedNotes[idx].string;
                    if (s >= 0 && s < 6)
                        chordFrets[s] = recordedNotes[idx].fret;
                }
                
                // Try database lookup first (if chord was detected)
                std::array<int, 6> fingers = { -1, -1, -1, -1, -1, -1 };
                if (chordFingerDB.isLoaded() && detectedChordName.isNotEmpty())
                {
                    fingers = chordFingerDB.findFingers(detectedChordName, chordFrets, standardTuning);
                }
                
                // Fallback: algorithmic chord finger assignment
                bool hasDBFingers = false;
                for (int f : fingers)
                    if (f >= 0) { hasDBFingers = true; break; }
                
                if (!hasDBFingers)
                    fingers = ChordFingerDB::calculateFingersForChord(chordFrets);
                
                // Apply fingers to recorded notes
                for (size_t idx : group)
                {
                    int s = recordedNotes[idx].string;
                    if (s >= 0 && s < 6)
                        recordedNotes[idx].fingerNumber = fingers[s];
                }
            }
            
            // Update reference with lookahead for chords
            noteCounter++;
            if (noteCounter >= lookahead && fretCount > 0)
            {
                referenceFret = avgFret / fretCount;
                referenceString = 2;  // Middle string as reference for chords
                noteCounter = 0;
            }
        }
        group.clear();
    };
    
    // Verarbeite Noten in Gruppen nach Beat
    // WICHTIG: Muss mit chordThreshold in getRecordedTabTrack() übereinstimmen!
    // Sonst werden Noten dort als Akkord gruppiert, aber hier einzeln verarbeitet
    // → gleiche Saite möglich → zweite Note wird in der Tab-Ausgabe überschrieben!
    const double beatTolerance = 0.06;  // Noten innerhalb von 0.06 Beats = gleichzeitig (=chordThreshold)
    for (size_t idx : sortedIndices)
    {
        double noteBeat = recordedNotes[idx].startBeat;
        
        if (currentBeat < 0 || std::abs(noteBeat - currentBeat) <= beatTolerance)
        {
            // Gleicher Beat - zur Gruppe hinzufügen
            currentGroup.push_back(idx);
            if (currentBeat < 0)
                currentBeat = noteBeat;
        }
        else
        {
            // Neuer Beat - vorherige Gruppe verarbeiten
            processGroup(currentGroup);
            currentGroup.push_back(idx);
            currentBeat = noteBeat;
        }
    }
    // Letzte Gruppe verarbeiten
    processGroup(currentGroup);
    
    // Reset für normale Verwendung
    lastPlayedString = -1;
    lastPlayedFret = -1;
}

void NewProjectAudioProcessor::updateRecordedNotesFromLive(const std::vector<LiveMidiNote>& liveNotes)
{
    // Diese Funktion wird vom UI-Thread aufgerufen während Recording aktiv ist.
    // Sie aktualisiert die aktiven aufgezeichneten Noten mit den optimierten Werten
    // aus der Live-Anzeige - so wird EXAKT das gespeichert was der User sieht.
    
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    for (const auto& liveNote : liveNotes)
    {
        // Finde die entsprechende aufgezeichnete Note (via activeRecordingNotes map)
        auto it = activeRecordingNotes.find(liveNote.midiNote);
        if (it != activeRecordingNotes.end() && it->second < recordedNotes.size())
        {
            // Aktualisiere mit den optimierten Werten aus der Live-Anzeige
            recordedNotes[it->second].string = liveNote.string;
            recordedNotes[it->second].fret = liveNote.fret;
        }
    }
}

std::vector<NewProjectAudioProcessor::RecordedNote> NewProjectAudioProcessor::getRecordedNotes() const
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    return recordedNotes;
}

void NewProjectAudioProcessor::updateRecordedNotePosition(int measureIndex, int beatIndex, int oldString, int newString, int newFret)
{
    // Diese Funktion aktualisiert sowohl den editedTrack als auch die recordedNotes
    // basierend auf Takt/Beat und alter String-Position
    // oldString ist die String-Position VOR der Änderung (um die Note in recordedNotes zu finden)
    
    // Aktualisiere editedTrack (wenn vorhanden) - hier ist die Änderung bereits in der UI erfolgt
    // Wir müssen den Track aus der UI holen (wird über setEditedTrack gemacht)
    
    // Aktualisiere recordedNotes
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    if (!recordedNotes.empty())
    {
        int numerator = hostTimeSigNumerator.load();
        int denominator = hostTimeSigDenominator.load();
        double beatsPerMeasure = numerator * (4.0 / denominator);
        
        // measureIndex in der UI ist 0-basiert, entspricht aber barNum-1 in getRecordedTabTrack
        int barNum = measureIndex + 1;
        double measureStartBeat = (barNum - 1) * beatsPerMeasure;
        
        // Sammle Noten in diesem Takt (gleiche Logik wie getRecordedTabTrack)
        std::vector<std::pair<size_t, RecordedNote*>> notesInMeasure;
        for (size_t i = 0; i < recordedNotes.size(); ++i)
        {
            auto& note = recordedNotes[i];
            double roundedPPQ = std::round(note.startBeat * 1000.0) / 1000.0;
            int noteBar = static_cast<int>(roundedPPQ / beatsPerMeasure) + 1;
            
            // Anwenden der gleichen Quantisierungs-Logik
            double positionInMeasure = roundedPPQ - (noteBar - 1) * beatsPerMeasure;
            double distanceToNextBar = beatsPerMeasure - positionInMeasure;
            double originalDuration = note.endBeat - note.startBeat;
            
            if (measureQuantizationEnabled.load() && distanceToNextBar < 0.5 && distanceToNextBar > 0.001)
            {
                double truncatedDuration = distanceToNextBar;
                double truncationRatio = truncatedDuration / std::max(0.001, originalDuration);
                if (truncationRatio < 0.25 && originalDuration > 0.25)
                {
                    noteBar = noteBar + 1;
                }
            }
            
            if (noteBar == barNum)
            {
                notesInMeasure.push_back({i, &note});
            }
        }
        
        if (notesInMeasure.empty())
        {
            DBG("No recorded notes found in measure " << (measureIndex + 1));
            return;
        }
        
        // Sortiere nach startBeat
        std::sort(notesInMeasure.begin(), notesInMeasure.end(), 
            [](const auto& a, const auto& b) { return a.second->startBeat < b.second->startBeat; });
        
        // Gruppiere nach Slots (gleiche Logik wie getRecordedTabTrack)
        double subdivision = 0.125; // 32nd note in quarter-note beats
        double chordThreshold = 0.08; // Notes within ~80ms are chord
        
        struct SlotGroup {
            int slot;
            std::vector<std::pair<size_t, RecordedNote*>> notes;
        };
        std::vector<SlotGroup> slotGroups;
        
        int lastOccupiedSlot = -1;
        double lastStartTime = -999.0;
        
        for (auto& [noteIdx, notePtr] : notesInMeasure)
        {
            double posInMeasure = notePtr->startBeat - measureStartBeat;
            if (posInMeasure < 0.0) posInMeasure = 0.0;
            
            int idealSlot = (int)(posInMeasure / subdivision + 0.5);
            
            // Prüfe ob diese Note zum letzten Slot gehört (Akkord)
            bool isChord = !slotGroups.empty() && 
                          std::abs(notePtr->startBeat - lastStartTime) < chordThreshold;
            
            if (isChord)
            {
                slotGroups.back().notes.push_back({noteIdx, notePtr});
            }
            else
            {
                int slot = std::max(idealSlot, lastOccupiedSlot + 1);
                int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
                if (slot >= maxSlots) slot = maxSlots - 1;
                if (slot < 0) slot = 0;
                
                SlotGroup group;
                group.slot = slot;
                group.notes.push_back({noteIdx, notePtr});
                slotGroups.push_back(group);
                lastOccupiedSlot = slot;
            }
            lastStartTime = notePtr->startBeat;
        }
        
        // beatIndex entspricht der Reihenfolge der Slot-Gruppen (nicht dem Slot-Wert selbst!)
        int currentBeatIdx = 0;
        int currentSlot = 0;
        int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
        size_t groupIdx = 0;
        
        while (currentSlot < maxSlots && groupIdx <= slotGroups.size())
        {
            if (groupIdx < slotGroups.size() && slotGroups[groupIdx].slot == currentSlot)
            {
                // Dies ist ein Beat mit Noten
                if (currentBeatIdx == beatIndex)
                {
                    // Gefunden! Suche nach der Note mit der alten String-Position
                    for (auto& [recIdx, notePtr] : slotGroups[groupIdx].notes)
                    {
                        if (notePtr->string == oldString)
                        {
                            notePtr->string = newString;
                            notePtr->fret = newFret;
                            DBG("Updated recordedNotes[" << recIdx << "] string " << oldString 
                                << " -> " << newFret << "/" << newString);
                            return;
                        }
                    }
                    DBG("Note not found on string " << oldString << " in beat " << beatIndex);
                    return;
                }
                
                // Berechne Slot-Sprung
                int nextSlot = maxSlots;
                if (groupIdx + 1 < slotGroups.size())
                    nextSlot = slotGroups[groupIdx + 1].slot;
                    
                currentSlot = nextSlot;
                groupIdx++;
                currentBeatIdx++;
            }
            else
            {
                // Pause
                int nextNoteSlot = (groupIdx < slotGroups.size()) ? slotGroups[groupIdx].slot : maxSlots;
                
                while (currentSlot < nextNoteSlot)
                {
                    int remaining = nextNoteSlot - currentSlot;
                    int pauseDuration;
                    if (remaining >= 32) pauseDuration = 32;
                    else if (remaining >= 16) pauseDuration = 16;
                    else if (remaining >= 8) pauseDuration = 8;
                    else if (remaining >= 4) pauseDuration = 4;
                    else if (remaining >= 2) pauseDuration = 2;
                    else pauseDuration = 1;
                    
                    currentSlot += pauseDuration;
                    currentBeatIdx++;
                }
            }
        }
        
        DBG("Beat " << beatIndex << " not found in measure " << (measureIndex + 1) 
            << " (had " << slotGroups.size() << " note groups, iterated to beat " << currentBeatIdx << ")");
    }
}

void NewProjectAudioProcessor::setEditedTrack(int trackIndex, const TabTrack& track)
{
    editedTracks[trackIndex] = track;
}

//==============================================================================
// Delete a recorded note by measure/beat/string
//==============================================================================
void NewProjectAudioProcessor::deleteRecordedNote(int measureIndex, int beatIndex, int stringIndex)
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    if (recordedNotes.empty()) return;
    
    int numerator = hostTimeSigNumerator.load();
    int denominator = hostTimeSigDenominator.load();
    double beatsPerMeasure = numerator * (4.0 / denominator);
    int barNum = measureIndex + 1;
    
    // Collect notes in this measure (same logic as updateRecordedNotePosition)
    std::vector<std::pair<size_t, RecordedNote*>> notesInMeasure;
    for (size_t i = 0; i < recordedNotes.size(); ++i)
    {
        auto& note = recordedNotes[i];
        double roundedPPQ = std::round(note.startBeat * 1000.0) / 1000.0;
        int noteBar = static_cast<int>(roundedPPQ / beatsPerMeasure) + 1;
        
        double positionInMeasure = roundedPPQ - (noteBar - 1) * beatsPerMeasure;
        double distanceToNextBar = beatsPerMeasure - positionInMeasure;
        double originalDuration = note.endBeat - note.startBeat;
        
        if (measureQuantizationEnabled.load() && distanceToNextBar < 0.5 && distanceToNextBar > 0.001)
        {
            double truncationRatio = distanceToNextBar / std::max(0.001, originalDuration);
            if (truncationRatio < 0.25 && originalDuration > 0.25)
                noteBar = noteBar + 1;
        }
        
        if (noteBar == barNum)
            notesInMeasure.push_back({i, &note});
    }
    
    if (notesInMeasure.empty()) return;
    
    // Sort by startBeat
    std::sort(notesInMeasure.begin(), notesInMeasure.end(),
        [](const auto& a, const auto& b) { return a.second->startBeat < b.second->startBeat; });
    
    // Group into slots (same logic as updateRecordedNotePosition)
    double subdivision = 0.125;
    double chordThreshold = 0.08;
    double measureStartBeat = (barNum - 1) * beatsPerMeasure;
    
    struct SlotGroup { int slot; std::vector<std::pair<size_t, RecordedNote*>> notes; };
    std::vector<SlotGroup> slotGroups;
    int lastOccupiedSlot = -1;
    double lastStartTime = -999.0;
    
    for (auto& [noteIdx, notePtr] : notesInMeasure)
    {
        double posInMeasure = notePtr->startBeat - measureStartBeat;
        if (posInMeasure < 0.0) posInMeasure = 0.0;
        int idealSlot = (int)(posInMeasure / subdivision + 0.5);
        
        bool isChord = !slotGroups.empty() && std::abs(notePtr->startBeat - lastStartTime) < chordThreshold;
        if (isChord)
        {
            slotGroups.back().notes.push_back({noteIdx, notePtr});
        }
        else
        {
            int slot = std::max(idealSlot, lastOccupiedSlot + 1);
            int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
            slot = std::clamp(slot, 0, maxSlots - 1);
            SlotGroup group;
            group.slot = slot;
            group.notes.push_back({noteIdx, notePtr});
            slotGroups.push_back(group);
            lastOccupiedSlot = slot;
        }
        lastStartTime = notePtr->startBeat;
    }
    
    // Walk through beats to find the matching beatIndex
    int currentBeatIdx = 0;
    int currentSlot = 0;
    int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
    size_t groupIdx = 0;
    
    while (currentSlot < maxSlots && groupIdx <= slotGroups.size())
    {
        if (groupIdx < slotGroups.size() && slotGroups[groupIdx].slot == currentSlot)
        {
            if (currentBeatIdx == beatIndex)
            {
                // Found the beat! Delete the note on stringIndex
                for (auto& [recIdx, notePtr] : slotGroups[groupIdx].notes)
                {
                    if (notePtr->string == stringIndex)
                    {
                        DBG("Deleting recordedNotes[" << recIdx << "] on string " << stringIndex);
                        recordedNotes.erase(recordedNotes.begin() + recIdx);
                        return;
                    }
                }
                DBG("Note not found on string " << stringIndex << " in beat " << beatIndex);
                return;
            }
            
            int nextSlot = maxSlots;
            if (groupIdx + 1 < slotGroups.size())
                nextSlot = slotGroups[groupIdx + 1].slot;
            currentSlot = nextSlot;
            groupIdx++;
            currentBeatIdx++;
        }
        else
        {
            int nextNoteSlot = (groupIdx < slotGroups.size()) ? slotGroups[groupIdx].slot : maxSlots;
            while (currentSlot < nextNoteSlot)
            {
                int remaining = nextNoteSlot - currentSlot;
                int pauseDuration;
                if (remaining >= 32) pauseDuration = 32;
                else if (remaining >= 16) pauseDuration = 16;
                else if (remaining >= 8) pauseDuration = 8;
                else if (remaining >= 4) pauseDuration = 4;
                else if (remaining >= 2) pauseDuration = 2;
                else pauseDuration = 1;
                currentSlot += pauseDuration;
                currentBeatIdx++;
            }
        }
    }
}

//==============================================================================
// Update recorded note duration (adjust endBeat based on new duration)
//==============================================================================
void NewProjectAudioProcessor::updateRecordedNoteDuration(int measureIndex, int beatIndex, int newDurationValue, bool isDotted)
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    if (recordedNotes.empty()) return;
    
    int numerator = hostTimeSigNumerator.load();
    int denominator = hostTimeSigDenominator.load();
    double beatsPerMeasure = numerator * (4.0 / denominator);
    int barNum = measureIndex + 1;
    double measureStartBeat = (barNum - 1) * beatsPerMeasure;
    
    // Calculate new duration in quarter notes
    double newDurInQuarters = 4.0 / static_cast<double>(newDurationValue);
    if (isDotted) newDurInQuarters *= 1.5;
    
    // Collect notes in this measure
    std::vector<std::pair<size_t, RecordedNote*>> notesInMeasure;
    for (size_t i = 0; i < recordedNotes.size(); ++i)
    {
        auto& note = recordedNotes[i];
        double roundedPPQ = std::round(note.startBeat * 1000.0) / 1000.0;
        int noteBar = static_cast<int>(roundedPPQ / beatsPerMeasure) + 1;
        
        double positionInMeasure = roundedPPQ - (noteBar - 1) * beatsPerMeasure;
        double distanceToNextBar = beatsPerMeasure - positionInMeasure;
        double originalDuration = note.endBeat - note.startBeat;
        
        if (measureQuantizationEnabled.load() && distanceToNextBar < 0.5 && distanceToNextBar > 0.001)
        {
            double truncationRatio = distanceToNextBar / std::max(0.001, originalDuration);
            if (truncationRatio < 0.25 && originalDuration > 0.25)
                noteBar = noteBar + 1;
        }
        
        if (noteBar == barNum)
            notesInMeasure.push_back({i, &note});
    }
    
    if (notesInMeasure.empty()) return;
    
    std::sort(notesInMeasure.begin(), notesInMeasure.end(),
        [](const auto& a, const auto& b) { return a.second->startBeat < b.second->startBeat; });
    
    // Group into slots
    double subdivision = 0.125;
    double chordThreshold = 0.08;
    
    struct SlotGroup { int slot; std::vector<std::pair<size_t, RecordedNote*>> notes; };
    std::vector<SlotGroup> slotGroups;
    int lastOccupiedSlot = -1;
    double lastStartTime = -999.0;
    
    for (auto& [noteIdx, notePtr] : notesInMeasure)
    {
        double posInMeasure = notePtr->startBeat - measureStartBeat;
        if (posInMeasure < 0.0) posInMeasure = 0.0;
        int idealSlot = (int)(posInMeasure / subdivision + 0.5);
        
        bool isChord = !slotGroups.empty() && std::abs(notePtr->startBeat - lastStartTime) < chordThreshold;
        if (isChord)
        {
            slotGroups.back().notes.push_back({noteIdx, notePtr});
        }
        else
        {
            int slot = std::max(idealSlot, lastOccupiedSlot + 1);
            int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
            slot = std::clamp(slot, 0, maxSlots - 1);
            SlotGroup group;
            group.slot = slot;
            group.notes.push_back({noteIdx, notePtr});
            slotGroups.push_back(group);
            lastOccupiedSlot = slot;
        }
        lastStartTime = notePtr->startBeat;
    }
    
    // Walk through beats to find the matching beatIndex
    int currentBeatIdx = 0;
    int currentSlot = 0;
    int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
    size_t groupIdx = 0;
    
    while (currentSlot < maxSlots && groupIdx <= slotGroups.size())
    {
        if (groupIdx < slotGroups.size() && slotGroups[groupIdx].slot == currentSlot)
        {
            if (currentBeatIdx == beatIndex)
            {
                // Found the beat! Update all notes' endBeat to reflect the new duration
                for (auto& [recIdx, notePtr] : slotGroups[groupIdx].notes)
                {
                    double newEnd = notePtr->startBeat + newDurInQuarters;
                    // Clamp to measure end
                    double measureEnd = measureStartBeat + beatsPerMeasure;
                    if (newEnd > measureEnd) newEnd = measureEnd;
                    notePtr->endBeat = newEnd;
                    DBG("Updated recordedNotes[" << recIdx << "] duration to " << newDurInQuarters << " quarters");
                }
                return;
            }
            
            int nextSlot = maxSlots;
            if (groupIdx + 1 < slotGroups.size())
                nextSlot = slotGroups[groupIdx + 1].slot;
            currentSlot = nextSlot;
            groupIdx++;
            currentBeatIdx++;
        }
        else
        {
            int nextNoteSlot = (groupIdx < slotGroups.size()) ? slotGroups[groupIdx].slot : maxSlots;
            while (currentSlot < nextNoteSlot)
            {
                int remaining = nextNoteSlot - currentSlot;
                int pauseDuration;
                if (remaining >= 32) pauseDuration = 32;
                else if (remaining >= 16) pauseDuration = 16;
                else if (remaining >= 8) pauseDuration = 8;
                else if (remaining >= 4) pauseDuration = 4;
                else if (remaining >= 2) pauseDuration = 2;
                else pauseDuration = 1;
                currentSlot += pauseDuration;
                currentBeatIdx++;
            }
        }
    }
}

//==============================================================================
// Update a note's pitch in recordedNotes (from manual editing)
//==============================================================================
void NewProjectAudioProcessor::updateRecordedNotePitch(int measureIndex, int beatIndex, int oldString, int newMidiNote, int newFret)
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    if (recordedNotes.empty()) return;
    
    int numerator = hostTimeSigNumerator.load();
    int denominator = hostTimeSigDenominator.load();
    double beatsPerMeasure = numerator * (4.0 / denominator);
    int barNum = measureIndex + 1;
    double measureStartBeat = (barNum - 1) * beatsPerMeasure;
    
    // Collect notes in this measure
    std::vector<std::pair<size_t, RecordedNote*>> notesInMeasure;
    for (size_t i = 0; i < recordedNotes.size(); ++i)
    {
        auto& note = recordedNotes[i];
        double roundedPPQ = std::round(note.startBeat * 1000.0) / 1000.0;
        int noteBar = static_cast<int>(roundedPPQ / beatsPerMeasure) + 1;
        
        double positionInMeasure = roundedPPQ - (noteBar - 1) * beatsPerMeasure;
        double distanceToNextBar = beatsPerMeasure - positionInMeasure;
        double originalDuration = note.endBeat - note.startBeat;
        
        if (measureQuantizationEnabled.load() && distanceToNextBar < 0.5 && distanceToNextBar > 0.001)
        {
            double truncationRatio = distanceToNextBar / std::max(0.001, originalDuration);
            if (truncationRatio < 0.25 && originalDuration > 0.25)
                noteBar = noteBar + 1;
        }
        
        if (noteBar == barNum)
            notesInMeasure.push_back({i, &note});
    }
    
    if (notesInMeasure.empty())
    {
        DBG("No recorded notes found in measure " << (measureIndex + 1) << " for pitch change");
        return;
    }
    
    std::sort(notesInMeasure.begin(), notesInMeasure.end(),
        [](const auto& a, const auto& b) { return a.second->startBeat < b.second->startBeat; });
    
    // Group into slots
    double subdivision = 0.125;
    double chordThreshold = 0.08;
    
    struct SlotGroup { int slot; std::vector<std::pair<size_t, RecordedNote*>> notes; };
    std::vector<SlotGroup> slotGroups;
    int lastOccupiedSlot = -1;
    double lastStartTime = -999.0;
    
    for (auto& [noteIdx, notePtr] : notesInMeasure)
    {
        double posInMeasure = notePtr->startBeat - measureStartBeat;
        if (posInMeasure < 0.0) posInMeasure = 0.0;
        int idealSlot = (int)(posInMeasure / subdivision + 0.5);
        
        bool isChord = !slotGroups.empty() && std::abs(notePtr->startBeat - lastStartTime) < chordThreshold;
        if (isChord)
        {
            slotGroups.back().notes.push_back({noteIdx, notePtr});
        }
        else
        {
            int slot = std::max(idealSlot, lastOccupiedSlot + 1);
            int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
            slot = std::clamp(slot, 0, maxSlots - 1);
            SlotGroup group;
            group.slot = slot;
            group.notes.push_back({noteIdx, notePtr});
            slotGroups.push_back(group);
            lastOccupiedSlot = slot;
        }
        lastStartTime = notePtr->startBeat;
    }
    
    // Walk through beats to find the matching beatIndex
    int currentBeatIdx = 0;
    int currentSlot = 0;
    int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
    size_t groupIdx = 0;
    
    while (currentSlot < maxSlots && groupIdx <= slotGroups.size())
    {
        if (groupIdx < slotGroups.size() && slotGroups[groupIdx].slot == currentSlot)
        {
            if (currentBeatIdx == beatIndex)
            {
                // Found the beat! Find note on the old string and update pitch
                for (auto& [recIdx, notePtr] : slotGroups[groupIdx].notes)
                {
                    if (notePtr->string == oldString)
                    {
                        int oldMidi = notePtr->midiNote;
                        notePtr->midiNote = newMidiNote;
                        notePtr->fret = newFret;
                        // String may also change if fret position calculation moved it
                        // We'll update string through updateRecordedNotePosition pattern
                        DBG("Updated recordedNotes[" << recIdx << "] pitch " << oldMidi 
                            << " -> " << newMidiNote << ", fret " << newFret);
                        return;
                    }
                }
                DBG("Note not found on string " << oldString << " for pitch change in beat " << beatIndex);
                return;
            }
            
            int nextSlot = maxSlots;
            if (groupIdx + 1 < slotGroups.size())
                nextSlot = slotGroups[groupIdx + 1].slot;
            currentSlot = nextSlot;
            groupIdx++;
            currentBeatIdx++;
        }
        else
        {
            int nextNoteSlot = (groupIdx < slotGroups.size()) ? slotGroups[groupIdx].slot : maxSlots;
            while (currentSlot < nextNoteSlot)
            {
                int remaining = nextNoteSlot - currentSlot;
                int pauseDuration;
                if (remaining >= 32) pauseDuration = 32;
                else if (remaining >= 16) pauseDuration = 16;
                else if (remaining >= 8) pauseDuration = 8;
                else if (remaining >= 4) pauseDuration = 4;
                else if (remaining >= 2) pauseDuration = 2;
                else pauseDuration = 1;
                currentSlot += pauseDuration;
                currentBeatIdx++;
            }
        }
    }
}

void NewProjectAudioProcessor::insertRecordedNote(int measureIndex, int beatIndex, int stringIndex, int fret, int midiNote)
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    int numerator = hostTimeSigNumerator.load();
    int denominator = hostTimeSigDenominator.load();
    double beatsPerMeasure = numerator * (4.0 / denominator);
    int barNum = measureIndex + 1;
    double measureStartBeat = (barNum - 1) * beatsPerMeasure;
    
    // We need to find the beat position (in PPQ) where this rest starts
    // Use the same slot-group logic to walk through beats
    
    // Collect notes in this measure
    std::vector<std::pair<size_t, RecordedNote*>> notesInMeasure;
    for (size_t i = 0; i < recordedNotes.size(); ++i)
    {
        auto& note = recordedNotes[i];
        double roundedPPQ = std::round(note.startBeat * 1000.0) / 1000.0;
        int noteBar = static_cast<int>(roundedPPQ / beatsPerMeasure) + 1;
        
        double positionInMeasure = roundedPPQ - (noteBar - 1) * beatsPerMeasure;
        double distanceToNextBar = beatsPerMeasure - positionInMeasure;
        double originalDuration = note.endBeat - note.startBeat;
        
        if (measureQuantizationEnabled.load() && distanceToNextBar < 0.5 && distanceToNextBar > 0.001)
        {
            double truncationRatio = distanceToNextBar / std::max(0.001, originalDuration);
            if (truncationRatio < 0.25 && originalDuration > 0.25)
                noteBar = noteBar + 1;
        }
        
        if (noteBar == barNum)
            notesInMeasure.push_back({i, &note});
    }
    
    // Sort by startBeat
    std::sort(notesInMeasure.begin(), notesInMeasure.end(),
        [](const auto& a, const auto& b) { return a.second->startBeat < b.second->startBeat; });
    
    // Group into slots
    double subdivision = 0.125;
    double chordThreshold = 0.08;
    
    struct SlotGroup { int slot; std::vector<std::pair<size_t, RecordedNote*>> notes; };
    std::vector<SlotGroup> slotGroups;
    int lastOccupiedSlot = -1;
    double lastStartTime = -999.0;
    
    for (auto& [noteIdx, notePtr] : notesInMeasure)
    {
        double posInMeasure = notePtr->startBeat - measureStartBeat;
        if (posInMeasure < 0.0) posInMeasure = 0.0;
        int idealSlot = (int)(posInMeasure / subdivision + 0.5);
        
        bool isChord = !slotGroups.empty() && std::abs(notePtr->startBeat - lastStartTime) < chordThreshold;
        if (isChord)
        {
            slotGroups.back().notes.push_back({noteIdx, notePtr});
        }
        else
        {
            int slot = std::max(idealSlot, lastOccupiedSlot + 1);
            int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
            slot = std::clamp(slot, 0, maxSlots - 1);
            SlotGroup group;
            group.slot = slot;
            group.notes.push_back({noteIdx, notePtr});
            slotGroups.push_back(group);
            lastOccupiedSlot = slot;
        }
        lastStartTime = notePtr->startBeat;
    }
    
    // Walk through beats to find the target rest position
    int currentBeatIdx = 0;
    int currentSlot = 0;
    int maxSlots = static_cast<int>(beatsPerMeasure / subdivision);
    size_t groupIdx = 0;
    
    while (currentSlot < maxSlots && groupIdx <= slotGroups.size())
    {
        if (groupIdx < slotGroups.size() && slotGroups[groupIdx].slot == currentSlot)
        {
            // This is a note beat - skip
            int nextSlot = maxSlots;
            if (groupIdx + 1 < slotGroups.size())
                nextSlot = slotGroups[groupIdx + 1].slot;
            currentSlot = nextSlot;
            groupIdx++;
            currentBeatIdx++;
        }
        else
        {
            // This is a rest region
            int nextNoteSlot = (groupIdx < slotGroups.size()) ? slotGroups[groupIdx].slot : maxSlots;
            while (currentSlot < nextNoteSlot)
            {
                int remaining = nextNoteSlot - currentSlot;
                int pauseDuration;
                if (remaining >= 32) pauseDuration = 32;
                else if (remaining >= 16) pauseDuration = 16;
                else if (remaining >= 8) pauseDuration = 8;
                else if (remaining >= 4) pauseDuration = 4;
                else if (remaining >= 2) pauseDuration = 2;
                else pauseDuration = 1;
                
                if (currentBeatIdx == beatIndex)
                {
                    // Found the rest position! Insert a new RecordedNote here
                    double insertBeat = measureStartBeat + currentSlot * subdivision;
                    double noteDuration = pauseDuration * subdivision;
                    
                    RecordedNote newNote;
                    newNote.midiNote = midiNote;
                    newNote.velocity = 100;
                    newNote.string = stringIndex;
                    newNote.fret = fret;
                    newNote.startBeat = insertBeat;
                    newNote.endBeat = insertBeat + noteDuration;
                    newNote.isActive = false;
                    newNote.midiChannel = 1;
                    
                    recordedNotes.push_back(newNote);
                    
                    // Sort by startBeat to keep order
                    std::sort(recordedNotes.begin(), recordedNotes.end(),
                        [](const RecordedNote& a, const RecordedNote& b) { return a.startBeat < b.startBeat; });
                    
                    DBG("Inserted note MIDI " << midiNote << " at beat " << insertBeat 
                        << " (measure " << (measureIndex + 1) << ", beat " << beatIndex 
                        << ", string " << stringIndex << ", fret " << fret << ")");
                    return;
                }
                
                currentSlot += pauseDuration;
                currentBeatIdx++;
            }
        }
    }
    
    // If we got here and didn't insert (e.g. empty measure with no notes)
    if (notesInMeasure.empty() && beatIndex == 0)
    {
        // Empty measure - insert at measure start
        double insertBeat = measureStartBeat;
        double noteDuration = 1.0;  // Default quarter note duration
        
        RecordedNote newNote;
        newNote.midiNote = midiNote;
        newNote.velocity = 100;
        newNote.string = stringIndex;
        newNote.fret = fret;
        newNote.startBeat = insertBeat;
        newNote.endBeat = insertBeat + noteDuration;
        newNote.isActive = false;
        newNote.midiChannel = 1;
        
        recordedNotes.push_back(newNote);
        std::sort(recordedNotes.begin(), recordedNotes.end(),
            [](const RecordedNote& a, const RecordedNote& b) { return a.startBeat < b.startBeat; });
        
        DBG("Inserted note MIDI " << midiNote << " at start of empty measure " << (measureIndex + 1));
        return;
    }
    
    DBG("Could not find rest at beat " << beatIndex << " in measure " << (measureIndex + 1) << " for insertion");
}

TabTrack NewProjectAudioProcessor::getRecordedTabTrack() const
{
    TabTrack track;
    track.name = "Recording";
    track.stringCount = 6;
    track.tuning = { 64, 59, 55, 50, 45, 40 };  // E-Standard (High to Low)
    track.colour = juce::Colours::red;
    
    int numerator = hostTimeSigNumerator.load();
    int denominator = hostTimeSigDenominator.load();
    
    // Bei 4/4: beatsPerMeasure = 4 (jeder Takt hat 4 Quarter Notes)
    double beatsPerMeasure = numerator * (4.0 / denominator);
    
    // Hole aufgezeichnete Noten - die string/fret Werte wurden bereits während
    // der Aufnahme von updateRecordedNotesFromLive() mit den Live-Werten synchronisiert!
    std::vector<RecordedNote> notes;
    double startBeatRef = 0.0;
    {
        std::lock_guard<std::mutex> lock(recordingMutex);
        notes = recordedNotes;
        startBeatRef = recordingStartBeat;
    }
    
    // =========================================================================
    // Build the TabTrack from the recorded notes (already optimized during recording)
    // =========================================================================
    
    // Berechne, in welchem DAW-Takt die Aufnahme begann (1-basiert)
    // ppqPosition 0-3.99 = Takt 1, 4-7.99 = Takt 2, etc.
    int firstMeasureNumber = (int)(startBeatRef / beatsPerMeasure) + 1;
    
    if (notes.empty())
    {
        // Leere Takte zurückgeben
        for (int m = 0; m < 16; ++m)
        {
            TabMeasure measure;
            measure.measureNumber = m + 1;
            measure.timeSignatureNumerator = numerator;
            measure.timeSignatureDenominator = denominator;
            track.measures.add(measure);
        }
        return track;
    }
    
    // Sortiere Noten nach Startzeit
    std::sort(notes.begin(), notes.end(), [](const RecordedNote& a, const RecordedNote& b) {
        return a.startBeat < b.startBeat;
    });
    
    // =========================================================================
    // LEGATO QUANTIZATION: Extend note durations to fill small gaps
    // This helps when the audio-to-MIDI engine produces too-short notes
    // =========================================================================
    double legatoThreshold = legatoQuantizationThreshold.load();
    if (legatoThreshold > 0.001)
    {
        // Group notes by string for legato processing
        // (notes on the same string should connect, different strings are independent)
        std::map<int, std::vector<size_t>> notesByString;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            notesByString[notes[i].string].push_back(i);
        }
        
        // Process each string independently
        for (auto& [stringNum, indices] : notesByString)
        {
            // Sort indices by start time (should already be sorted, but be safe)
            std::sort(indices.begin(), indices.end(), [&notes](size_t a, size_t b) {
                return notes[a].startBeat < notes[b].startBeat;
            });
            
            // Extend each note to the next note if the gap is small enough
            for (size_t i = 0; i + 1 < indices.size(); ++i)
            {
                RecordedNote& currentNote = notes[indices[i]];
                const RecordedNote& nextNote = notes[indices[i + 1]];
                
                double gap = nextNote.startBeat - currentNote.endBeat;
                
                // If gap is small (likely an artifact of early note-off), extend to next note
                if (gap > 0.0 && gap <= legatoThreshold)
                {
                    currentNote.endBeat = nextNote.startBeat;
                }
            }
        }
        
        // Also extend notes that end just before the next event on ANY string
        // (to avoid tiny rests before chord changes)
        for (size_t i = 0; i < notes.size(); ++i)
        {
            RecordedNote& note = notes[i];
            
            // Find the next event (any note starting after this note's current end)
            double nextEventStart = std::numeric_limits<double>::max();
            for (size_t j = 0; j < notes.size(); ++j)
            {
                if (notes[j].startBeat > note.endBeat)
                {
                    nextEventStart = std::min(nextEventStart, notes[j].startBeat);
                }
            }
            
            // If there's a tiny gap to the next event, extend
            if (nextEventStart < std::numeric_limits<double>::max())
            {
                double gap = nextEventStart - note.endBeat;
                if (gap > 0.0 && gap <= legatoThreshold * 0.5) // Tighter threshold for cross-string
                {
                    note.endBeat = nextEventStart;
                }
            }
        }
    }
    
    // Finde den letzten Beat
    double maxBeat = 0.0;
    for (const auto& note : notes)
    {
        maxBeat = std::max(maxBeat, note.endBeat);
    }
    
    // Berechne Anzahl der Takte ab dem Start-Takt
    // Takte werden relativ zum Aufnahme-Start erstellt
    int lastMeasureNumber = (int)(maxBeat / beatsPerMeasure) + 1;
    int numMeasures = std::max(16, lastMeasureNumber + 2);
    
    // Erstelle Takte - die measureNumber entspricht direkt der DAW-Taktnummer
    // ppqPosition 0-3.99 = Takt 1, ppqPosition 4-7.99 = Takt 2, etc.
    // Wir iterieren über Taktnummern (1-basiert wie im DAW)
    for (int barNum = 1; barNum <= numMeasures; ++barNum)
    {
        TabMeasure measure;
        measure.measureNumber = barNum;
        measure.timeSignatureNumerator = numerator;
        measure.timeSignatureDenominator = denominator;
        
        // Der Takt barNum entspricht ppqPosition [(barNum-1)*beatsPerMeasure, barNum*beatsPerMeasure)
        double measureStartBeat = (barNum - 1) * beatsPerMeasure;
        
        // Sammle alle Noten in diesem Takt
        // Mit intelligenter Quantisierung: Wenn eine Note nahe am Taktanfang beginnt
        // und stark gekürzt werden müsste, verschiebe sie in den nächsten Takt
        std::vector<const RecordedNote*> notesInMeasure;
        for (const auto& note : notes)
        {
            // Bestimme den Takt dieser Note mit Toleranz für Floating-Point-Fehler
            // Ein startBeat von z.B. 7.999 soll als Takt 3 (= Beat 8.0) erkannt werden,
            // nicht als kurze Note am Ende von Takt 2
            double startBeat = note.startBeat;
            double originalDuration = note.endBeat - note.startBeat;
            
            // Berechne den "natürlichen" Takt der Note
            int noteBar = static_cast<int>(startBeat / beatsPerMeasure) + 1;
            double positionInMeasure = startBeat - (noteBar - 1) * beatsPerMeasure;
            double distanceToNextBar = beatsPerMeasure - positionInMeasure;
            
            // Takt-Quantisierung: Verschiebe Noten am Taktende in den nächsten Takt
            if (measureQuantizationEnabled.load() && distanceToNextBar < 0.5 && distanceToNextBar > 0.0001)
            {
                // Wie viel der Note passt noch in diesen Takt?
                double truncatedDuration = distanceToNextBar;
                
                // Bedingung 1: Note ist kürzer als eine 32tel-Note im aktuellen Takt
                // (= sie wird zu kurz um sinnvoll dargestellt zu werden)
                bool tooShortInCurrentBar = truncatedDuration < 0.125;
                
                // Bedingung 2: Note würde auf weniger als 33% ihrer Länge gekürzt
                // UND ist mindestens eine 16tel-Note lang
                double truncationRatio = truncatedDuration / std::max(0.001, originalDuration);
                bool severelyTruncated = (truncationRatio < 0.33 && originalDuration >= 0.125);
                
                if (tooShortInCurrentBar || severelyTruncated)
                {
                    noteBar = noteBar + 1; // Verschiebe in nächsten Takt
                }
            }
            
            if (noteBar == barNum)
            {
                notesInMeasure.push_back(&note);
            }
        }
        
        if (notesInMeasure.empty())
        {
            // Fülle leere Takte mit Pausen
            double remainingSlots = beatsPerMeasure / 0.125;
            while (remainingSlots > 0.1) // Float safety
            {
                TabBeat beat;
                beat.isRest = true;
                
                // Max duration check
                if (remainingSlots >= 32.0) { beat.duration = NoteDuration::Whole; remainingSlots -= 32.0; }
                else if (remainingSlots >= 16.0) { beat.duration = NoteDuration::Half; remainingSlots -= 16.0; }
                else if (remainingSlots >= 8.0) { beat.duration = NoteDuration::Quarter; remainingSlots -= 8.0; }
                else if (remainingSlots >= 4.0) { beat.duration = NoteDuration::Eighth; remainingSlots -= 4.0; }
                else if (remainingSlots >= 2.0) { beat.duration = NoteDuration::Sixteenth; remainingSlots -= 2.0; }
                else { beat.duration = NoteDuration::ThirtySecond; remainingSlots -= 1.0; }
                
                // Add empty notes for GP5 writer
                for (int s = 0; s < 6; ++s) {
                   TabNote emptyNote; emptyNote.string = s; emptyNote.fret = -1; beat.notes.add(emptyNote);
                }
                measure.beats.add(beat);
            }
            track.measures.add(measure);
            continue;
        }
        
        // 32nd note resolution (0.125 beats)
        const double subdivision = 0.125;
        const int maxSlots = (int)(beatsPerMeasure / subdivision + 0.5);
        
        // --- CHORD DETECTION & SEQUENCING ---
        // Gruppiere Noten nach "Musikalischen Events" (Akkorde vs. Sequenzen)
        // um zu verhindern, dass schnelle Triller als Akkord im selben Slot landen.
        
        // 1. Sortiere Noten zeitlich
        std::vector<const RecordedNote*> sortedNotes = notesInMeasure;
        std::sort(sortedNotes.begin(), sortedNotes.end(), 
            [](const RecordedNote* a, const RecordedNote* b) { return a->startBeat < b->startBeat; });

        // 2. Cluster Noten in Events
        struct MusicalEvent {
            double startTimeSum = 0.0;
            std::vector<const RecordedNote*> notes;
            double getStartTime() const { return notes.empty() ? 0.0 : startTimeSum / notes.size(); }
        };
        std::vector<MusicalEvent> events;
        
        // Toleranz für Akkorde: 0.06 Beats (ca. 30ms bei 120bpm)
        const double chordThreshold = 0.06;

        for (const auto* note : sortedNotes) {
            if (events.empty()) {
                MusicalEvent evt;
                evt.notes.push_back(note);
                evt.startTimeSum = note->startBeat;
                events.push_back(evt);
            } else {
                auto& lastEvt = events.back();
                double avgStart = lastEvt.getStartTime();
                
                // Wenn Note sehr nah am vorherigen Event startet -> Teil des Akkords
                if ((note->startBeat - avgStart) < chordThreshold) {
                    lastEvt.notes.push_back(note);
                    lastEvt.startTimeSum += note->startBeat;
                } else {
                    // Neues Event (z.B. nächster Ton im Triller)
                    MusicalEvent evt;
                    evt.notes.push_back(note);
                    evt.startTimeSum = note->startBeat;
                    events.push_back(evt);
                }
            }
        }

        // 3. Map Events auf Slots (mit Konfliktlösung)
        std::map<int, std::vector<const RecordedNote*>> noteGroups;
        int lastOccupiedSlot = -1;

        for (int evtIdx = 0; evtIdx < (int)events.size(); ++evtIdx) {
            const auto& evt = events[evtIdx];
            double posInMeasure = evt.getStartTime() - measureStartBeat;
            
            // Wenn die Position negativ ist (Note wurde aus dem vorherigen Takt verschoben),
            // platziere sie am Taktanfang (Slot 0)
            if (posInMeasure < 0.0)
                posInMeasure = 0.0;
            
            int idealSlot = (int)(posInMeasure / subdivision + 0.5);
            
            // Stelle sicher, dass Events in unterschiedlichen Slots landen
            // (außer sie sind extrem weit weg vom Grid und wir müssen mergen, aber hier priorisieren wir Trennung)
            int slot = std::max(idealSlot, lastOccupiedSlot + 1);
            
            // Limit auf Taktlänge
            if (slot >= maxSlots) slot = maxSlots - 1;
            if (slot < 0) slot = 0;

            for (const auto* n : evt.notes) {
                noteGroups[slot].push_back(n);
            }
            
            // Update last occupied (nur wenn wir wirklich in einem neuen Slot sind)
            if (slot > lastOccupiedSlot) {
                lastOccupiedSlot = slot;
            }
        }
        
        // Iteriere durch die Slots und fülle Lücken mit Pausen
        int currentSlot = 0;
        while (currentSlot < maxSlots)
        {
            auto it = noteGroups.find(currentSlot);
            bool hasNotes = (it != noteGroups.end());
            
            TabBeat beat;
            int durationInSlots = 0;
            
            if (hasNotes)
            {
                beat.isRest = false;
                const auto& group = it->second;
                
                // Bestimme die Dauer basierend auf den Noten und dem nächsten Event
                double minNoteLen = std::numeric_limits<double>::max();
                for (const auto* note : group)
                {
                     double noteLen = note->endBeat - note->startBeat;
                     if (noteLen < 0.001) noteLen = 0.001; // safety
                     minNoteLen = std::min(minNoteLen, noteLen);
                }
                
                int desiredSlots = (int)(minNoteLen / subdivision + 0.5);
                if (desiredSlots < 1) desiredSlots = 1;
                
                // Finde nächstes Event (Note oder Taktende) um Überlappung zu vermeiden
                int nextEventSlot = maxSlots;
                auto nextIt = noteGroups.upper_bound(currentSlot);
                if (nextIt != noteGroups.end())
                    nextEventSlot = nextIt->first;
                
                // Duration limitieren auf Gap zum nächsten Event
                durationInSlots = std::min(desiredSlots, nextEventSlot - currentSlot);
                
                // Snap to standard durations (prefer standard values)
                // 32(1), 24(0.75), 16(0.5), 12(0.375), 8(0.25), 6, 4, 3, 2, 1
                if (durationInSlots >= 32) durationInSlots = 32;      // Whole
                else if (durationInSlots >= 24) durationInSlots = 24; // Dotted Half
                else if (durationInSlots >= 16) durationInSlots = 16; // Half
                else if (durationInSlots >= 12) durationInSlots = 12; // Dotted Quarter
                else if (durationInSlots >= 8) durationInSlots = 8;   // Quarter
                else if (durationInSlots >= 6) durationInSlots = 6;   // Dotted Eighth
                else if (durationInSlots >= 4) durationInSlots = 4;   // Eighth
                else if (durationInSlots >= 3) durationInSlots = 3;   // Dotted 16th
                else if (durationInSlots >= 2) durationInSlots = 2;   // 16th
                else durationInSlots = 1;                             // 32nd
                
                // Noten setzen
                for (int s = 0; s < 6; ++s)
                {
                    TabNote emptyNote; emptyNote.string = s; emptyNote.fret = -1; beat.notes.add(emptyNote);
                }
                
                // === Sicherheitsnetz: String-Konflikte auflösen ===
                // Falls zwei Noten im gleichen Beat die gleiche Saite haben,
                // versuche die zweite auf eine andere Saite umzulegen.
                // Das passiert z.B. bei Oktav-Noten (C3+C4) die im reoptimize
                // nicht korrekt gruppiert wurden.
                {
                    std::set<int> occupiedStrings;
                    // Kopie der Noten, damit wir Konflikte auflösen können
                    struct ResolvedNote {
                        int midiNote;
                        int string;
                        int fret;
                        int velocity;
                        const RecordedNote* original;
                    };
                    std::vector<ResolvedNote> resolvedNotes;
                    for (const auto* note : group)
                    {
                        resolvedNotes.push_back({note->midiNote, note->string, note->fret, note->velocity, note});
                    }
                    
                    for (size_t ni = 0; ni < resolvedNotes.size(); ++ni)
                    {
                        auto& rn = resolvedNotes[ni];
                        if (rn.string >= 0 && rn.string < 6 && occupiedStrings.count(rn.string) == 0)
                        {
                            occupiedStrings.insert(rn.string);
                        }
                        else if (rn.string >= 0 && rn.string < 6)
                        {
                            // Konflikt! Diese Saite ist bereits belegt.
                            // Versuche alternative Saite zu finden
                            bool found = false;
                            int bestAltScore = -100000;
                            int bestAltString = -1;
                            int bestAltFret = -1;
                            
                            for (int s = 0; s < 6; ++s)
                            {
                                if (occupiedStrings.count(s) > 0) continue;
                                int fret = rn.midiNote - standardTuning[s];
                                if (fret >= 0 && fret <= 24)
                                {
                                    // Einfache Bewertung: bevorzuge Positionen nahe der aktuellen
                                    int score = 100 - std::abs(fret - rn.fret) * 10;
                                    // Check Fret-Spannweite mit bereits platzierten Noten
                                    int minF = fret, maxF = fret;
                                    for (size_t oi = 0; oi < ni; ++oi)
                                    {
                                        if (resolvedNotes[oi].fret > 0)
                                        {
                                            minF = std::min(minF, resolvedNotes[oi].fret);
                                            maxF = std::max(maxF, resolvedNotes[oi].fret);
                                        }
                                    }
                                    if (fret > 0) { minF = std::min(minF, fret); maxF = std::max(maxF, fret); }
                                    if (minF <= maxF && maxF - minF > 3) 
                                        score -= 500;  // Starke Strafe für > 3 Bünde Spannweite
                                    
                                    if (score > bestAltScore)
                                    {
                                        bestAltScore = score;
                                        bestAltString = s;
                                        bestAltFret = fret;
                                    }
                                }
                            }
                            
                            if (bestAltString >= 0)
                            {
                                rn.string = bestAltString;
                                rn.fret = bestAltFret;
                                occupiedStrings.insert(bestAltString);
                            }
                            // Sonst: Note kann leider nicht platziert werden (alle Saiten belegt)
                        }
                    }
                    
                    // Jetzt die aufgelösten Noten in den Beat schreiben
                    for (const auto& rn : resolvedNotes)
                    {
                        if (rn.string >= 0 && rn.string < 6)
                        {
                            beat.notes.getReference(rn.string).fret = rn.fret;
                            beat.notes.getReference(rn.string).velocity = rn.velocity;
                            
                            // === Apply Finger Number ===
                            if (rn.original->fingerNumber >= 0)
                                beat.notes.getReference(rn.string).fingerNumber = rn.original->fingerNumber;
                            
                            // === Apply Recorded Effects ===
                            auto& tabNote = beat.notes.getReference(rn.string);
                            const auto* note = rn.original;
                        
                            // Vibrato
                            if (note->hasVibrato)
                                tabNote.effects.vibrato = true;
                            
                            // Bending - Threshold: 0.5 Halbtöne (50 cents)
                            // MIDI Pitch Wheel Bends sind immer bewusst vom Spieler
                            // (Audio-Transcription hat eigenen höheren Threshold)
                            if (note->maxBendValue >= 0.5f)
                            {
                                tabNote.effects.bend = true;
                                tabNote.effects.bendValue = note->maxBendValue;
                                
                                // Convert recorded raw events to GP5BendPoints (0-60 scale)
                                if (!note->rawBendEvents.empty())
                                {
                                    double noteStart = note->startBeat;
                                    double noteLen = note->endBeat - note->startBeat;
                                    if (noteLen < 0.001) noteLen = 0.001; // Safety
                                    
                                    tabNote.effects.bendPoints.clear();
                                    
                                    // Always start with a 0-point if not present
                                    if (note->rawBendEvents.front().beat > noteStart + 0.01)
                                    {
                                        TabBendPoint startPt;
                                        startPt.position = 0;
                                        startPt.value = 0;
                                        tabNote.effects.bendPoints.push_back(startPt);
                                    }
                                    
                                    for (const auto& ev : note->rawBendEvents)
                                    {
                                        double relPos = (ev.beat - noteStart) / noteLen;
                                        if (relPos < 0.0) relPos = 0.0;
                                        if (relPos > 1.0) relPos = 1.0;
                                        
                                        TabBendPoint bp;
                                        bp.position = (int)(relPos * 60.0);
                                        // Clamp small values to 0 - only show significant bends in curve
                                        bp.value = (std::abs(ev.value) < 10) ? 0 : ev.value;
                                        
                                        // Filter too close points (GP5 restriction)
                                        if (!tabNote.effects.bendPoints.empty())
                                        {
                                            if (bp.position == tabNote.effects.bendPoints.back().position)
                                               tabNote.effects.bendPoints.back().value = bp.value; // Overwrite same pos
                                            else
                                               tabNote.effects.bendPoints.push_back(bp);
                                        }
                                        else
                                            tabNote.effects.bendPoints.push_back(bp);
                                    }
                                    
                                    // Ensure end point
                                    if (tabNote.effects.bendPoints.empty() || tabNote.effects.bendPoints.back().position < 60)
                                    {
                                         TabBendPoint endPt;
                                         endPt.position = 60;
                                         // Hold last value
                                         if (!tabNote.effects.bendPoints.empty())
                                             endPt.value = tabNote.effects.bendPoints.back().value;
                                         else
                                             endPt.value = 0;
                                         tabNote.effects.bendPoints.push_back(endPt);
                                    }
                                    
                                    // Determine Bend Type based on curve shape
                                    // 1=Bend, 2=Bend+Release, 3=Release, 4=PreBend, 5=PreBend+Release
                                    bool startsZero = (tabNote.effects.bendPoints.front().value < 10);
                                    bool endsLow = (tabNote.effects.bendPoints.back().value < 10);
                                    
                                    if (startsZero)
                                    {
                                        if (endsLow) tabNote.effects.bendType = 2; // Bend+Release
                                        else tabNote.effects.bendType = 1; // Bend
                                    }
                                    else
                                    {
                                        if (endsLow) tabNote.effects.bendType = 3; // Release (PreBend+Release?)
                                        else tabNote.effects.bendType = 4; // PreBend or Hold
                                    }
                                }
                            }
                        }
                    }
                }  // Ende Sicherheitsnetz-Block
            }
            else
            {
                // Pause einfügen
                beat.isRest = true;
                
                // Finde Länge bis zum nächsten Event
                int nextEventSlot = maxSlots;
                auto nextIt = noteGroups.upper_bound(currentSlot);
                if (nextIt != noteGroups.end()) 
                    nextEventSlot = nextIt->first;
                
                int gap = nextEventSlot - currentSlot;
                
                // Wähle größtmögliche Standarddauer
                if (gap >= 32) durationInSlots = 32;
                else if (gap >= 16) durationInSlots = 16;
                else if (gap >= 8) durationInSlots = 8;
                else if (gap >= 4) durationInSlots = 4;
                else if (gap >= 2) durationInSlots = 2;
                else durationInSlots = 1;
                
                // Leere Noten für GP5
                for (int s = 0; s < 6; ++s) {
                    TabNote emptyNote; emptyNote.string = s; emptyNote.fret = -1; beat.notes.add(emptyNote);
                }
            }
            
            // Setze Duration und Dotted Flags
            beat.isDotted = false;
            switch (durationInSlots)
            {
                case 32: beat.duration = NoteDuration::Whole; break;
                case 24: beat.duration = NoteDuration::Half; beat.isDotted = true; break;
                case 16: beat.duration = NoteDuration::Half; break;
                case 12: beat.duration = NoteDuration::Quarter; beat.isDotted = true; break;
                case 8:  beat.duration = NoteDuration::Quarter; break;
                case 6:  beat.duration = NoteDuration::Eighth; beat.isDotted = true; break;
                case 4:  beat.duration = NoteDuration::Eighth; break;
                case 3:  beat.duration = NoteDuration::Sixteenth; beat.isDotted = true; break;
                case 2:  beat.duration = NoteDuration::Sixteenth; break;
                default: beat.duration = NoteDuration::ThirtySecond; break;
            }
            
            measure.beats.add(beat);
            currentSlot += durationInSlots;
        }
        
        track.measures.add(measure);
    }
    
    return track;
}

std::vector<TabTrack> NewProjectAudioProcessor::getRecordedTabTracks() const
{
    // Group recorded notes by MIDI channel and create a separate TabTrack for each
    std::vector<RecordedNote> allNotes;
    double startBeatRef = 0.0;
    {
        std::lock_guard<std::mutex> lock(recordingMutex);
        allNotes = recordedNotes;
        startBeatRef = recordingStartBeat;
    }
    
    if (allNotes.empty())
    {
        // Return single empty track
        std::vector<TabTrack> result;
        result.push_back(getRecordedTabTrack());
        return result;
    }
    
    // Find all unique MIDI channels used
    std::set<int> usedChannels;
    for (const auto& note : allNotes)
    {
        usedChannels.insert(note.midiChannel);
    }
    
    // If only one channel, use the regular single-track method
    if (usedChannels.size() <= 1)
    {
        std::vector<TabTrack> result;
        result.push_back(getRecordedTabTrack());
        return result;
    }
    
    // Multiple channels - create a track for each
    std::vector<TabTrack> tracks;
    
    int numerator = hostTimeSigNumerator.load();
    int denominator = hostTimeSigDenominator.load();
    double beatsPerMeasure = numerator * (4.0 / denominator);
    
    // Find max beat to determine measure count
    double maxBeat = 0.0;
    for (const auto& note : allNotes)
    {
        maxBeat = std::max(maxBeat, note.endBeat);
    }
    int lastMeasureNumber = (int)(maxBeat / beatsPerMeasure) + 1;
    int numMeasures = std::max(16, lastMeasureNumber + 2);
    
    // Create a track for each channel
    for (int channel : usedChannels)
    {
        // Filter notes for this channel
        std::vector<RecordedNote> channelNotes;
        for (const auto& note : allNotes)
        {
            if (note.midiChannel == channel)
            {
                channelNotes.push_back(note);
            }
        }
        
        TabTrack track;
        int instrument = channelInstruments[channel - 1];
        
        // Channel 10 is the drum channel - use "Drums" as name
        if (channel == 10)
        {
            track.name = "Drums";
            instrument = 0;  // Standard Kit for drums
        }
        else
        {
            const char* instrumentName = (instrument >= 0 && instrument < 128) ? gmInstrumentNames[instrument] : "Unknown";
            track.name = juce::String(instrumentName);
        }
        
        track.stringCount = 6;
        track.tuning = { 64, 59, 55, 50, 45, 40 };  // E-Standard
        track.midiChannel = channel - 1;  // 0-based for GP5
        track.midiInstrument = instrument;  // Use captured instrument
        
        // Assign different colors per channel
        static const juce::Colour channelColors[] = {
            juce::Colours::red, juce::Colours::blue, juce::Colours::green,
            juce::Colours::orange, juce::Colours::purple, juce::Colours::cyan,
            juce::Colours::yellow, juce::Colours::magenta
        };
        track.colour = channelColors[(channel - 1) % 8];
        
        // Sort notes by start time
        std::sort(channelNotes.begin(), channelNotes.end(), 
            [](const RecordedNote& a, const RecordedNote& b) { return a.startBeat < b.startBeat; });
        
        // Build measures for this channel (simplified version)
        for (int barNum = 1; barNum <= numMeasures; ++barNum)
        {
            TabMeasure measure;
            measure.measureNumber = barNum;
            measure.timeSignatureNumerator = numerator;
            measure.timeSignatureDenominator = denominator;
            
            double measureStartBeat = (barNum - 1) * beatsPerMeasure;
            
            // Collect notes in this measure for this channel
            std::vector<const RecordedNote*> notesInMeasure;
            for (const auto& note : channelNotes)
            {
                double roundedPPQ = std::round(note.startBeat * 1000.0) / 1000.0;
                int noteBar = static_cast<int>(roundedPPQ / beatsPerMeasure) + 1;
                if (noteBar == barNum)
                {
                    notesInMeasure.push_back(&note);
                }
            }
            
            if (notesInMeasure.empty())
            {
                // Empty measure - add whole rest
                TabBeat beat;
                beat.isRest = true;
                beat.duration = NoteDuration::Whole;
                for (int s = 0; s < 6; ++s)
                {
                    TabNote emptyNote; emptyNote.string = s; emptyNote.fret = -1;
                    beat.notes.add(emptyNote);
                }
                measure.beats.add(beat);
            }
            else
            {
                // Use 32nd note resolution
                const double subdivision = 0.125;
                const int maxSlots = (int)(beatsPerMeasure / subdivision + 0.5);
                
                // Map notes to slots
                std::map<int, std::vector<const RecordedNote*>> noteGroups;
                for (const auto* note : notesInMeasure)
                {
                    double posInMeasure = note->startBeat - measureStartBeat;
                    int slot = (int)(posInMeasure / subdivision + 0.5);
                    slot = juce::jlimit(0, maxSlots - 1, slot);
                    noteGroups[slot].push_back(note);
                }
                
                int currentSlot = 0;
                while (currentSlot < maxSlots)
                {
                    auto it = noteGroups.find(currentSlot);
                    bool hasNotes = (it != noteGroups.end());
                    
                    TabBeat beat;
                    int durationInSlots = 0;
                    
                    if (hasNotes)
                    {
                        beat.isRest = false;
                        const auto& group = it->second;
                        
                        // Determine duration based on note length
                        double minNoteLen = 999.0;
                        for (const auto* note : group)
                            minNoteLen = std::min(minNoteLen, note->endBeat - note->startBeat);
                        
                        int desiredSlots = (int)(minNoteLen / subdivision + 0.5);
                        if (desiredSlots < 1) desiredSlots = 1;
                        
                        // Find next event
                        int nextEventSlot = maxSlots;
                        auto nextIt = noteGroups.upper_bound(currentSlot);
                        if (nextIt != noteGroups.end())
                            nextEventSlot = nextIt->first;
                        
                        durationInSlots = std::min(desiredSlots, nextEventSlot - currentSlot);
                        
                        // Snap to standard durations
                        if (durationInSlots >= 32) durationInSlots = 32;
                        else if (durationInSlots >= 16) durationInSlots = 16;
                        else if (durationInSlots >= 8) durationInSlots = 8;
                        else if (durationInSlots >= 4) durationInSlots = 4;
                        else if (durationInSlots >= 2) durationInSlots = 2;
                        else durationInSlots = 1;
                        
                        // Initialize notes
                        for (int s = 0; s < 6; ++s)
                        {
                            TabNote emptyNote; emptyNote.string = s; emptyNote.fret = -1;
                            beat.notes.add(emptyNote);
                        }
                        
                        // Set notes
                        for (const auto* note : group)
                        {
                            if (note->string >= 0 && note->string < 6)
                            {
                                beat.notes.getReference(note->string).fret = note->fret;
                                beat.notes.getReference(note->string).velocity = note->velocity;
                            }
                        }
                    }
                    else
                    {
                        // Rest
                        beat.isRest = true;
                        
                        int nextEventSlot = maxSlots;
                        auto nextIt = noteGroups.upper_bound(currentSlot);
                        if (nextIt != noteGroups.end())
                            nextEventSlot = nextIt->first;
                        
                        int gap = nextEventSlot - currentSlot;
                        
                        if (gap >= 32) durationInSlots = 32;
                        else if (gap >= 16) durationInSlots = 16;
                        else if (gap >= 8) durationInSlots = 8;
                        else if (gap >= 4) durationInSlots = 4;
                        else if (gap >= 2) durationInSlots = 2;
                        else durationInSlots = 1;
                        
                        for (int s = 0; s < 6; ++s)
                        {
                            TabNote emptyNote; emptyNote.string = s; emptyNote.fret = -1;
                            beat.notes.add(emptyNote);
                        }
                    }
                    
                    // Set duration
                    switch (durationInSlots)
                    {
                        case 32: beat.duration = NoteDuration::Whole; break;
                        case 16: beat.duration = NoteDuration::Half; break;
                        case 8:  beat.duration = NoteDuration::Quarter; break;
                        case 4:  beat.duration = NoteDuration::Eighth; break;
                        case 2:  beat.duration = NoteDuration::Sixteenth; break;
                        default: beat.duration = NoteDuration::ThirtySecond; break;
                    }
                    
                    measure.beats.add(beat);
                    currentSlot += durationInSlots;
                }
            }
            
            track.measures.add(measure);
        }
        
        tracks.push_back(track);
    }
    
    return tracks;
}

//==============================================================================
// MIDI Export Functionality
//==============================================================================

bool NewProjectAudioProcessor::exportTrackToMidi(int trackIndex, const juce::File& outputFile)
{
    // Wenn keine Datei geladen ist (Audio-to-Tab Modus), verwende aufgenommene Noten
    if (!isFileLoaded())
    {
        return exportRecordedTrackToMidi(trackIndex, outputFile);
    }
    
    const auto& tracks = getActiveTracks();
    const auto& measureHeaders = getActiveMeasureHeaders();
    
    if (trackIndex < 0 || trackIndex >= tracks.size())
        return false;
    
    const auto& track = tracks[trackIndex];
    const auto& songInfo = getActiveSongInfo();
    
    // Erstelle MIDI-Sequenz
    juce::MidiMessageSequence midiSequence;
    
    // Tempo (in Mikrosekunden pro Viertelnote)
    int tempoMicrosecondsPerBeat = (int)(60000000.0 / songInfo.tempo);
    midiSequence.addEvent(juce::MidiMessage::tempoMetaEvent(tempoMicrosecondsPerBeat), 0.0);
    
    // Time Signature vom ersten Takt
    if (measureHeaders.size() > 0)
    {
        int num = measureHeaders[0].numerator;
        int denLog2 = (int)std::log2(measureHeaders[0].denominator);
        midiSequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(num, denLog2), 0.0);
    }
    
    // Track Name
    juce::MidiMessage trackNameMsg = juce::MidiMessage::textMetaEvent(3, track.name);
    midiSequence.addEvent(trackNameMsg, 0.0);
    
    // MIDI Channel = 1 (Einkanal-Export)
    const int midiChannel = 1;
    
    // Program Change (Instrument)
    // Standard: Nylon Guitar = 24, Steel Guitar = 25
    int program = track.isPercussion ? 0 : 25;
    midiSequence.addEvent(juce::MidiMessage::programChange(midiChannel, program), 0.0);
    
    // Berechne Zeitposition für jede Note
    double currentTimeInBeats = 0.0;
    
    for (int measureIndex = 0; measureIndex < track.measures.size() && measureIndex < measureHeaders.size(); ++measureIndex)
    {
        const auto& measure = track.measures[measureIndex];
        const auto& header = measureHeaders[measureIndex];
        
        double beatsPerMeasure = header.numerator * (4.0 / header.denominator);
        double beatTimeInMeasure = 0.0;
        
        const auto& beats = measure.voice1;
        
        for (const auto& beat : beats)
        {
            // Berechne Notendauer in Beats
            double beatDurationBeats = 4.0 / std::pow(2.0, beat.duration + 2);
            if (beat.isDotted) beatDurationBeats *= 1.5;
            if (beat.tupletN > 0)
            {
                int tupletDenom = (beat.tupletN == 3) ? 2 : (beat.tupletN == 5 || beat.tupletN == 6) ? 4 : beat.tupletN - 1;
                beatDurationBeats = beatDurationBeats * tupletDenom / beat.tupletN;
            }
            
            double noteStartTime = currentTimeInBeats + beatTimeInMeasure;
            double noteEndTime = noteStartTime + beatDurationBeats;
            
            if (!beat.isRest)
            {
                for (auto it = beat.notes.begin(); it != beat.notes.end(); ++it)
                {
                    int stringIndex = it->first;
                    const auto& gpNote = it->second;
                    
                    if (gpNote.isDead || gpNote.isTied)
                        continue;
                    
                    // MIDI-Note berechnen
                    int midiNote = 0;
                    if (stringIndex < track.tuning.size())
                    {
                        midiNote = track.tuning[stringIndex] + gpNote.fret;
                    }
                    else if (stringIndex < 6)
                    {
                        const int defaultTuning[] = { 64, 59, 55, 50, 45, 40 };
                        midiNote = defaultTuning[stringIndex] + gpNote.fret;
                    }
                    
                    if (midiNote <= 0 || midiNote >= 128)
                        continue;
                    
                    // Velocity
                    int velocity = gpNote.velocity > 0 ? gpNote.velocity : 95;
                    if (gpNote.isGhost) velocity = 50;
                    if (gpNote.hasAccent) velocity = 115;
                    if (gpNote.hasHeavyAccent) velocity = 127;
                    velocity = juce::jlimit(1, 127, velocity);
                    
                    // Note-On und Note-Off Events (in Ticks, 480 Ticks pro Beat)
                    double ticksPerBeat = 480.0;
                    double noteOnTicks = noteStartTime * ticksPerBeat;
                    double noteOffTicks = noteEndTime * ticksPerBeat;
                    
                    midiSequence.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity), noteOnTicks);
                    midiSequence.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote), noteOffTicks);
                }
            }
            
            beatTimeInMeasure += beatDurationBeats;
        }
        
        currentTimeInBeats += beatsPerMeasure;
    }
    
    // End of Track Meta Event (FF 2F 00) - required for MIDI standard compliance
    midiSequence.addEvent(juce::MidiMessage::endOfTrack(), currentTimeInBeats * 480.0);
    midiSequence.updateMatchedPairs();
    
    // Erstelle MIDI-File im Format 0 (Single-Track)
    // Format 0: Alle Daten in einem Track (Metadaten + Noten)
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);
    midiFile.addTrack(midiSequence);
    
    // Speichere die Datei
    outputFile.deleteFile();
    
    {
        // Scope für FileOutputStream - muss geschlossen werden bevor wir die Datei erneut öffnen
        juce::FileOutputStream outputStream(outputFile);
        
        if (!outputStream.openedOk())
            return false;
        
        // JUCE's writeTo() schreibt leider Format 1 auch bei einem Track
        if (!midiFile.writeTo(outputStream))
            return false;
        
        outputStream.flush();
    }  // outputStream wird hier geschlossen
    
    // Fix: JUCE schreibt Format 1 auch für Single-Track - korrigiere zu Format 0
    fixMidiFileFormat(outputFile);
    
    return true;
}

// Hilfsfunktion: Exportiert einen einzelnen TabTrack als MIDI-Sequenz
// Wird für Audio-to-Tab Modus verwendet, wo keine GP5-Daten vorliegen
bool NewProjectAudioProcessor::exportRecordedTrackToMidi(int trackIndex, const juce::File& outputFile)
{
    // Hole aufgenommene Tracks (TabTrack-Format), mit Edits falls vorhanden
    std::vector<TabTrack> tracks;
    auto baseTracks = getRecordedTabTracks();
    for (int i = 0; i < (int)baseTracks.size(); ++i)
    {
        if (hasEditedTrack(i))
            tracks.push_back(getEditedTrack(i));
        else
            tracks.push_back(baseTracks[i]);
    }
    
    if (trackIndex < 0 || trackIndex >= (int)tracks.size())
    {
        DBG("exportRecordedTrackToMidi: trackIndex " << trackIndex << " out of range (" << tracks.size() << " tracks)");
        return false;
    }
    
    const auto& tabTrack = tracks[trackIndex];
    
    if (tabTrack.measures.isEmpty())
    {
        DBG("exportRecordedTrackToMidi: track has no measures");
        return false;
    }
    
    // Erstelle MIDI-Sequenz
    juce::MidiMessageSequence midiSequence;
    
    // Tempo vom Host
    double tempo = hostTempo.load();
    if (tempo <= 0) tempo = 120.0;
    int tempoMicrosecondsPerBeat = (int)(60000000.0 / tempo);
    midiSequence.addEvent(juce::MidiMessage::tempoMetaEvent(tempoMicrosecondsPerBeat), 0.0);
    
    // Time Signature vom ersten Takt
    if (tabTrack.measures.size() > 0)
    {
        int num = tabTrack.measures[0].timeSignatureNumerator;
        int den = tabTrack.measures[0].timeSignatureDenominator;
        int denLog2 = (int)std::log2(den > 0 ? den : 4);
        midiSequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(num, denLog2), 0.0);
    }
    
    // Track Name
    juce::String trackName = tabTrack.name.isNotEmpty() ? tabTrack.name : "Track " + juce::String(trackIndex + 1);
    midiSequence.addEvent(juce::MidiMessage::textMetaEvent(3, trackName), 0.0);
    
    // MIDI Channel aus dem TabTrack verwenden (midiChannel ist 0-basiert, MIDI Messages sind 1-basiert)
    const int midiChannel = juce::jlimit(1, 16, tabTrack.midiChannel + 1);
    
    // Program Change
    int program = juce::jlimit(0, 127, tabTrack.midiInstrument);
    midiSequence.addEvent(juce::MidiMessage::programChange(midiChannel, program), 0.0);
    
    // Berechne Zeitposition für jede Note
    double currentTimeInBeats = 0.0;
    const double ticksPerBeat = 480.0;
    
    for (int measureIndex = 0; measureIndex < tabTrack.measures.size(); ++measureIndex)
    {
        const auto& measure = tabTrack.measures[measureIndex];
        double beatsPerMeasure = measure.timeSignatureNumerator * (4.0 / measure.timeSignatureDenominator);
        double beatTimeInMeasure = 0.0;
        
        for (const auto& beat : measure.beats)
        {
            // Berechne Notendauer in Vierteln (Beats)
            double beatDurationBeats = beat.getDurationInQuarters();
            
            double noteStartTime = currentTimeInBeats + beatTimeInMeasure;
            double noteEndTime = noteStartTime + beatDurationBeats;
            
            if (!beat.isRest)
            {
                for (const auto& tabNote : beat.notes)
                {
                    if (tabNote.isTied || tabNote.effects.deadNote)
                        continue;
                    
                    // MIDI-Note berechnen
                    int midiNote = 0;
                    if (tabNote.midiNote > 0)
                    {
                        // Verwende direkt gespeicherte MIDI-Note falls vorhanden
                        midiNote = tabNote.midiNote;
                    }
                    else if (tabNote.string < tabTrack.tuning.size())
                    {
                        midiNote = tabTrack.tuning[tabNote.string] + tabNote.fret;
                    }
                    else if (tabNote.string < 6)
                    {
                        const int defaultTuning[] = { 64, 59, 55, 50, 45, 40 };
                        midiNote = defaultTuning[tabNote.string] + tabNote.fret;
                    }
                    
                    if (midiNote <= 0 || midiNote >= 128)
                        continue;
                    
                    // Velocity
                    int velocity = tabNote.velocity > 0 ? tabNote.velocity : 95;
                    velocity = juce::jlimit(1, 127, velocity);
                    
                    double noteOnTicks = noteStartTime * ticksPerBeat;
                    double noteOffTicks = noteEndTime * ticksPerBeat;
                    
                    midiSequence.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity), noteOnTicks);
                    midiSequence.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote), noteOffTicks);
                }
            }
            
            beatTimeInMeasure += beatDurationBeats;
        }
        
        currentTimeInBeats += beatsPerMeasure;
    }
    
    // End of Track Meta Event
    midiSequence.addEvent(juce::MidiMessage::endOfTrack(), currentTimeInBeats * ticksPerBeat);
    midiSequence.updateMatchedPairs();
    
    // Erstelle MIDI-File im Format 0 (Single-Track)
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);
    midiFile.addTrack(midiSequence);
    
    // Speichere die Datei
    outputFile.deleteFile();
    
    {
        juce::FileOutputStream outputStream(outputFile);
        
        if (!outputStream.openedOk())
            return false;
        
        if (!midiFile.writeTo(outputStream))
            return false;
        
        outputStream.flush();
    }
    
    fixMidiFileFormat(outputFile);
    
    DBG("MIDI exported from recorded notes: " << outputFile.getFullPathName());
    return true;
}

bool NewProjectAudioProcessor::exportAllTracksToMidi(const juce::File& outputFile)
{
    // Wenn keine Datei geladen ist (Audio-to-Tab Modus), verwende aufgenommene Noten
    if (!isFileLoaded())
    {
        return exportAllRecordedTracksToMidi(outputFile);
    }
    
    const auto& tracks = getActiveTracks();
    const auto& measureHeaders = getActiveMeasureHeaders();
    const auto& songInfo = getActiveSongInfo();
    
    if (tracks.size() == 0)
        return false;
    
    // Bei nur einem Track: Format 0 verwenden (alles in einem Track)
    if (tracks.size() == 1)
    {
        return exportTrackToMidi(0, outputFile);
    }
    
    // Mehrere Tracks: Format 1 (Multi-Track MIDI)
    // Track 0 = nur Metadaten (Tempo, Time Signature, Titel)
    // Tracks 1+ = Musikdaten (Noten, Controller)
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);
    
    // Track 0: NUR Tempo und Time Signature (Standard für Format 1 MIDI)
    // Keine Musikdaten in Track 0!
    juce::MidiMessageSequence tempoTrack;
    
    // Tempo (in Mikrosekunden pro Viertelnote)
    int tempoMicrosecondsPerBeat = (int)(60000000.0 / songInfo.tempo);
    tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent(tempoMicrosecondsPerBeat), 0.0);
    
    // Time Signature vom ersten Takt
    if (measureHeaders.size() > 0)
    {
        int num = measureHeaders[0].numerator;
        int denLog2 = (int)std::log2(measureHeaders[0].denominator);
        tempoTrack.addEvent(juce::MidiMessage::timeSignatureMetaEvent(num, denLog2), 0.0);
    }
    
    // Song Title
    if (songInfo.title.isNotEmpty())
    {
        juce::MidiMessage titleMsg = juce::MidiMessage::textMetaEvent(3, songInfo.title);
        tempoTrack.addEvent(titleMsg, 0.0);
    }
    
    // Berechne Gesamtlänge für End-of-Track
    double totalLength = 0.0;
    for (const auto& header : measureHeaders)
    {
        totalLength += header.numerator * (4.0 / header.denominator);
    }
    
    // End of Track für Tempo-Track (FF 2F 00) - required for MIDI standard
    tempoTrack.addEvent(juce::MidiMessage::endOfTrack(), totalLength * 480.0);
    midiFile.addTrack(tempoTrack);
    
    // Für jeden Track eine MIDI-Spur erstellen (Tracks 1+ in Format 1)
    for (int trackIdx = 0; trackIdx < tracks.size() && trackIdx < 16; ++trackIdx)
    {
        const auto& track = tracks[trackIdx];
        juce::MidiMessageSequence midiSequence;
        
        // MIDI Channel (1-16, Track 10 für Drums vermeiden wenn nicht Percussion)
        int midiChannel = track.isPercussion ? 10 : ((trackIdx < 9) ? trackIdx + 1 : trackIdx + 2);
        if (midiChannel > 16) midiChannel = 16;
        
        // Track Name
        juce::MidiMessage trackNameMsg = juce::MidiMessage::textMetaEvent(3, track.name);
        midiSequence.addEvent(trackNameMsg, 0.0);
        
        // Program Change (Instrument)
        int program = track.isPercussion ? 0 : 25;  // 25 = Acoustic Guitar (steel)
        midiSequence.addEvent(juce::MidiMessage::programChange(midiChannel, program), 0.0);
        
        // Pan und Volume
        midiSequence.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 7, track.volume), 0.0);  // Volume
        midiSequence.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 10, track.pan), 0.0);   // Pan
        
        // Berechne Zeitposition für jede Note
        double currentTimeInBeats = 0.0;
        
        for (int measureIndex = 0; measureIndex < track.measures.size() && measureIndex < measureHeaders.size(); ++measureIndex)
        {
            const auto& measure = track.measures[measureIndex];
            const auto& header = measureHeaders[measureIndex];
            
            double beatsPerMeasure = header.numerator * (4.0 / header.denominator);
            double beatTimeInMeasure = 0.0;
            
            const auto& beats = measure.voice1;
            
            for (const auto& beat : beats)
            {
                // Berechne Notendauer in Beats
                double beatDurationBeats = 4.0 / std::pow(2.0, beat.duration + 2);
                if (beat.isDotted) beatDurationBeats *= 1.5;
                if (beat.tupletN > 0)
                {
                    int tupletDenom = (beat.tupletN == 3) ? 2 : (beat.tupletN == 5 || beat.tupletN == 6) ? 4 : beat.tupletN - 1;
                    beatDurationBeats = beatDurationBeats * tupletDenom / beat.tupletN;
                }
                
                double noteStartTime = currentTimeInBeats + beatTimeInMeasure;
                double noteEndTime = noteStartTime + beatDurationBeats;
                
                if (!beat.isRest)
                {
                    for (auto it = beat.notes.begin(); it != beat.notes.end(); ++it)
                    {
                        int stringIndex = it->first;
                        const auto& gpNote = it->second;
                        
                        if (gpNote.isDead || gpNote.isTied)
                            continue;
                        
                        // MIDI-Note berechnen
                        int midiNote = 0;
                        if (stringIndex < track.tuning.size())
                        {
                            midiNote = track.tuning[stringIndex] + gpNote.fret;
                        }
                        else if (stringIndex < 6)
                        {
                            const int defaultTuning[] = { 64, 59, 55, 50, 45, 40 };
                            midiNote = defaultTuning[stringIndex] + gpNote.fret;
                        }
                        
                        if (midiNote <= 0 || midiNote >= 128)
                            continue;
                        
                        // Velocity
                        int velocity = gpNote.velocity > 0 ? gpNote.velocity : 95;
                        if (gpNote.isGhost) velocity = 50;
                        if (gpNote.hasAccent) velocity = 115;
                        if (gpNote.hasHeavyAccent) velocity = 127;
                        velocity = juce::jlimit(1, 127, velocity);
                        
                        // Note-On und Note-Off Events (in Ticks, 480 Ticks pro Beat)
                        double ticksPerBeat = 480.0;
                        double noteOnTicks = noteStartTime * ticksPerBeat;
                        double noteOffTicks = noteEndTime * ticksPerBeat;
                        
                        midiSequence.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity), noteOnTicks);
                        midiSequence.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote), noteOffTicks);
                    }
                }
                
                beatTimeInMeasure += beatDurationBeats;
            }
            
            currentTimeInBeats += beatsPerMeasure;
        }
        
        // End of Track (FF 2F 00) - required for MIDI standard compliance
        midiSequence.addEvent(juce::MidiMessage::endOfTrack(), totalLength * 480.0);
        midiSequence.updateMatchedPairs();
        
        midiFile.addTrack(midiSequence);
    }
    
    // Speichere die Datei
    // JUCE erkennt automatisch: >1 Track = Format 1
    outputFile.deleteFile();
    juce::FileOutputStream outputStream(outputFile);
    
    if (!outputStream.openedOk())
        return false;
    
    // JUCE's writeTo() schreibt Format 1 bei mehreren Tracks
    // und fügt das End-of-Track Event (FF 2F 00) korrekt für jeden Track ein
    return midiFile.writeTo(outputStream);
}

// Hilfsfunktion: Exportiert alle aufgenommenen TabTracks als Multi-Track MIDI
bool NewProjectAudioProcessor::exportAllRecordedTracksToMidi(const juce::File& outputFile)
{
    // Hole aufgenommene Tracks mit Edits
    std::vector<TabTrack> tracks;
    auto baseTracks = getRecordedTabTracks();
    for (int i = 0; i < (int)baseTracks.size(); ++i)
    {
        if (hasEditedTrack(i))
            tracks.push_back(getEditedTrack(i));
        else
            tracks.push_back(baseTracks[i]);
    }
    
    if (tracks.empty())
    {
        DBG("exportAllRecordedTracksToMidi: no tracks available");
        return false;
    }
    
    // Bei nur einem Track: Format 0
    if (tracks.size() == 1)
    {
        return exportRecordedTrackToMidi(0, outputFile);
    }
    
    // Mehrere Tracks: Format 1 (Multi-Track MIDI)
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);
    const double ticksPerBeat = 480.0;
    
    // Track 0: Tempo und Time Signature
    juce::MidiMessageSequence tempoTrack;
    
    double tempo = hostTempo.load();
    if (tempo <= 0) tempo = 120.0;
    int tempoMicrosecondsPerBeat = (int)(60000000.0 / tempo);
    tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent(tempoMicrosecondsPerBeat), 0.0);
    
    // Time Signature vom ersten Takt des ersten Tracks
    if (!tracks.empty() && tracks[0].measures.size() > 0)
    {
        int num = tracks[0].measures[0].timeSignatureNumerator;
        int den = tracks[0].measures[0].timeSignatureDenominator;
        int denLog2 = (int)std::log2(den > 0 ? den : 4);
        tempoTrack.addEvent(juce::MidiMessage::timeSignatureMetaEvent(num, denLog2), 0.0);
    }
    
    // Berechne Gesamtlänge für End-of-Track
    double totalLength = 0.0;
    if (!tracks.empty())
    {
        for (const auto& measure : tracks[0].measures)
        {
            totalLength += measure.timeSignatureNumerator * (4.0 / measure.timeSignatureDenominator);
        }
    }
    
    tempoTrack.addEvent(juce::MidiMessage::endOfTrack(), totalLength * ticksPerBeat);
    midiFile.addTrack(tempoTrack);
    
    // Für jeden Track eine MIDI-Spur erstellen
    for (int trackIdx = 0; trackIdx < (int)tracks.size() && trackIdx < 16; ++trackIdx)
    {
        const auto& tabTrack = tracks[trackIdx];
        juce::MidiMessageSequence midiSequence;
        
        // MIDI Channel aus dem TabTrack verwenden (midiChannel ist 0-basiert, MIDI Messages sind 1-basiert)
        int midiChannel = juce::jlimit(1, 16, tabTrack.midiChannel + 1);
        
        // Track Name
        juce::String trackName = tabTrack.name.isNotEmpty() ? tabTrack.name : "Track " + juce::String(trackIdx + 1);
        midiSequence.addEvent(juce::MidiMessage::textMetaEvent(3, trackName), 0.0);
        
        // Program Change
        int program = juce::jlimit(0, 127, tabTrack.midiInstrument);
        midiSequence.addEvent(juce::MidiMessage::programChange(midiChannel, program), 0.0);
        
        // Volume und Pan
        midiSequence.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 7, 100), 0.0);  // Volume
        midiSequence.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 10, 64), 0.0);  // Pan center
        
        // Noten-Events
        double currentTimeInBeats = 0.0;
        
        for (int measureIndex = 0; measureIndex < tabTrack.measures.size(); ++measureIndex)
        {
            const auto& measure = tabTrack.measures[measureIndex];
            double beatsPerMeasure = measure.timeSignatureNumerator * (4.0 / measure.timeSignatureDenominator);
            double beatTimeInMeasure = 0.0;
            
            for (const auto& beat : measure.beats)
            {
                double beatDurationBeats = beat.getDurationInQuarters();
                
                double noteStartTime = currentTimeInBeats + beatTimeInMeasure;
                double noteEndTime = noteStartTime + beatDurationBeats;
                
                if (!beat.isRest)
                {
                    for (const auto& tabNote : beat.notes)
                    {
                        if (tabNote.isTied || tabNote.effects.deadNote)
                            continue;
                        
                        int midiNote = 0;
                        if (tabNote.midiNote > 0)
                        {
                            midiNote = tabNote.midiNote;
                        }
                        else if (tabNote.string < tabTrack.tuning.size())
                        {
                            midiNote = tabTrack.tuning[tabNote.string] + tabNote.fret;
                        }
                        else if (tabNote.string < 6)
                        {
                            const int defaultTuning[] = { 64, 59, 55, 50, 45, 40 };
                            midiNote = defaultTuning[tabNote.string] + tabNote.fret;
                        }
                        
                        if (midiNote <= 0 || midiNote >= 128)
                            continue;
                        
                        int velocity = tabNote.velocity > 0 ? tabNote.velocity : 95;
                        velocity = juce::jlimit(1, 127, velocity);
                        
                        double noteOnTicks = noteStartTime * ticksPerBeat;
                        double noteOffTicks = noteEndTime * ticksPerBeat;
                        
                        midiSequence.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity), noteOnTicks);
                        midiSequence.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote), noteOffTicks);
                    }
                }
                
                beatTimeInMeasure += beatDurationBeats;
            }
            
            currentTimeInBeats += beatsPerMeasure;
        }
        
        midiSequence.addEvent(juce::MidiMessage::endOfTrack(), totalLength * ticksPerBeat);
        midiSequence.updateMatchedPairs();
        
        midiFile.addTrack(midiSequence);
    }
    
    // Speichere die Datei
    outputFile.deleteFile();
    juce::FileOutputStream outputStream(outputFile);
    
    if (!outputStream.openedOk())
        return false;
    
    DBG("MIDI exported from recorded notes (all tracks): " << outputFile.getFullPathName()
        << " (" << tracks.size() << " tracks)");
    return midiFile.writeTo(outputStream);
}

//==============================================================================
// Guitar Pro Export Functionality
//==============================================================================

bool NewProjectAudioProcessor::hasRecordedNotes() const
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    return !recordedNotes.empty();
}

int NewProjectAudioProcessor::getRecordedTrackMidiChannel(int trackIndex) const
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    if (recordedNotes.empty())
        return -1;
    
    // Find all unique MIDI channels used (sorted)
    std::set<int> usedChannels;
    for (const auto& note : recordedNotes)
    {
        usedChannels.insert(note.midiChannel);
    }
    
    // Convert set to vector for indexed access
    std::vector<int> channelList(usedChannels.begin(), usedChannels.end());
    
    if (trackIndex < 0 || trackIndex >= (int)channelList.size())
        return -1;
    
    return channelList[trackIndex];
}

juce::Array<GP5Track> NewProjectAudioProcessor::getDisplayTracks() const
{
    // If file is loaded, return the file's tracks
    if (fileLoaded)
    {
        return getActiveTracks();
    }
    
    // Otherwise, create tracks from recorded notes (one per MIDI channel)
    juce::Array<GP5Track> tracks;
    
    std::vector<RecordedNote> allNotes;
    {
        std::lock_guard<std::mutex> lock(recordingMutex);
        allNotes = recordedNotes;
    }
    
    if (allNotes.empty())
    {
        return tracks;  // Return empty
    }
    
    // Find all unique MIDI channels used
    std::set<int> usedChannels;
    for (const auto& note : allNotes)
    {
        usedChannels.insert(note.midiChannel);
    }
    
    // Create a GP5Track for each channel
    for (int channel : usedChannels)
    {
        GP5Track track;
        int instrument = channelInstruments[channel - 1];
        
        // Channel 10 is the drum channel
        if (channel == 10)
        {
            track.name = "Drums";
            track.isPercussion = true;
            instrument = 0;
        }
        else
        {
            const char* instrumentName = (instrument >= 0 && instrument < 128) ? gmInstrumentNames[instrument] : "Unknown";
            track.name = juce::String(instrumentName);
            track.isPercussion = false;
        }
        
        track.stringCount = 6;
        track.midiChannel = channel;
        track.port = 0;
        track.volume = 100;
        track.pan = 64;
        
        // Standard guitar tuning
        track.tuning.clear();
        track.tuning.add(64);  // E4
        track.tuning.add(59);  // B3
        track.tuning.add(55);  // G3
        track.tuning.add(50);  // D3
        track.tuning.add(45);  // A2
        track.tuning.add(40);  // E2
        
        tracks.add(track);
    }
    
    return tracks;
}

int NewProjectAudioProcessor::getDisplayTrackCount() const
{
    if (fileLoaded)
    {
        return getActiveTracks().size();
    }
    
    // Count unique MIDI channels in recorded notes
    std::set<int> usedChannels;
    {
        std::lock_guard<std::mutex> lock(recordingMutex);
        for (const auto& note : recordedNotes)
        {
            usedChannels.insert(note.midiChannel);
        }
    }
    
    return static_cast<int>(usedChannels.size());
}

juce::String NewProjectAudioProcessor::getDisplayTrackName(int trackIndex) const
{
    if (fileLoaded)
    {
        const auto& tracks = getActiveTracks();
        if (trackIndex >= 0 && trackIndex < tracks.size())
        {
            return tracks[trackIndex].name;
        }
        return "Unknown";
    }
    
    // Get track names from recorded notes
    auto displayTracks = getDisplayTracks();
    if (trackIndex >= 0 && trackIndex < displayTracks.size())
    {
        return displayTracks[trackIndex].name;
    }
    return "Recording";
}

bool NewProjectAudioProcessor::exportRecordingToGP5(const juce::File& outputFile, const juce::String& title)
{
    std::vector<TabTrack> tracks;
    
    if (isFileLoaded())
    {
        // Use edited tracks if available, otherwise convert from active parser data
        const auto& loadedTracks = getActiveTracks();
        for (int i = 0; i < loadedTracks.size(); ++i)
        {
            if (hasEditedTrack(i))
            {
                tracks.push_back(getEditedTrack(i));
                DBG("Track " << i << ": using edited version");
            }
            else if (usingPTBParser)
            {
                tracks.push_back(ptbParser.convertToTabTrack(i));
                DBG("Track " << i << ": using PTB parser data");
            }
            else
            {
                tracks.push_back(gp5Parser.convertToTabTrack(i));
                DBG("Track " << i << ": using original GP5 data");
            }
        }
    }
    else
    {
        // Get recorded tracks, but use edited versions where available
        auto baseTracks = getRecordedTabTracks();
        for (int i = 0; i < (int)baseTracks.size(); ++i)
        {
            if (hasEditedTrack(i))
            {
                tracks.push_back(getEditedTrack(i));
                DBG("Recording Track " << i << ": using edited version");
            }
            else
            {
                tracks.push_back(baseTracks[i]);
                DBG("Recording Track " << i << ": using generated version");
            }
        }
    }
    
    if (tracks.empty() || (tracks.size() == 1 && tracks[0].measures.isEmpty()))
    {
        DBG("No notes to export");
        return false;
    }
    
    // Create GP5 writer
    GP5Writer writer;
    writer.setTitle(title.isEmpty() ? "Untitled" : title);
    writer.setArtist("GP5 VST Editor");
    writer.setTempo(static_cast<int>(hostTempo.load()));
    
    // Write to file (multi-track if multiple channels, single-track otherwise)
    bool success = writer.writeToFile(tracks, outputFile);
    
    if (!success)
    {
        DBG("GP5 export failed: " << writer.getLastError());
    }
    else
    {
        DBG("GP5 exported successfully to: " << outputFile.getFullPathName() 
            << " (" << tracks.size() << " track(s))");
    }
    
    return success;
}

bool NewProjectAudioProcessor::exportRecordingToGP5WithMetadata(
    const juce::File& outputFile,
    const juce::String& title,
    const std::vector<std::pair<juce::String, int>>& trackData)
{
    std::vector<TabTrack> tracks;
    
    if (isFileLoaded())
    {
        // Use edited tracks if available, otherwise convert from active parser data
        const auto& loadedTracks = getActiveTracks();
        for (int i = 0; i < loadedTracks.size(); ++i)
        {
            if (hasEditedTrack(i))
            {
                tracks.push_back(getEditedTrack(i));
                DBG("Track " << i << ": using edited version");
            }
            else if (usingPTBParser)
            {
                tracks.push_back(ptbParser.convertToTabTrack(i));
                DBG("Track " << i << ": using PTB parser data");
            }
            else
            {
                tracks.push_back(gp5Parser.convertToTabTrack(i));
                DBG("Track " << i << ": using original GP5 data");
            }
        }
        DBG("Exporting from loaded file: " << loadedTracks.size() << " tracks");
    }
    else
    {
        // Get recorded tracks, but use edited versions where available
        auto baseTracks = getRecordedTabTracks();
        for (int i = 0; i < (int)baseTracks.size(); ++i)
        {
            if (hasEditedTrack(i))
            {
                tracks.push_back(getEditedTrack(i));
                DBG("Recording Track " << i << ": using edited version");
            }
            else
            {
                tracks.push_back(baseTracks[i]);
                DBG("Recording Track " << i << ": using generated version");
            }
        }
    }
    
    if (tracks.empty() || (tracks.size() == 1 && tracks[0].measures.isEmpty()))
    {
        DBG("No notes to export");
        return false;
    }
    
    // Apply user-defined metadata to tracks
    for (size_t i = 0; i < tracks.size() && i < trackData.size(); ++i)
    {
        tracks[i].name = trackData[i].first;          // Track name
        tracks[i].midiInstrument = trackData[i].second;  // GM instrument (0-127)
        DBG("Track " << i << ": name=" << tracks[i].name 
            << ", instrument=" << tracks[i].midiInstrument);
    }
    
    // Create GP5 writer
    GP5Writer writer;
    writer.setTitle(title.isEmpty() ? "Untitled" : title);
    writer.setArtist("GP5 VST Editor");
    writer.setTempo(static_cast<int>(hostTempo.load()));
    
    // Write to file with user metadata
    bool success = writer.writeToFile(tracks, outputFile);
    
    if (!success)
    {
        DBG("GP5 export with metadata failed: " << writer.getLastError());
    }
    else
    {
        DBG("GP5 exported successfully with metadata to: " << outputFile.getFullPathName() 
            << " (" << tracks.size() << " track(s))");
    }
    
    return success;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}
