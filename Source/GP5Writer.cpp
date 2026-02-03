/*
  ==============================================================================

    GP5WriterNew.cpp
    
    Guitar Pro 5 (.gp5) File Writer - Rewritten based on PyGuitarPro
    Reference: https://github.com/Perlence/PyGuitarPro/blob/main/src/guitarpro/gp5.py

  ==============================================================================
*/

#include "GP5Writer.h"

//==============================================================================
// PyGuitarPro-compatible GP5 constants
//==============================================================================
namespace GP5Constants
{
    // Version string
    static const char* VERSION_STRING = "FICHIER GUITAR PRO v5.00";
    
    // Page setup defaults
    static const int PAGE_WIDTH = 210;
    static const int PAGE_HEIGHT = 297;
    static const int MARGIN_LEFT = 10;
    static const int MARGIN_RIGHT = 10;
    static const int MARGIN_TOP = 15;
    static const int MARGIN_BOTTOM = 10;
    static const int SCORE_SIZE_PROPORTION = 100;
    
    // Header/footer template strings (11 strings)
    static const char* HEADER_FOOTER_STRINGS[] = {
        "%TITLE%",
        "%SUBTITLE%",
        "%ARTIST%",
        "%ALBUM%",
        "Words by %WORDS%",
        "Music by %MUSIC%",
        "Words & Music by %WORDSMUSIC%",
        "Copyright %COPYRIGHT%",           // copyright line 1
        "All Rights Reserved - International Copyright Secured",  // copyright line 2
        "Page %N%/%P%",                   // page number
        ""                                 // extra placeholder (11th string)
    };
    
    // Beat duration values
    // Duration value -> note type: -2=whole, -1=half, 0=quarter, 1=eighth, 2=16th, 3=32nd, 4=64th
    
    // Bend constants (from PyGuitarPro)
    static const int BEND_POSITION = 60;
    static const int BEND_SEMITONE = 25;
}

//==============================================================================
GP5Writer::GP5Writer()
{
}

