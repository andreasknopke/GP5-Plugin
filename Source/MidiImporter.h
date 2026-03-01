/*
  ==============================================================================

    MidiImporter.h
    
    Imports Standard MIDI Files (.mid / .midi) and converts them to
    GP5Track / GP5MeasureHeader structures for display in the tab view.
    
    Uses JUCE's MidiFile class for parsing and provides the same interface
    as GP5Parser/GP7Parser/PTBParser (getTracks, getMeasureHeaders, getSongInfo).

  ==============================================================================
*/

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "GP5Parser.h"  // For GP5Track, GP5Beat, GP5Note, GP5MeasureHeader, GP5SongInfo
#include "TabModels.h"
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

class MidiImporter
{
public:
    MidiImporter() = default;
    ~MidiImporter() = default;

    //==============================================================================
    // Main entry point — parse a .mid / .midi file
    bool parseFile(const juce::File& file)
    {
        tracks.clear();
        measureHeaders.clear();
        lastError = "";
        
        juce::FileInputStream fis(file);
        if (!fis.openedOk())
        {
            lastError = "Could not open file: " + file.getFullPathName();
            return false;
        }
        
        juce::MidiFile midiFile;
        if (!midiFile.readFrom(fis))
        {
            lastError = "Invalid MIDI file: " + file.getFileName();
            return false;
        }
        
        songInfo = GP5SongInfo();
        songInfo.title = file.getFileNameWithoutExtension();
        
        // Get ticks per quarter note
        int ticksPerQuarter = midiFile.getTimeFormat();
        if (ticksPerQuarter <= 0)
        {
            // SMPTE format — use default
            ticksPerQuarter = 480;
        }
        
        DBG("MidiImporter: " << midiFile.getNumTracks() << " MIDI tracks, "
            << ticksPerQuarter << " ticks/quarter");
        
        // =====================================================================
        // Pass 1: Collect tempo/time-signature events and note events per channel
        // =====================================================================
        struct TempoEvent { double tick; double bpm; };
        struct TimeSigEvent { double tick; int numerator; int denominator; };
        struct NoteEvent {
            double startTick;
            double endTick;
            int midiNote;
            int velocity;
            int channel;
        };
        
        std::vector<TempoEvent> tempoEvents;
        std::vector<TimeSigEvent> timeSigEvents;
        
        // Collect notes per channel (0-15)
        std::map<int, std::vector<NoteEvent>> channelNotes;
        
        // Track names per channel
        std::map<int, juce::String> channelNames;
        
        for (int t = 0; t < midiFile.getNumTracks(); ++t)
        {
            const auto* sequence = midiFile.getTrack(t);
            if (sequence == nullptr) continue;
            
            // Collect note-on events (pending note-off)
            std::map<int, std::pair<double, int>> pendingNotes; // note -> (startTick, velocity)
            
            juce::String trackName;
            
            for (int e = 0; e < sequence->getNumEvents(); ++e)
            {
                const auto& midiEvent = sequence->getEventPointer(e)->message;
                double tick = midiEvent.getTimeStamp();
                
                // Tempo change
                if (midiEvent.isTempoMetaEvent())
                {
                    double bpm = 60000000.0 / midiEvent.getTempoSecondsPerQuarterNote() / 1000000.0;
                    // Simpler: getTempoMetaEventTickLength returns ticks, but we want BPM
                    double spqn = midiEvent.getTempoSecondsPerQuarterNote();
                    bpm = 60.0 / spqn;
                    tempoEvents.push_back({ tick, bpm });
                }
                
                // Time signature
                if (midiEvent.isTimeSignatureMetaEvent())
                {
                    int num, den;
                    midiEvent.getTimeSignatureInfo(num, den);
                    timeSigEvents.push_back({ tick, num, den });
                }
                
                // Track name
                if (midiEvent.isTrackNameEvent())
                {
                    trackName = midiEvent.getTextFromTextMetaEvent();
                }
                
                // Note On
                if (midiEvent.isNoteOn())
                {
                    int note = midiEvent.getNoteNumber();
                    int vel = midiEvent.getVelocity();
                    int ch = midiEvent.getChannel();  // 1-based in JUCE
                    
                    // If there's already a pending note-on for this note, close it
                    int key = ch * 128 + note;
                    if (pendingNotes.count(key))
                    {
                        auto& [startTick, startVel] = pendingNotes[key];
                        if (tick > startTick)
                        {
                            channelNotes[ch].push_back({ startTick, tick, note, startVel, ch });
                        }
                    }
                    pendingNotes[key] = { tick, vel };
                }
                
                // Note Off (or velocity-0 note-on)
                if (midiEvent.isNoteOff())
                {
                    int note = midiEvent.getNoteNumber();
                    int ch = midiEvent.getChannel();
                    int key = ch * 128 + note;
                    
                    if (pendingNotes.count(key))
                    {
                        auto& [startTick, startVel] = pendingNotes[key];
                        if (tick > startTick)
                        {
                            channelNotes[ch].push_back({ startTick, tick, note, startVel, ch });
                        }
                        pendingNotes.erase(key);
                    }
                }
            }
            
            // Close any remaining pending notes at last tick
            for (auto& [key, pair] : pendingNotes)
            {
                int ch = key / 128;
                int note = key % 128;
                double lastTick = sequence->getEndTime();
                if (lastTick > pair.first)
                {
                    channelNotes[ch].push_back({ pair.first, lastTick, note, pair.second, ch });
                }
            }
            
            // Assign track name to channels that had notes
            if (trackName.isNotEmpty())
            {
                for (auto& [ch, notes] : channelNotes)
                {
                    if (!channelNames.count(ch))
                        channelNames[ch] = trackName;
                }
            }
        }
        
        if (channelNotes.empty())
        {
            lastError = "MIDI file contains no note events";
            return false;
        }
        
        // Default tempo if none found
        if (tempoEvents.empty())
            tempoEvents.push_back({ 0.0, 120.0 });
        
        // Default time signature if none found
        if (timeSigEvents.empty())
            timeSigEvents.push_back({ 0.0, 4, 4 });
        
        // Sort tempo/timesig events by tick
        std::sort(tempoEvents.begin(), tempoEvents.end(), 
            [](const TempoEvent& a, const TempoEvent& b) { return a.tick < b.tick; });
        std::sort(timeSigEvents.begin(), timeSigEvents.end(),
            [](const TimeSigEvent& a, const TimeSigEvent& b) { return a.tick < b.tick; });
        
        songInfo.tempo = (int)std::round(tempoEvents[0].bpm);
        
        // =====================================================================
        // Pass 2: Build measure map from time signatures
        // =====================================================================
        
        // Find the total duration (last note-off across all channels)
        double totalTicks = 0.0;
        for (auto& [ch, notes] : channelNotes)
        {
            for (auto& n : notes)
                totalTicks = std::max(totalTicks, n.endTick);
        }
        
        // Build measures
        struct MeasureInfo {
            double startTick;
            double endTick;
            int numerator;
            int denominator;
        };
        
        std::vector<MeasureInfo> measureMap;
        double currentTick = 0.0;
        int tsIdx = 0;
        int currentNum = timeSigEvents[0].numerator;
        int currentDen = timeSigEvents[0].denominator;
        int measureNum = 1;
        
        while (currentTick < totalTicks + ticksPerQuarter)
        {
            // Check for time signature change at this position
            while (tsIdx + 1 < (int)timeSigEvents.size() && 
                   timeSigEvents[tsIdx + 1].tick <= currentTick + 1.0)
            {
                tsIdx++;
                currentNum = timeSigEvents[tsIdx].numerator;
                currentDen = timeSigEvents[tsIdx].denominator;
            }
            
            // Measure length in ticks
            double measureLengthTicks = (double)ticksPerQuarter * 4.0 * currentNum / currentDen;
            
            MeasureInfo mi;
            mi.startTick = currentTick;
            mi.endTick = currentTick + measureLengthTicks;
            mi.numerator = currentNum;
            mi.denominator = currentDen;
            measureMap.push_back(mi);
            
            // Create measure header
            GP5MeasureHeader header;
            header.number = measureNum++;
            header.numerator = currentNum;
            header.denominator = currentDen;
            measureHeaders.add(header);
            
            currentTick += measureLengthTicks;
            
            // Safety: limit to 1000 measures
            if (measureMap.size() > 1000) break;
        }
        
        DBG("MidiImporter: " << measureMap.size() << " measures, "
            << channelNotes.size() << " channels with notes");
        
        // =====================================================================
        // Pass 3: Build GP5Track per channel
        // =====================================================================
        
        // Standard GM instrument names (subset)
        auto getGMInstrumentName = [](int /*channel*/, int program) -> juce::String
        {
            const char* gmNames[] = {
                "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
                "Honky-tonk Piano", "Electric Piano 1", "Electric Piano 2", "Harpsichord",
                "Clavi", "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba",
                "Xylophone", "Tubular Bells", "Dulcimer", "Drawbar Organ", "Percussive Organ",
                "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica",
                "Tango Accordion", "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)",
                "Electric Guitar (jazz)", "Electric Guitar (clean)", "Electric Guitar (muted)",
                "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics", "Acoustic Bass",
                "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass", "Slap Bass 1",
                "Slap Bass 2", "Synth Bass 1", "Synth Bass 2"
            };
            if (program >= 0 && program < 40)
                return gmNames[program];
            return "Track";
        };
        
        // Guitar tuning (standard E)
        const int standardTuning[] = { 64, 59, 55, 50, 45, 40 };
        // Bass tuning (standard 4-string)
        const int bassTuning[] = { 43, 38, 33, 28, -1, -1 };
        
        int trackIdx = 0;
        for (auto& [channel, notes] : channelNotes)
        {
            if (notes.empty()) continue;
            
            // Sort notes by start tick
            std::sort(notes.begin(), notes.end(),
                [](const NoteEvent& a, const NoteEvent& b) { return a.startTick < b.startTick; });
            
            GP5Track gp5Track;
            
            // Determine if this is a drum channel
            bool isDrums = (channel == 10);
            gp5Track.isPercussion = isDrums;
            
            // Determine note range to pick tuning
            int minNote = 127, maxNote = 0;
            for (auto& n : notes) {
                minNote = std::min(minNote, n.midiNote);
                maxNote = std::max(maxNote, n.midiNote);
            }
            
            // Bass detection: most notes below MIDI 50 (D3)
            bool isBass = !isDrums && maxNote < 55 && minNote < 45;
            
            // Set track properties
            if (channelNames.count(channel))
                gp5Track.name = channelNames[channel];
            else if (isDrums)
                gp5Track.name = "Drums";
            else if (isBass)
                gp5Track.name = "Bass";
            else
                gp5Track.name = "Track " + juce::String(trackIdx + 1);
            
            gp5Track.midiChannel = channel;
            gp5Track.channelIndex = channel - 1;
            gp5Track.volume = 100;
            gp5Track.pan = 64;
            
            if (isDrums)
            {
                gp5Track.stringCount = 6;
                for (int s = 0; s < 6; ++s)
                    gp5Track.tuning.add(standardTuning[s]);
            }
            else if (isBass)
            {
                gp5Track.stringCount = 4;
                gp5Track.tuning.add(43);  // G2
                gp5Track.tuning.add(38);  // D2
                gp5Track.tuning.add(33);  // A1
                gp5Track.tuning.add(28);  // E1
            }
            else
            {
                gp5Track.stringCount = 6;
                for (int s = 0; s < 6; ++s)
                    gp5Track.tuning.add(standardTuning[s]);
            }
            
            // Set colour (cycle through some defaults)
            const juce::Colour trackColours[] = {
                juce::Colour(0xFFFF0000), juce::Colour(0xFF0000FF),
                juce::Colour(0xFF00AA00), juce::Colour(0xFFFF8800),
                juce::Colour(0xFF8800FF), juce::Colour(0xFF00AAAA)
            };
            gp5Track.colour = trackColours[trackIdx % 6];
            
            // =====================================================================
            // Build measures with beats and notes
            // =====================================================================
            for (int m = 0; m < (int)measureMap.size(); ++m)
            {
                const auto& mi = measureMap[m];
                GP5TrackMeasure gp5Measure;
                
                // Collect notes that start in this measure
                std::vector<NoteEvent> measureNotes;
                for (auto& n : notes)
                {
                    if (n.startTick >= mi.startTick && n.startTick < mi.endTick)
                        measureNotes.push_back(n);
                }
                
                if (measureNotes.empty())
                {
                    // Empty measure — add a whole rest
                    GP5Beat restBeat;
                    restBeat.duration = durationToGP5(NoteDuration::Whole);
                    restBeat.isRest = true;
                    gp5Measure.voice1.add(restBeat);
                }
                else
                {
                    // Group simultaneous notes into chords
                    // Notes starting within a small window are grouped
                    double chordThreshold = ticksPerQuarter / 8.0; // 1/32 note
                    
                    struct Chord {
                        double startTick;
                        double endTick;  // shortest note in the chord
                        std::vector<NoteEvent> chordNotes;
                    };
                    
                    std::vector<Chord> chords;
                    
                    for (auto& n : measureNotes)
                    {
                        bool addedToChord = false;
                        for (auto& chord : chords)
                        {
                            if (std::abs(n.startTick - chord.startTick) < chordThreshold)
                            {
                                chord.chordNotes.push_back(n);
                                chord.endTick = std::min(chord.endTick, n.endTick);
                                addedToChord = true;
                                break;
                            }
                        }
                        if (!addedToChord)
                        {
                            Chord c;
                            c.startTick = n.startTick;
                            c.endTick = n.endTick;
                            c.chordNotes.push_back(n);
                            chords.push_back(c);
                        }
                    }
                    
                    // Sort chords by start position
                    std::sort(chords.begin(), chords.end(),
                        [](const Chord& a, const Chord& b) { return a.startTick < b.startTick; });
                    
                    // Convert chords to beats, filling gaps with rests
                    double beatPos = mi.startTick;
                    
                    for (size_t ci = 0; ci < chords.size(); ++ci)
                    {
                        auto& chord = chords[ci];
                        
                        // If there's a gap before this chord, insert rest(s)
                        double gap = chord.startTick - beatPos;
                        if (gap > chordThreshold)
                        {
                            addRestsForDuration(gp5Measure.voice1, gap, ticksPerQuarter);
                            beatPos = chord.startTick;
                        }
                        
                        // Determine beat duration from the chord
                        double nextPos;
                        if (ci + 1 < chords.size())
                            nextPos = chords[ci + 1].startTick;
                        else
                            nextPos = mi.endTick;
                        
                        double durationTicks = std::min(chord.endTick - chord.startTick,
                                                        nextPos - chord.startTick);
                        durationTicks = std::max(durationTicks, (double)ticksPerQuarter / 8.0);
                        
                        NoteDuration noteDur = ticksToDuration(durationTicks, ticksPerQuarter);
                        
                        GP5Beat beat;
                        beat.duration = durationToGP5(noteDur);
                        beat.isRest = false;
                        
                        // Check for dotted: if actual duration is ~1.5x the quantized duration
                        double quantizedTicks = durationToTicks(noteDur, ticksPerQuarter);
                        if (durationTicks > quantizedTicks * 1.3 && durationTicks < quantizedTicks * 1.7)
                        {
                            beat.isDotted = true;
                            quantizedTicks *= 1.5;
                        }
                        
                        // Assign notes to strings
                        for (auto& cn : chord.chordNotes)
                        {
                            auto [stringIdx, fret] = midiNoteToStringFret(
                                cn.midiNote, gp5Track.tuning, gp5Track.stringCount, isDrums);
                            
                            if (stringIdx >= 0 && stringIdx < gp5Track.stringCount)
                            {
                                // Don't overwrite if string already used in this beat
                                if (beat.notes.find(stringIdx) == beat.notes.end())
                                {
                                    GP5Note gp5Note;
                                    gp5Note.fret = fret;
                                    gp5Note.velocity = cn.velocity;
                                    beat.notes[stringIdx] = gp5Note;
                                }
                                else
                                {
                                    // String already taken — try adjacent string
                                    for (int offset : { -1, 1, -2, 2 })
                                    {
                                        int altString = stringIdx + offset;
                                        if (altString >= 0 && altString < gp5Track.stringCount &&
                                            beat.notes.find(altString) == beat.notes.end())
                                        {
                                            int altFret = cn.midiNote - gp5Track.tuning[altString];
                                            if (altFret >= 0 && altFret <= 24)
                                            {
                                                GP5Note gp5Note;
                                                gp5Note.fret = altFret;
                                                gp5Note.velocity = cn.velocity;
                                                beat.notes[altString] = gp5Note;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        gp5Measure.voice1.add(beat);
                        beatPos += quantizedTicks;
                    }
                    
                    // If there's remaining time after the last chord, add rest(s)
                    double remaining = mi.endTick - beatPos;
                    if (remaining > chordThreshold)
                    {
                        addRestsForDuration(gp5Measure.voice1, remaining, ticksPerQuarter);
                    }
                }
                
                gp5Track.measures.add(gp5Measure);
            }
            
            tracks.add(gp5Track);
            trackIdx++;
        }
        
        DBG("MidiImporter: Created " << tracks.size() << " tracks, " 
            << measureHeaders.size() << " measures");
        
        return true;
    }
    
    //==============================================================================
    // Accessors (same interface as GP5Parser, GP7Parser, PTBParser)
    
    const GP5SongInfo& getSongInfo() const { return songInfo; }
    const juce::Array<GP5Track>& getTracks() const { return tracks; }
    const juce::Array<GP5MeasureHeader>& getMeasureHeaders() const { return measureHeaders; }
    juce::String getLastError() const { return lastError; }
    int getTrackCount() const { return tracks.size(); }
    int getMeasureCount() const { return measureHeaders.size(); }
    
    //==============================================================================
    // Convert to tab model (delegates to the same logic as GP5Parser)
    TabTrack convertToTabTrack(int trackIndex) const
    {
        TabTrack tabTrack;
        
        if (trackIndex < 0 || trackIndex >= tracks.size())
            return tabTrack;
        
        const auto& gp5Track = tracks[trackIndex];
        
        tabTrack.name = gp5Track.name;
        tabTrack.stringCount = gp5Track.stringCount;
        tabTrack.tuning = gp5Track.tuning;
        tabTrack.capo = gp5Track.capo;
        tabTrack.colour = gp5Track.colour;
        tabTrack.midiChannel = gp5Track.midiChannel - 1;  // GP5 is 1-based
        
        for (int m = 0; m < gp5Track.measures.size() && m < measureHeaders.size(); ++m)
        {
            const auto& gp5Measure = gp5Track.measures[m];
            const auto& header = measureHeaders[m];
            
            TabMeasure tabMeasure;
            tabMeasure.measureNumber = header.number;
            tabMeasure.timeSignatureNumerator = header.numerator;
            tabMeasure.timeSignatureDenominator = header.denominator;
            
            for (const auto& gp5Beat : gp5Measure.voice1)
            {
                TabBeat tabBeat;
                tabBeat.duration = gp5ToDuration(gp5Beat.duration);
                tabBeat.isDotted = gp5Beat.isDotted;
                tabBeat.isRest = gp5Beat.isRest;
                tabBeat.isPalmMuted = gp5Beat.isPalmMute;
                tabBeat.text = gp5Beat.text;
                tabBeat.chordName = gp5Beat.chordName;
                
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
                            tabNote.fret = gp5Note.fret;
                            tabNote.velocity = gp5Note.velocity;
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
    
private:
    GP5SongInfo songInfo;
    juce::Array<GP5Track> tracks;
    juce::Array<GP5MeasureHeader> measureHeaders;
    juce::String lastError;
    
    //==============================================================================
    // Helper: Convert NoteDuration to GP5 duration encoding
    static int durationToGP5(NoteDuration dur)
    {
        switch (dur)
        {
            case NoteDuration::Whole:       return -2;
            case NoteDuration::Half:        return -1;
            case NoteDuration::Quarter:     return 0;
            case NoteDuration::Eighth:      return 1;
            case NoteDuration::Sixteenth:   return 2;
            case NoteDuration::ThirtySecond: return 3;
            default:                        return 0;
        }
    }
    
    // Helper: Convert GP5 duration encoding to NoteDuration
    static NoteDuration gp5ToDuration(int gpDur)
    {
        switch (gpDur)
        {
            case -2: return NoteDuration::Whole;
            case -1: return NoteDuration::Half;
            case 0:  return NoteDuration::Quarter;
            case 1:  return NoteDuration::Eighth;
            case 2:  return NoteDuration::Sixteenth;
            case 3:  return NoteDuration::ThirtySecond;
            default: return NoteDuration::Quarter;
        }
    }
    
    //==============================================================================
    // Helper: Convert NoteDuration to ticks
    static double durationToTicks(NoteDuration dur, int ticksPerQuarter)
    {
        double quarterTicks = (double)ticksPerQuarter;
        switch (dur)
        {
            case NoteDuration::Whole:       return quarterTicks * 4.0;
            case NoteDuration::Half:        return quarterTicks * 2.0;
            case NoteDuration::Quarter:     return quarterTicks;
            case NoteDuration::Eighth:      return quarterTicks / 2.0;
            case NoteDuration::Sixteenth:   return quarterTicks / 4.0;
            case NoteDuration::ThirtySecond: return quarterTicks / 8.0;
            default:                        return quarterTicks;
        }
    }
    
    //==============================================================================
    // Helper: Quantize tick duration to the closest NoteDuration
    static NoteDuration ticksToDuration(double ticks, int ticksPerQuarter)
    {
        double q = (double)ticksPerQuarter;
        
        struct DurOption { NoteDuration dur; double ticks; };
        DurOption options[] = {
            { NoteDuration::Whole,       q * 4.0 },
            { NoteDuration::Half,        q * 2.0 },
            { NoteDuration::Quarter,     q },
            { NoteDuration::Eighth,      q / 2.0 },
            { NoteDuration::Sixteenth,   q / 4.0 },
            { NoteDuration::ThirtySecond, q / 8.0 },
        };
        
        NoteDuration best = NoteDuration::Quarter;
        double bestDiff = 1e9;
        
        for (auto& opt : options)
        {
            double diff = std::abs(ticks - opt.ticks);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                best = opt.dur;
            }
        }
        
        return best;
    }
    
    //==============================================================================
    // Helper: Fill a gap with rest beats of appropriate durations
    static void addRestsForDuration(juce::Array<GP5Beat>& beats, double durationTicks, int ticksPerQuarter)
    {
        double remaining = durationTicks;
        double q = (double)ticksPerQuarter;
        
        // Try largest durations first
        struct DurOption { NoteDuration dur; double ticks; };
        DurOption options[] = {
            { NoteDuration::Whole,       q * 4.0 },
            { NoteDuration::Half,        q * 2.0 },
            { NoteDuration::Quarter,     q },
            { NoteDuration::Eighth,      q / 2.0 },
            { NoteDuration::Sixteenth,   q / 4.0 },
            { NoteDuration::ThirtySecond, q / 8.0 },
        };
        
        int safety = 0;
        while (remaining > q / 16.0 && safety < 32) // At least 1/64 note remaining
        {
            for (auto& opt : options)
            {
                if (remaining >= opt.ticks * 0.9) // 90% tolerance
                {
                    GP5Beat restBeat;
                    restBeat.duration = durationToGP5(opt.dur);
                    restBeat.isRest = true;
                    beats.add(restBeat);
                    remaining -= opt.ticks;
                    break;
                }
            }
            safety++;
        }
        
        // If no rest was added at all (very short gap), add a 32nd rest
        if (safety == 0 && durationTicks > 0)
        {
            GP5Beat restBeat;
            restBeat.duration = durationToGP5(NoteDuration::ThirtySecond);
            restBeat.isRest = true;
            beats.add(restBeat);
        }
    }
    
    //==============================================================================
    // Helper: Map a MIDI note to the best string/fret on the guitar
    static std::pair<int, int> midiNoteToStringFret(int midiNote, 
                                                     const juce::Array<int>& tuning,
                                                     int stringCount,
                                                     bool isDrums)
    {
        if (isDrums)
        {
            // For drums, distribute across strings based on note ranges
            // Kick: string 5, Snare: string 3, Hi-hat: string 0, etc.
            int drumString = 0;
            if (midiNote >= 35 && midiNote <= 36)       drumString = 5;  // Bass drum
            else if (midiNote >= 37 && midiNote <= 40)  drumString = 4;  // Snare
            else if (midiNote >= 41 && midiNote <= 47)  drumString = 3;  // Toms
            else if (midiNote >= 48 && midiNote <= 53)  drumString = 2;  // Toms high
            else if (midiNote >= 54 && midiNote <= 59)  drumString = 1;  // Cymbals
            else                                         drumString = 0;  // Hi-hat / other
            
            return { drumString, midiNote };
        }
        
        // For melodic instruments: find the string/fret with the lowest fret position
        // that is still comfortable (prefer middle positions)
        int bestString = -1;
        int bestFret = -1;
        int bestScore = 9999;
        
        for (int s = 0; s < stringCount; ++s)
        {
            int openStringNote = tuning[s];
            int fret = midiNote - openStringNote;
            
            if (fret >= 0 && fret <= 24)
            {
                // Score: prefer lower frets (open=0 is great, fret 1-5 is good)
                // But also consider playability — middle strings are slightly preferred
                int score = fret;
                // Slight preference for middle strings when frets are similar
                if (s == 0 || s == stringCount - 1)
                    score += 1;
                    
                if (score < bestScore)
                {
                    bestScore = score;
                    bestString = s;
                    bestFret = fret;
                }
            }
        }
        
        // If note can't be played on any string, use the closest string
        if (bestString < 0)
        {
            // Find the closest string
            int minDist = 9999;
            for (int s = 0; s < stringCount; ++s)
            {
                int fret = midiNote - tuning[s];
                int dist = std::abs(fret);
                if (dist < minDist)
                {
                    minDist = dist;
                    bestString = s;
                    bestFret = std::max(0, std::min(24, fret));
                }
            }
        }
        
        return { bestString, bestFret };
    }
};
