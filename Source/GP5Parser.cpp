/*
  ==============================================================================

    GP5Parser.cpp
    
    Guitar Pro 3-5 (.gp3/.gp4/.gp5) File Parser - Extended Implementation
    Ported from Python library pyguitarpro

  ==============================================================================
*/

#include "GP5Parser.h"

//==============================================================================
GP5Parser::GP5Parser() {}
GP5Parser::~GP5Parser() {}

//==============================================================================
bool GP5Parser::parse(const juce::File& file)
{
    // Reset state
    songInfo = GP5SongInfo();
    midiChannels.clear();
    measureHeaders.clear();
    tracks.clear();
    lastError.clear();
    fileVersion = GPFileVersion::Unknown;
    tripletFeel = false;
    
    if (!file.existsAsFile())
    {
        lastError = "File does not exist: " + file.getFullPathName();
        return false;
    }
    
    inputStream = file.createInputStream();
    if (inputStream == nullptr)
    {
        lastError = "Could not open file: " + file.getFullPathName();
        return false;
    }
    
    try
    {
        // 1. Read version
        readVersion();
        
        // Detect file version
        if (songInfo.version.contains("v3."))
        {
            fileVersion = GPFileVersion::GP3;
            versionMajor = 3;
            versionMinor = 0;
            versionPatch = 0;
            DBG("Detected GP3 file: " << songInfo.version);
        }
        else if (songInfo.version.contains("v4."))
        {
            fileVersion = GPFileVersion::GP4;
            versionMajor = 4;
            versionMinor = 0;
            versionPatch = 0;
            if (songInfo.version.contains("v4.06"))
                versionPatch = 6;
            DBG("Detected GP4 file: " << songInfo.version);
        }
        else if (songInfo.version.contains("v5."))
        {
            fileVersion = GPFileVersion::GP5;
            versionMajor = 5;
            if (songInfo.version.contains("v5.00"))
                { versionMinor = 0; versionPatch = 0; }
            else if (songInfo.version.contains("v5.10"))
                { versionMinor = 1; versionPatch = 0; }
            else
                { versionMinor = 1; versionPatch = 0; }
            DBG("Detected GP5 file: " << songInfo.version);
        }
        else
        {
            lastError = "Unknown Guitar Pro version: " + songInfo.version;
            return false;
        }
        
        DBG("Stream position after version: " << inputStream->getPosition());
        
        // Branch based on file version
        if (fileVersion == GPFileVersion::GP3)
        {
            return parseGP3();
        }
        else if (fileVersion == GPFileVersion::GP4)
        {
            return parseGP4();
        }
        else
        {
            return parseGP5();
        }
    }
    catch (const std::exception& e)
    {
        lastError = juce::String("Parse error: ") + e.what();
        return false;
    }
}

//==============================================================================
// GP3 PARSING
//==============================================================================
bool GP5Parser::parseGP3()
{
    try
    {
        // 2. Read song info (GP3 format)
        readInfoGP3();
        DBG("Title: " << songInfo.title << " | Artist: " << songInfo.artist);
        DBG("Stream position after info: " << inputStream->getPosition());
        
        // 3. Triplet feel
        tripletFeel = readBool();
        DBG("Triplet feel: " << (tripletFeel ? "yes" : "no"));
        
        // 4. Tempo
        songInfo.tempo = readI32();
        currentTempo = songInfo.tempo;
        DBG("Tempo: " << songInfo.tempo);
        
        // 5. Key signature
        readI32();  // key
        DBG("Stream position after key: " << inputStream->getPosition());
        
        // 6. MIDI channels
        readMidiChannels();
        DBG("Stream position after MIDI channels: " << inputStream->getPosition());
        
        // 7. Measure and track count
        int measureCount = readI32();
        int trackCount = readI32();
        DBG("Measures: " << measureCount << " | Tracks: " << trackCount);
        DBG("Stream position after counts: " << inputStream->getPosition());
        
        // 8. Measure headers (GP3 format)
        readMeasureHeadersGP3(measureCount);
        DBG("Stream position after measure headers: " << inputStream->getPosition());
        
        // 9. Tracks (GP3 format)
        readTracksGP3(trackCount);
        DBG("Stream position after tracks: " << inputStream->getPosition());
        
        // 10. Assign MIDI channels
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto& track = tracks.getReference(i);
            int channelIdx = track.channelIndex;
            
            if (channelIdx >= 0 && channelIdx < midiChannels.size())
            {
                track.midiChannel = (channelIdx % 16) + 1;
                track.volume = midiChannels[channelIdx].volume;
                track.pan = midiChannels[channelIdx].balance;
                
                if (track.midiChannel == 10)
                    track.isPercussion = true;
            }
            else
            {
                track.midiChannel = (i % 16) + 1;
            }
        }
        
        // 11. Read measures (the actual notes!)
        readMeasuresGP3();
        
        DBG("GP3 parsing complete! Track count: " << tracks.size());
        return true;
    }
    catch (const std::exception& e)
    {
        lastError = juce::String("GP3 parse error: ") + e.what();
        return false;
    }
}

//==============================================================================
// GP4 PARSING (similar to GP3 but with some additions)
//==============================================================================
bool GP5Parser::parseGP4()
{
    try
    {
        // GP4 is similar to GP3 but has lyrics and octave field
        
        // 2. Read song info (GP3 format works for GP4)
        readInfoGP3();
        DBG("Title: " << songInfo.title << " | Artist: " << songInfo.artist);
        
        // 3. Triplet feel
        tripletFeel = readBool();
        
        // 4. Lyrics (GP4 has lyrics)
        readLyrics();
        
        // 5. Tempo
        songInfo.tempo = readI32();
        currentTempo = songInfo.tempo;
        DBG("Tempo: " << songInfo.tempo);
        
        // 6. Key signature + octave
        readI32();  // key
        readI8();   // octave (GP4 only)
        
        // 7. MIDI channels
        readMidiChannels();
        
        // 8. Measure and track count
        int measureCount = readI32();
        int trackCount = readI32();
        DBG("Measures: " << measureCount << " | Tracks: " << trackCount);
        
        // 9. Measure headers (GP3 format works for GP4)
        readMeasureHeadersGP3(measureCount);
        
        // 10. Tracks (GP3 format works for GP4)
        readTracksGP3(trackCount);
        
        // 11. Assign MIDI channels
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto& track = tracks.getReference(i);
            int channelIdx = track.channelIndex;
            
            if (channelIdx >= 0 && channelIdx < midiChannels.size())
            {
                track.midiChannel = (channelIdx % 16) + 1;
                track.volume = midiChannels[channelIdx].volume;
                track.pan = midiChannels[channelIdx].balance;
                
                if (track.midiChannel == 10)
                    track.isPercussion = true;
            }
            else
            {
                track.midiChannel = (i % 16) + 1;
            }
        }
        
        // 12. Read measures
        readMeasuresGP3();
        
        DBG("GP4 parsing complete! Track count: " << tracks.size());
        return true;
    }
    catch (const std::exception& e)
    {
        lastError = juce::String("GP4 parse error: ") + e.what();
        return false;
    }
}

