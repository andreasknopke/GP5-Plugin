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
    // =========================================================================
    if (fileLoaded && midiOutputEnabled.load())
    {
        bool isPlaying = hostIsPlaying.load();
        double currentBeat = hostPositionBeats.load();
        int trackIdx = selectedTrackIndex.load();
        
        const auto& tracks = gp5Parser.getTracks();
        
        // Stop-Erkennung: Wenn Playback stoppt, alle Noten beenden
        if (!isPlaying && wasPlaying)
        {
            for (int note : activeNotes)
            {
                generatedMidi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
            }
            activeNotes.clear();
            lastProcessedMeasure = -1;
            lastProcessedBeatIndex = -1;
            DBG("MIDI: Playback stopped, all notes off");
        }
        
        // Nur MIDI generieren wenn:
        // - Playback läuft
        // - Gültiger Track ausgewählt
        if (isPlaying && trackIdx >= 0 && trackIdx < tracks.size())
        {
            const auto& track = tracks[trackIdx];
            
            // Berechne aktuellen Takt und Beat-Position
            int timeSigNum = hostTimeSigNumerator.load();
            int timeSigDen = hostTimeSigDenominator.load();
            double beatsPerMeasure = timeSigNum * (4.0 / timeSigDen);
            
            int measureIndex = static_cast<int>(currentBeat / beatsPerMeasure);
            double beatInMeasure = fmod(currentBeat, beatsPerMeasure);
            
            // Begrenze auf gültige Takte
            if (measureIndex >= 0 && measureIndex < track.measures.size())
            {
                const auto& measure = track.measures[measureIndex];
                const auto& beats = measure.voice1;  // Verwende Voice 1
                
                if (beats.size() > 0)
                {
                    // Berechne welcher Beat gerade gespielt wird
                    double beatDuration = beatsPerMeasure / beats.size();
                    int beatIndex = static_cast<int>(beatInMeasure / beatDuration);
                    beatIndex = juce::jlimit(0, beats.size() - 1, beatIndex);
                    
                    // Nur neue Noten spielen wenn sich der Beat geändert hat
                    if (measureIndex != lastProcessedMeasure || beatIndex != lastProcessedBeatIndex)
                    {
                        // Prüfe ob nächste Note ein Hammer-On/Pull-Off ist (Legato)
                        bool nextIsLegato = false;
                        if (beatIndex + 1 < beats.size())
                        {
                            const auto& nextBeat = beats[beatIndex + 1];
                            for (const auto& [si, n] : nextBeat.notes)
                            {
                                if (n.hasHammerOn) nextIsLegato = true;
                            }
                        }
                        
                        // Alle aktiven Noten stoppen (außer bei Legato)
                        if (!nextIsLegato)
                        {
                            for (int note : activeNotes)
                            {
                                generatedMidi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
                            }
                            activeNotes.clear();
                        }
                        
                        // Pitch Bend zurücksetzen am Anfang jedes Beats
                        generatedMidi.addEvent(juce::MidiMessage::pitchWheel(1, 8192), 0);  // 8192 = center
                        
                        // Neue Noten starten
                        const auto& beat = beats[beatIndex];
                        
                        if (!beat.isRest)
                        {
                            for (const auto& [stringIndex, gpNote] : beat.notes)
                            {
                                if (!gpNote.isDead && !gpNote.isTied)
                                {
                                    // Konvertiere Fret zu MIDI-Note
                                    // MIDI = Tuning der Saite + Fret
                                    int midiNote = 0;
                                    if (stringIndex < track.tuning.size())
                                    {
                                        midiNote = track.tuning[stringIndex] + gpNote.fret;
                                    }
                                    else
                                    {
                                        // Standard-Tuning als Fallback (E2, A2, D3, G3, B3, E4)
                                        const int standardTuning[] = { 64, 59, 55, 50, 45, 40 };
                                        if (stringIndex < 6)
                                            midiNote = standardTuning[stringIndex] + gpNote.fret;
                                    }
                                    
                                    // Velocity basierend auf Note-Velocity
                                    int velocity = gpNote.velocity > 0 ? gpNote.velocity : 95;
                                    if (gpNote.isGhost) velocity = 60;
                                    if (gpNote.hasAccent) velocity = 120;
                                    
                                    // =========================================
                                    // MIDI Controller für Spieltechniken
                                    // =========================================
                                    
                                    // Vibrato -> CC1 (Modulation Wheel)
                                    if (gpNote.hasVibrato)
                                    {
                                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 80), 0);  // Mod wheel
                                        DBG("MIDI: Vibrato ON (CC1=80)");
                                    }
                                    else
                                    {
                                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 0), 0);  // Mod wheel off
                                    }
                                    
                                    // Hammer-On / Pull-Off -> CC68 (Legato Pedal)
                                    if (gpNote.hasHammerOn)
                                    {
                                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 68, 127), 0);  // Legato ON
                                        velocity = juce::jmax(40, velocity - 20);  // Etwas leiser für Legato
                                        DBG("MIDI: Hammer-On/Pull-Off (CC68=127)");
                                    }
                                    else
                                    {
                                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 68, 0), 0);  // Legato OFF
                                    }
                                    
                                    // Slide -> CC5 (Portamento Time) + CC65 (Portamento ON)
                                    if (gpNote.hasSlide)
                                    {
                                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 65, 127), 0);  // Portamento ON
                                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 5, 64), 0);   // Portamento Time
                                        DBG("MIDI: Slide (Portamento ON)");
                                    }
                                    else
                                    {
                                        generatedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 65, 0), 0);  // Portamento OFF
                                    }
                                    
                                    // Bend -> Pitch Bend
                                    if (gpNote.hasBend && gpNote.bendValue != 0)
                                    {
                                        // GP5 bend value ist in "cents" oder Halbtonschritten
                                        // Pitch Bend Range: 0-16383, 8192 = center
                                        // Typisch: +/- 2 Halbtöne = 8192 pro 2 Halbtöne
                                        // bendValue von 100 = 1 Halbton in GP5
                                        int pitchBend = 8192 + (gpNote.bendValue * 4096 / 100);
                                        pitchBend = juce::jlimit(0, 16383, pitchBend);
                                        generatedMidi.addEvent(juce::MidiMessage::pitchWheel(1, pitchBend), 0);
                                        DBG("MIDI: Bend value=" << gpNote.bendValue << " -> PitchBend=" << pitchBend);
                                    }
                                    
                                    if (midiNote > 0 && midiNote < 128)
                                    {
                                        auto msg = juce::MidiMessage::noteOn(1, midiNote, (juce::uint8)velocity);
                                        generatedMidi.addEvent(msg, 0);
                                        activeNotes.insert(midiNote);
                                        DBG("MIDI: Note ON - " << midiNote << " vel=" << velocity << " (Measure " << measureIndex << ", Beat " << beatIndex << ")");
                                    }
                                }
                            }
                        }
                        
                        lastProcessedMeasure = measureIndex;
                        lastProcessedBeatIndex = beatIndex;
                    }
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
