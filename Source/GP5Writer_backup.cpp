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
    // Delete existing file to prevent appending (JUCE doesn't truncate by default)
    if (outputFile.existsAsFile())
        outputFile.deleteFile();
    
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
        
        // GP5 file structure (in correct order according to GP5Parser):
        // 1. Version string
        writeVersion();
        
        // 2. Song info (title, subtitle, artist, etc. + notice lines)
        writeSongInfo();
        
        // 3. Lyrics
        writeLyrics();
        
        // 4. RSE Master Effect - skipped for v5.00
        
        // 5. Page Setup
        writePageSetup();
        
        // 6. Tempo info (tempo name + tempo value + key signature)
        writeTempoInfo();
        
        // 7. MIDI channels (64 channels = 4 ports x 16)
        writeMidiChannels();
        
        // 8. Directions (19 direction signs)
        writeDirections();
        
        // 9. Master reverb
        writeInt(0);  // reverb setting
        
        // 10. Measure and track count
        writeInt(numMeasures);
        writeInt(1);  // 1 track
        
        // 11. Measure headers
        writeMeasureHeaders(numMeasures, numerator, denominator);
        
        // 12. Tracks
        writeTracks(track);
        
        // 13. Measures (the actual note data)
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
void GP5Writer::writeVersion()
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
    // Song info: 9 IntByteSizeStrings + notice lines
    // Order: title, subtitle, artist, album, words, music, copyright, tab, instructions
    
    writeStringWithLength(songTitle);      // Title
    writeStringWithLength("");             // Subtitle
    writeStringWithLength(songArtist);     // Artist
    writeStringWithLength("");             // Album
    writeStringWithLength("");             // Words (lyricist)
    writeStringWithLength("");             // Music (composer)
    writeStringWithLength("");             // Copyright
    writeStringWithLength("GP5 VST Editor"); // Tab author
    writeStringWithLength("");             // Instructions
    
    // Notice lines (number of lines, then each line)
    writeInt(0);  // 0 notice lines
}

void GP5Writer::writeLyrics()
{
    // Lyrics section:
    // 1 int: track choice (1-based, 0 = none)
    // 5 x (int startMeasure + IntSizeString lyricText)
    
    writeInt(0);  // No lyrics track
    
    for (int i = 0; i < 5; ++i)
    {
        writeInt(0);  // Starting measure
        // IntSizeString (4 bytes length + string, no length byte)
        writeInt(0);  // Empty string (length 0)
    }
}

void GP5Writer::writePageSetup()
{
    // Page setup section (per PyGuitarPro gp5.writePageSetup)
    // Page size (x, y)
    writeInt(210);  // Page width
    writeInt(297);  // Page height (A4 dimensions)
    
    // Margins (left, right, top, bottom)
    writeInt(10);   // Left margin
    writeInt(10);   // Right margin
    writeInt(15);   // Top margin
    writeInt(10);   // Bottom margin
    
    // Score size proportion (100 = 100%)
    writeInt(100);
    
    // Header/footer flags - 2 bytes (PyGuitarPro writes U8 + U8)
    writeByte(0xFF);  // flags & 0xFF - show all header/footer elements
    writeByte(0x01);  // flags2 - page number flag
    
    // 10 placeholder strings (IntByteSizeString format)
    // Order: title, subtitle, artist, album, words, music, wordsAndMusic, 
    //        copyright1, copyright2, pageNumber
    writeStringWithLength("%TITLE%");
    writeStringWithLength("%SUBTITLE%");
    writeStringWithLength("%ARTIST%");
    writeStringWithLength("%ALBUM%");
    writeStringWithLength("Words by %WORDS%");
    writeStringWithLength("Music by %MUSIC%");
    writeStringWithLength("Words & Music by %WORDSMUSIC%");
    writeStringWithLength("Copyright %COPYRIGHT%");
    writeStringWithLength("All Rights Reserved - International Copyright Secured");
    writeStringWithLength("Page %N%/%P%");
}

