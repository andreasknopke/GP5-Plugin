/*
  ==============================================================================

    GP5Writer.cpp
    
    Guitar Pro 5 (.gp5) File Writer
    Creates GP5 files from recorded notes

  ==============================================================================
*/

#include "GP5Writer.h"

//==============================================================================
GP5Writer::GP5Writer()
{
}

//==============================================================================
bool GP5Writer::writeToFile(const TabTrack& track, const juce::File& outputFile)
{
    // Create output stream
    outputStream = outputFile.createOutputStream();
    if (!outputStream)
    {
        lastError = "Could not create output file";
        return false;
    }
    
    try
    {
        // Determine time signature from first measure
        int numerator = 4;
        int denominator = 4;
        if (!track.measures.isEmpty())
        {
            numerator = track.measures[0].timeSignatureNumerator;
            denominator = track.measures[0].timeSignatureDenominator;
        }
        
        int numMeasures = juce::jmax(1, (int)track.measures.size());
        
        writeHeader();
        writeSongInfo();
        writeMidiChannels();
        
        // Number of measures and tracks
        writeInt(numMeasures);
        writeInt(1);  // 1 track
        
        writeMeasureHeaders(numMeasures, numerator, denominator);
        writeTracks(track);
        writeMeasures(track);
        
        outputStream->flush();
        outputStream.reset();
        
        return true;
    }
    catch (const std::exception& e)
    {
        lastError = juce::String("Write error: ") + e.what();
        outputStream.reset();
        return false;
    }
}

//==============================================================================
void GP5Writer::writeHeader()
{
    // GP5 version string
    juce::String version = "FICHIER GUITAR PRO v5.00";
    
    // Write version string (31 bytes total: 1 byte length + 30 bytes string)
    writeByte((juce::uint8)version.length());
    for (int i = 0; i < 30; ++i)
    {
        if (i < version.length())
            writeByte((juce::uint8)version[i]);
        else
            writeByte(0);
    }
}

void GP5Writer::writeSongInfo()
{
    // Title
    writeStringWithLength(songTitle);
    
    // Subtitle
    writeStringWithLength("");
    
    // Artist
    writeStringWithLength(songArtist);
    
    // Album
    writeStringWithLength("");
    
    // Words (lyricist)
    writeStringWithLength("");
    
    // Music (composer)
    writeStringWithLength("");
    
    // Copyright
    writeStringWithLength("");
    
    // Tab author
    writeStringWithLength("GP5 VST Editor");
    
    // Instructions
    writeStringWithLength("");
    
    // Notice lines (number of lines, then each line)
    writeInt(0);
    
    // Lyrics
    writeInt(0);  // No lyrics
    
    // Tempo name
    writeStringWithLength("Moderate");
    
    // Tempo value
    writeInt(tempo);
    
    // Key (0 = C major)
    writeByte(0);
    writeInt(0);  // Octave (not used in GP5)
    
    // MIDI port (4 bytes)
    writeInt(0);
}

void GP5Writer::writeMidiChannels()
{
    // GP5 has 64 MIDI channels (4 ports x 16 channels)
    // Each channel: instrument (4), volume (1), balance (1), chorus (1), reverb (1),
    //               phaser (1), tremolo (1), 2 blank bytes = 12 bytes per channel
    
    for (int port = 0; port < 4; ++port)
    {
        for (int channel = 0; channel < 16; ++channel)
        {
            // Instrument (program number, 0-127)
            if (channel == 9)  // Drum channel
                writeInt(0);
            else if (channel == 0)  // First channel - use clean guitar
                writeInt(25);  // Acoustic Guitar (steel)
            else
                writeInt(25);
            
            // Volume (0-127, default 104)
            writeByte(104);
            
            // Balance (0-127, 64 = center)
            writeByte(64);
            
            // Chorus
            writeByte(0);
            
            // Reverb
            writeByte(0);
            
            // Phaser
            writeByte(0);
            
            // Tremolo
            writeByte(0);
            
            // Two blank bytes
            writeByte(0);
            writeByte(0);
        }
    }
}

