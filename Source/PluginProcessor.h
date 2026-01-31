/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GP5Parser.h"
#include "GP7Parser.h"
// MidiExpressionEngine deaktiviert - crasht bei erster Note
// #include "MidiExpressionEngine.h"
#include <atomic>
#include <set>
#include <map>
#include <vector>
#include <utility>

//==============================================================================
/**
*/
class NewProjectAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    NewProjectAudioProcessor();
    ~NewProjectAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // GP5/GP7 Parser - persistent data
    GP5Parser& getGP5Parser() { return gp5Parser; }
    const GP5Parser& getGP5Parser() const { return gp5Parser; }
    GP7Parser& getGP7Parser() { return gp7Parser; }
    const GP7Parser& getGP7Parser() const { return gp7Parser; }
    bool isUsingGP7Parser() const { return usingGP7Parser; }
    
    // Convenience methods that work with whichever parser is active
    const juce::Array<GP5Track>& getActiveTracks() const 
    {
        return usingGP7Parser ? gp7Parser.getTracks() : gp5Parser.getTracks();
    }
    
    const juce::Array<GP5MeasureHeader>& getActiveMeasureHeaders() const
    {
        return usingGP7Parser ? gp7Parser.getMeasureHeaders() : gp5Parser.getMeasureHeaders();
    }
    
    const GP5SongInfo& getActiveSongInfo() const
    {
        return usingGP7Parser ? gp7Parser.getSongInfo() : gp5Parser.getSongInfo();
    }
    
    juce::String getLoadedFilePath() const { return loadedFilePath; }
    void setLoadedFilePath(const juce::String& path) { loadedFilePath = path; }
    
    bool loadGP5File(const juce::File& file);
    bool isFileLoaded() const { return fileLoaded; }
    
    //==============================================================================
    // DAW Synchronisation - Thread-safe access from Editor
    bool isHostPlaying() const { return hostIsPlaying.load(); }
    double getHostTempo() const { return hostTempo.load(); }
    double getHostPositionInBeats() const { return hostPositionBeats.load(); }
    double getHostPositionInSeconds() const { return hostPositionSeconds.load(); }
    int getHostTimeSignatureNumerator() const { return hostTimeSigNumerator.load(); }
    int getHostTimeSignatureDenominator() const { return hostTimeSigDenominator.load(); }
    
    // Berechnet den aktuellen Takt (0-basiert) basierend auf GP5-Struktur
    int getCurrentMeasureIndex() const;
    
    // Position innerhalb des aktuellen Taktes (0.0 = Anfang, 1.0 = Ende)
    double getPositionInCurrentMeasure() const;
    
    // GP5 Taktart für einen bestimmten Takt (0-basiert)
    std::pair<int, int> getGP5TimeSignature(int measureIndex) const;
    
    // GP5 Tempo aus dem Song
    int getGP5Tempo() const;
    
    // Prüft ob DAW-Taktart mit GP5-Taktart übereinstimmt
    bool isTimeSignatureMatching() const;
    
    //==============================================================================
    // Seek Position - für Klick auf Tabulatur
    void setSeekPosition(int measureIndex, double positionInMeasure);
    double getSeekPositionInBeats() const { return seekPositionInBeats.load(); }
    int getSeekMeasureIndex() const { return seekMeasureIndex.load(); }
    double getSeekPositionInMeasure() const { return seekPositionInMeasure.load(); }
    bool hasSeekPosition() const { return seekPositionValid.load(); }
    void clearSeekPosition() { seekPositionValid.store(false); }
    
    //==============================================================================
    // Track Selection for MIDI Output
    void setSelectedTrack(int trackIndex) { selectedTrackIndex.store(trackIndex); }
    int getSelectedTrack() const { return selectedTrackIndex.load(); }
    
    // Saved UI state (for restoring after editor close/open)
    int getSavedSelectedTrack() const { return savedSelectedTrack; }
    void clearSavedSelectedTrack() { savedSelectedTrack = -1; }  // Mark as consumed
    
    bool isAutoScrollEnabled() const { return autoScrollEnabled.load(); }
    void setAutoScrollEnabled(bool enabled) { autoScrollEnabled.store(enabled); }
    
    // MIDI Output enable/disable
    void setMidiOutputEnabled(bool enabled) { midiOutputEnabled.store(enabled); }
    bool isMidiOutputEnabled() const { return midiOutputEnabled.load(); }
    
    //==============================================================================
    // Per-Track MIDI Settings
    //==============================================================================
    
    // MIDI Channel per track (1-16)
    int getTrackMidiChannel(int trackIndex) const 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            return trackMidiChannels[trackIndex].load();
        return 1;
    }
    void setTrackMidiChannel(int trackIndex, int channel) 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            trackMidiChannels[trackIndex].store(juce::jlimit(1, 16, channel));
    }
    
    // Mute per track
    bool isTrackMuted(int trackIndex) const 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            return trackMuted[trackIndex].load();
        return false;
    }
    void setTrackMuted(int trackIndex, bool muted) 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            trackMuted[trackIndex].store(muted);
    }
    
    // Solo per track
    bool isTrackSolo(int trackIndex) const 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            return trackSolo[trackIndex].load();
        return false;
    }
    void setTrackSolo(int trackIndex, bool solo) 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            trackSolo[trackIndex].store(solo);
    }
    
    // Check if any track has solo enabled
    bool hasAnySolo() const
    {
        for (int i = 0; i < maxTracks; ++i)
            if (trackSolo[i].load()) return true;
        return false;
    }
    
    // Volume per track (0-127, 100 = default)
    int getTrackVolume(int trackIndex) const 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            return trackVolume[trackIndex].load();
        return 100;
    }
    void setTrackVolume(int trackIndex, int volume) 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            trackVolume[trackIndex].store(juce::jlimit(0, 127, volume));
    }
    
    // Pan per track (0-127, 64 = center)
    int getTrackPan(int trackIndex) const 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            return trackPan[trackIndex].load();
        return 64;
    }
    void setTrackPan(int trackIndex, int pan) 
    { 
        if (trackIndex >= 0 && trackIndex < maxTracks)
            trackPan[trackIndex].store(juce::jlimit(0, 127, pan));
    }
    
    // Initialize track settings based on GP5 file
    void initializeTrackSettings();

