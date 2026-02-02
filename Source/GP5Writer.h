/*
  ==============================================================================

    GP5Writer.h
    
    Guitar Pro 5 (.gp5) File Writer
    Creates GP5 files from recorded notes

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "TabModels.h"
#include <vector>

//==============================================================================
// GP5 File Writer - creates minimal GP5 files from TabTrack data
//==============================================================================
class GP5Writer
{
public:
    GP5Writer();
    ~GP5Writer() = default;
    
    // Song metadata
    void setTitle(const juce::String& title) { songTitle = title; }
    void setArtist(const juce::String& artist) { songArtist = artist; }
    void setTempo(int bpm) { tempo = bpm; }
    
    // Write a single TabTrack to a GP5 file
    bool writeToFile(const TabTrack& track, const juce::File& outputFile);
    
    // Write multiple TabTracks to a GP5 file (multi-channel recording)
    bool writeToFile(const std::vector<TabTrack>& tracks, const juce::File& outputFile);
    
    // Get last error message
    juce::String getLastError() const { return lastError; }
    
private:
    // Helper functions for writing GP5 binary format (in correct order!)
    void writeVersion();
    void writeSongInfo();           // 9 strings + notice lines
    void writeLyrics();             // Lyrics section
    void writePageSetup();          // Page layout settings
    void writeTempoInfo();          // Tempo name + value + key
    void writeMidiChannels();       // 64 MIDI channel settings (default instruments)
    void writeMidiChannels(const std::vector<TabTrack>& tracks);  // With track instruments
    void writeDirections();         // 19 direction signs
    void writeMeasureHeaders(int numMeasures, int numerator, int denominator);
    void writeTracks(const TabTrack& track);
    void writeTrack(const TabTrack& track, int trackIndex, int totalTracks);
    void writeMeasures(const TabTrack& track);
    void writeMeasuresMultiTrack(const std::vector<TabTrack>& tracks);
    void writeBeat(const TabBeat& beat, int stringCount);
    void writeNote(const TabNote& note);
    void writeNoteEffects(const NoteEffects& effects);
    void writeBend(const NoteEffects& effects);
    void writeBeatEffects(const TabBeat& beat);
    
    // Binary writing helpers
    void writeByte(juce::uint8 value);
    void writeShort(juce::int16 value);
    void writeInt(juce::int32 value);
    void writeBool(bool value);
    void writeString(const juce::String& str, int maxLength);
    void writeStringWithLength(const juce::String& str);
    void writeColor(juce::Colour color);
    
    // Constants for bend encoding (from PyGuitarPro)
    static constexpr int bendPosition = 60;    // Max position in GP file
    static constexpr int bendSemitone = 25;    // Value per semitone in GP file
    
    // Output stream
    std::unique_ptr<juce::FileOutputStream> outputStream;
    
    // Song metadata
    juce::String songTitle = "Untitled";
    juce::String songArtist = "Unknown";
    int tempo = 120;
    
    juce::String lastError;
};
