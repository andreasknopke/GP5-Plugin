/*
  ==============================================================================

    PTBParser.cpp
    
    Power Tab (.ptb) File Parser - Implementation
    Uses the powertab document library (wxWindows license) from
    https://github.com/powertab/powertabeditor
    
    Reads PTB files via PowerTabDocument::Document and converts the internal
    representation into GP5Track / GP5MeasureHeader / GP5SongInfo structures
    so the rest of the plugin can handle them identically to Guitar Pro files.

  ==============================================================================
*/

#include "PTBParser.h"

// PowerTab document library headers
#include "powertabdocument.h"
#include "score.h"
#include "guitar.h"
#include "system.h"
#include "staff.h"
#include "position.h"
#include "note.h"
#include "barline.h"
#include "timesignature.h"
#include "tempomarker.h"
#include "alternateending.h"
#include "chorddiagram.h"
#include "chordtext.h"
#include "direction.h"
#include "dynamic.h"
#include "rehearsalsign.h"
#include "tuning.h"
#include "guitarin.h"

//==============================================================================
PTBParser::PTBParser() {}
PTBParser::~PTBParser() {}

//==============================================================================
// Helper: convert PTB duration type to GP5 duration encoding
// PTB: 1=whole, 2=half, 4=quarter, 8=8th, 16=16th, 32=32nd, 64=64th
// GP5: -2=whole, -1=half, 0=quarter, 1=eighth, 2=16th, 3=32nd, 4=64th
static int convertPTBDurationToGP5(uint8_t ptbDuration)
{
    switch (ptbDuration)
    {
        case 1:  return -2;   // whole
        case 2:  return -1;   // half
        case 4:  return 0;    // quarter
        case 8:  return 1;    // eighth
        case 16: return 2;    // 16th
        case 32: return 3;    // 32nd
        case 64: return 4;    // 64th
        default: return 0;    // quarter as fallback
    }
}

//==============================================================================
// Helper: convert PTB slide types to GP5 slide type values
// GP5 slide types: 1=shift, 2=legato, 3=outDown, 4=outUp, 5=intoBelow, 6=intoAbove
static int convertPTBSlideToGP5(uint8_t slideInto, uint8_t slideOut)
{
    using namespace PowerTabDocument;
    
    // Prioritize slide-out types
    if (slideOut == Note::slideOutOfShiftSlide)  return 1;  // Shift slide
    if (slideOut == Note::slideOutOfLegatoSlide) return 2;  // Legato slide
    if (slideOut == Note::slideOutOfDownwards)    return 3;  // Slide out down
    if (slideOut == Note::slideOutOfUpwards)      return 4;  // Slide out up
    
    // Slide-into types
    if (slideInto == Note::slideIntoFromBelow)   return 5;  // Slide into from below
    if (slideInto == Note::slideIntoFromAbove)   return 6;  // Slide into from above
    
    return 1; // default shift slide
}

//==============================================================================
// Helper: convert PTB bend type to GP5 bend type
// GP5: 1=bend, 2=bend+release, 3=release, 4=pre-bend, 5=pre-bend+release
static int convertPTBBendToGP5(uint8_t ptbBendType)
{
    using namespace PowerTabDocument;
    
    switch (ptbBendType)
    {
        case Note::normalBend:          return 1;  // bend
        case Note::bendAndRelease:      return 2;  // bend + release
        case Note::bendAndHold:         return 1;  // bend (held)
        case Note::preBend:             return 4;  // pre-bend
        case Note::preBendAndRelease:   return 5;  // pre-bend + release
        case Note::preBendAndHold:      return 4;  // pre-bend (held)
        case Note::gradualRelease:      return 3;  // release
        case Note::immediateRelease:    return 3;  // release
        default:                        return 1;  // bend
    }
}