//==============================================================================
bool GP5Writer::writeToFile(const TabTrack& track, const juce::File& outputFile)
{
    // Delete existing file to prevent appending
    if (outputFile.existsAsFile())
        outputFile.deleteFile();
    
    outputStream = outputFile.createOutputStream();
    if (!outputStream)
    {
        lastError = "Could not create output file";
        return false;
    }
    
    try
    {
        // Determine time signature
        int numerator = 4;
        int denominator = 4;
        if (!track.measures.isEmpty())
        {
            numerator = track.measures[0].timeSignatureNumerator;
            denominator = track.measures[0].timeSignatureDenominator;
        }
        
        int numMeasures = juce::jmax(1, (int)track.measures.size());
        
        // === PyGuitarPro GP5File.writeSong() order ===
        
        // 1. writeVersion()
        writeVersion();
        
        // 2. writeClipboard() - skip if not clipboard
        
        // 3. writeInfo()
        writeSongInfo();
        
        // 4. writeLyrics()
        writeLyrics();
        
        // 5. writeRSEMasterEffect() - ONLY for GP5.1+, skip for GP5.0.0!
        // (We're writing v5.00, so this is empty)
        
        // 6. writePageSetup()
        writePageSetup();
        
        // 7. writeIntByteSizeString(tempoName) + writeI32(tempo)
        writeStringWithLength("");  // Empty tempo name
        writeInt(tempo);
        
        // 8. writeBool(hideTempo) - ONLY for GP5.1+, skip for GP5.0.0
        
        // 9. writeI8(key) + writeI32(octave)
        writeByte(0);    // Key signature (0 = C major/A minor)
        writeInt(0);     // Octave (always 0)
        
        // 10. writeMidiChannels()
        writeMidiChannels();
        
        // 11. writeDirections()
        writeDirections();
        
        // 12. writeMasterReverb()
        writeInt(0);  // Master reverb = 0
        
        // 13. writeI32(measureCount) + writeI32(trackCount)
        writeInt(numMeasures);
        writeInt(1);  // 1 track
        
        // 14. writeMeasureHeaders()
        writeMeasureHeaders(numMeasures, numerator, denominator);
        
        // 15. writeTracks()
        writeTracks(track);
        
        // 16. writeMeasures()
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
// Multi-track version
//==============================================================================
bool GP5Writer::writeToFile(const std::vector<TabTrack>& tracks, const juce::File& outputFile)
{
    if (tracks.empty())
    {
        lastError = "No tracks to write";
        return false;
    }
    
    // If only one track, use the simpler single-track method
    if (tracks.size() == 1)
    {
        return writeToFile(tracks[0], outputFile);
    }
    
    // Delete existing file to prevent appending
    if (outputFile.existsAsFile())
        outputFile.deleteFile();
    
    outputStream = outputFile.createOutputStream();
    if (!outputStream)
    {
        lastError = "Could not create output file";
        return false;
    }
    
    try
    {
        // Determine time signature from first track
        int numerator = 4;
        int denominator = 4;
        if (!tracks[0].measures.isEmpty())
        {
            numerator = tracks[0].measures[0].timeSignatureNumerator;
            denominator = tracks[0].measures[0].timeSignatureDenominator;
        }
        
        // Find max measure count across all tracks
        int numMeasures = 1;
        for (const auto& track : tracks)
        {
            numMeasures = juce::jmax(numMeasures, (int)track.measures.size());
        }
        
        int numTracks = (int)tracks.size();
        
        // === PyGuitarPro GP5File.writeSong() order ===
        
        // 1. writeVersion()
        writeVersion();
        
        // 3. writeInfo()
        writeSongInfo();
        
        // 4. writeLyrics()
        writeLyrics();
        
        // 6. writePageSetup()
        writePageSetup();
        
        // 7. writeIntByteSizeString(tempoName) + writeI32(tempo)
        writeStringWithLength("");  // Empty tempo name
        writeInt(tempo);
        
        // 9. writeI8(key) + writeI32(octave)
        writeByte(0);    // Key signature (0 = C major/A minor)
        writeInt(0);     // Octave (always 0)
        
        // 10. writeMidiChannels() - use track instruments
        writeMidiChannels(tracks);
        
        // 11. writeDirections()
        writeDirections();
        
        // 12. writeMasterReverb()
        writeInt(0);  // Master reverb = 0
        
        // 13. writeI32(measureCount) + writeI32(trackCount)
        writeInt(numMeasures);
        writeInt(numTracks);
        
        // 14. writeMeasureHeaders()
        writeMeasureHeaders(numMeasures, numerator, denominator);
        
        // 15. writeTracks() - for each track
        for (int t = 0; t < numTracks; ++t)
        {
            writeTrack(tracks[t], t, numTracks);
        }
        
        // 16. writeMeasures() - interleaved: for each measure, write all tracks
        writeMeasuresMultiTrack(tracks);
        
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
    // PyGuitarPro: writeByteSizeString(version, 30)
    // 1 byte length + 30 bytes string
    juce::String version = GP5Constants::VERSION_STRING;
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
    // PyGuitarPro GP5File.writeInfo():
    // 9 IntByteSizeStrings: title, subtitle, artist, album, words, music, copyright, tab, instructions
    // Then: writeNotice() = writeI32(count) + count * writeIntByteSizeString
    
    writeStringWithLength(songTitle);      // Title
    writeStringWithLength("");             // Subtitle
    writeStringWithLength(songArtist);     // Artist
    writeStringWithLength("");             // Album
    writeStringWithLength("");             // Words (lyricist)
    writeStringWithLength("");             // Music (composer)
    writeStringWithLength("");             // Copyright
    writeStringWithLength("GP5 VST Editor"); // Tab author
    writeStringWithLength("");             // Instructions
    
    // Notice: count (int32) + lines
    writeInt(0);  // 0 notice lines
}

void GP5Writer::writeLyrics()
{
    // PyGuitarPro GP4File.writeLyrics():
    // writeI32(trackChoice) + 5 * (writeI32(startingMeasure) + writeIntSizeString(lyrics))
    
    writeInt(0);  // Track choice (0 = no lyrics)
    
    for (int i = 0; i < 5; ++i)
    {
        writeInt(0);  // Starting measure (1-based, 0 = not used)
        writeInt(0);  // Empty string (IntSizeString with length 0)
    }
}

void GP5Writer::writePageSetup()
{
    // PyGuitarPro GP5File.writePageSetup():
    // 7 ints: width, height, marginLeft, marginRight, marginTop, marginBottom, scoreSizeProportion
    // 1 byte: flags & 0xFF
    // 1 byte: flags2 (page number flag)
    // 10 IntByteSizeStrings: title, subtitle, artist, album, words, music, wordsAndMusic, 
    //                        copyright1, copyright2, pageNumber
    
    writeInt(GP5Constants::PAGE_WIDTH);
    writeInt(GP5Constants::PAGE_HEIGHT);
    writeInt(GP5Constants::MARGIN_LEFT);
    writeInt(GP5Constants::MARGIN_RIGHT);
    writeInt(GP5Constants::MARGIN_TOP);
    writeInt(GP5Constants::MARGIN_BOTTOM);
    writeInt(GP5Constants::SCORE_SIZE_PROPORTION);
    
    // Header/footer flags
    writeByte(0xFF);  // flags & 0xFF (show all elements)
    writeByte(0x01);  // flags2 (page number enabled)
    
    // 10 header/footer template strings
    // Note: PyGuitarPro reads copyright as 2 strings combined into 1
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
    // This is now handled inline in writeToFile() for clarity
}

void GP5Writer::writeMidiChannels()
{
    // PyGuitarPro GP3File.writeMidiChannels():
    // 64 channels (4 ports * 16 channels)
    // Each channel: writeI32(program) + 8 bytes (volume, balance, chorus, reverb, phaser, tremolo, blank, blank)
    // Values are compressed: toChannelShort = (value + 1) >> 3, clamped to [0, 16]
    
    for (int port = 0; port < 4; ++port)
    {
        for (int channel = 0; channel < 16; ++channel)
        {
            // Program/instrument
            if (channel == 9)  // Drum channel
                writeInt(0);
            else if (channel == 0)  // First channel
                writeInt(25);  // Acoustic Guitar (steel)
            else
                writeInt(25);
            
            // Volume (8-bit, compressed: 13 = roughly 104 MIDI)
            writeByte(13);
            // Balance (8 = center)
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

void GP5Writer::writeMidiChannels(const std::vector<TabTrack>& tracks)
{
    // Multi-track version: use instruments from tracks
    // Build a map of channel -> instrument from tracks
    std::array<int, 16> channelInstruments;
    channelInstruments.fill(25);  // Default: Acoustic Guitar (steel)
    
    for (const auto& track : tracks)
    {
        int ch = track.midiChannel;
        if (ch >= 0 && ch < 16)
        {
            channelInstruments[ch] = track.midiInstrument;
        }
    }
    
    for (int port = 0; port < 4; ++port)
    {
        for (int channel = 0; channel < 16; ++channel)
        {
            // Program/instrument
            if (channel == 9)  // Drum channel
                writeInt(0);
            else if (port == 0)  // First port uses our track instruments
                writeInt(channelInstruments[channel]);
            else
                writeInt(25);
            
            // Volume (8-bit, compressed: 13 = roughly 104 MIDI)
            writeByte(13);
            // Balance (8 = center)
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
    // PyGuitarPro GP5File.writeDirections():
    // 19 shorts for musical directions (coda, segno, etc.)
    for (int i = 0; i < 19; ++i)
    {
        writeShort(-1);  // -1 = not used
    }
}

void GP5Writer::writeMeasureHeaders(int numMeasures, int numerator, int denominator)
{
    // PyGuitarPro GP5File.writeMeasureHeader():
    // For each measure:
    //   - If not first: placeholder(1)
    //   - writeMeasureHeaderValues(header, flags)
    
    for (int m = 0; m < numMeasures; ++m)
    {
        // Placeholder before each measure (except first)
        if (m > 0)
            writeByte(0);
        
        juce::uint8 flags = 0;
        
        if (m == 0)
        {
            // First measure: set time signature
            flags |= 0x01;  // numerator
            flags |= 0x02;  // denominator
        }
        
        // Write flags byte
        writeByte(flags);
        
        // Numerator (if flags & 0x01)
        if (flags & 0x01)
            writeByte((juce::uint8)numerator);
        
        // Denominator (if flags & 0x02)
        if (flags & 0x02)
            writeByte((juce::uint8)denominator);
        
        // repeatClose (if flags & 0x08) - skipped
        // marker (if flags & 0x20) - skipped
        // keySignature (if flags & 0x40) - skipped
        // repeatAlternative (if flags & 0x10) - skipped
        
        // Beams (if flags & 0x03 - time sig changed)
        if (flags & 0x03)
        {
            // 4 beam bytes (eighth note grouping)
            writeByte(2);
            writeByte(2);
            writeByte(2);
            writeByte(2);
        }
        
        // Placeholder if no repeat alternative (flag 0x10 not set)
        if ((flags & 0x10) == 0)
            writeByte(0);
        
        // Triplet feel (0 = none)
        writeByte(0);
    }
}

void GP5Writer::writeTracks(const TabTrack& track)
{
    // PyGuitarPro GP5File.writeTrack():
    // - placeholder(1) if first track or GP5.0.0
    // - flags1 byte
    // - writeByteSizeString(name, 40)
    // - writeI32(numStrings)
    // - 7 * writeI32(tuning)
    // - writeI32(port)
    // - writeChannel() = writeI32(channel+1) + writeI32(effectChannel+1)
    // - writeI32(fretCount)
    // - writeI32(offset)
    // - writeColor()
    // - flags2 (short)
    // - autoAccentuation (byte)
    // - bank (byte)
    // - writeTrackRSE()
    
    // Placeholder for first track (GP5.0.0)
    writeByte(0);
    
    // Track flags1
    juce::uint8 flags1 = 0x08;  // isVisible = true
    writeByte(flags1);
    
    // Track name (1 byte length + 40 bytes)
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
    
    // 7 string tunings (MIDI notes, high to low: E4=64, B3=59, G3=55, D3=50, A2=45, E2=40)
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
    
    // Port (1-based)
    writeInt(1);
    
    // Channel (1-based)
    writeInt(1);
    
    // Effect channel (1-based)
    writeInt(2);
    
    // Fret count
    writeInt(24);
    
    // Offset (capo)
    writeInt(0);
    
    // Color
    writeColor(track.colour);
    
    // Flags2 (short) - track settings
    juce::int16 flags2 = 0x0003;  // tablature + notation
    writeShort(flags2);
    
    // Auto accentuation
    writeByte(0);
    
    // Bank
    writeByte(0);
    
    // Track RSE (GP5File.writeTrackRSE)
    writeByte(0);    // humanize
    writeInt(0);     // unknown int
    writeInt(0);     // unknown int
    writeInt(100);   // unknown int (PyGuitarPro uses 100)
    
    // 12 placeholder bytes
    for (int i = 0; i < 12; ++i)
        writeByte(0);
    
    // RSE Instrument (GP5File.writeRSEInstrument)
    writeInt(-1);    // instrument (-1 = none)
    writeInt(0);     // unknown
    writeInt(0);     // soundBank
    
    // GP5.0.0: effectNumber (short) + placeholder(1)
    writeShort(0);
    writeByte(0);
    
    // GP5File.writeTracks() adds placeholder(2) for GP5.0.0 after all tracks
    writeByte(0);
    writeByte(0);
}

void GP5Writer::writeTrack(const TabTrack& track, int trackIndex, int totalTracks)
{
    // PyGuitarPro GP5File.writeTrack():
    // - placeholder(1) if first track or GP5.0.0
    // - flags1 byte
    // - writeByteSizeString(name, 40)
    // etc.
    
    // Placeholder for first track (GP5.0.0)
    writeByte(0);
    
    // Track flags1
    juce::uint8 flags1 = 0x08;  // isVisible = true
    writeByte(flags1);
    
    // Track name (1 byte length + 40 bytes)
    juce::String trackName = track.name.isEmpty() ? 
        juce::String("Track ") + juce::String(trackIndex + 1) : track.name;
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
    
    // 7 string tunings (MIDI notes, high to low: E4=64, B3=59, G3=55, D3=50, A2=45, E2=40)
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
    
    // Port (1-based)
    writeInt(1);
    
    // Channel (1-based) - use trackIndex to assign different channels
    writeInt(trackIndex + 1);
    
    // Effect channel (1-based) - offset by track count
    writeInt(totalTracks + trackIndex + 1);
    
    // Fret count
    writeInt(24);
    
    // Offset (capo)
    writeInt(0);
    
    // Color - assign different colors per track
    static const juce::Colour trackColors[] = {
        juce::Colours::red,
        juce::Colours::blue,
        juce::Colours::green,
        juce::Colours::orange,
        juce::Colours::purple,
        juce::Colours::cyan,
        juce::Colours::yellow,
        juce::Colours::magenta
    };
    juce::Colour trackColour = trackColors[trackIndex % 8];
    if (track.colour != juce::Colour())
        trackColour = track.colour;
    writeColor(trackColour);
    
    // Flags2 (short) - track settings
    juce::int16 flags2 = 0x0003;  // tablature + notation
    writeShort(flags2);
    
    // Auto accentuation
    writeByte(0);
    
    // Bank
    writeByte(0);
    
    // Track RSE (GP5File.writeTrackRSE)
    writeByte(0);    // humanize
    writeInt(0);     // unknown int
    writeInt(0);     // unknown int
    writeInt(100);   // unknown int (PyGuitarPro uses 100)
    
    // 12 placeholder bytes
    for (int i = 0; i < 12; ++i)
        writeByte(0);
    
    // RSE Instrument (GP5File.writeRSEInstrument)
    writeInt(-1);    // instrument (-1 = none)
    writeInt(0);     // unknown
    writeInt(0);     // soundBank
    
    // GP5.0.0: effectNumber (short) + placeholder(1)
    writeShort(0);
    writeByte(0);
    
    // GP5File.writeTracks() adds placeholder(2) for GP5.0.0 ONLY after the LAST track
    if (trackIndex == totalTracks - 1)
    {
        writeByte(0);
        writeByte(0);
    }
}

void GP5Writer::writeMeasures(const TabTrack& track)
{
    // PyGuitarPro GP5File.writeMeasure():
    // For each voice (0 and 1):
    //   writeVoice() = writeI32(beatCount) + for each beat: writeBeat()
    // Then: writeU8(lineBreak)
    
    int numMeasures = juce::jmax(1, (int)track.measures.size());
    
    for (int m = 0; m < numMeasures; ++m)
    {
        // Voice 1
        if (m < (int)track.measures.size())
        {
            const auto& measure = track.measures[m];
            int numBeats = juce::jmax(1, (int)measure.beats.size());
            
            writeInt(numBeats);
            
            if (measure.beats.isEmpty())
            {
                // Write single whole rest beat for empty measure
                writeByte(0x40);   // flags: rest
                writeByte(0x02);   // beat status (0x02 = Rest)
                writeByte((juce::uint8)-2); // duration (Whole note = -2) to fill 4/4 measure
                writeByte(0);      // stringFlags (PyGuitarPro ALWAYS reads this!)
                writeShort(0);     // flags2
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
            // Empty measure
            writeInt(1);       // 1 beat
            writeByte(0x40);   // rest
            writeByte(0x02);   // beat status (0x02 = Rest)
            writeByte((juce::uint8)-2); // duration (Whole note = -2)
            writeByte(0);      // stringFlags (PyGuitarPro ALWAYS reads this!)
            writeShort(0);     // flags2
        }
        
        // Voice 2 (empty)
        writeInt(0);
        
        // LineBreak
        writeByte(0);
    }
}

void GP5Writer::writeMeasuresMultiTrack(const std::vector<TabTrack>& tracks)
{
    // GP5 file format: for each measure, write data for ALL tracks
    // Order: Measure 1 (Track 1, Track 2, ...), Measure 2 (Track 1, Track 2, ...), ...
    
    // Find max measure count
    int numMeasures = 1;
    for (const auto& track : tracks)
    {
        numMeasures = juce::jmax(numMeasures, (int)track.measures.size());
    }
    
    for (int m = 0; m < numMeasures; ++m)
    {
        // Write this measure for each track
        for (size_t t = 0; t < tracks.size(); ++t)
        {
            const auto& track = tracks[t];
            
            // Voice 1
            if (m < (int)track.measures.size())
            {
                const auto& measure = track.measures[m];
                int numBeats = juce::jmax(1, (int)measure.beats.size());
                
                writeInt(numBeats);
                
                if (measure.beats.isEmpty())
                {
                    // Write single whole rest beat for empty measure
                    writeByte(0x40);   // flags: rest
                    writeByte(0x02);   // beat status (0x02 = Rest)
                    writeByte((juce::uint8)-2); // duration (Whole note = -2)
                    writeByte(0);      // stringFlags
                    writeShort(0);     // flags2
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
                // Empty measure for this track
                writeInt(1);       // 1 beat
                writeByte(0x40);   // rest
                writeByte(0x02);   // beat status (0x02 = Rest)
                writeByte((juce::uint8)-2); // duration (Whole note = -2)
                writeByte(0);      // stringFlags
                writeShort(0);     // flags2
            }
            
            // Voice 2 (empty)
            writeInt(0);
            
            // LineBreak - must be after EACH track (not just last)
            writeByte(0);
        }
    }
}

void GP5Writer::writeBeat(const TabBeat& beat, int stringCount)
{
    // PyGuitarPro GP5File.writeBeat() calls GP3File.writeBeat() + flags2
    // GP3File.writeBeat():
    //   - flags byte
    //   - beatStatus (if flags & 0x40)
    //   - duration (signed byte)
    //   - tuplet (int, if flags & 0x20)
    //   - chord (if flags & 0x02)
    //   - text (if flags & 0x04)
    //   - beatEffects (if flags & 0x08)
    //   - mixTableChange (if flags & 0x10)
    //   - writeNotes() - stringFlags + notes
    // GP5File adds: flags2 (short) at the end
    
    juce::uint8 flags = 0;
    
    // Count notes
    int noteCount = 0;
    juce::uint8 stringBits = 0;
    
    for (int s = 0; s < stringCount && s < (int)beat.notes.size(); ++s)
    {
        const auto& note = beat.notes[s];
        if (note.fret >= 0)
        {
            noteCount++;
            // String bits: bit 6 = string 1, bit 5 = string 2, etc.
            stringBits |= (1 << (6 - s));
        }
    }
    
    if (noteCount == 0)
        flags |= 0x40;  // Rest
    
    if (beat.isDotted)
        flags |= 0x01;  // Dotted
    
    if (beat.tupletNumerator > 1)
        flags |= 0x20;  // Tuplet
    
    // Check for beat effects
    bool hasEffects = beat.isPalmMuted || beat.isLetRing || 
                      beat.hasDownstroke || beat.hasUpstroke;
    if (hasEffects)
        flags |= 0x08;
    
    writeByte(flags);
    
    // Beat status (if rest)
    if (flags & 0x40)
        writeByte(0x02); // 0x02 = Rest status (0x00 = Empty)
    
    // Duration
    int duration = 0;
    switch (beat.duration)
    {
        case NoteDuration::Whole:        duration = -2; break;  // Whole
        case NoteDuration::Half:         duration = -1; break;  // Half
        case NoteDuration::Quarter:      duration = 0; break;   // Quarter
        case NoteDuration::Eighth:       duration = 1; break;   // Eighth
        case NoteDuration::Sixteenth:    duration = 2; break;   // 16th
        case NoteDuration::ThirtySecond: duration = 3; break;   // 32nd
        default: duration = 0; break;
    }
    writeByte((juce::int8)duration);
    
    // Tuplet
    if (flags & 0x20)
        writeInt(beat.tupletNumerator);
    
    // Beat effects
    if (flags & 0x08)
        writeBeatEffects(beat);
    
    // Notes - PyGuitarPro ALWAYS reads stringFlags, even for rests!
    // So we must always write it
    writeByte(stringBits);
    
    // Write notes (if any)
    for (int s = 0; s < stringCount && s < (int)beat.notes.size(); ++s)
    {
        if (beat.notes[s].fret >= 0)
        {
            writeNote(beat.notes[s]);
        }
    }
    
    // GP5: flags2 (short)
    writeShort(0);
}

void GP5Writer::writeBeatEffects(const TabBeat& beat)
{
    // PyGuitarPro GP4File.writeBeatEffects():
    // flags1:
    //   0x01 = vibrato
    //   0x02 = wide vibrato
    //   0x04 = natural harmonic
    //   0x08 = artificial harmonic
    //   0x10 = fade in
    //   0x20 = slap effect
    //   0x40 = stroke
    // flags2:
    //   0x01 = rasgueado
    //   0x02 = pick stroke
    //   0x04 = tremolo bar
    
    juce::uint8 flags1 = 0x00;
    juce::uint8 flags2 = 0x00;
    
    // Check for vibrato in any note
    for (const auto& note : beat.notes)
    {
        if (note.fret >= 0)
        {
            if (note.effects.vibrato)
                flags1 |= 0x01;
            if (note.effects.wideVibrato)
                flags1 |= 0x02;
        }
    }
    
    if (beat.hasDownstroke || beat.hasUpstroke)
        flags1 |= 0x40;  // Stroke
    
    writeByte(flags1);
    writeByte(flags2);
    
    // Stroke (if flags1 & 0x40)
    if (flags1 & 0x40)
    {
        if (beat.hasDownstroke)
        {
            writeByte(2);  // Down stroke speed (eighth note)
            writeByte(0);  // Up stroke speed
        }
        else
        {
            writeByte(0);  // Down
            writeByte(2);  // Up
        }
    }
}

void GP5Writer::writeNote(const TabNote& note)
{
    // PyGuitarPro GP5File.writeNote():
    // - flags byte
    // - noteType (if flags & 0x20)
    // - velocity (if flags & 0x10)
    // - fret (if flags & 0x20)
    // - fingering (if flags & 0x80)
    // - durationPercent (if flags & 0x01)
    // - flags2 byte
    // - noteEffects (if flags & 0x08)
    
    juce::uint8 flags = 0x00;
    
    // 0x20 = has note type and fret
    flags |= 0x20;
    
    // 0x10 = has velocity
    flags |= 0x10;
    
    // 0x08 = has note effects
    bool hasEffects = note.effects.bend || 
                      note.effects.hammerOn || note.effects.pullOff ||
                      note.effects.letRing ||
                      note.effects.slideType != SlideType::None ||
                      note.effects.vibrato || note.effects.wideVibrato ||
                      note.effects.staccato ||
                      note.effects.harmonic != HarmonicType::None;
    if (hasEffects)
        flags |= 0x08;
    
    // 0x02 = heavy accent
    if (note.effects.heavyAccentuatedNote)
        flags |= 0x02;
    
    // 0x04 = ghost note
    if (note.effects.ghostNote)
        flags |= 0x04;
    
    writeByte(flags);
    
    // Note type (if flags & 0x20)
    if (flags & 0x20)
    {
        juce::uint8 noteType = 1;  // Normal
        if (note.isTied)
            noteType = 2;
        if (note.effects.deadNote)
            noteType = 3;
        writeByte(noteType);
    }
    
    // Velocity (if flags & 0x10)
    if (flags & 0x10)
    {
        // PyGuitarPro packVelocity: (velocity + increment - minVel) / increment
        // where increment = 16, minVelocity = 15
        int dynamic = (note.velocity + 16 - 15) / 16;
        dynamic = juce::jlimit(1, 8, dynamic);
        writeByte((juce::int8)dynamic);
    }
    
    // Fret (if flags & 0x20)
    if (flags & 0x20)
    {
        int fret = note.isTied ? 0 : note.fret;
        writeByte((juce::int8)fret);
    }
    
    // Flags2
    writeByte(0);
    
    // Note effects (if flags & 0x08)
    if (flags & 0x08)
    {
        writeNoteEffects(note.effects);
    }
}

void GP5Writer::writeNoteEffects(const NoteEffects& effects)
{
    // PyGuitarPro GP4File.writeNoteEffects():
    // flags1:
    //   0x01 = bend
    //   0x02 = hammer-on
    //   0x08 = let ring
    //   0x10 = grace note
    // flags2:
    //   0x01 = staccato
    //   0x02 = palm mute
    //   0x04 = tremolo picking
    //   0x08 = slide
    //   0x10 = harmonic
    //   0x20 = trill
    //   0x40 = vibrato
    
    juce::uint8 flags1 = 0x00;
    juce::uint8 flags2 = 0x00;
    
    if (effects.bend)
        flags1 |= 0x01;
    if (effects.hammerOn || effects.pullOff)
        flags1 |= 0x02;
    if (effects.letRing)
        flags1 |= 0x08;
    
    if (effects.staccato)
        flags2 |= 0x01;
    if (effects.slideType != SlideType::None)
        flags2 |= 0x08;
    if (effects.harmonic != HarmonicType::None)
        flags2 |= 0x10;
    if (effects.vibrato || effects.wideVibrato)
        flags2 |= 0x40;
    
    writeByte(flags1);
    writeByte(flags2);
    
    // Bend
    if (flags1 & 0x01)
        writeBend(effects);
    
    // Slide
    if (flags2 & 0x08)
    {
        juce::uint8 slideType = 0;
        switch (effects.slideType)
        {
            case SlideType::ShiftSlide:        slideType = 0x01; break;
            case SlideType::LegatoSlide:       slideType = 0x02; break;
            case SlideType::SlideOutDownwards: slideType = 0x04; break;
            case SlideType::SlideOutUpwards:   slideType = 0x08; break;
            case SlideType::SlideIntoFromBelow:slideType = 0x10; break;
            case SlideType::SlideIntoFromAbove:slideType = 0x20; break;
            default: slideType = 0x01; break;
        }
        writeByte(slideType);
    }
    
    // Harmonic
    if (flags2 & 0x10)
    {
        juce::uint8 harmonicType = 1;
        switch (effects.harmonic)
        {
            case HarmonicType::Natural:    harmonicType = 1; break;
            case HarmonicType::Artificial: harmonicType = 2; break;
            case HarmonicType::Tapped:     harmonicType = 3; break;
            case HarmonicType::Pinch:      harmonicType = 4; break;
            case HarmonicType::Semi:       harmonicType = 5; break;
            default: harmonicType = 1; break;
        }
        writeByte(harmonicType);
    }
}

void GP5Writer::writeBend(const NoteEffects& effects)
{
    // PyGuitarPro GP3File.writeBend():
    // writeI8(type) + writeI32(value) + writeI32(pointCount)
    // For each point: writeI32(position) + writeI32(value) + writeBool(vibrato)
    
    juce::int8 bendType = (juce::int8)effects.bendType;
    if (bendType == 0)
        bendType = 1;  // Default to simple bend
    writeByte(bendType);
    
    // Value in 1/100 semitones
    int bendValue = (int)(effects.bendValue * 100.0f);
    writeInt(bendValue);
    
    // Check if we have detailed bend points available
    if (!effects.bendPoints.empty())
    {
        writeInt((int)effects.bendPoints.size());
        for (const auto& bp : effects.bendPoints)
        {
            writeInt(bp.position);
            writeInt(bp.value);
            writeBool(bp.vibrato != 0);
        }
        return; // Done
    }
    
    // Bend points (synthetic fallback)
    int numPoints = 2;
    if (effects.bendType == 2)  // Bend + release
        numPoints = 3;
    if (effects.bendType == 4)  // Pre-bend
        numPoints = 1;
    
    writeInt(numPoints);
    
    // GP bend value (in 1/50 semitones = semitone * 25)
    int gpValue = (int)(effects.bendValue * GP5Constants::BEND_SEMITONE);
    
    if (effects.bendType == 4)  // Pre-bend
    {
        writeInt(0);         // Position
        writeInt(gpValue);   // Value
        writeBool(false);    // Vibrato
    }
    else if (effects.bendType == 2)  // Bend + release
    {
        writeInt(0);         // Start
        writeInt(0);
        writeBool(false);
        
        writeInt(GP5Constants::BEND_POSITION / 2);  // Middle
        writeInt(gpValue);
        writeBool(false);
        
        writeInt(GP5Constants::BEND_POSITION);  // End
        writeInt(0);
        writeBool(false);
    }
    else  // Simple bend
    {
        writeInt(0);         // Start
        writeInt(0);
        writeBool(false);
        
        writeInt(GP5Constants::BEND_POSITION);  // End
        writeInt(gpValue);
        writeBool(false);
    }
}

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
    // IntByteSizeString: writeI32(length+1) + writeU8(length) + bytes
    int strLen = str.length();
    writeInt(strLen + 1);
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
    writeByte(0);  // Padding
}

void GP5Writer::writeBool(bool value)
{
    writeByte(value ? 1 : 0);
}
