/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
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
    // MIDI Output - Generiere MIDI-Noten basierend auf der aktuellen Position
    // ALLE TRACKS auf ihren jeweiligen MIDI-Kanälen
    // =========================================================================
    if (fileLoaded && midiOutputEnabled.load())
    {
        bool isPlaying = hostIsPlaying.load();
        double currentBeat = hostPositionBeats.load();
        
        const auto& tracks = gp5Parser.getTracks();
        
        // Stop-Erkennung: Wenn Playback stoppt, alle Noten auf allen Kanälen beenden
        if (!isPlaying && wasPlaying)
        {
            for (auto& [channel, notes] : activeNotesPerChannel)
            {
                for (int note : notes)
                {
                    generatedMidi.addEvent(juce::MidiMessage::noteOff(channel, note), 0);
                }
                notes.clear();
            }
            activeNotesPerChannel.clear();
            
            // Reset per-track beat tracking
            for (int i = 0; i < maxTracks; ++i)
            {
                lastProcessedBeatPerTrack[i] = -1;
                lastProcessedMeasurePerTrack[i] = -1;
            }
            
            DBG("MIDI: Playback stopped, all notes off on all channels");
        }
        
        // Nur MIDI generieren wenn Playback läuft
        if (isPlaying)
        {
            // Check if any track has Solo enabled
            bool anySoloActive = hasAnySolo();
            
            // Berechne aktuellen Takt und Beat-Position
            int timeSigNum = hostTimeSigNumerator.load();
            int timeSigDen = hostTimeSigDenominator.load();
            double beatsPerMeasure = timeSigNum * (4.0 / timeSigDen);
            
            int measureIndex = static_cast<int>(currentBeat / beatsPerMeasure);
            double beatInMeasure = fmod(currentBeat, beatsPerMeasure);
            
            // Iteriere über ALLE Tracks
            for (int trackIdx = 0; trackIdx < juce::jmin((int)tracks.size(), maxTracks); ++trackIdx)
            {
                // Check Mute/Solo status
                bool isMuted = isTrackMuted(trackIdx);
                bool isSolo = isTrackSolo(trackIdx);
                
                // Skip if muted OR if solo is active on another track but not this one
                if (isMuted || (anySoloActive && !isSolo))
                    continue;
                
                const auto& track = tracks[trackIdx];
                int midiChannel = getTrackMidiChannel(trackIdx);
                int volumeScale = getTrackVolume(trackIdx);
                int pan = getTrackPan(trackIdx);
                
                // Begrenze auf gültige Takte
                if (measureIndex < 0 || measureIndex >= track.measures.size())
                    continue;
                
                const auto& measure = track.measures[measureIndex];
                const auto& beats = measure.voice1;  // Verwende Voice 1
                
                if (beats.size() == 0)
                    continue;
                
                // Berechne welcher Beat gerade gespielt wird
                double beatDuration = beatsPerMeasure / beats.size();
                int beatIndex = static_cast<int>(beatInMeasure / beatDuration);
                beatIndex = juce::jlimit(0, (int)beats.size() - 1, beatIndex);
                
                // Nur neue Noten spielen wenn sich der Beat für diesen Track geändert hat
                if (measureIndex != lastProcessedMeasurePerTrack[trackIdx] || 
                    beatIndex != lastProcessedBeatPerTrack[trackIdx])
                {
                    // Prüfe ob nächste Note ein Hammer-On/Pull-Off ist (Legato)
                    bool nextIsLegato = false;
                    if (beatIndex + 1 < (int)beats.size())
                    {
                        const auto& nextBeat = beats[beatIndex + 1];
                        for (const auto& [si, n] : nextBeat.notes)
                        {
                            if (n.hasHammerOn) nextIsLegato = true;
                        }
                    }
                    
                    // Alle aktiven Noten auf diesem Kanal stoppen (außer bei Legato)
                    if (!nextIsLegato && activeNotesPerChannel.count(midiChannel))
                    {
                        for (int note : activeNotesPerChannel[midiChannel])
                        {
                            generatedMidi.addEvent(juce::MidiMessage::noteOff(midiChannel, note), 0);
                        }
                        activeNotesPerChannel[midiChannel].clear();
                    }
                    
                    // Pitch Bend zurücksetzen am Anfang jedes Beats
                    generatedMidi.addEvent(juce::MidiMessage::pitchWheel(midiChannel, 8192), 0);
                    
                    // Neue Noten starten
                    const auto& beat = beats[beatIndex];
                    
                    if (!beat.isRest)
                    {
                        // Send Volume (CC7) and Pan (CC10) at start of each beat
                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 7, volumeScale), 0);
                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 10, pan), 0);
                        
                        for (const auto& [stringIndex, gpNote] : beat.notes)
                        {
                            if (!gpNote.isDead && !gpNote.isTied)
                            {
                                // Konvertiere Fret zu MIDI-Note
                                int midiNote = 0;
                                if (stringIndex < track.tuning.size())
                                {
                                    midiNote = track.tuning[stringIndex] + gpNote.fret;
                                }
                                else
                                {
                                    // Standard-Tuning als Fallback
                                    const int standardTuning[] = { 64, 59, 55, 50, 45, 40 };
                                    if (stringIndex < 6)
                                        midiNote = standardTuning[stringIndex] + gpNote.fret;
                                }
                                
                                // Velocity basierend auf Note-Velocity, skaliert mit Track-Volume
                                int velocity = gpNote.velocity > 0 ? gpNote.velocity : 95;
                                if (gpNote.isGhost) velocity = 60;
                                if (gpNote.hasAccent) velocity = 120;
                                
                                // MIDI Controller für Spieltechniken
                                if (gpNote.hasVibrato)
                                {
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 1, 80), 0);
                                }
                                else
                                {
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 1, 0), 0);
                                }
                                
                                if (gpNote.hasHammerOn)
                                {
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 68, 127), 0);
                                    velocity = juce::jmax(40, velocity - 20);
                                }
                                else
                                {
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 68, 0), 0);
                                }
                                
                                if (gpNote.hasSlide)
                                {
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 65, 127), 0);
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 5, 64), 0);
                                }
                                else
                                {
                                    generatedMidi.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 65, 0), 0);
                                }
                                
                                if (gpNote.hasBend && gpNote.bendValue != 0)
                                {
                                    int pitchBend = 8192 + (gpNote.bendValue * 4096 / 100);
                                    pitchBend = juce::jlimit(0, 16383, pitchBend);
                                    generatedMidi.addEvent(juce::MidiMessage::pitchWheel(midiChannel, pitchBend), 0);
                                }
                                
                                if (midiNote > 0 && midiNote < 128)
                                {
                                    auto msg = juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity);
                                    generatedMidi.addEvent(msg, 0);
                                    activeNotesPerChannel[midiChannel].insert(midiNote);
                                }
                            }
                        }
                    }
                    
                    lastProcessedMeasurePerTrack[trackIdx] = measureIndex;
                    lastProcessedBeatPerTrack[trackIdx] = beatIndex;
                }
            }
        }
        
        wasPlaying = isPlaying;
        lastProcessedBeat = currentBeat;
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
    // Speichere den Dateipfad der geladenen GP5-Datei
    juce::ValueTree state ("GP5PluginState");
    state.setProperty ("filePath", loadedFilePath, nullptr);
    
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
    }
}