//==============================================================================
// GP5 PARSING (original implementation)
//==============================================================================
bool GP5Parser::parseGP5()
{
    try
    {
        // 2. Read song info
        readInfo();
        DBG("Title: " << songInfo.title << " | Artist: " << songInfo.artist);
        DBG("Stream position after info: " << inputStream->getPosition());
        
        // 3. Lyrics
        readLyrics();
        DBG("Stream position after lyrics: " << inputStream->getPosition());
        
        // 4. RSE Master Effect (GP5.1+)
        readRSEMasterEffect();
        DBG("Stream position after RSE: " << inputStream->getPosition());
        
        // 5. Page Setup
        readPageSetup();
        DBG("Stream position after page setup: " << inputStream->getPosition());
        
        // 6. Tempo
        songInfo.tempoName = readIntByteSizeString();
        songInfo.tempo = readI32();
        currentTempo = songInfo.tempo;
        DBG("Tempo: " << songInfo.tempo);
        DBG("Stream position after tempo: " << inputStream->getPosition());
        
        // 7. Hide tempo (GP5.1+)
        if (versionMinor > 0)
            readBool();
        
        // 8. Key signature
        readI8();   // key
        readI32();  // octave
        
        DBG("Stream position after key sig: " << inputStream->getPosition());
        
        // 9. MIDI channels
        readMidiChannels();
        DBG("Stream position after MIDI channels: " << inputStream->getPosition());
        
        // 10. Directions
        readDirections();
        DBG("Stream position after directions: " << inputStream->getPosition());
        
        // 11. Master reverb
        readI32();
        DBG("Stream position after reverb: " << inputStream->getPosition());
        
        // 12. Measure and track count
        int measureCount = readI32();
        int trackCount = readI32();
        DBG("Measures: " << measureCount << " | Tracks: " << trackCount);
        DBG("Stream position after counts: " << inputStream->getPosition());
        
        // 13. Measure headers
        for (int i = 0; i < measureCount; ++i)
        {
            GP5MeasureHeader header;
            header.number = i + 1;
            
            // Blank byte before measure (except first)
            if (i > 0)
                skip(1);
            
            juce::uint8 flags = readU8();
            
            // Time signature numerator
            if (flags & 0x01)
                header.numerator = readI8();
            else if (i > 0)
                header.numerator = measureHeaders[i-1].numerator;
            
            // Time signature denominator
            if (flags & 0x02)
                header.denominator = readI8();
            else if (i > 0)
                header.denominator = measureHeaders[i-1].denominator;
            
            // Repeat open
            header.isRepeatOpen = (flags & 0x04) != 0;
            
            // Repeat close
            if (flags & 0x08)
                header.repeatClose = readI8();
            
            // Marker
            if (flags & 0x20)
            {
                header.marker = readIntByteSizeString();
                readColor();  // Marker color
            }
            
            // Key signature
            if (flags & 0x40)
            {
                readI8();  // root
                readI8();  // type
            }
            
            // Repeat alternative
            if (flags & 0x10)
                header.repeatAlternative = readU8();
            
            // Double bar
            header.hasDoubleBar = (flags & 0x80) != 0;
            
            // Beams (if time sig changed)
            if (flags & 0x03)
            {
                for (int b = 0; b < 4; ++b)
                    readU8();
            }
            
            // Blank byte if no repeat alt
            if ((flags & 0x10) == 0)
                skip(1);
            
            // Triplet feel
            readU8();
            
            measureHeaders.add(header);
        }
        
        DBG("Parsed " << measureHeaders.size() << " measure headers");
        
        // 14. Tracks
        for (int i = 0; i < trackCount; ++i)
        {
            GP5Track track;
            
            // Blank byte (first track or GP5.0)
            if (i == 0 || versionMinor == 0)
                skip(1);
            
            // Flags
            juce::uint8 flags1 = readU8();
            track.isPercussion = (flags1 & 0x01) != 0;
            track.is12String = (flags1 & 0x02) != 0;
            track.isBanjo = (flags1 & 0x04) != 0;
            
            // Name
            track.name = readByteSizeString(40);
            
            // String count and tuning
            track.stringCount = readI32();
            track.tuning.clear();
            for (int s = 0; s < 7; ++s)
            {
                int tuning = readI32();
                if (s < track.stringCount)
                    track.tuning.add(tuning);
            }
            
            // Port, channel
            track.port = readI32();
            track.channelIndex = readI32() - 1;
            readI32();  // effectChannel
            
            // Frets, capo, color
            track.fretCount = readI32();
            track.capo = readI32();
            track.colour = readColor();
            
            // GP5 Track Settings
            readI16();  // flags2
            readU8();   // autoAccentuation
            readU8();   // MIDI bank
            
            // Track RSE
            readU8();   // humanize
            readI32(); readI32(); readI32();  // Unknown 3 ints
            skip(12);   // Unknown 12 bytes
            
            // RSE Instrument
            readI32();  // instrument
            readI32();  // unknown
            readI32();  // soundBank
            
            // Version-specific: effectNumber
            if (versionMinor == 0)
            {
                readI16();
                skip(1);
            }
            else
            {
                readI32();
                for (int eq = 0; eq < 4; ++eq)
                    readI8();
                readIntByteSizeString();
                readIntByteSizeString();
            }
            
            // Initialize measures
            for (int m = 0; m < measureCount; ++m)
            {
                track.measures.add(GP5TrackMeasure());
            }
            
            tracks.add(track);
            DBG("Track " << (i+1) << ": " << track.name << " (" << track.stringCount << " strings)");
        }
        
        // Blank bytes after tracks
        skip(versionMinor == 0 ? 2 : 1);
        
        // Assign MIDI channels
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto& track = tracks.getReference(i);
            int channelIdx = track.channelIndex;
            
            if (channelIdx >= 0 && channelIdx < midiChannels.size())
            {
                track.midiChannel = (channelIdx % 16) + 1;
                track.volume = midiChannels[channelIdx].volume;
                track.pan = midiChannels[channelIdx].balance;
                
                if (track.midiChannel == 10)
                    track.isPercussion = true;
            }
            else
            {
                track.midiChannel = (i % 16) + 1;
            }
        }
        
        DBG("Stream position before measures: " << inputStream->getPosition() << " / " << inputStream->getTotalLength());
        
        // 15. Read measures (the actual notes!)
        readMeasures();
        
        DBG("GP5 parsing complete! Track count: " << tracks.size());
        return true;
    }
    catch (const std::exception& e)
    {
        lastError = juce::String("GP5 parse error: ") + e.what();
        return false;
    }
}

//==============================================================================
void GP5Parser::readMeasures()
{
    for (int m = 0; m < measureHeaders.size(); ++m)
    {
        if (inputStream == nullptr || inputStream->isExhausted())
        {
            DBG("Warning: Stream exhausted at measure " << m);
            return;
        }
        
        for (int t = 0; t < tracks.size(); ++t)
        {
            try 
            {
                readMeasure(tracks.getReference(t), m);
            }
            catch (const std::exception& e)
            {
                DBG("Exception in readMeasure(" << m << ", " << t << "): " << e.what());
                return;
            }
        }
    }
}