void GP5Writer::writeMeasureHeaders(int numMeasures, int numerator, int denominator)
{
    for (int m = 0; m < numMeasures; ++m)
    {
        juce::uint8 flags = 0;
        
        if (m == 0)
        {
            // First measure: set time signature
            flags |= 0x01;  // numerator
            flags |= 0x02;  // denominator
        }
        
        writeByte(flags);
        
        if (flags & 0x01)
            writeByte((juce::uint8)numerator);
        
        if (flags & 0x02)
            writeByte((juce::uint8)denominator);
        
        // End of repeat (if flags & 0x04)
        // No marker (flags & 0x08)
        // No alternate ending (flags & 0x10)
        // No key signature change (flags & 0x20)
        // No double bar (flags & 0x80)
        
        // Triplet feel (0 = none)
        if (m == 0)
            writeByte(0);
    }
}

void GP5Writer::writeTracks(const TabTrack& track)
{
    // Track header byte
    writeByte(0);  // No drums, not 12-string, not banjo
    
    // Track name (40 bytes: 1 byte length + 39 bytes string)
    juce::String trackName = track.name.isEmpty() ? "Track 1" : track.name;
    writeByte((juce::uint8)juce::jmin(39, trackName.length()));
    for (int i = 0; i < 39; ++i)
    {
        if (i < trackName.length())
            writeByte((juce::uint8)trackName[i]);
        else
            writeByte(0);
    }
    
    // Number of strings
    int numStrings = juce::jmax(6, track.stringCount);
    writeInt(numStrings);
    
    // String tuning (7 integers, E2 A2 D3 G3 B3 E4 + 1 unused)
    // Standard guitar tuning MIDI notes: 64, 59, 55, 50, 45, 40 (high to low)
    std::array<int, 7> defaultTuning = { 64, 59, 55, 50, 45, 40, 0 };
    
    for (int i = 0; i < 7; ++i)
    {
        if (i < (int)track.tuning.size())
            writeInt(track.tuning[track.tuning.size() - 1 - i]);  // Reverse order
        else if (i < 6)
            writeInt(defaultTuning[i]);
        else
            writeInt(0);
    }
    
    // MIDI port (1-based)
    writeInt(1);
    
    // MIDI channel (1-16)
    writeInt(1);
    
    // MIDI channel for effects
    writeInt(2);
    
    // Number of frets
    writeInt(24);
    
    // Capo position
    writeInt(0);
    
    // Track color
    writeColor(track.colour);
}

void GP5Writer::writeMeasures(const TabTrack& track)
{
    int numMeasures = juce::jmax(1, (int)track.measures.size());
    
    for (int m = 0; m < numMeasures; ++m)
    {
        // For each track (we only have 1)
        {
            if (m < (int)track.measures.size())
            {
                const auto& measure = track.measures[m];
                
                // Number of beats in this voice
                int numBeats = (int)measure.beats.size();
                if (numBeats == 0)
                    numBeats = 1;  // At least one rest beat
                
                writeInt(numBeats);
                
                if (measure.beats.isEmpty())
                {
                    // Write a rest beat
                    writeByte(0x40);  // Rest flag
                    writeByte(0);     // Quarter note duration
                }
                else
                {
                    for (const auto& beat : measure.beats)
                    {
                        writeBeat(beat, track.stringCount);
                    }
                }
            }
            else
            {
                // Empty measure - write one rest beat
                writeInt(1);
                writeByte(0x40);  // Rest flag
                writeByte(0);     // Quarter note duration
            }
            
            // Voice 2 (empty)
            writeInt(0);
        }
    }
}