void GP5Writer::writeTempoInfo()
{
    // Tempo name (IntByteSizeString)
    writeStringWithLength("Moderate");
    
    // Tempo value (int)
    writeInt(tempo);
    
    // Hide tempo (only GP5.1+, we're v5.00 so skip this)
    
    // Key signature - per PyGuitarPro: writeI8 + writeI32
    writeByte(0);   // Key (0 = C major) - 1 byte signed
    writeInt(0);    // Octave
}

void GP5Writer::writeMidiChannels()
{
    // GP5 has 64 MIDI channels (4 ports x 16 channels)
    // Each channel: instrument (4), volume (1), balance (1), chorus (1), reverb (1),
    //               phaser (1), tremolo (1), 2 blank bytes = 12 bytes per channel
    
    // Note: GP5 uses compressed channel values that need to be converted
    // The parser uses: (value << 3) - 1 clamped to [-1, 32767] + 1
    // For writing, we use: (value + 1) >> 3, but for simplicity we store raw values
    // since most parsers accept uncompressed values too
    
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
            
            // Volume - store as compressed value (13 = ~104 after decompression)
            writeByte(13);
            
            // Balance (8 = 64 center after decompression)
            writeByte(8);
            
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

void GP5Writer::writeDirections()
{
    // 19 direction signs (each is a short/int16)
    // Per GP5Parser::readDirections()
    for (int i = 0; i < 19; ++i)
    {
        writeShort(0);  // No directions
    }
}

void GP5Writer::writeMeasureHeaders(int numMeasures, int numerator, int denominator)
{
    for (int m = 0; m < numMeasures; ++m)
    {
        // Blank byte before each measure (except first)
        if (m > 0)
            writeByte(0);
        
        juce::uint8 flags = 0;
        
        if (m == 0)
        {
            // First measure: set time signature
            flags |= 0x01;  // numerator present
            flags |= 0x02;  // denominator present
        }
        
        writeByte(flags);
        
        // Time signature numerator (if flag 0x01)
        if (flags & 0x01)
            writeByte((juce::uint8)numerator);
        
        // Time signature denominator (if flag 0x02)
        if (flags & 0x02)
            writeByte((juce::uint8)denominator);
        
        // End of repeat count (if flags & 0x04) - skipped
        // Marker (if flags & 0x08) - skipped
        // Key signature (if flags & 0x40) - skipped
        // Repeat alternative (if flags & 0x10) - skipped
        // Double bar (flags & 0x80) - just a flag, no data
        
        // Beams (if time sig changed, i.e., flags & 0x03)
        if (flags & 0x03)
        {
            // 4 beam bytes for eighth note grouping
            writeByte(2);  // Beam 1
            writeByte(2);  // Beam 2
            writeByte(2);  // Beam 3
            writeByte(2);  // Beam 4
        }
        
        // Blank byte if no repeat alternative (flag 0x10 not set)
        if ((flags & 0x10) == 0)
            writeByte(0);
        
        // Triplet feel (0 = none, 1 = eighth, 2 = sixteenth)
        writeByte(0);
    }
}

void GP5Writer::writeTracks(const TabTrack& track)
{
    // Blank byte before first track (GP5.0)
    writeByte(0);
    
    // Track flags1 byte - GP5 has more flags than GP3/4
    // 0x01 = drums, 0x02 = 12-string, 0x04 = banjo
    // 0x08 = visible (set for normal tracks)
    juce::uint8 flags1 = 0x08;  // Track is visible by default
    writeByte(flags1);
    
    // Track name (41 bytes: 1 byte length + 40 bytes string)
    juce::String trackName = track.name.isEmpty() ? "Track 1" : track.name;
    writeByte((juce::uint8)juce::jmin(40, trackName.length()));
    for (int i = 0; i < 40; ++i)
    {
        if (i < trackName.length())
            writeByte((juce::uint8)trackName[i]);
        else
            writeByte(0);
    }
    
    // Number of strings
    int numStrings = juce::jmax(6, track.stringCount);
    writeInt(numStrings);
    
    // String tuning (7 integers, E4 B3 G3 D3 A2 E2 + 1 unused)
    // Standard guitar tuning MIDI notes: 64, 59, 55, 50, 45, 40 (high to low, strings 1-6)
    std::array<int, 7> defaultTuning = { 64, 59, 55, 50, 45, 40, 0 };
    
    for (int i = 0; i < 7; ++i)
    {
        if (i < (int)track.tuning.size())
            writeInt(track.tuning[i]);
        else if (i < 6)
            writeInt(defaultTuning[i]);
        else
            writeInt(0);
    }
    
    // MIDI port (1-based)
    writeInt(1);
    
    // MIDI channel (1-based, 1-16)
    writeInt(1);
    
    // MIDI channel for effects
    writeInt(2);
    
    // Number of frets
    writeInt(24);
    
    // Capo position (offset)
    writeInt(0);
    
    // Track color
    writeColor(track.colour);
    
    // GP5: flags2 (track settings, Int16)
    // 0x0001 = tablature, 0x0002 = notation
    juce::int16 flags2 = 0x0003;  // Show both tablature and notation
    writeShort(flags2);
    
    // autoAccentuation (byte)
    writeByte(0);
    
    // MIDI bank (byte)
    writeByte(0);
    
    // Track RSE settings
    writeByte(0);     // humanize
    writeInt(0);      // Unknown int 1
    writeInt(0);      // Unknown int 2
    writeInt(100);    // Unknown int 3 (PyGuitarPro uses 100)
    
    // 12 unknown bytes (placeholder)
    for (int i = 0; i < 12; ++i)
        writeByte(0);
    
    // RSE Instrument settings
    writeInt(-1);     // instrument (-1 = none)
    writeInt(0);      // unknown
    writeInt(0);      // soundBank
    
    // GP5.0 specific: effectNumber (short) + padding
    writeShort(0);    // effect number
    writeByte(0);     // padding
    
    // GP5.0: 2 placeholder bytes after all tracks
    writeByte(0);
    writeByte(0);
}

void GP5Writer::writeMeasures(const TabTrack& track)
{
    int numMeasures = juce::jmax(1, (int)track.measures.size());
    
    for (int m = 0; m < numMeasures; ++m)
    {
        // For each track (we only have 1)
        {
            // GP5 has 2 voices per measure
            // Voice 1
            if (m < (int)track.measures.size())
            {
                const auto& measure = track.measures[m];
                
                // Number of beats in voice 1
                int numBeats = (int)measure.beats.size();
                if (numBeats == 0)
                    numBeats = 1;  // At least one rest beat
                
                writeInt(numBeats);
                
                if (measure.beats.isEmpty())
                {
                    // Write a rest beat (must match writeBeat structure)
                    writeByte(0x40);  // Rest flag
                    writeByte(0x00);  // Empty beat status (required for rest)
                    writeByte(0);     // Quarter note duration
                    writeShort(0);    // Flags2 at end of beat
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
                writeByte(0x00);  // Empty beat status (required for rest)
                writeByte(0);     // Quarter note duration
                writeShort(0);    // Flags2 at end of beat
            }
            
            // Voice 2 (empty - 0 beats)
            writeInt(0);
            
            // LineBreak byte (per PyGuitarPro gp5.writeMeasure)
            writeByte(0);  // LineBreak.none = 0
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
    
    // GP5: flags2 at end of beat (Int16)
    // Bit 0x01 = break beam, 0x02 = break secondary beam, etc.
    writeShort(0);  // No display flags for now
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
    // Pack note flags based on PyGuitarPro gp5.writeNote
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
    
    // GP5: flags2 byte (always written)
    // 0x02 = swap accidentals
    writeByte(0);
    
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