//==============================================================================
bool PTBParser::parse(const juce::File& file)
{
    // Reset state
    songInfo = GP5SongInfo();
    measureHeaders.clear();
    tracks.clear();
    lastError.clear();
    
    if (!file.existsAsFile())
    {
        lastError = "File does not exist: " + file.getFullPathName();
        return false;
    }
    
    try
    {
        // Load the PTB document using the powertab library
        PowerTabDocument::Document ptbDoc;
        
        auto stdPath = std::filesystem::path(file.getFullPathName().toStdString());
        ptbDoc.Load(stdPath);
        
        // ================================================================
        // 1. Extract song info from header
        // ================================================================
        const auto& header = ptbDoc.GetHeader();
        
        songInfo.title = juce::String(header.GetSongTitle());
        songInfo.artist = juce::String(header.GetSongArtist());
        songInfo.version = "ptb";
        
        // Try to get tempo from the first tempo marker
        songInfo.tempo = 120; // default
        
        // ================================================================
        // 2. PTB has 2 scores: Guitar Score (index 0) and Bass Score (index 1)
        //    We merge them into a flat list of tracks like GP5 does.
        // ================================================================
        
        // Collect all guitars (instruments) from both scores
        struct PTBGuitarInfo
        {
            std::shared_ptr<PowerTabDocument::Guitar> guitar;
            int scoreIndex; // 0=guitar score, 1=bass score
            int guitarIndexInScore;
        };
        std::vector<PTBGuitarInfo> allGuitars;
        
        for (size_t scoreIdx = 0; scoreIdx < ptbDoc.GetNumberOfScores(); ++scoreIdx)
        {
            auto score = ptbDoc.GetScore(scoreIdx);
            if (!score) continue;
            
            for (size_t g = 0; g < score->GetGuitarCount(); ++g)
            {
                auto guitar = score->GetGuitar(g);
                if (guitar)
                {
                    allGuitars.push_back({guitar, (int)scoreIdx, (int)g});
                }
            }
        }
        
        if (allGuitars.empty())
        {
            lastError = "No guitars/instruments found in PTB file";
            return false;
        }
        
        // ================================================================
        // 3. Build measure headers from the system/barline structure
        //    PTB organizes music as Systems (each with barlines inside).
        //    We need to flatten this into a linear measure list.
        // ================================================================
        
        // Use the guitar score (index 0) as the primary reference for structure.
        // If it's empty, fall back to bass score (index 1).
        int primaryScoreIdx = 0;
        auto primaryScore = ptbDoc.GetScore(0);
        if (!primaryScore || primaryScore->GetSystemCount() == 0)
        {
            primaryScoreIdx = 1;
            primaryScore = ptbDoc.GetScore(1);
        }
        
        if (!primaryScore || primaryScore->GetSystemCount() == 0)
        {
            lastError = "PTB file contains no musical systems";
            return false;
        }
        
        // Extract tempo from the first tempo marker
        for (size_t t = 0; t < primaryScore->GetTempoMarkerCount(); ++t)
        {
            auto tempo = primaryScore->GetTempoMarker(t);
            if (tempo)
            {
                songInfo.tempo = (int)tempo->GetBeatsPerMinute();
                break;
            }
        }
        
        // ================================================================
        // 3a. Flatten systems into measures
        // ================================================================
        // Each PTB System has:
        //   - A start barline (with time signature, key, repeat info)
        //   - Internal barlines (dividing the system into measures)
        //   - An end barline
        //   - Staves containing the actual notes
        //
        // We process each system and extract measures between barlines.
        
        // Track the current time signature (persists across systems)
        int currentNumerator = 4;
        int currentDenominator = 4;
        int measureNumber = 1;
        
        // Structure to hold per-system measure info for note extraction later
        struct SystemMeasureInfo
        {
            int systemIndex;
            int startPosition;  // position index of the start barline
            int endPosition;    // position index of the end barline
            int numerator;
            int denominator;
            bool isRepeatOpen;
            int repeatClose;
            int alternateEnding;
            juce::String marker;
        };
        std::vector<SystemMeasureInfo> allMeasureInfos;
        
        for (size_t sysIdx = 0; sysIdx < primaryScore->GetSystemCount(); ++sysIdx)
        {
            auto system = primaryScore->GetSystem(sysIdx);
            if (!system) continue;
            
            // Get the start bar's time signature
            const auto& startBar = *system->GetStartBar();
            const auto& startTimeSig = startBar.GetTimeSignature();
            
            // Update time signature if the start bar has one
            uint8_t beats = startTimeSig.GetBeatsPerMeasure();
            uint8_t beatValue = startTimeSig.GetBeatAmount();
            if (beats > 0 && beatValue > 0)
            {
                currentNumerator = beats;
                currentDenominator = beatValue;
            }
            
            // Collect all barline positions within this system (excluding start/end)
            std::vector<int> barPositions;
            barPositions.push_back(0); // virtual start position
            
            for (size_t bIdx = 0; bIdx < system->GetBarlineCount(); ++bIdx)
            {
                auto barline = system->GetBarline(bIdx);
                if (barline)
                {
                    barPositions.push_back((int)barline->GetPosition());
                    
                    // Check if barline changes time signature
                    const auto& barTimeSig = barline->GetTimeSignature();
                    uint8_t bb = barTimeSig.GetBeatsPerMeasure();
                    uint8_t bv = barTimeSig.GetBeatAmount();
                    if (bb > 0 && bv > 0)
                    {
                        currentNumerator = bb;
                        currentDenominator = bv;
                    }
                }
            }
            
            // Add the end position (use a large value to capture everything after last barline)
            int endBarPosition = 255; // positions in PTB are uint8_t, max 255
            barPositions.push_back(endBarPosition);
            
            // Sort barPositions
            std::sort(barPositions.begin(), barPositions.end());
            // Remove duplicates
            barPositions.erase(std::unique(barPositions.begin(), barPositions.end()), barPositions.end());
            
            // Create measure headers for each region between barlines
            for (size_t bpIdx = 0; bpIdx + 1 < barPositions.size(); ++bpIdx)
            {
                int startPos = barPositions[bpIdx];
                int endPos = barPositions[bpIdx + 1];
                
                GP5MeasureHeader mh;
                mh.number = measureNumber++;
                mh.numerator = currentNumerator;
                mh.denominator = currentDenominator;
                
                // Check barline properties at startPos
                bool isRepeatOpen = false;
                int repeatClose = 0;
                juce::String marker;
                
                if (bpIdx == 0)
                {
                    // Start bar of the system
                    isRepeatOpen = startBar.IsRepeatStart();
                    if (startBar.GetRehearsalSign().IsSet())
                        marker = juce::String(startBar.GetRehearsalSign().GetDescription());
                }
                
                // Check the barline at startPos for repeat info
                for (size_t bIdx = 0; bIdx < system->GetBarlineCount(); ++bIdx)
                {
                    auto bl = system->GetBarline(bIdx);
                    if (bl && (int)bl->GetPosition() == startPos)
                    {
                        isRepeatOpen = bl->IsRepeatStart();
                        if (bl->GetRehearsalSign().IsSet())
                            marker = juce::String(bl->GetRehearsalSign().GetDescription());
                        break;
                    }
                }
                
                // Check if the barline at endPos is a repeat end
                if (bpIdx + 2 < barPositions.size())
                {
                    for (size_t bIdx = 0; bIdx < system->GetBarlineCount(); ++bIdx)
                    {
                        auto bl = system->GetBarline(bIdx);
                        if (bl && (int)bl->GetPosition() == endPos)
                        {
                            if (bl->IsRepeatEnd())
                                repeatClose = (int)bl->GetRepeatCount();
                            break;
                        }
                    }
                }
                else
                {
                    // End bar of the system
                    const auto& endBar = *system->GetEndBar();
                    if (endBar.IsRepeatEnd())
                        repeatClose = (int)endBar.GetRepeatCount();
                }
                
                mh.isRepeatOpen = isRepeatOpen;
                mh.repeatClose = repeatClose;
                mh.marker = marker;
                
                // Check alternate endings
                int altEnding = 0;
                // PTB alternate endings are stored at system level
                // (we'll handle them simplified for now)
                mh.repeatAlternative = altEnding;
                
                measureHeaders.add(mh);
                
                SystemMeasureInfo smi;
                smi.systemIndex = (int)sysIdx;
                smi.startPosition = startPos;
                smi.endPosition = endPos;
                smi.numerator = mh.numerator;
                smi.denominator = mh.denominator;
                smi.isRepeatOpen = isRepeatOpen;
                smi.repeatClose = repeatClose;
                smi.alternateEnding = altEnding;
                smi.marker = marker;
                allMeasureInfos.push_back(smi);
            }
        }
        
        // ================================================================
        // 4. Create GP5Track for each guitar, extracting notes from staves
        // ================================================================
        
        // PTB maps guitars to staves via "GuitarIn" objects.
        // For simplicity, we map Staff[i] in a score to Guitar[i], which is
        // the common case for most PTB files.
        
        for (size_t gIdx = 0; gIdx < allGuitars.size(); ++gIdx)
        {
            const auto& gInfo = allGuitars[gIdx];
            auto guitar = gInfo.guitar;
            auto score = ptbDoc.GetScore(gInfo.scoreIndex);
            
            GP5Track gp5Track;
            gp5Track.name = juce::String(guitar->GetDescription());
            
            // Fallback name for unnamed instruments
            if (gp5Track.name.isEmpty() || gp5Track.name == "Untitled")
            {
                juce::String scorePrefix = (gInfo.scoreIndex == 0) ? "Guitar" : "Bass";
                gp5Track.name = scorePrefix + " " + juce::String(gInfo.guitarIndexInScore + 1);
            }
            
            // Tuning
            const auto& tuning = guitar->GetTuning();
            auto tuningNotes = tuning.GetTuningNotes();
            gp5Track.stringCount = (int)tuningNotes.size();
            
            // PTB tuning note order: high string to low string (same as GP5)
            for (int s = 0; s < (int)tuningNotes.size(); ++s)
            {
                gp5Track.tuning.add(tuningNotes[s]);
            }
            
            gp5Track.capo = guitar->GetCapo();
            gp5Track.midiChannel = gInfo.guitarIndexInScore + 1; // 1-based
            gp5Track.volume = guitar->GetInitialVolume();
            gp5Track.pan = guitar->GetPan();
            
            // Instrument preset
            uint8_t preset = guitar->GetPreset();
            
            // Check if it's a percussion track (MIDI channel 10)
            gp5Track.isPercussion = false;
            
            // Color
            int hue = (int)(gIdx * 47) % 360;
            gp5Track.colour = juce::Colour::fromHSV(hue / 360.0f, 0.6f, 0.9f, 1.0f);
            
            // ============================================================
            // 4a. Extract notes for each measure
            // ============================================================
            
            // Determine which staff index this guitar corresponds to
            // In PTB, Guitar N typically maps to Staff N in its score
            int staffIndex = gInfo.guitarIndexInScore;
            
            int totalBeatsExtracted = 0;
            
            for (size_t mIdx = 0; mIdx < allMeasureInfos.size(); ++mIdx)
            {
                const auto& mInfo = allMeasureInfos[mIdx];
                
                GP5TrackMeasure trackMeasure;
                
                // Find the system for this score
                // The allMeasureInfos was built from primaryScore.
                // For the secondary score, we need to find the corresponding system.
                // For simplicity, use the same system index (PTB files typically
                // have matching system counts for guitar and bass scores).
                
                int sysIdx = mInfo.systemIndex;
                
                if (sysIdx < (int)score->GetSystemCount())
                {
                    auto system = score->GetSystem(sysIdx);
                    
                    // Only use the staff if this guitar's index exists in the system.
                    // Do NOT fall back to staff 0, because that belongs to a different guitar.
                    // Systems with fewer staves than guitars simply don't contain data for this guitar.
                    if (system && staffIndex < (int)system->GetStaffCount())
                    {
                        auto staff = system->GetStaff(staffIndex);
                        if (staff)
                        {
                            // Process voice 0 (primary voice)
                            for (size_t posIdx = 0; posIdx < staff->GetPositionCount(0); ++posIdx)
                            {
                                const auto* position = staff->GetPosition(0, posIdx);
                                if (!position) continue;
                                
                                int posValue = (int)position->GetPosition();
                                
                                // Check if this position falls within our measure range
                                if (posValue >= mInfo.startPosition && posValue < mInfo.endPosition)
                                {
                                    GP5Beat beat;
                                    
                                    // Duration
                                    beat.duration = convertPTBDurationToGP5(position->GetDurationType());
                                    beat.isDotted = position->IsDotted() || position->IsDoubleDotted();
                                    beat.isRest = position->IsRest();
                                    
                                    // Beat effects
                                    beat.isPalmMute = position->HasPalmMuting();
                                    beat.hasDownstroke = position->HasPickStrokeDown();
                                    beat.hasUpstroke = position->HasPickStrokeUp();
                                    
                                    // Tuplet info
                                    if (position->HasIrregularGroupingTiming())
                                    {
                                        uint8_t notesPlayed = 0, notesPlayedOver = 0;
                                        position->GetIrregularGroupingTiming(notesPlayed, notesPlayedOver);
                                        if (notesPlayed > 0)
                                            beat.tupletN = notesPlayed;
                                    }
                                    
                                    // Chord text for this position
                                    for (size_t cIdx = 0; cIdx < system->GetChordTextCount(); ++cIdx)
                                    {
                                        auto chordText = system->GetChordText(cIdx);
                                        if (chordText && (int)chordText->GetPosition() == posValue)
                                        {
                                            // Build chord name from the ChordName object
                                            const auto& chordName = chordText->GetChordNameConstRef();
                                            uint8_t tonicKey = 0, tonicVar = 0;
                                            chordName.GetTonic(tonicKey, tonicVar);
                                            
                                            static const char* noteNames[] = {"C", "D", "E", "F", "G", "A", "B"};
                                            if (tonicKey < 7)
                                            {
                                                juce::String name = noteNames[tonicKey];
                                                if (tonicVar == 1) name += "#";
                                                else if (tonicVar == 2) name += "b";
                                                
                                                // Add formula suffix
                                                uint16_t formula = chordName.GetFormula();
                                                if (formula & 0x2) name += "m";     // minor
                                                if (formula & 0x40) name += "7";    // 7th
                                                if (formula & 0x80) name += "maj7"; // maj7
                                                
                                                beat.chordName = name;
                                            }
                                            break;
                                        }
                                    }
                                    
                                    // Notes
                                    if (!beat.isRest)
                                    {
                                        for (size_t nIdx = 0; nIdx < position->m_noteArray.size(); ++nIdx)
                                        {
                                            const auto* ptbNote = position->m_noteArray[nIdx];
                                            if (!ptbNote) continue;
                                            
                                            int stringIdx = (int)ptbNote->GetString();
                                            
                                            GP5Note gp5Note;
                                            gp5Note.fret = ptbNote->GetFretNumber();
                                            gp5Note.velocity = 95; // PTB doesn't store per-note velocity in the note object
                                            gp5Note.isTied = ptbNote->IsTied();
                                            gp5Note.isDead = ptbNote->IsMuted();
                                            gp5Note.isGhost = ptbNote->IsGhostNote();
                                            
                                            // Accent from position level
                                            gp5Note.hasAccent = position->HasMarcato();
                                            gp5Note.hasHeavyAccent = position->HasSforzando();
                                            
                                            // Vibrato from position level
                                            gp5Note.hasVibrato = position->HasVibrato() || position->HasWideVibrato();
                                            
                                            // Hammer-on / Pull-off
                                            gp5Note.hasHammerOn = ptbNote->HasHammerOn() || ptbNote->HasPullOff() ||
                                                                   ptbNote->HasHammerOnFromNowhere() || ptbNote->HasPullOffToNowhere();
                                            
                                            // Harmonics
                                            if (ptbNote->IsNaturalHarmonic())
                                            {
                                                gp5Note.hasHarmonic = true;
                                                gp5Note.harmonicType = 1; // Natural
                                            }
                                            if (ptbNote->HasArtificialHarmonic())
                                            {
                                                gp5Note.hasHarmonic = true;
                                                gp5Note.harmonicType = 2; // Artificial
                                                uint8_t key = 0, keyVar = 0, octave = 0;
                                                ptbNote->GetArtificialHarmonic(key, keyVar, octave);
                                                gp5Note.harmonicSemitone = key;
                                                gp5Note.harmonicAccidental = keyVar;
                                                gp5Note.harmonicOctave = octave;
                                            }
                                            if (ptbNote->HasTappedHarmonic())
                                            {
                                                gp5Note.hasHarmonic = true;
                                                gp5Note.harmonicType = 3; // Tapped
                                                uint8_t tFret = 0;
                                                ptbNote->GetTappedHarmonic(tFret);
                                                gp5Note.harmonicFret = tFret;
                                            }
                                            
                                            // Slides
                                            uint8_t slideIntoType = 0, slideOutType = 0;
                                            int8_t slideSteps = 0;
                                            ptbNote->GetSlideInto(slideIntoType);
                                            ptbNote->GetSlideOutOf(slideOutType, slideSteps);
                                            
                                            if (slideIntoType != 0 || slideOutType != 0)
                                            {
                                                gp5Note.hasSlide = true;
                                                gp5Note.slideType = convertPTBSlideToGP5(slideIntoType, slideOutType);
                                            }
                                            
                                            // Bends
                                            if (ptbNote->HasBend())
                                            {
                                                uint8_t bendType = 0, bentPitch = 0, releasePitch = 0;
                                                uint8_t bendDuration = 0, drawStart = 0, drawEnd = 0;
                                                ptbNote->GetBend(bendType, bentPitch, releasePitch, 
                                                                 bendDuration, drawStart, drawEnd);
                                                
                                                gp5Note.hasBend = true;
                                                gp5Note.bendType = convertPTBBendToGP5(bendType);
                                                // PTB bend pitch is in quarter steps (1 = 1/4 tone)
                                                // GP5 bend value is in 1/100 semitones (100 = 1/2 tone)
                                                gp5Note.bendValue = bentPitch * 50; // quarter step * 50 = 1/100 semitones
                                                
                                                if (releasePitch > 0)
                                                    gp5Note.hasReleaseBend = true;
                                                
                                                // Create simple bend points
                                                GP5BendPoint bp0, bp1;
                                                bp0.position = 0;
                                                bp0.value = 0;
                                                bp1.position = 60;
                                                bp1.value = gp5Note.bendValue;
                                                gp5Note.bendPoints.push_back(bp0);
                                                gp5Note.bendPoints.push_back(bp1);
                                            }
                                            
                                            beat.notes[stringIdx] = gp5Note;
                                        }
                                    }
                                    
                                    trackMeasure.voice1.add(beat);
                                    totalBeatsExtracted++;
                                }
                            }
                            
                            // Process voice 1 (secondary voice) if it exists
                            if (staff->GetPositionCount(1) > 0)
                            {
                                for (size_t posIdx = 0; posIdx < staff->GetPositionCount(1); ++posIdx)
                                {
                                    const auto* position = staff->GetPosition(1, posIdx);
                                    if (!position) continue;
                                    
                                    int posValue = (int)position->GetPosition();
                                    if (posValue >= mInfo.startPosition && posValue < mInfo.endPosition)
                                    {
                                        GP5Beat beat;
                                        beat.duration = convertPTBDurationToGP5(position->GetDurationType());
                                        beat.isDotted = position->IsDotted();
                                        beat.isRest = position->IsRest();
                                        
                                        if (!beat.isRest)
                                        {
                                            for (size_t nIdx = 0; nIdx < position->m_noteArray.size(); ++nIdx)
                                            {
                                                const auto* ptbNote = position->m_noteArray[nIdx];
                                                if (!ptbNote) continue;
                                                
                                                GP5Note gp5Note;
                                                gp5Note.fret = ptbNote->GetFretNumber();
                                                gp5Note.isTied = ptbNote->IsTied();
                                                gp5Note.isDead = ptbNote->IsMuted();
                                                gp5Note.isGhost = ptbNote->IsGhostNote();
                                                gp5Note.hasHammerOn = ptbNote->HasHammerOn() || ptbNote->HasPullOff();
                                                
                                                beat.notes[(int)ptbNote->GetString()] = gp5Note;
                                            }
                                        }
                                        
                                        trackMeasure.voice2.add(beat);
                                    }
                                }
                            }
                        }
                    }
                }
                
                // If a measure has no beats, add a whole rest
                if (trackMeasure.voice1.isEmpty())
                {
                    GP5Beat restBeat;
                    restBeat.isRest = true;
                    restBeat.duration = -2; // whole note rest
                    trackMeasure.voice1.add(restBeat);
                }
                
                gp5Track.measures.add(trackMeasure);
            }
            
            // Skip truly empty tracks (only whole rests, no real notes)
            if (totalBeatsExtracted == 0)
            {
                continue;
            }
            tracks.add(gp5Track);
        }
        
        return true;
    }
    catch (const std::exception& e)
    {
        lastError = "Error parsing PTB file: " + juce::String(e.what());
        return false;
    }
    catch (...)
    {
        lastError = "Unknown error parsing PTB file";
        return false;
    }
}