void GP5Writer::writeBeat(const TabBeat& beat, int stringCount)
{
    juce::uint8 flags = 0;
    
    // Count notes and check for effects
    int noteCount = 0;
    juce::uint8 stringBits = 0;
    bool hasEffects = beat.isPalmMuted || beat.isLetRing || beat.hasDownstroke || beat.hasUpstroke;
    
    // Check for vibrato or harmonics in any note
    for (int s = 0; s < stringCount && s < (int)beat.notes.size(); ++s)
    {
        const auto& note = beat.notes[s];
        if (note.fret >= 0)
        {
            noteCount++;
            stringBits |= (1 << (stringCount - 1 - s));  // String bits are reversed
            
            if (note.effects.vibrato || note.effects.wideVibrato)
                hasEffects = true;
            if (note.effects.harmonic != HarmonicType::None)
                hasEffects = true;
        }
    }
    
    if (noteCount == 0)
        flags |= 0x40;  // Rest
    
    if (beat.isDotted)
        flags |= 0x01;
    
    if (hasEffects)
        flags |= 0x08;  // Beat has effects
    
    if (beat.tupletNumerator > 1)
        flags |= 0x20;  // Tuplet
    
    writeByte(flags);
    
    if (flags & 0x40)
    {
        writeByte(0x00);  // Empty beat status
    }
    
    // Duration (-2=whole, -1=half, 0=quarter, 1=eighth, 2=16th, 3=32nd)
    int duration = 0;
    switch (beat.duration)
    {
        case 1: duration = -2; break;   // Whole
        case 2: duration = -1; break;   // Half
        case 4: duration = 0; break;    // Quarter
        case 8: duration = 1; break;    // Eighth
        case 16: duration = 2; break;   // 16th
        case 32: duration = 3; break;   // 32nd
        default: duration = 0; break;
    }
    writeByte((juce::int8)duration);
    
    // Tuplet value (if flag 0x20)
    if (flags & 0x20)
    {
        writeInt(beat.tupletNumerator);
    }
    
    // Beat effects (if flag 0x08)
    if (flags & 0x08)
    {
        writeBeatEffects(beat);
    }
    
    if (noteCount > 0)
    {
        // String flags
        writeByte(stringBits);
        
        // Write each note (from high string to low)
        for (int s = stringCount - 1; s >= 0; --s)
        {
            if (s < (int)beat.notes.size())
            {
                const auto& note = beat.notes[s];
                if (note.fret >= 0)
                {
                    writeNote(note);
                }
            }
        }
    }
}

void GP5Writer::writeBeatEffects(const TabBeat& beat)
{
    // GP3/4/5 beat effects flags
    juce::uint8 flags1 = 0x00;
    
    // Check for vibrato in notes
    for (const auto& note : beat.notes)
    {
        if (note.fret >= 0 && note.effects.vibrato)
            flags1 |= 0x01;
        if (note.fret >= 0 && note.effects.wideVibrato)
            flags1 |= 0x02;
        if (note.fret >= 0 && note.effects.harmonic == HarmonicType::Natural)
            flags1 |= 0x04;
        if (note.fret >= 0 && note.effects.harmonic == HarmonicType::Artificial)
            flags1 |= 0x08;
    }
    
    // Fade in
    // Beat stroke
    if (beat.hasDownstroke || beat.hasUpstroke)
        flags1 |= 0x40;
    
    writeByte(flags1);
    
    // GP4+ second flags byte
    juce::uint8 flags2 = 0x00;
    // Palm mute is per-note in GP5, but we can set it here
    writeByte(flags2);
    
    // Beat stroke direction
    if (flags1 & 0x40)
    {
        // Stroke down value, stroke up value
        if (beat.hasDownstroke)
        {
            writeByte(0);   // Down value
            writeByte(2);   // Eighth note speed
        }
        else
        {
            writeByte(2);   // Up: Eighth note speed
            writeByte(0);   // Down value
        }
    }
}

