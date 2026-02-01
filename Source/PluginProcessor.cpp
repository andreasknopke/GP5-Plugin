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
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
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
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    // Inline MIDI-Generierung - keine externe Engine
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
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

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
                        DBG("Recording started at beat " << currentBeat << ", measure start: " << recordingStartBeat);
                    }
                    
                    RecordedNote recNote;
                    recNote.midiNote = midiNote;
                    recNote.velocity = velocity;
                    recNote.string = tabNote.string;
                    recNote.fret = tabNote.fret;
                    // Quantisiere startBeat auf 1/64-Noten (0.0625 Beats) um Floating-Point-Fehler zu beheben
                    // 7.99887 wird zu 8.0, 8.51234 bleibt ca. 8.5
                    double quantizeGrid = 0.0625;  // 1/64 Note
                    recNote.startBeat = std::round(currentBeat / quantizeGrid) * quantizeGrid;
                    recNote.endBeat = recNote.startBeat;  // Wird beim Note-Off aktualisiert
                    recNote.isActive = true;
                    
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
                    lastPlayedString = -1;
                    lastPlayedFret = -1;
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
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                liveMidiNotes.clear();
                
                // Reset last played position für Kostenfunktion
                lastPlayedString = -1;
                lastPlayedFret = -1;
                
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
    }
    
    // =========================================================================
    // MIDI Output - Mit Echtzeit-Bend-Interpolation
    // =========================================================================
    if (fileLoaded && midiOutputEnabled.load())
    {
        bool isPlaying = hostIsPlaying.load();
        double currentBeat = hostPositionBeats.load();
        
        const auto& tracks = usingGP7Parser ? gp7Parser.getTracks() : gp5Parser.getTracks();
        
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
                
                if (bend.points.size() >= 2)
                {
                    // Find the two surrounding points
                    int prevIdx = 0;
                    int nextIdx = (int)bend.points.size() - 1;
                    
                    for (int i = 0; i < (int)bend.points.size(); ++i)
                    {
                        if (bend.points[i].position <= positionInBend)
                            prevIdx = i;
                    }
                    for (int i = (int)bend.points.size() - 1; i >= 0; --i)
                    {
                        if (bend.points[i].position >= positionInBend)
                            nextIdx = i;
                    }
                    
                    if (nextIdx < prevIdx) nextIdx = prevIdx;
                    
                    const auto& prevPoint = bend.points[prevIdx];
                    const auto& nextPoint = bend.points[nextIdx];
                    
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
                else if (bend.points.size() == 1)
                {
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
            const auto& measureHeaders = usingGP7Parser ? gp7Parser.getMeasureHeaders() : gp5Parser.getMeasureHeaders();
            
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
                                    newBend.points = gpNote.bendPoints;
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
        }
        
        if (isPlaying && currentBeat >= 0.0)
        {
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
        
        wasPlaying = isPlaying;
    }
    
    // Generierte MIDI-Events zum Output hinzufügen
    midiMessages.addEvents(generatedMidi, 0, buffer.getNumSamples(), 0);

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        juce::ignoreUnused(channelData);
        // ..do something to the data...
    }
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
    }
}

void NewProjectAudioProcessor::unloadFile()
{
    fileLoaded = false;
    loadedFilePath = "";
    
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
    
    DBG("Processor: File unloaded");
}