void GP5Parser::readMeasure(GP5Track& track, int measureIndex)
{
    if (measureIndex < 0 || measureIndex >= track.measures.size() || measureIndex >= measureHeaders.size())
        return;
    
    auto& measure = track.measures.getReference(measureIndex);
    const auto& header = measureHeaders[measureIndex];
    
    // DEBUG: Zeige Position für wichtige Takte
    int trackIdx = &track - &tracks.getReference(0);
    if (measureIndex == 41 && trackIdx == 2)  // Measure 42, Track 3
    {
        DBG("=== DEBUG: Reading Measure 42, Track 3 at stream pos " << inputStream->getPosition() << " ===");
    }
    
    // Voice 1
    readVoice(measure.voice1, header);
    
    // Voice 2
    readVoice(measure.voice2, header);
    
    // Line break
    readU8();
}

int GP5Parser::readVoice(juce::Array<GP5Beat>& beats, const GP5MeasureHeader& header)
{
    int beatCount = readI32();
    
    DBG("    readVoice: beatCount=" << beatCount);
    
    // Sanity check: a single voice should not have more than 128 beats
    if (beatCount < 0 || beatCount > 128)
    {
        DBG("Warning: Invalid beatCount: " << beatCount);
        return 0;
    }
    
    for (int i = 0; i < beatCount; ++i)
    {
        if (inputStream == nullptr || inputStream->isExhausted())
            break;
            
        GP5Beat beat;
        DBG("      readBeat " << i << " at pos " << inputStream->getPosition());
        readBeat(beat);
        beats.add(beat);
    }
    
    return beatCount;
}

void GP5Parser::readBeat(GP5Beat& beat)
{
    if (inputStream == nullptr || inputStream->isExhausted())
        return;
    
    juce::int64 startPos = inputStream->getPosition();
    juce::uint8 flags = readU8();
    DBG("        beat flags=0x" << juce::String::toHexString(flags) << " at pos " << startPos);
    
    // Dotted note (flags & 0x01)
    beat.isDotted = (flags & 0x01) != 0;
    
    // Status byte - REST indicator (flags & 0x40) - MUST BE READ BEFORE DURATION!
    if (flags & 0x40)
    {
        juce::uint8 status = readU8();
        beat.isRest = (status == 0x02);
        DBG("        -> status=" << (int)status << " isRest=" << (beat.isRest ? 1 : 0));
    }
    
    // Duration - ALWAYS present
    beat.duration = readI8();
    DBG("        -> duration=" << beat.duration);
    
    // Tuplet (flags & 0x20)
    if (flags & 0x20)
    {
        beat.tupletN = readI32();
        DBG("        -> tuplet=" << beat.tupletN);
    }
    
    // Chord diagram (flags & 0x02)
    if (flags & 0x02)
    {
        DBG("        -> reading chord diagram at pos " << inputStream->getPosition());
        readChord();
        DBG("        -> chord diagram done, pos=" << inputStream->getPosition());
    }
    
    // Text (flags & 0x04)
    if (flags & 0x04)
    {
        DBG("        -> reading text");
        beat.text = readIntByteSizeString();
    }
    
    // Beat effects (flags & 0x08)
    if (flags & 0x08)
    {
        DBG("        -> reading beat effects");
        readBeatEffects(beat);
    }
    
    // Mix table change (flags & 0x10)
    if (flags & 0x10)
    {
        DBG("        -> reading mix table");
        readMixTableChange();
    }
    
    // String flags - ALWAYS present
    int stringFlags = readU8();
    DBG("        -> stringFlags=0x" << juce::String::toHexString(stringFlags));
    
    // Read notes (from highest to lowest string, bit 6 = string 1, bit 0 = string 7)
    for (int s = 6; s >= 0; --s)
    {
        if (stringFlags & (1 << s))
        {
            int stringNum = 6 - s;  // Convert: bit 6 -> string 0, bit 0 -> string 6
            DBG("          -> reading note for string " << stringNum);
            GP5Note note;
            readNote(note);
            beat.notes[stringNum] = note;
        }
    }
    
    // GP5: Additional beat data (from PyGuitarPro gp5.py readBeat)
    // Read 2 bytes flags2 (short)
    juce::int16 flags2 = readI16();
    DBG("        -> flags2=0x" << juce::String::toHexString(flags2));
    
    // Read break secondary beam byte - ONLY if flag 0x0800 is set!
    if (flags2 & 0x0800)
    {
        juce::uint8 breakSecondary = readU8();
        DBG("        -> breakSecondary=" << (int)breakSecondary);
    }
    
    DBG("        -> beat done at pos " << inputStream->getPosition());
}

void GP5Parser::readNote(GP5Note& note)
{
    juce::int64 noteStartPos = inputStream->getPosition();
    juce::uint8 flags = readU8();
    DBG("            note flags=0x" << juce::String::toHexString(flags) << " at pos " << noteStartPos);
    
    // Accentuated (flags & 0x02)
    note.hasHeavyAccent = (flags & 0x02) != 0;
    // Ghost note (flags & 0x04)
    note.isGhost = (flags & 0x04) != 0;
    // Accent (flags & 0x40)
    note.hasAccent = (flags & 0x40) != 0;
    
    // Note type and fret (flags & 0x20)
    if (flags & 0x20)
    {
        juce::uint8 noteType = readU8();
        note.isTied = (noteType == 0x02);
        note.isDead = (noteType == 0x03);
        DBG("            -> noteType=" << (int)noteType);
    }
    
    // Dynamics (flags & 0x10)
    if (flags & 0x10)
    {
        note.velocity = readI8();
        DBG("            -> velocity=" << note.velocity);
    }
    
    // Fret (flags & 0x20)
    if (flags & 0x20)
    {
        note.fret = readI8();
        DBG("            -> fret=" << note.fret);
        
        // DEBUG: Bei hohen Frets extra warnen
        if (note.fret >= 10)
        {
            DBG("            *** HIGH FRET DETECTED: " << note.fret << " ***");
        }
    }
    
    // Left/right hand fingering (flags & 0x80)
    if (flags & 0x80)
    {
        DBG("            -> reading fingering (0x80)");
        readI8();  // left hand finger
        readI8();  // right hand finger
    }
    
    // Duration percent (flags & 0x01) - ONLY double (8 bytes) in GP5
    if (flags & 0x01)
    {
        DBG("            -> reading durationPercent (0x01)");
        skip(8);  // double value only, no extra byte!
    }
    
    // GP5: Second flags byte - ALWAYS present in GP5!
    juce::uint8 flags2 = readU8();
    DBG("            -> flags2=0x" << juce::String::toHexString(flags2));
    
    // Note effects (flags & 0x08)
    if (flags & 0x08)
    {
        DBG("            -> reading note effects");
        readNoteEffects(note);
        DBG("            -> note effects done, pos=" << inputStream->getPosition());
    }
    
    DBG("            -> note done at pos " << inputStream->getPosition());
}