void GP5Writer::writeNote(const TabNote& note)
{
    // Pack note flags based on PyGuitarPro gp4.writeNote / gp5.writeNote
    juce::uint8 flags = 0x00;
    
    // 0x02 = heavy accent
    if (note.effects.heavyAccentuatedNote)
        flags |= 0x02;
    
    // 0x04 = ghost note
    if (note.effects.ghostNote)
        flags |= 0x04;
    
    // 0x08 = has note effects
    bool hasNoteEffects = note.effects.bend || 
                          note.effects.hammerOn || 
                          note.effects.pullOff ||
                          note.effects.letRing ||
                          note.effects.slideType != SlideType::None ||
                          note.effects.vibrato ||
                          note.effects.staccato ||
                          note.effects.harmonic != HarmonicType::None;
    if (hasNoteEffects)
        flags |= 0x08;
    
    // 0x10 = has velocity
    flags |= 0x10;
    
    // 0x20 = has note type and fret value
    flags |= 0x20;
    
    writeByte(flags);
    
    // Note type (if flags & 0x20)
    if (flags & 0x20)
    {
        juce::uint8 noteType = 1;  // Normal note
        if (note.isTied)
            noteType = 2;  // Tie
        if (note.effects.deadNote)
            noteType = 3;  // Dead note
        writeByte(noteType);
    }
    
    // Velocity (if flags & 0x10)
    if (flags & 0x10)
    {
        // Convert velocity 0-127 to GP dynamic value
        // GP uses: ppp=1, pp=2, p=3, mp=4, mf=5, f=6, ff=7, fff=8
        int dynamic = 5;  // Default mf
        if (note.velocity < 30) dynamic = 1;
        else if (note.velocity < 50) dynamic = 2;
        else if (note.velocity < 70) dynamic = 3;
        else if (note.velocity < 85) dynamic = 4;
        else if (note.velocity < 100) dynamic = 5;
        else if (note.velocity < 115) dynamic = 6;
        else if (note.velocity < 125) dynamic = 7;
        else dynamic = 8;
        writeByte((juce::int8)dynamic);
    }
    
    // Fret value (if flags & 0x20)
    if (flags & 0x20)
    {
        int fret = note.isTied ? 0 : note.fret;
        writeByte((juce::int8)fret);
    }
    
    // Note effects (if flags & 0x08)
    if (flags & 0x08)
    {
        writeNoteEffects(note.effects);
    }
}

void GP5Writer::writeNoteEffects(const NoteEffects& effects)
{
    // GP4 note effects: 2 flag bytes
    juce::uint8 flags1 = 0x00;
    juce::uint8 flags2 = 0x00;
    
    // flags1:
    // 0x01 = bend
    // 0x02 = hammer-on/pull-off
    // 0x04 = (GP3: slide, GP4+: unused)
    // 0x08 = let ring
    // 0x10 = grace note
    
    if (effects.bend)
        flags1 |= 0x01;
    if (effects.hammerOn || effects.pullOff)
        flags1 |= 0x02;
    if (effects.letRing)
        flags1 |= 0x08;
    
    // flags2:
    // 0x01 = staccato
    // 0x02 = palm mute
    // 0x04 = tremolo picking
    // 0x08 = slide
    // 0x10 = harmonic
    // 0x20 = trill
    // 0x40 = vibrato
    
    if (effects.staccato)
        flags2 |= 0x01;
    // Palm mute per note
    if (effects.slideType != SlideType::None)
        flags2 |= 0x08;
    if (effects.harmonic != HarmonicType::None)
        flags2 |= 0x10;
    if (effects.vibrato || effects.wideVibrato)
        flags2 |= 0x40;
    
    writeByte(flags1);
    writeByte(flags2);
    
    // Bend (if flags1 & 0x01)
    if (flags1 & 0x01)
    {
        writeBend(effects);
    }
    
    // Slides (if flags2 & 0x08)
    if (flags2 & 0x08)
    {
        juce::int8 slideType = 0;
        switch (effects.slideType)
        {
            case SlideType::ShiftSlide: slideType = 1; break;
            case SlideType::LegatoSlide: slideType = 2; break;
            case SlideType::SlideOutDownwards: slideType = 4; break;
            case SlideType::SlideOutUpwards: slideType = 8; break;
            case SlideType::SlideIntoFromAbove: slideType = 16; break;
            case SlideType::SlideIntoFromBelow: slideType = 32; break;
            default: slideType = 1; break;
        }
        writeByte(slideType);
    }
    
    // Harmonic (if flags2 & 0x10)
    if (flags2 & 0x10)
    {
        juce::uint8 harmonicType = 0;
        switch (effects.harmonic)
        {
            case HarmonicType::Natural: harmonicType = 1; break;
            case HarmonicType::Artificial: harmonicType = 2; break;
            case HarmonicType::Tapped: harmonicType = 3; break;
            case HarmonicType::Pinch: harmonicType = 4; break;
            case HarmonicType::Semi: harmonicType = 5; break;
            default: harmonicType = 1; break;
        }
        writeByte(harmonicType);
    }
}

