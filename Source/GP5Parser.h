/*
  ==============================================================================

    GP5Parser.h
    
    Guitar Pro 5 (.gp5) File Parser
    Ported from Python library pyguitarpro

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "TabModels.h"
#include <map>

//==============================================================================
// GP5 Data Structures
//==============================================================================

struct GP5SongInfo
{
    juce::String version;
    juce::String title;
    juce::String subtitle;
    juce::String artist;
    juce::String album;
    juce::String words;
    juce::String music;
    juce::String copyright;
    juce::String tab;
    juce::String instructions;
    juce::StringArray notice;
    juce::String tempoName;
    int tempo = 120;
};

struct GP5MidiChannel
{
    int channel = 0;
    int instrument = 25;
    int volume = 100;
    int balance = 64;
    int chorus = 0;
    int reverb = 0;
    int phaser = 0;
    int tremolo = 0;
};

struct GP5MeasureHeader
{
    int number = 1;
    int numerator = 4;
    int denominator = 4;
    bool isRepeatOpen = false;
    int repeatClose = 0;
    int repeatAlternative = 0;
    juce::String marker;
    bool hasDoubleBar = false;
};

struct GP5Note
{
    int fret = 0;
    int velocity = 95;
    bool isTied = false;
    bool isDead = false;
    bool isGhost = false;
    bool hasAccent = false;
    bool hasHeavyAccent = false;
    bool hasVibrato = false;
    bool hasHammerOn = false;
    bool hasBend = false;
    int bendValue = 0;        // Max bend value in 1/100 semitones (100 = 1/2 tone, 200 = full tone)
    int bendType = 0;         // 0=none, 1=bend, 2=bend+release, 3=release, 4=pre-bend, 5=pre-bend+release
    bool hasReleaseBend = false;
    bool hasSlide = false;
    int slideType = 0;
    bool hasHarmonic = false;
    int harmonicType = 0;
};

struct GP5Beat
{
    std::map<int, GP5Note> notes;  // string index -> note
    int duration = 0;              // -2=whole, -1=half, 0=quarter, 1=eighth, etc.
    bool isDotted = false;
    bool isRest = false;
    int tupletN = 0;
    juce::String text;
    bool isPalmMute = false;
    bool hasDownstroke = false;
    bool hasUpstroke = false;
};

struct GP5TrackMeasure
{
    juce::Array<GP5Beat> voice1;
    juce::Array<GP5Beat> voice2;
};

struct GP5Track
{
    juce::String name;
    int stringCount = 6;
    juce::Array<int> tuning;
    int port = 0;
    int channelIndex = 0;
    int midiChannel = 1;      // MIDI channel (1-16)
    int volume = 100;         // Track volume (0-127)
    int pan = 64;             // Track pan (0-127, 64 = center)
    int fretCount = 24;
    int capo = 0;
    juce::Colour colour;
    bool isPercussion = false;
    bool is12String = false;
    bool isBanjo = false;
    juce::Array<GP5TrackMeasure> measures;
};

//==============================================================================
// GP5 Parser Class
//==============================================================================

class GP5Parser
{
public:
    GP5Parser();
    ~GP5Parser();
    
    // Parse a GP5 file
    bool parse(const juce::File& file);
    
    // Get parsed data
    const GP5SongInfo& getSongInfo() const { return songInfo; }
    const juce::Array<GP5Track>& getTracks() const { return tracks; }
    const juce::Array<GP5MeasureHeader>& getMeasureHeaders() const { return measureHeaders; }
    juce::String getLastError() const { return lastError; }
    int getTrackCount() const { return tracks.size(); }
    int getMeasureCount() const { return measureHeaders.size(); }
    
    // Convert to tab model
    TabTrack convertToTabTrack(int trackIndex) const;
    
private:
    // Parsed data
    GP5SongInfo songInfo;
    juce::Array<GP5MidiChannel> midiChannels;
    juce::Array<GP5MeasureHeader> measureHeaders;
    juce::Array<GP5Track> tracks;
    
    // State
    std::unique_ptr<juce::FileInputStream> inputStream;
    juce::String lastError;
    int versionMajor = 5;
    int versionMinor = 0;
    int versionPatch = 0;
    int currentTempo = 120;
    
    // High-level reading
    void readVersion();
    void readInfo();
    void readLyrics();
    void readRSEMasterEffect();
    void readPageSetup();
    void readDirections();
    void readMidiChannels();
    void readMeasures();
    void readMeasure(GP5Track& track, int measureIndex);
    int readVoice(juce::Array<GP5Beat>& beats, const GP5MeasureHeader& header);
    void readBeat(GP5Beat& beat);
    void readNote(GP5Note& note);
    void readNoteEffects(GP5Note& note);
    void readBeatEffects(GP5Beat& beat);
    void readMixTableChange();
    void readChord();
    
    // Low-level reading
    juce::uint8 readU8();
    juce::int8 readI8();
    juce::uint16 readU16();
    juce::int16 readI16();
    juce::int32 readI32();
    bool readBool();
    void skip(int count);
    juce::Colour readColor();
    juce::String readByteSizeString(int count);
    juce::String readIntSizeString();
    juce::String readIntByteSizeString();
    
    // Conversion helpers
    NoteDuration convertDuration(int gpDuration) const;
    SlideType convertSlideType(int gpSlide) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GP5Parser)
};