private:
    //==============================================================================
    GP5Parser gp5Parser;
    GP7Parser gp7Parser;
    bool usingGP7Parser = false;  // Which parser was used for current file
    juce::String loadedFilePath;
    bool fileLoaded = false;
    
    // Selected track for MIDI output
    std::atomic<int> selectedTrackIndex { 0 };
    std::atomic<bool> midiOutputEnabled { true };
    
    // Per-track MIDI settings (max 16 tracks)
    static constexpr int maxTracks = 16;
    std::atomic<int> trackMidiChannels[maxTracks];
    std::atomic<bool> trackMuted[maxTracks];
    std::atomic<bool> trackSolo[maxTracks];
    std::atomic<int> trackVolume[maxTracks];
    std::atomic<int> trackPan[maxTracks];
    
    // Per-track active notes (for proper note-off per channel)
    std::map<int, std::set<int>> activeNotesPerChannel;  // channel -> active MIDI notes
    
    // Active bend tracking for real-time pitch bend interpolation
    struct ActiveBend {
        int midiChannel = 0;
        int midiNote = 0;
        double startBeat = 0.0;       // When the note started
        double durationBeats = 0.0;   // Total note duration in beats
        int bendType = 0;             // 1=bend, 2=bend+release, 3=release, 4=pre-bend, 5=pre-bend+release
        int maxBendValue = 0;         // Maximum bend value in 1/100 semitones
        std::vector<GP5BendPoint> points;
        int lastSentPitchBend = 8192; // Track last sent value to avoid redundant messages
    };
    static constexpr int maxActiveBends = 32;
    ActiveBend activeBends[maxActiveBends];
    int activeBendCount = 0;
    
    // MidiExpressionEngine deaktiviert - crasht bei erster Note
    // MidiExpressionEngine expressionEngines[maxTracks];
    
    // MIDI note tracking - which notes are currently playing
    std::set<int> activeNotes;  // MIDI note numbers currently playing (legacy, single track)
    double lastProcessedBeat = -1.0;
    int lastProcessedMeasure = -1;
    int lastProcessedBeatIndex = -1;
    bool wasPlaying = false;
    
    // Seek position (for click-to-seek functionality)
    std::atomic<double> seekPositionInBeats { 0.0 };
    std::atomic<int> seekMeasureIndex { 0 };
    std::atomic<double> seekPositionInMeasure { 0.0 };
    std::atomic<bool> seekPositionValid { false };
    
    // Per-track beat tracking
    std::vector<int> lastProcessedBeatPerTrack;
    std::vector<int> lastProcessedMeasurePerTrack;
    
    // DAW sync state (atomic for thread-safe access from UI)
    std::atomic<bool> hostIsPlaying { false };
    std::atomic<double> hostTempo { 120.0 };
    std::atomic<double> hostPositionBeats { 0.0 };
    std::atomic<double> hostPositionSeconds { 0.0 };
    std::atomic<int> hostTimeSigNumerator { 4 };
    std::atomic<int> hostTimeSigDenominator { 4 };
    
    // Saved UI state (restored when editor opens)
    int savedSelectedTrack = 0;
    std::atomic<bool> autoScrollEnabled { true };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessor)
};
