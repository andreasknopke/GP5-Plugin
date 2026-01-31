/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GP5Parser.h"
#include "GP7Parser.h"
#include "TabModels.h"
// MidiExpressionEngine deaktiviert - crasht bei erster Note
// #include "MidiExpressionEngine.h"
#include <atomic>
#include <mutex>
#include <set>
#include <map>
#include <array>
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
    void unloadFile();
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
    
    // Check if a track is currently playing notes
    bool isTrackPlaying(int trackIndex) const
    {
        if (trackIndex >= 0 && trackIndex < maxTracks)
        {
            double endTime = trackNoteEndTime[trackIndex].load();
            double now = juce::Time::getMillisecondCounterHiRes();
            return now < endTime;
        }
        return false;
    }
    
    // Initialize track settings based on GP5 file
    void initializeTrackSettings();
    
    //==============================================================================
    // MIDI Input -> Tab Display (Editor Mode)
    //==============================================================================
    
    // Fret position preference for MIDI to Tab conversion
    enum class FretPosition { Low, Mid, High };
    void setFretPosition(FretPosition pos) { fretPosition.store(static_cast<int>(pos)); }
    FretPosition getFretPosition() const { return static_cast<FretPosition>(fretPosition.load()); }
    
    // Get live MIDI notes for display (thread-safe)
    struct LiveMidiNote {
        int midiNote = 0;
        int velocity = 0;
        int string = 0;    // Calculated string (0 = highest)
        int fret = 0;      // Calculated fret
    };
    
    // Recorded note with timing information
    struct RecordedNote {
        int midiNote = 0;
        int velocity = 0;
        int string = 0;
        int fret = 0;
        double startBeat = 0.0;   // When note started (in beats)
        double endBeat = 0.0;     // When note ended (in beats)
        bool isActive = false;    // Still being held
    };
    
    // Get currently held notes for display
    std::vector<LiveMidiNote> getLiveMidiNotes() const;
    
    // Get empty tab track with DAW time signature for editor mode
    TabTrack getEmptyTabTrack() const;
    
    // Check if we have live MIDI input
    bool hasLiveMidiInput() const { return !liveMidiNotes.empty(); }
    
    //==============================================================================
    // Recording functionality (Editor Mode)
    //==============================================================================
    void setRecordingEnabled(bool enabled);
    bool isRecordingEnabled() const { return recordingEnabled.load(); }
    bool isRecording() const { return recordingEnabled.load() && hostIsPlaying.load(); }
    void clearRecording();
    std::vector<RecordedNote> getRecordedNotes() const;
    TabTrack getRecordedTabTrack() const;  // Convert recorded notes to TabTrack for display

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
    std::atomic<double> trackNoteEndTime[maxTracks];  // Millisecond timestamp when current note ends
    
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
    
    //==============================================================================
    // MIDI Input -> Tab Display (Editor Mode)
    //==============================================================================
    mutable std::mutex liveMidiMutex;
    std::map<int, LiveMidiNote> liveMidiNotes;  // midiNote -> LiveMidiNote
    
    // Recording state
    std::atomic<bool> recordingEnabled { false };
    mutable std::mutex recordingMutex;
    std::vector<RecordedNote> recordedNotes;
    std::map<int, size_t> activeRecordingNotes;  // midiNote -> index in recordedNotes
    
    // Fret position preference (0=Low, 1=Mid, 2=High)
    std::atomic<int> fretPosition { 0 };  // Default: Low
    
    // Standard guitar tuning for MIDI to Tab conversion (E2, A2, D3, G3, B3, E4)
    const std::array<int, 6> standardTuning = { 40, 45, 50, 55, 59, 64 };
    
    // Convert MIDI note to string/fret (standard position)
    LiveMidiNote midiNoteToTab(int midiNote, int velocity) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessor)
};