//==============================================================================
TabTrack PTBParser::convertToTabTrack(int trackIndex) const
{
    // This is essentially the same conversion as GP5Parser::convertToTabTrack
    // since we store data in the same GP5Track format.
    
    TabTrack tabTrack;
    
    if (trackIndex < 0 || trackIndex >= tracks.size())
    {
        return tabTrack;
    }
    
    const auto& gp5Track = tracks[trackIndex];
    
    tabTrack.name = gp5Track.name;
    tabTrack.stringCount = gp5Track.stringCount;
    tabTrack.tuning = gp5Track.tuning;
    tabTrack.capo = gp5Track.capo;
    tabTrack.colour = gp5Track.colour;
    tabTrack.midiChannel = gp5Track.midiChannel - 1; // 0-based
    tabTrack.midiInstrument = 25; // default acoustic guitar
    
    // Tracker for tied notes
    std::map<int, int> lastFretPerString;
    
    for (int m = 0; m < gp5Track.measures.size() && m < measureHeaders.size(); ++m)
    {
        const auto& gp5Measure = gp5Track.measures[m];
        const auto& header = measureHeaders[m];
        
        TabMeasure tabMeasure;
        tabMeasure.measureNumber = header.number;
        tabMeasure.timeSignatureNumerator = header.numerator;
        tabMeasure.timeSignatureDenominator = header.denominator;
        tabMeasure.isRepeatOpen = header.isRepeatOpen;
        tabMeasure.isRepeatClose = (header.repeatClose > 0);
        tabMeasure.repeatCount = header.repeatClose;
        tabMeasure.alternateEnding = header.repeatAlternative;
        tabMeasure.marker = header.marker;
        
        for (const auto& gp5Beat : gp5Measure.voice1)
        {
            TabBeat tabBeat;
            
            // Convert GP5 duration encoding to NoteDuration
            switch (gp5Beat.duration)
            {
                case -2: tabBeat.duration = NoteDuration::Whole; break;
                case -1: tabBeat.duration = NoteDuration::Half; break;
                case 0:  tabBeat.duration = NoteDuration::Quarter; break;
                case 1:  tabBeat.duration = NoteDuration::Eighth; break;
                case 2:  tabBeat.duration = NoteDuration::Sixteenth; break;
                case 3:  tabBeat.duration = NoteDuration::ThirtySecond; break;
                default: tabBeat.duration = NoteDuration::Quarter; break;
            }
            
            tabBeat.isDotted = gp5Beat.isDotted;
            tabBeat.isRest = gp5Beat.isRest;
            tabBeat.isPalmMuted = gp5Beat.isPalmMute;
            tabBeat.hasDownstroke = gp5Beat.hasDownstroke;
            tabBeat.hasUpstroke = gp5Beat.hasUpstroke;
            tabBeat.text = gp5Beat.text;
            tabBeat.chordName = gp5Beat.chordName;
            
            if (gp5Beat.tupletN > 0)
            {
                tabBeat.tupletNumerator = gp5Beat.tupletN;
                tabBeat.tupletDenominator = (gp5Beat.tupletN == 3) ? 2 :
                                            (gp5Beat.tupletN == 5 || gp5Beat.tupletN == 6) ? 4 :
                                            gp5Beat.tupletN - 1;
            }
            
            // Create one TabNote per string (unified format)
            for (int s = 0; s < gp5Track.stringCount; ++s)
            {
                TabNote tabNote;
                tabNote.string = s;
                tabNote.fret = -1;
                
                if (!gp5Beat.isRest)
                {
                    auto noteIt = gp5Beat.notes.find(s);
                    if (noteIt != gp5Beat.notes.end())
                    {
                        const auto& gp5Note = noteIt->second;
                        tabNote.velocity = gp5Note.velocity;
                        tabNote.isTied = gp5Note.isTied;
                        
                        if (gp5Note.isTied && lastFretPerString.count(s))
                            tabNote.fret = lastFretPerString[s];
                        else
                            tabNote.fret = gp5Note.fret;
                        
                        if (!gp5Note.isTied)
                            lastFretPerString[s] = gp5Note.fret;
                        
                        // Effects
                        tabNote.effects.vibrato = gp5Note.hasVibrato;
                        tabNote.effects.ghostNote = gp5Note.isGhost;
                        tabNote.effects.deadNote = gp5Note.isDead;
                        tabNote.effects.accentuatedNote = gp5Note.hasAccent;
                        tabNote.effects.heavyAccentuatedNote = gp5Note.hasHeavyAccent;
                        tabNote.effects.hammerOn = gp5Note.hasHammerOn;
                        tabNote.effects.bend = gp5Note.hasBend;
                        tabNote.effects.bendValue = gp5Note.bendValue / 100.0f;
                        tabNote.effects.bendType = gp5Note.bendType;
                        tabNote.effects.releaseBend = gp5Note.hasReleaseBend;
                        
                        for (const auto& bp : gp5Note.bendPoints)
                        {
                            TabBendPoint tbp;
                            tbp.position = bp.position;
                            tbp.value = bp.value;
                            tbp.vibrato = bp.vibrato;
                            tabNote.effects.bendPoints.push_back(tbp);
                        }
                        
                        if (gp5Note.hasSlide)
                        {
                            switch (gp5Note.slideType)
                            {
                                case 1: tabNote.effects.slideType = SlideType::ShiftSlide; break;
                                case 2: tabNote.effects.slideType = SlideType::LegatoSlide; break;
                                case 3: tabNote.effects.slideType = SlideType::SlideOutDownwards; break;
                                case 4: tabNote.effects.slideType = SlideType::SlideOutUpwards; break;
                                case 5: tabNote.effects.slideType = SlideType::SlideIntoFromBelow; break;
                                case 6: tabNote.effects.slideType = SlideType::SlideIntoFromAbove; break;
                                default: tabNote.effects.slideType = SlideType::ShiftSlide; break;
                            }
                        }
                        
                        if (gp5Note.hasHarmonic)
                        {
                            tabNote.effects.harmonic = static_cast<HarmonicType>(gp5Note.harmonicType);
                            tabNote.effects.harmonicSemitone = gp5Note.harmonicSemitone;
                            tabNote.effects.harmonicAccidental = gp5Note.harmonicAccidental;
                            tabNote.effects.harmonicOctave = gp5Note.harmonicOctave;
                            tabNote.effects.harmonicFret = gp5Note.harmonicFret;
                        }
                    }
                }
                
                tabBeat.notes.add(tabNote);
            }
            
            tabMeasure.beats.add(tabBeat);
        }
        
        tabTrack.measures.add(tabMeasure);
    }
    
    return tabTrack;
}