void GP5Parser::readNoteEffects(GP5Note& note)
{
    juce::int64 startPos = inputStream->getPosition();
    juce::uint8 flags1 = readU8();
    juce::uint8 flags2 = readU8();
    DBG("              noteEffects flags1=0x" << juce::String::toHexString(flags1) 
        << " flags2=0x" << juce::String::toHexString(flags2) << " at pos " << startPos);
    
    // Bend
    if (flags1 & 0x01)
    {
        DBG("              -> reading bend");
        note.hasBend = true;
        // Read bend structure
        note.bendType = readU8();  // type: 1=bend, 2=bend+release, 3=release, 4=pre-bend, 5=pre-bend+release
        int maxBendValue = readI32(); // value in 1/100 semitones (100 = 1/2 tone, 200 = full tone)
        note.bendValue = maxBendValue;
        
        // Release bend detection
        if (note.bendType == 2 || note.bendType == 3 || note.bendType == 5)
        {
            note.hasReleaseBend = true;
        }
        
        int pointCount = readI32();
        DBG("              -> bend type=" << note.bendType << " value=" << maxBendValue << " pointCount=" << pointCount);
        
        // Store all bend points for proper bend curve interpolation
        note.bendPoints.reserve(pointCount);
        int finalValue = 0;
        
        for (int p = 0; p < pointCount; ++p)
        {
            GP5BendPoint bp;
            bp.position = readI32(); // position (0-60, 60 = full duration)
            bp.value = readI32();    // value in 1/100 semitones
            bp.vibrato = readU8();   // vibrato type
            
            note.bendPoints.push_back(bp);
            
            if (bp.value > note.bendValue)
                note.bendValue = bp.value;
            finalValue = bp.value;
            
            DBG("                 point " << p << ": pos=" << bp.position << " val=" << bp.value << " vib=" << bp.vibrato);
        }
        
        // Wenn der finale Wert niedriger als der Max ist, ist es ein Release
        if (finalValue < note.bendValue && finalValue < note.bendValue * 0.5)
        {
            note.hasReleaseBend = true;
        }
    }
    
    // Grace note
    if (flags1 & 0x10)
    {
        readU8();  // fret
        readU8();  // velocity
        readU8();  // transition
        readU8();  // duration
        readU8();  // flags (GP5)
    }
    
    // Tremolo picking
    if (flags2 & 0x04)
        readU8();
    
    // Slide
    if (flags2 & 0x08)
    {
        note.hasSlide = true;
        note.slideType = readU8();
    }
    
    // Harmonic
    if (flags2 & 0x10)
    {
        note.hasHarmonic = true;
        note.harmonicType = readI8();
        
        if (note.harmonicType == 2)  // Artificial
        {
            readU8(); readI8(); readU8();  // note, accidental, octave
        }
        else if (note.harmonicType == 3)  // Tapped
        {
            readU8();  // fret
        }
    }
    
    // Trill
    if (flags2 & 0x20)
    {
        readU8();  // fret
        readU8();  // duration
    }
    
    // Vibrato
    note.hasVibrato = (flags2 & 0x40) != 0;
    
    // Hammer-on / Pull-off
    note.hasHammerOn = (flags1 & 0x02) != 0;
    
    // Let ring
    // note.hasLetRing = (flags1 & 0x08) != 0;
    
    // Staccato
    // note.hasStaccato = (flags1 & 0x04) != 0;
}

void GP5Parser::readBeatEffects(GP5Beat& beat)
{
    juce::int64 startPos = inputStream->getPosition();
    juce::uint8 flags1 = readU8();
    juce::uint8 flags2 = readU8();
    DBG("          beatEffects flags1=0x" << juce::String::toHexString(flags1) 
        << " flags2=0x" << juce::String::toHexString(flags2) << " at pos " << startPos);
    
    // Tapping/slapping/popping
    if (flags1 & 0x20)
    {
        DBG("          -> reading tap/slap/pop");
        readU8();
    }
    
    // Tremolo bar
    if (flags2 & 0x04)
    {
        DBG("          -> reading tremolo bar");
        readU8();  // type
        readI32(); // value
        int points = readI32();
        DBG("          -> tremolo bar points=" << points);
        if (points < 0 || points > 100) 
        {
            DBG("          -> WARNING: invalid tremolo bar points!");
            return;
        }
        for (int p = 0; p < points; ++p)
        {
            readI32(); readI32(); readU8();
        }
    }
    
    // Stroke
    if (flags1 & 0x40)
    {
        DBG("          -> reading stroke");
        int down = readI8();
        int up = readI8();
        beat.hasDownstroke = (down > 0);
        beat.hasUpstroke = (up > 0);
    }
    
    // Pickstroke
    if (flags2 & 0x02)
    {
        DBG("          -> reading pickstroke");
        int dir = readU8();
        beat.hasDownstroke = (dir == 1);
        beat.hasUpstroke = (dir == 2);
    }
}

void GP5Parser::readChord()
{
    // Chord diagram structure based on PyGuitarPro gp4.py readNewChord
    juce::uint8 newFormat = readU8();  // 0 = old format, 1 = new format
    
    DBG("          chord newFormat=" << (int)newFormat);
    
    if (newFormat == 0)
    {
        // Old format (GP3) - readOldChord
        juce::String name = readIntByteSizeString();  // chord name
        juce::int32 firstFret = readI32();
        if (firstFret > 0)
        {
            for (int i = 0; i < 6; ++i) 
                readI32();  // fret values
        }
    }
    else
    {
        // New format (GP4/GP5) - from PyGuitarPro gp4.py readNewChord
        readBool();  // sharp
        skip(3);     // blank bytes
        readU8();    // root (byte)
        readU8();    // type (byte)
        readU8();    // extension (byte)
        readI32();   // bass
        readI32();   // tonality
        readBool();  // add
        readByteSizeString(22);  // name: 1 byte length + 22 bytes content (GP4 uses 22)
        
        // Alterations (bytes, not ints!)
        readU8();    // fifth
        readU8();    // ninth  
        readU8();    // eleventh
        
        readI32();   // firstFret (baseFret)
        
        // Frets for 7 strings - INTS (4 bytes each)!
        for (int i = 0; i < 7; ++i) 
            readI32();  // fret value per string (int)
        
        // Barres: barreCount + 5 frets + 5 starts + 5 ends = 16 bytes
        readU8();  // barreCount
        for (int b = 0; b < 5; ++b) readU8();  // barre frets
        for (int b = 0; b < 5; ++b) readU8();  // barre starts  
        for (int b = 0; b < 5; ++b) readU8();  // barre ends
        
        // Omissions (7 bools)
        for (int o = 0; o < 7; ++o)
            readBool();
        
        skip(1);  // blank
        
        // Fingerings (7 signed bytes)
        for (int f = 0; f < 7; ++f) 
            readI8();
        
        readBool();  // show fingering
    }
    
    DBG("          chord done at pos " << inputStream->getPosition());
}

void GP5Parser::readMixTableChange()
{
    // GP5 readMixTableChange per PyGuitarPro gp5.py lines 610-625
    // Reads: values, durations, flags (GP4), wah, RSE instrument effect (GP5.1+)
    
    // 1. Read values (GP5 readMixTableChangeValues)
    juce::int8 instrument = readI8();
    
    // RSE Instrument
    readI32(); readI32(); readI32();  // instrument, unknown, soundBank
    if (versionMinor == 0)
    {
        readI16();  // effectNumber
        skip(1);    // GP5.0 extra byte
    }
    else
    {
        readI32();  // effectNumber for GP5.1+
    }
    
    if (versionMinor == 0)
        skip(1);  // GP5.0 extra byte after RSE instrument
    
    juce::int8 volume = readI8();
    juce::int8 balance = readI8();
    juce::int8 chorus = readI8();
    juce::int8 reverb = readI8();
    juce::int8 phaser = readI8();
    juce::int8 tremolo = readI8();
    readIntByteSizeString();  // tempo name
    juce::int32 tempo = readI32();
    if (tempo > 0) currentTempo = tempo;
    
    // 2. Read durations (GP3/GP5 readMixTableChangeDurations)
    // Duration is only read if the corresponding value was >= 0
    if (volume >= 0) readI8();   // volume duration
    if (balance >= 0) readI8();  // balance duration
    if (chorus >= 0) readI8();   // chorus duration
    if (reverb >= 0) readI8();   // reverb duration
    if (phaser >= 0) readI8();   // phaser duration
    if (tremolo >= 0) readI8();  // tremolo duration
    if (tempo >= 0)
    {
        readI8();  // tempo duration
        if (versionMinor > 0)
            readBool();  // hide tempo (GP5.1+ only)
    }
    
    // 3. Read flags (GP4 readMixTableChangeFlags)
    readU8();  // flags byte (allTracks for various params + useRSE + showWah)
    
    // 4. Read wah effect (GP5 readWahEffect) - always read
    readI8();  // wah value
    
    // 5. Read RSE instrument effect (GP5.1+ only)
    if (versionMinor > 0)
    {
        readIntByteSizeString();  // effect name
        readIntByteSizeString();  // effect category
    }
}