void GP5Writer::writeBend(const NoteEffects& effects)
{
    // Bend type
    juce::int8 bendTypeValue = (juce::int8)effects.bendType;
    if (bendTypeValue == 0)
        bendTypeValue = 1;  // Default to simple bend
    writeByte(bendTypeValue);
    
    // Bend value (in 1/100 semitones, we convert from semitones)
    int bendValue = (int)(effects.bendValue * 100.0f);
    writeInt(bendValue);
    
    // Number of bend points
    // Create simple bend curve: 0 -> target value
    int numPoints = 2;
    if (effects.bendType == 2)  // Bend + Release
        numPoints = 3;
    if (effects.bendType == 4)  // Pre-bend
        numPoints = 1;
    
    writeInt(numPoints);
    
    // Write bend points
    // Position is 0-60 in GP format, value is in 1/50 semitones
    int gpBendValue = (int)(effects.bendValue * bendSemitone);
    
    if (effects.bendType == 4)  // Pre-bend: start at target
    {
        writeInt(0);           // Position 0
        writeInt(gpBendValue); // Value at target
        writeBool(false);      // No vibrato
    }
    else if (effects.bendType == 2)  // Bend + Release
    {
        writeInt(0);           // Position 0 (start)
        writeInt(0);           // Value 0
        writeBool(false);      // No vibrato
        
        writeInt(bendPosition / 2);  // Position 30 (middle)
        writeInt(gpBendValue);       // Value at target
        writeBool(false);            // No vibrato
        
        writeInt(bendPosition);      // Position 60 (end)
        writeInt(0);                 // Back to 0
        writeBool(false);            // No vibrato
    }
    else  // Simple bend
    {
        writeInt(0);           // Position 0 (start)
        writeInt(0);           // Value 0
        writeBool(false);      // No vibrato
        
        writeInt(bendPosition); // Position 60 (end)
        writeInt(gpBendValue);  // Value at target
        writeBool(false);       // No vibrato
    }
}

//==============================================================================
// Binary writing helpers
//==============================================================================

void GP5Writer::writeByte(juce::uint8 value)
{
    outputStream->writeByte((char)value);
}

void GP5Writer::writeShort(juce::int16 value)
{
    outputStream->writeShort(value);
}

void GP5Writer::writeInt(juce::int32 value)
{
    outputStream->writeInt(value);
}

void GP5Writer::writeString(const juce::String& str, int maxLength)
{
    for (int i = 0; i < maxLength; ++i)
    {
        if (i < str.length())
            writeByte((juce::uint8)str[i]);
        else
            writeByte(0);
    }
}

void GP5Writer::writeStringWithLength(const juce::String& str)
{
    // GP5 uses: 4-byte total size, 1-byte string length, then string bytes
    int strLen = str.length();
    writeInt(strLen + 1);  // Total size including length byte
    writeByte((juce::uint8)strLen);
    for (int i = 0; i < strLen; ++i)
    {
        writeByte((juce::uint8)str[i]);
    }
}

void GP5Writer::writeColor(juce::Colour color)
{
    writeByte(color.getRed());
    writeByte(color.getGreen());
    writeByte(color.getBlue());
    writeByte(0);  // Unused/padding
}

void GP5Writer::writeBool(bool value)
{
    writeByte(value ? 1 : 0);
}
