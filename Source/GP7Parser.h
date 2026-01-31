/*
  ==============================================================================

    GP7Parser.h
    
    Parser for Guitar Pro 7/8 (.gp) files
    Based on alphaTab's GpifParser (https://github.com/CoderLine/alphaTab)
    
    GP7/8 files are ZIP archives containing:
    - Content/score.gpif (XML with all music data)
    - BinaryStylesheet (optional)
    - PartConfiguration (optional)
    - LayoutConfiguration (optional)

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "GP5Parser.h"  // Reuse existing data structures

//==============================================================================
// GPIF-specific intermediate structures
//==============================================================================

struct GpifRhythm
{
    int duration = 0;      // -2=whole, -1=half, 0=quarter, 1=eighth, etc.
    bool isDotted = false;
    bool isDoubleDotted = false;
    int tupletN = 0;       // Tuplet numerator (e.g., 3 for triplet)
    int tupletD = 0;       // Tuplet denominator (e.g., 2 for triplet)
};

struct GpifNote
{
    int string = 0;
    int fret = 0;
    int velocity = 100;
    bool isTied = false;
    bool isGhost = false;
    bool isLetRing = false;
    bool isPalmMuted = false;
    bool isDead = false;
    bool isHammerOn = false;
    bool hasVibrato = false;
    bool hasSlide = false;
    int slideType = 0;
    bool hasBend = false;
    float bendValue = 0.0f;
    int bendType = 0;
    int harmonicType = 0;
};

struct GpifBeat
{
    juce::String id;
    juce::String rhythmRef;
    juce::StringArray noteRefs;
    juce::String chordName;
    juce::String text;
    bool isRest = false;
    bool isPalmMuted = false;
    bool hasDownstroke = false;
    bool hasUpstroke = false;
};

struct GpifVoice
{
    juce::String id;
    juce::StringArray beatRefs;
};

struct GpifBar
{
    juce::String id;
    juce::StringArray voiceRefs;
    int clef = 0;  // 0=G2, 1=F4, 2=C3, 3=C4, 4=Neutral
};

struct GpifMasterBar
{
    juce::StringArray barRefs;  // Bar IDs per track
    int timeNumerator = 4;
    int timeDenominator = 4;
    int keySignature = 0;
    bool isRepeatStart = false;
    bool isRepeatEnd = false;
    int repeatCount = 0;
    int alternateEnding = 0;
    juce::String marker;
    juce::String chordName;
};

//==============================================================================
// GP7 Parser Class
//==============================================================================

class GP7Parser
{
public:
    GP7Parser();
    ~GP7Parser();
    
    //==========================================================================
    // Main parsing interface
    //==========================================================================
    bool parseFile(const juce::File& file);
    
    juce::String getLastError() const { return lastError; }
    
    //==========================================================================
    // Access parsed data (same interface as GP5Parser)
    //==========================================================================
    const GP5SongInfo& getSongInfo() const { return songInfo; }
    const juce::Array<GP5MeasureHeader>& getMeasureHeaders() const { return measureHeaders; }
    const juce::Array<GP5Track>& getTracks() const { return tracks; }
    
    // Convert to TabModels for rendering
    juce::Array<TabMeasure> convertToTabMeasures(int trackIndex) const;
    
private:
    //==========================================================================
    // ZIP extraction
    //==========================================================================
    bool extractGpifFromZip(const juce::File& file, juce::String& xmlContent);
    
    //==========================================================================
    // XML Parsing - Pass 1: Collect all elements
    //==========================================================================
    void parseGPIF(juce::XmlElement* root);
    void parseScore(juce::XmlElement* node);
    void parseMasterTrack(juce::XmlElement* node);
    void parseTracks(juce::XmlElement* node);
    void parseTrack(juce::XmlElement* node);
    void parseMasterBars(juce::XmlElement* node);
    void parseMasterBar(juce::XmlElement* node);
    void parseBars(juce::XmlElement* node);
    void parseBar(juce::XmlElement* node);
    void parseVoices(juce::XmlElement* node);
    void parseVoice(juce::XmlElement* node);
    void parseBeats(juce::XmlElement* node);
    void parseBeat(juce::XmlElement* node);
    void parseNotes(juce::XmlElement* node);
    void parseNote(juce::XmlElement* node);
    void parseRhythms(juce::XmlElement* node);
    void parseRhythm(juce::XmlElement* node);
    
    // Note property parsing
    void parseNoteProperties(juce::XmlElement* node, GpifNote& note);
    
    //==========================================================================
    // XML Parsing - Pass 2: Build model from collected elements
    //==========================================================================
    void buildModel();
    
    //==========================================================================
    // Helper functions
    //==========================================================================
    static juce::StringArray splitString(const juce::String& text, const juce::String& separator = " ");
    static int parseIntSafe(const juce::String& text, int fallback = 0);
    static float parseFloatSafe(const juce::String& text, float fallback = 0.0f);
    int durationToGP5(int gpifDuration);
    
    //==========================================================================
    // Collected data (Pass 1)
    //==========================================================================
    std::map<juce::String, GP5Track> tracksById;
    std::map<juce::String, GpifBar> barsById;
    std::map<juce::String, GpifVoice> voicesById;
    std::map<juce::String, GpifBeat> beatsById;
    std::map<juce::String, GpifNote> notesById;
    std::map<juce::String, GpifRhythm> rhythmsById;
    
    juce::StringArray trackMapping;  // Order of tracks
    juce::Array<GpifMasterBar> masterBars;
    
    //==========================================================================
    // Final model data (Pass 2)
    //==========================================================================
    GP5SongInfo songInfo;
    juce::Array<GP5MeasureHeader> measureHeaders;
    juce::Array<GP5Track> tracks;
    
    int currentTempo = 120;
    juce::String lastError;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GP7Parser)
};
