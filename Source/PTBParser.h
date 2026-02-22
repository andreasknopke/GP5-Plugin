/*
  ==============================================================================

    PTBParser.h
    
    Power Tab (.ptb) File Parser
    Uses the powertab document library (wxWindows license) from
    https://github.com/powertab/powertabeditor
    
    Converts PTB data into the same GP5Track/GP5MeasureHeader/GP5SongInfo
    structures used by GP5Parser, so the rest of the plugin can treat
    PTB files identically to Guitar Pro files.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "GP5Parser.h"   // We reuse GP5Track, GP5MeasureHeader, GP5SongInfo, etc.

//==============================================================================
// PTB Parser Class
//==============================================================================

class PTBParser
{
public:
    PTBParser();
    ~PTBParser();
    
    /// Parse a Power Tab (.ptb) file
    bool parse(const juce::File& file);
    
    // --- Accessors (same interface as GP5Parser) ---
    const GP5SongInfo& getSongInfo() const { return songInfo; }
    const juce::Array<GP5Track>& getTracks() const { return tracks; }
    const juce::Array<GP5MeasureHeader>& getMeasureHeaders() const { return measureHeaders; }
    juce::String getLastError() const { return lastError; }
    int getTrackCount() const { return tracks.size(); }
    int getMeasureCount() const { return measureHeaders.size(); }
    
    /// Convert a track to TabTrack (same as GP5Parser::convertToTabTrack)
    TabTrack convertToTabTrack(int trackIndex) const;
    
private:
    GP5SongInfo songInfo;
    juce::Array<GP5MeasureHeader> measureHeaders;
    juce::Array<GP5Track> tracks;
    juce::String lastError;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PTBParser)
};