//==============================================================================
// CONVERSION TO TAB MODEL
//==============================================================================
TabTrack GP5Parser::convertToTabTrack(int trackIndex) const
{
    TabTrack tabTrack;
    
    if (trackIndex < 0 || trackIndex >= tracks.size())
    {
        DBG("convertToTabTrack: Invalid trackIndex " << trackIndex << ", tracks.size()=" << tracks.size());
        return tabTrack;
    }
    
    const auto& gp5Track = tracks[trackIndex];
    
    DBG("convertToTabTrack: Track " << trackIndex << " has " << gp5Track.measures.size() << " measures");
    
    tabTrack.name = gp5Track.name;
    tabTrack.stringCount = gp5Track.stringCount;
    tabTrack.tuning = gp5Track.tuning;
    tabTrack.capo = gp5Track.capo;
    tabTrack.colour = gp5Track.colour;
    
    // Tracker für letzten Fret pro Saite (für tied notes)
    std::map<int, int> lastFretPerString;
    
    // Convert each measure
    for (int m = 0; m < gp5Track.measures.size() && m < measureHeaders.size(); ++m)
    {
        const auto& gp5Measure = gp5Track.measures[m];
        const auto& header = measureHeaders[m];
        
        // DEBUG: Log fret values for measures 40-50
        if (m >= 39 && m < 50)
        {
            DBG("=== Measure " << (m+1) << " Track " << trackIndex << " ===");
            for (int b = 0; b < gp5Measure.voice1.size(); ++b)
            {
                const auto& beat = gp5Measure.voice1[b];
                for (const auto& [sIdx, note] : beat.notes)
                {
                    DBG("  Beat " << b << " string " << sIdx << " fret=" << note.fret);
                }
            }
        }
        
        DBG("  Measure " << m << ": voice1 has " << gp5Measure.voice1.size() << " beats");
        
        TabMeasure tabMeasure;
        tabMeasure.measureNumber = header.number;
        tabMeasure.timeSignatureNumerator = header.numerator;
        tabMeasure.timeSignatureDenominator = header.denominator;
        tabMeasure.isRepeatOpen = header.isRepeatOpen;
        tabMeasure.repeatCount = header.repeatClose;
        tabMeasure.alternateEnding = header.repeatAlternative;
        tabMeasure.marker = header.marker;
        
        // Convert beats from voice 1 (primary voice)
        for (const auto& gp5Beat : gp5Measure.voice1)
        {
            TabBeat tabBeat;
            tabBeat.duration = convertDuration(gp5Beat.duration);
            tabBeat.isDotted = gp5Beat.isDotted;
            tabBeat.isRest = gp5Beat.isRest;
            tabBeat.isPalmMuted = gp5Beat.isPalmMute;
            tabBeat.hasDownstroke = gp5Beat.hasDownstroke;
            tabBeat.hasUpstroke = gp5Beat.hasUpstroke;
            tabBeat.text = gp5Beat.text;
            
            if (gp5Beat.tupletN > 0)
            {
                tabBeat.tupletNumerator = gp5Beat.tupletN;
                tabBeat.tupletDenominator = (gp5Beat.tupletN == 3) ? 2 : 
                                            (gp5Beat.tupletN == 5 || gp5Beat.tupletN == 6) ? 4 : 
                                            gp5Beat.tupletN - 1;
            }
            
            // Convert notes - aber NICHT wenn es eine Pause ist!
            if (!gp5Beat.isRest)
            {
              for (const auto& [stringIndex, gp5Note] : gp5Beat.notes)
              {
                TabNote tabNote;
                tabNote.string = stringIndex;
                tabNote.velocity = gp5Note.velocity;
                tabNote.isTied = gp5Note.isTied;
                
                // Bei tied notes: Fret von der vorherigen Note auf dieser Saite übernehmen
                if (gp5Note.isTied && lastFretPerString.count(stringIndex))
                {
                    tabNote.fret = lastFretPerString[stringIndex];
                }
                else
                {
                    tabNote.fret = gp5Note.fret;
                }
                
                // Aktualisiere den letzten Fret für diese Saite
                if (!gp5Note.isTied)
                {
                    lastFretPerString[stringIndex] = gp5Note.fret;
                }
                
                // Effects
                tabNote.effects.vibrato = gp5Note.hasVibrato;
                tabNote.effects.ghostNote = gp5Note.isGhost;
                tabNote.effects.deadNote = gp5Note.isDead;
                tabNote.effects.accentuatedNote = gp5Note.hasAccent;
                tabNote.effects.heavyAccentuatedNote = gp5Note.hasHeavyAccent;
                tabNote.effects.hammerOn = gp5Note.hasHammerOn;
                tabNote.effects.bend = gp5Note.hasBend;
                tabNote.effects.bendValue = gp5Note.bendValue / 100.0f;  // Convert to semitones (100 = 0.5, 200 = 1.0)
                tabNote.effects.bendType = gp5Note.bendType;
                tabNote.effects.releaseBend = gp5Note.hasReleaseBend;
                
                if (gp5Note.hasSlide)
                    tabNote.effects.slideType = convertSlideType(gp5Note.slideType);
                
                if (gp5Note.hasHarmonic)
                    tabNote.effects.harmonic = static_cast<HarmonicType>(gp5Note.harmonicType);
                
                tabBeat.notes.add(tabNote);
              }
            } // Ende if (!gp5Beat.isRest)
            
            tabMeasure.beats.add(tabBeat);
        }
        
        tabTrack.measures.add(tabMeasure);
    }
    
    return tabTrack;
}