bool NewProjectAudioProcessor::loadGP5File(const juce::File& file)
{
    if (gp5Parser.parse(file))
    {
        loadedFilePath = file.getFullPathName();
        fileLoaded = true;
        
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
    const auto& tracks = gp5Parser.getTracks();
    
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
    
    // Einfache Berechnung basierend auf der aktuellen DAW-Taktart
    // Die meisten DAWs verwenden PPQ (Pulses Per Quarter note)
    // Bei 4/4: 4 Beats pro Takt, bei 3/4: 3 Beats pro Takt
    int timeSigNum = hostTimeSigNumerator.load();
    int timeSigDen = hostTimeSigDenominator.load();
    
    // Beats pro Takt = Zähler * (4 / Nenner)
    double beatsPerMeasure = timeSigNum * (4.0 / timeSigDen);
    
    // Takt-Index berechnen (0-basiert)
    int measureIndex = static_cast<int>(positionInBeats / beatsPerMeasure);
    
    // Begrenzen auf gültige Takte
    int maxMeasure = gp5Parser.getMeasureCount() - 1;
    return juce::jlimit(0, juce::jmax(0, maxMeasure), measureIndex);
}

double NewProjectAudioProcessor::getPositionInCurrentMeasure() const
{
    double positionInBeats = hostPositionBeats.load();
    int timeSigNum = hostTimeSigNumerator.load();
    int timeSigDen = hostTimeSigDenominator.load();
    
    // Beats pro Takt
    double beatsPerMeasure = timeSigNum * (4.0 / timeSigDen);
    
    // Position innerhalb des Taktes (0.0 - 1.0)
    double posInMeasure = fmod(positionInBeats, beatsPerMeasure) / beatsPerMeasure;
    return juce::jlimit(0.0, 1.0, posInMeasure);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}