bool NewProjectAudioProcessor::loadGP5File(const juce::File& file)
{
    // Check file extension to determine which parser to use
    auto extension = file.getFileExtension().toLowerCase();
    
    // Try GP7/8 parser for .gp files (ZIP-based format)
    if (extension == ".gp")
    {
        if (gp7Parser.parseFile(file))
        {
            loadedFilePath = file.getFullPathName();
            fileLoaded = true;
            usingGP7Parser = true;
            
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
    if (gp5Parser.parse(file))
    {
        loadedFilePath = file.getFullPathName();
        fileLoaded = true;
        usingGP7Parser = false;
        
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
    const auto& tracks = usingGP7Parser ? gp7Parser.getTracks() : gp5Parser.getTracks();
    
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
    const auto& measureHeaders = usingGP7Parser ? gp7Parser.getMeasureHeaders() : gp5Parser.getMeasureHeaders();
    
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
    const auto& measureHeaders = usingGP7Parser ? gp7Parser.getMeasureHeaders() : gp5Parser.getMeasureHeaders();
    
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
    const auto& measureHeaders = usingGP7Parser ? gp7Parser.getMeasureHeaders() : gp5Parser.getMeasureHeaders();
    
    if (measureIndex >= 0 && measureIndex < (int)measureHeaders.size())
    {
        return { measureHeaders[measureIndex].numerator, measureHeaders[measureIndex].denominator };
    }
    
    // Default: 4/4
    return { 4, 4 };
}

int NewProjectAudioProcessor::getGP5Tempo() const
{
    return usingGP7Parser ? gp7Parser.getSongInfo().tempo : gp5Parser.getSongInfo().tempo;
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
        
    const auto& headers = usingGP7Parser ? gp7Parser.getMeasureHeaders() : gp5Parser.getMeasureHeaders();
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

    // 1. Fret-Distanz (Horizontal) - Bewegung entlang des Halses
    int fretDiff = std::abs(current.fret - previous.fret);
    cost += fretDiff * 1.0f; 

    // 2. String-Distanz (Vertikal) - Saitenwechsel
    int stringDiff = std::abs(current.stringIndex - previous.stringIndex);
    cost += stringDiff * 0.5f; 

    // 3. Hand-Spanne (Stretch Penalty)
    // Wenn wir nicht rutschen, können wir ca. 4 Bünde greifen
    // Alles darüber hinaus erfordert eine Handbewegung (teuer!)
    if (fretDiff > 4 && current.fret != 0 && previous.fret != 0)
    {
        cost += 5.0f; // Hohe Strafe für Positionswechsel
    }

    // 4. Bevorzugung offener Saiten (Open String Bonus)
    if (current.fret == 0)
    {
        // Wenn wir vorher hoch am Hals waren (>5), ist 0 eher schlecht (Klangunterschied)
        if (previous.fret > 5)
            cost += 2.0f; 
        else
            cost -= 2.0f; // Bonus! Macht es attraktiv
    }
    
    // 5. Kleine Präferenz für höhere Saiten bei Melodien (dünnere Saiten)
    cost -= current.stringIndex * 0.1f;

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
    
    // Finde beste Position mit Kostenfunktion
    GuitarPosition bestPos = findBestPosition(midiNote, lastPlayedString, lastPlayedFret);
    
    // Update last played position für nächste Note
    lastPlayedString = bestPos.stringIndex;
    lastPlayedFret = bestPos.fret;
    
    // Tuning array is index 0=E2 (lowest), 5=E4 (highest)
    // Display expects index 0=top line (highest), 5=bottom line (lowest)
    // So we need to invert: display_string = 5 - tuning_string
    result.string = 5 - bestPos.stringIndex;
    result.fret = bestPos.fret;
    
    return result;
}

std::vector<NewProjectAudioProcessor::LiveMidiNote> NewProjectAudioProcessor::getLiveMidiNotes() const
{
    std::lock_guard<std::mutex> lock(liveMidiMutex);
    
    if (liveMidiNotes.empty())
    {
        detectedChordName = "";  // Kein Akkord wenn keine Noten
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
        
        // Versuche erst mit exaktem Bass-Matching
        auto chordResult = chordMatcher.findBestChord(midiNoteNumbers, currentFretPosition, true);
        
        // Falls kein Match mit exaktem Bass, versuche ohne Bass-Constraint
        if (!chordResult.isMatch)
        {
            chordResult = chordMatcher.findBestChord(midiNoteNumbers, currentFretPosition, false);
        }
        
        if (chordResult.isMatch && chordResult.shape != nullptr)
        {
            // Akkord gefunden! Verwende das Shape direkt
            std::vector<LiveMidiNote> result;
            const auto& shape = *chordResult.shape;
            
            // Speichere erkannten Akkordnamen für die UI
            detectedChordName = shape.name;
            
            DBG("Chord matched: " << shape.name << " (cost: " << chordResult.totalCost << ")");
            
            for (int s = 0; s < 6; ++s)
            {
                if (shape.frets[s] >= 0)  // Nicht gedämpft
                {
                    int midiNote = standardTuning[s] + shape.frets[s];
                    
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
                    ln.string = 5 - s;  // Display: 0=top, 5=bottom; Tuning: 0=low, 5=high
                    ln.fret = shape.frets[s];
                    result.push_back(ln);
                }
            }
            
            // Update lastPlayedFret für nächsten Akkord
            lastPlayedFret = shape.baseFret;
            
            return result;
        }
    }
    
    // =========================================================================
    // FALLBACK: Kein Akkord erkannt - verwende bestehenden Algorithmus
    // =========================================================================
    detectedChordName = "";  // Kein bekannter Akkord
    
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
                    int dist = 0;
                    if (fret < preferredMinFret)
                        dist = preferredMinFret - fret;
                    else
                        dist = fret - preferredMaxFret;
                    score -= dist * 10;
                }
                
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
    // Maximal 4 Bünde Unterschied (typische Handspanne), aber nur für Bünde > 0
    const int maxFretSpan = 4;
    
    std::vector<LiveMidiNote> bestResult;
    int bestScore = -10000;
    
    // Rekursive Suche nach der besten Kombination
    std::function<void(int, std::vector<NoteOption>&, std::set<int>&)> findBest;
    findBest = [&](int noteIdx, std::vector<NoteOption>& current, std::set<int>& usedStrings) {
        if (noteIdx >= (int)allOptions.size())
        {
            // Prüfe ob diese Kombination gültig ist
            int minFret = 100, maxFret = 0;
            for (const auto& opt : current)
            {
                // Leersaiten zählen nicht für die Spannweite
                if (opt.fret > 0)
                {
                    minFret = std::min(minFret, opt.fret);
                    maxFret = std::max(maxFret, opt.fret);
                }
            }
            
            // Wenn alle Leersaiten, ist es gültig
            if (minFret > maxFret) minFret = maxFret = 0;
            
            int fretSpan = maxFret - minFret;
            if (fretSpan > maxFretSpan)
                return;  // Zu große Spannweite
            
            // Berechne Score basierend auf bevorzugtem Fret-Bereich
            int score = 0;
            
            // Summiere die Scores aller Optionen (basierend auf Fret-Position-Präferenz)
            for (const auto& opt : current)
            {
                score += opt.score;
            }
            
            // Kleine Strafe für größere Spannweite
            score -= fretSpan * 5;
            
            if (score > bestScore)
            {
                bestScore = score;
                bestResult.clear();
                for (int i = 0; i < (int)current.size(); ++i)
                {
                    LiveMidiNote ln;
                    ln.midiNote = notesWithVelocity[i].first;
                    ln.velocity = notesWithVelocity[i].second;
                    // Convert string index: tuning[0]=E2(lowest) -> display[5]=bottom
                    ln.string = 5 - current[i].string;
                    ln.fret = current[i].fret;
                    bestResult.push_back(ln);
                }
            }
            return;
        }
        
        // Versuche jede Option für diese Note (bereits nach Score sortiert)
        for (const auto& opt : allOptions[noteIdx])
        {
            if (usedStrings.count(opt.string) > 0)
                continue;  // Saite bereits verwendet
            
            // Frühe Prüfung: Passt dieser Bund zur bisherigen Auswahl?
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
                if (newMin <= newMax && newMax - newMin > maxFretSpan)
                    continue;  // Würde Spannweite überschreiten
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
    
    // Fallback: Wenn keine gültige Kombination gefunden, zeige einzelne Noten
    if (bestResult.empty())
    {
        for (const auto& [midiNote, velocity] : notesWithVelocity)
        {
            LiveMidiNote ln = midiNoteToTab(midiNote, velocity);
            bestResult.push_back(ln);
        }
    }
    
    return bestResult;
}

TabTrack NewProjectAudioProcessor::getEmptyTabTrack() const
{
    TabTrack track;
    track.name = "MIDI Input";
    track.stringCount = 6;
    track.tuning = { 40, 45, 50, 55, 59, 64 };  // E-Standard
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

void NewProjectAudioProcessor::clearRecording()
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    recordedNotes.clear();
    activeRecordingNotes.clear();
    recordingStartBeat = 0.0;
    recordingStartSet = false;
    
    // Reset playback state
    activePlaybackNotes.clear();
    lastPlaybackBeat = -1.0;
    
    DBG("Recording cleared");
}

std::vector<NewProjectAudioProcessor::RecordedNote> NewProjectAudioProcessor::getRecordedNotes() const
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    return recordedNotes;
}

TabTrack NewProjectAudioProcessor::getRecordedTabTrack() const
{
    TabTrack track;
    track.name = "Recording";
    track.stringCount = 6;
    track.tuning = { 40, 45, 50, 55, 59, 64 };  // E-Standard
    track.colour = juce::Colours::red;
    
    int numerator = hostTimeSigNumerator.load();
    int denominator = hostTimeSigDenominator.load();
    
    // Bei 4/4: beatsPerMeasure = 4 (jeder Takt hat 4 Quarter Notes)
    double beatsPerMeasure = numerator * (4.0 / denominator);
    
    // Hole aufgezeichnete Noten
    std::vector<RecordedNote> notes;
    double startBeatRef = 0.0;
    {
        std::lock_guard<std::mutex> lock(recordingMutex);
        notes = recordedNotes;
        startBeatRef = recordingStartBeat;  // Taktanfang der ersten Note
    }
    
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
        // Runde ppq auf 1000stel um Floating-Point-Präzisionsprobleme zu beheben
        std::vector<const RecordedNote*> notesInMeasure;
        for (const auto& note : notes)
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
            track.measures.add(measure);
            continue;
        }
        
        // Gruppiere Noten nach quantisierter Startzeit (für Akkorde)
        std::map<int, std::vector<const RecordedNote*>> noteGroups;
        for (const auto* note : notesInMeasure)
        {
            // Quantisiere auf 16tel-Grid innerhalb des Takts
            double posInMeasure = note->startBeat - measureStartBeat;
            int quantizedSlot = (int)(posInMeasure / (beatsPerMeasure / 16.0) + 0.5);
            quantizedSlot = juce::jlimit(0, 15, quantizedSlot);
            noteGroups[quantizedSlot].push_back(note);
        }
        
        // Erstelle Beats für jede Notengruppe
        for (const auto& [slot, group] : noteGroups)
        {
            TabBeat beat;
            beat.isRest = false;
            
            // Finde die kürzeste Notendauer in der Gruppe
            double shortestDuration = std::numeric_limits<double>::max();
            for (const auto* note : group)
            {
                double dur = note->endBeat - note->startBeat;
                shortestDuration = std::min(shortestDuration, dur);
            }
            
            // Konvertiere zu NoteDuration
            if (shortestDuration >= 3.5)
                beat.duration = NoteDuration::Whole;
            else if (shortestDuration >= 1.75)
                beat.duration = NoteDuration::Half;
            else if (shortestDuration >= 0.875)
                beat.duration = NoteDuration::Quarter;
            else if (shortestDuration >= 0.4375)
                beat.duration = NoteDuration::Eighth;
            else
                beat.duration = NoteDuration::Sixteenth;
            
            // Füge alle Noten der Gruppe zum Beat hinzu (Akkord)
            for (const auto* note : group)
            {
                TabNote tabNote;
                tabNote.string = note->string;
                tabNote.fret = note->fret;
                tabNote.velocity = note->velocity;
                beat.notes.add(tabNote);
            }
            
            measure.beats.add(beat);
        }
        
        track.measures.add(measure);
    }
    
    return track;
}

//==============================================================================
// MIDI Export Functionality
//==============================================================================

bool NewProjectAudioProcessor::exportTrackToMidi(int trackIndex, const juce::File& outputFile)
{
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
    // JUCE erkennt automatisch: 1 Track = Format 0
    outputFile.deleteFile();
    juce::FileOutputStream outputStream(outputFile);
    
    if (!outputStream.openedOk())
        return false;
    
    // JUCE's writeTo() schreibt automatisch Format 0 bei einem Track
    // und fügt das End-of-Track Event (FF 2F 00) korrekt ein
    return midiFile.writeTo(outputStream);
}

bool NewProjectAudioProcessor::exportAllTracksToMidi(const juce::File& outputFile)
{
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

//==============================================================================
// Guitar Pro Export Functionality
//==============================================================================

bool NewProjectAudioProcessor::hasRecordedNotes() const
{
    std::lock_guard<std::mutex> lock(recordingMutex);
    return !recordedNotes.empty();
}

bool NewProjectAudioProcessor::exportRecordingToGP5(const juce::File& outputFile, const juce::String& title)
{
    // Get the recorded track
    TabTrack track = getRecordedTabTrack();
    
    if (track.measures.isEmpty())
    {
        DBG("No recorded notes to export");
        return false;
    }
    
    // Create GP5 writer
    GP5Writer writer;
    writer.setTitle(title.isEmpty() ? "Untitled" : title);
    writer.setArtist("GP5 VST Editor");
    writer.setTempo(static_cast<int>(hostTempo.load()));
    
    // Write to file
    bool success = writer.writeToFile(track, outputFile);
    
    if (!success)
    {
        DBG("GP5 export failed: " << writer.getLastError());
    }
    else
    {
        DBG("GP5 exported successfully to: " << outputFile.getFullPathName());
    }
    
    return success;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}