NoteDuration GP5Parser::convertDuration(int gpDuration) const
{
    // GP: -2=whole, -1=half, 0=quarter, 1=eighth, 2=sixteenth, 3=32nd
    switch (gpDuration)
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

SlideType GP5Parser::convertSlideType(int gpSlide) const
{
    // GP5 slide flags
    if (gpSlide & 0x01) return SlideType::ShiftSlide;
    if (gpSlide & 0x02) return SlideType::LegatoSlide;
    if (gpSlide & 0x04) return SlideType::SlideOutDownwards;
    if (gpSlide & 0x08) return SlideType::SlideOutUpwards;
    if (gpSlide & 0x10) return SlideType::SlideIntoFromBelow;
    if (gpSlide & 0x20) return SlideType::SlideIntoFromAbove;
    return SlideType::None;
}

//==============================================================================
// LOW-LEVEL READING
//==============================================================================
juce::uint8 GP5Parser::readU8()
{
    juce::uint8 value = 0;
    if (inputStream != nullptr && !inputStream->isExhausted())
        inputStream->read(&value, 1);
    return value;
}

juce::int8 GP5Parser::readI8()
{
    juce::int8 value = 0;
    if (inputStream != nullptr && !inputStream->isExhausted())
        inputStream->read(&value, 1);
    return value;
}

juce::uint16 GP5Parser::readU16()
{
    juce::uint8 bytes[2] = {0, 0};
    if (inputStream != nullptr && !inputStream->isExhausted())
        inputStream->read(bytes, 2);
    return static_cast<juce::uint16>(bytes[0]) | (static_cast<juce::uint16>(bytes[1]) << 8);
}

juce::int16 GP5Parser::readI16()
{
    return static_cast<juce::int16>(readU16());
}

juce::int32 GP5Parser::readI32()
{
    juce::uint8 bytes[4] = {0, 0, 0, 0};
    if (inputStream != nullptr && !inputStream->isExhausted())
        inputStream->read(bytes, 4);
    return static_cast<juce::int32>(bytes[0]) |
           (static_cast<juce::int32>(bytes[1]) << 8) |
           (static_cast<juce::int32>(bytes[2]) << 16) |
           (static_cast<juce::int32>(bytes[3]) << 24);
}

bool GP5Parser::readBool()
{
    return readU8() != 0;
}

void GP5Parser::skip(int count)
{
    if (inputStream != nullptr && count > 0)
        inputStream->setPosition(inputStream->getPosition() + count);
}

juce::Colour GP5Parser::readColor()
{
    juce::uint8 r = readU8();
    juce::uint8 g = readU8();
    juce::uint8 b = readU8();
    skip(1);  // padding
    return juce::Colour(r, g, b);
}

juce::String GP5Parser::readByteSizeString(int count)
{
    if (inputStream == nullptr)
        return juce::String();
    
    // Always read the length byte first
    int actualLength = static_cast<int>(readU8());
    
    // If no content bytes to read, return empty string
    if (count <= 0)
        return juce::String();
    
    // Sanity check to prevent huge allocations
    if (count > 10000 || actualLength < 0)
    {
        DBG("Warning: readByteSizeString invalid: count=" << count << " actualLength=" << actualLength);
        skip(count);  // Skip the bytes anyway
        return juce::String();
    }
    
    juce::MemoryBlock buffer(static_cast<size_t>(count));
    if (!inputStream->isExhausted())
        inputStream->read(buffer.getData(), count);
    
    // Make sure we don't pass negative length to fromUTF8
    int len = juce::jmin(actualLength, count);
    if (len <= 0)
        return juce::String();
        
    return juce::String::fromUTF8(static_cast<const char*>(buffer.getData()), len);
}

juce::String GP5Parser::readIntSizeString()
{
    int length = readI32();
    if (length <= 0 || inputStream == nullptr) return juce::String();
    
    // Sanity check
    if (length > 100000)
    {
        DBG("Warning: readIntSizeString length too large: " << length);
        return juce::String();
    }
    
    juce::MemoryBlock buffer(static_cast<size_t>(length));
    if (!inputStream->isExhausted())
        inputStream->read(buffer.getData(), length);
    return juce::String::fromUTF8(static_cast<const char*>(buffer.getData()), length);
}

juce::String GP5Parser::readIntByteSizeString()
{
    int count = readI32();
    DBG("  readIntByteSizeString: count=" << count << " at pos " << (inputStream->getPosition() - 4));
    if (count <= 0) return juce::String();
    // count includes the length byte, so we read (count - 1) content bytes
    return readByteSizeString(count - 1);
}

void GP5Parser::readVersion()
{
    // The version string is stored as: 1 byte length + up to 30 characters
    // Total fixed size is 31 bytes (1 + 30)
    songInfo.version = readByteSizeString(30);
    // No additional skip needed - readByteSizeString already reads exactly 30 bytes after the length byte
}

void GP5Parser::readInfo()
{
    DBG("readInfo() starting at pos " << inputStream->getPosition());
    songInfo.title = readIntByteSizeString();
    DBG("  title: " << songInfo.title);
    songInfo.subtitle = readIntByteSizeString();
    DBG("  subtitle: " << songInfo.subtitle);
    songInfo.artist = readIntByteSizeString();
    DBG("  artist: " << songInfo.artist);
    songInfo.album = readIntByteSizeString();
    DBG("  album: " << songInfo.album);
    songInfo.words = readIntByteSizeString();
    DBG("  words: " << songInfo.words);
    songInfo.music = readIntByteSizeString();
    DBG("  music: " << songInfo.music);
    songInfo.copyright = readIntByteSizeString();
    DBG("  copyright: " << songInfo.copyright);
    songInfo.tab = readIntByteSizeString();
    DBG("  tab: " << songInfo.tab);
    songInfo.instructions = readIntByteSizeString();
    DBG("  instructions: " << songInfo.instructions);
    
    int noticeCount = readI32();
    DBG("  noticeCount: " << noticeCount);
    for (int i = 0; i < noticeCount; ++i)
        songInfo.notice.add(readIntByteSizeString());
}

void GP5Parser::readLyrics()
{
    readI32();  // track choice
    for (int i = 0; i < 5; ++i)
    {
        readI32();  // starting measure
        readIntSizeString();  // lyrics text
    }
}

void GP5Parser::readRSEMasterEffect()
{
    // GP5.0 has no RSE master effect section
    // GP5.1+ has RSE master effect
    if (versionMinor >= 1)
    {
        readI32();  // master volume
        readI32();  // ???
        for (int i = 0; i < 11; ++i)
            readI8();  // equalizer
        readU8();  // gain preset
    }
}

void GP5Parser::readPageSetup()
{
    readI32(); readI32();  // page size
    readI32(); readI32(); readI32(); readI32();  // margins
    readI32();  // score size
    readI16();  // header/footer flags
    for (int i = 0; i < 10; ++i)
        readIntByteSizeString();  // placeholders
}

void GP5Parser::readDirections()
{
    // 19 direction signs
    for (int i = 0; i < 19; ++i)
        readI16();
}

void GP5Parser::readMidiChannels()
{
    for (int port = 0; port < 4; ++port)
    {
        for (int ch = 0; ch < 16; ++ch)
        {
            GP5MidiChannel channel;
            channel.channel = ch;
            channel.instrument = readI32();
            channel.volume = readU8();
            channel.balance = readU8();
            channel.chorus = readU8();
            channel.reverb = readU8();
            channel.phaser = readU8();
            channel.tremolo = readU8();
            skip(2);  // padding
            midiChannels.add(channel);
        }
    }
}

//==============================================================================
// GP3/GP4 SPECIFIC METHODS
//==============================================================================

void GP5Parser::readInfoGP3()
{
    // GP3 info format is simpler than GP5
    // Per pyguitarpro gp3.py readInfo()
    DBG("readInfoGP3() starting at pos " << inputStream->getPosition());
    
    songInfo.title = readIntByteSizeString();
    DBG("  title: " << songInfo.title);
    
    songInfo.subtitle = readIntByteSizeString();
    DBG("  subtitle: " << songInfo.subtitle);
    
    songInfo.artist = readIntByteSizeString();
    DBG("  artist: " << songInfo.artist);
    
    songInfo.album = readIntByteSizeString();
    DBG("  album: " << songInfo.album);
    
    // GP3: 'words' is author/composer, music is same as words
    songInfo.words = readIntByteSizeString();
    songInfo.music = songInfo.words;
    DBG("  words/music: " << songInfo.words);
    
    songInfo.copyright = readIntByteSizeString();
    DBG("  copyright: " << songInfo.copyright);
    
    songInfo.tab = readIntByteSizeString();
    DBG("  tab: " << songInfo.tab);
    
    songInfo.instructions = readIntByteSizeString();
    DBG("  instructions: " << songInfo.instructions);
    
    // Notice lines
    int noticeCount = readI32();
    DBG("  noticeCount: " << noticeCount);
    for (int i = 0; i < noticeCount; ++i)
        songInfo.notice.add(readIntByteSizeString());
}

void GP5Parser::readMeasureHeadersGP3(int measureCount)
{
    // Per pyguitarpro gp3.py readMeasureHeaders()
    DBG("Reading " << measureCount << " measure headers (GP3 format)");
    
    for (int i = 0; i < measureCount; ++i)
    {
        GP5MeasureHeader header;
        header.number = i + 1;
        
        juce::uint8 flags = readU8();
        
        // Time signature numerator
        if (flags & 0x01)
            header.numerator = readI8();
        else if (i > 0)
            header.numerator = measureHeaders[i-1].numerator;
        
        // Time signature denominator
        if (flags & 0x02)
            header.denominator = readI8();
        else if (i > 0)
            header.denominator = measureHeaders[i-1].denominator;
        
        // Repeat open
        header.isRepeatOpen = (flags & 0x04) != 0;
        
        // Repeat close
        if (flags & 0x08)
            header.repeatClose = readI8();
        
        // Repeat alternative
        if (flags & 0x10)
            header.repeatAlternative = readU8();
        
        // Marker
        if (flags & 0x20)
        {
            header.marker = readIntByteSizeString();
            readColor();  // Marker color
        }
        
        // Key signature
        if (flags & 0x40)
        {
            readI8();  // root
            readI8();  // type
        }
        
        // Double bar
        header.hasDoubleBar = (flags & 0x80) != 0;
        
        measureHeaders.add(header);
    }
}

void GP5Parser::readTracksGP3(int trackCount)
{
    // Per pyguitarpro gp3.py readTracks() and readTrack()
    DBG("Reading " << trackCount << " tracks (GP3 format)");
    
    int measureCount = measureHeaders.size();
    
    for (int i = 0; i < trackCount; ++i)
    {
        GP5Track track;
        
        // Track flags
        juce::uint8 flags = readU8();
        track.isPercussion = (flags & 0x01) != 0;
        track.is12String = (flags & 0x02) != 0;
        track.isBanjo = (flags & 0x04) != 0;
        
        // Name (byte-size string, 40 bytes)
        track.name = readByteSizeString(40);
        
        // String count
        track.stringCount = readI32();
        
        // Tuning (7 ints, but only stringCount are used)
        track.tuning.clear();
        for (int s = 0; s < 7; ++s)
        {
            int tuning = readI32();
            if (s < track.stringCount)
                track.tuning.add(tuning);
        }
        
        // Port
        track.port = readI32();
        
        // Channel (2 ints: channel index, effect channel)
        track.channelIndex = readI32() - 1;  // 1-based to 0-based
        readI32();  // effect channel (ignored)
        
        // Check for percussion channel (channel 9 = drums)
        if (track.channelIndex >= 0 && (track.channelIndex % 16) == 9)
            track.isPercussion = true;
        
        // Fret count
        track.fretCount = readI32();
        
        // Capo (offset in GP3 terminology)
        track.capo = readI32();
        
        // Color
        track.colour = readColor();
        
        // Initialize measures
        for (int m = 0; m < measureCount; ++m)
        {
            track.measures.add(GP5TrackMeasure());
        }
        
        tracks.add(track);
        DBG("Track " << (i+1) << ": " << track.name << " (" << track.stringCount << " strings)");
    }
}

void GP5Parser::readMeasuresGP3()
{
    // Per pyguitarpro gp3.py readMeasures()
    // Measures are read: measure1/track1, measure1/track2, ..., measure2/track1, ...
    DBG("Reading measures (GP3 format)");
    
    for (int m = 0; m < measureHeaders.size(); ++m)
    {
        if (inputStream == nullptr || inputStream->isExhausted())
        {
            DBG("Warning: Stream exhausted at measure " << m);
            return;
        }
        
        for (int t = 0; t < tracks.size(); ++t)
        {
            try 
            {
                readMeasureGP3(tracks.getReference(t), m);
            }
            catch (const std::exception& e)
            {
                DBG("Exception in readMeasureGP3(" << m << ", " << t << "): " << e.what());
                return;
            }
        }
    }
}

void GP5Parser::readMeasureGP3(GP5Track& track, int measureIndex)
{
    // Per pyguitarpro gp3.py readMeasure()
    // GP3 has only 1 voice per measure
    if (measureIndex < 0 || measureIndex >= track.measures.size() || measureIndex >= measureHeaders.size())
        return;
    
    auto& measure = track.measures.getReference(measureIndex);
    
    // Read beat count
    int beatCount = readI32();
    DBG("  Measure " << measureIndex << ": " << beatCount << " beats");
    
    if (beatCount < 0 || beatCount > 128)
    {
        DBG("Warning: Invalid beatCount: " << beatCount);
        return;
    }
    
    // Read beats
    for (int i = 0; i < beatCount; ++i)
    {
        if (inputStream == nullptr || inputStream->isExhausted())
            break;
            
        GP5Beat beat;
        readBeatGP3(beat);
        measure.voice1.add(beat);
    }
}

void GP5Parser::readBeatGP3(GP5Beat& beat)
{
    // Per pyguitarpro gp3.py readBeat()
    if (inputStream == nullptr || inputStream->isExhausted())
        return;
    
    juce::uint8 flags = readU8();
    DBG("        beat flags=0x" << juce::String::toHexString(flags));
    
    // Dotted note (flags & 0x01)
    beat.isDotted = (flags & 0x01) != 0;
    
    // Status byte (flags & 0x40) - REST indicator
    if (flags & 0x40)
    {
        juce::uint8 status = readU8();
        beat.isRest = (status == 0x02);  // 0x00 = empty, 0x02 = rest
    }
    
    // Duration - ALWAYS present
    beat.duration = readI8();
    
    // Tuplet (flags & 0x20)
    if (flags & 0x20)
    {
        beat.tupletN = readI32();
    }
    
    // Chord diagram (flags & 0x02)
    if (flags & 0x02)
    {
        // TODO: Get string count from current track
        readChordGP3(6);  // Default to 6 strings
    }
    
    // Text (flags & 0x04)
    if (flags & 0x04)
    {
        beat.text = readIntByteSizeString();
    }
    
    // Beat effects (flags & 0x08)
    if (flags & 0x08)
    {
        readBeatEffectsGP3(beat);
    }
    
    // Mix table change (flags & 0x10)
    if (flags & 0x10)
    {
        readMixTableChangeGP3();
    }
    
    // String flags - ALWAYS present
    int stringFlags = readU8();
    
    // Read notes (from highest to lowest string, bit 6 = string 1, bit 0 = string 7)
    for (int s = 6; s >= 0; --s)
    {
        if (stringFlags & (1 << s))
        {
            int stringNum = 6 - s;
            GP5Note note;
            readNoteGP3(note);
            beat.notes[stringNum] = note;
        }
    }
    
    // GP3 has no flags2 or breakSecondary
}

void GP5Parser::readNoteGP3(GP5Note& note)
{
    // Per pyguitarpro gp3.py readNote()
    juce::uint8 flags = readU8();
    
    // Ghost note (flags & 0x04)
    note.isGhost = (flags & 0x04) != 0;
    
    // Heavy accent (flags & 0x02)
    note.hasHeavyAccent = (flags & 0x02) != 0;
    
    // Accent (flags & 0x40) - GP3 uses this as accentuated note
    note.hasAccent = (flags & 0x40) != 0;
    
    // Note type and fret (flags & 0x20)
    if (flags & 0x20)
    {
        juce::uint8 noteType = readU8();
        note.isTied = (noteType == 0x02);  // 1 = normal, 2 = tied, 3 = dead
        note.isDead = (noteType == 0x03);
    }
    
    // Time-independent duration (flags & 0x01)
    if (flags & 0x01)
    {
        readI8();  // duration
        readI8();  // tuplet
    }
    
    // Dynamics (flags & 0x10)
    if (flags & 0x10)
    {
        // GP3 velocity encoding: value * 8 + 1
        juce::int8 dyn = readI8();
        note.velocity = (dyn * 8) + 1;
    }
    
    // Fret (flags & 0x20)
    if (flags & 0x20)
    {
        note.fret = readI8();
    }
    
    // Fingering (flags & 0x80)
    if (flags & 0x80)
    {
        readI8();  // left hand finger
        readI8();  // right hand finger
    }
    
    // Note effects (flags & 0x08)
    if (flags & 0x08)
    {
        readNoteEffectsGP3(note);
    }
    
    // GP3 has no flags2 byte for notes
}

void GP5Parser::readNoteEffectsGP3(GP5Note& note)
{
    // Per pyguitarpro gp3.py readNoteEffects()
    // GP3 has only 1 byte of flags
    juce::uint8 flags = readU8();
    
    // Hammer-on / Pull-off (flags & 0x02)
    note.hasHammerOn = (flags & 0x02) != 0;
    
    // Let ring (flags & 0x08) - not stored in our model
    // bool letRing = (flags & 0x08) != 0;
    
    // Bend (flags & 0x01)
    if (flags & 0x01)
    {
        note.hasBend = true;
        note.bendType = readI8();  // type
        note.bendValue = readI32();  // value
        int pointCount = readI32();
        
        int maxValue = note.bendValue;
        int finalValue = 0;
        for (int p = 0; p < pointCount; ++p)
        {
            readI32(); // position
            int pointValue = readI32(); // value
            readBool();  // vibrato
            
            if (pointValue > maxValue)
                maxValue = pointValue;
            finalValue = pointValue;
        }
        note.bendValue = maxValue;
        
        // Detect release bend
        if (note.bendType == 2 || note.bendType == 3 || note.bendType == 5 ||
            (finalValue < maxValue * 0.5))
        {
            note.hasReleaseBend = true;
        }
    }
    
    // Grace note (flags & 0x10)
    if (flags & 0x10)
    {
        readI8();  // fret
        readU8();  // velocity
        readU8();  // duration
        readI8();  // transition
    }
    
    // Slide (flags & 0x04)
    if (flags & 0x04)
    {
        note.hasSlide = true;
        note.slideType = 1;  // GP3 only has shift slide
    }
}

void GP5Parser::readBeatEffectsGP3(GP5Beat& beat)
{
    // Per pyguitarpro gp3.py readBeatEffects()
    // GP3 has only 1 byte of flags (GP4+ has 2)
    juce::uint8 flags1 = readU8();
    
    // Vibrato (flags1 & 0x01 or 0x02)
    // These set vibrato on the note effect
    
    // Fade in (flags1 & 0x10) - not stored
    
    // Tremolo bar or slap effect (flags1 & 0x20)
    if (flags1 & 0x20)
    {
        juce::uint8 slapEffect = readU8();
        if (slapEffect == 0)
        {
            // Tremolo bar: read dip value
            readI32();  // dip value
        }
        else
        {
            // Slap effect: value was already read
            readI32();  // unknown
        }
    }
    
    // Stroke direction (flags1 & 0x40)
    if (flags1 & 0x40)
    {
        int down = readI8();  // downstroke duration
        int up = readI8();    // upstroke duration
        beat.hasDownstroke = (down > 0);
        beat.hasUpstroke = (up > 0);
    }
    
    // Natural harmonic (flags1 & 0x04) - sets harmonic on notes
    // Artificial harmonic (flags1 & 0x08) - sets harmonic on notes
}

void GP5Parser::readChordGP3(int stringCount)
{
    // Per pyguitarpro gp3.py readChord()
    bool newFormat = readBool();
    
    if (!newFormat)
    {
        // Old format (GP3) - readOldChord
        juce::String name = readIntByteSizeString();
        juce::int32 firstFret = readI32();
        if (firstFret > 0)
        {
            for (int i = 0; i < 6; ++i) 
                readI32();  // fret values
        }
    }
    else
    {
        // New format (GP4) - readNewChord
        readBool();  // sharp
        skip(3);     // blank
        readI32();   // root
        readI32();   // type
        readI32();   // extension
        readI32();   // bass
        readI32();   // tonality
        readBool();  // add
        readByteSizeString(22);  // name (GP3/GP4 uses 22)
        
        // Alterations
        readI32();   // fifth
        readI32();   // ninth
        readI32();   // eleventh
        
        readI32();   // firstFret (baseFret)
        
        // Frets for 6 strings (ints)
        for (int i = 0; i < 6; ++i) 
            readI32();
        
        // Barres
        juce::int32 barreCount = readI32();
        for (int b = 0; b < 2; ++b) readI32();  // barre frets
        for (int b = 0; b < 2; ++b) readI32();  // barre starts
        for (int b = 0; b < 2; ++b) readI32();  // barre ends
        
        // Omissions (7 bools)
        for (int o = 0; o < 7; ++o)
            readBool();
        
        skip(1);  // blank
    }
}

void GP5Parser::readMixTableChangeGP3()
{
    // Per pyguitarpro gp3.py readMixTableChange()
    
    // Read values
    juce::int8 instrument = readI8();
    juce::int8 volume = readI8();
    juce::int8 balance = readI8();
    juce::int8 chorus = readI8();
    juce::int8 reverb = readI8();
    juce::int8 phaser = readI8();
    juce::int8 tremolo = readI8();
    juce::int32 tempo = readI32();
    
    if (tempo > 0) currentTempo = tempo;
    
    // Read durations (only if value was >= 0)
    if (volume >= 0) readI8();
    if (balance >= 0) readI8();
    if (chorus >= 0) readI8();
    if (reverb >= 0) readI8();
    if (phaser >= 0) readI8();
    if (tremolo >= 0) readI8();
    if (tempo >= 0) readI8();
}
