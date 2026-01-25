// GP5 File Analyzer - Standalone tool for debugging GP5 parsing
// Compile with: cl /EHsc GP5Dump.cpp

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

class GP5Dump {
    std::vector<uint8_t> data;
    size_t pos = 0;
    int versionMajor = 5;
    int versionMinor = 0;

public:
    bool load(const char* filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        std::cout << "File size: " << size << " bytes\n";
        return true;
    }
    
    uint8_t readU8() { return data[pos++]; }
    int8_t readI8() { return static_cast<int8_t>(data[pos++]); }
    
    int16_t readI16() {
        int16_t v = data[pos] | (data[pos+1] << 8);
        pos += 2;
        return v;
    }
    
    int32_t readI32() {
        int32_t v = data[pos] | (data[pos+1] << 8) | (data[pos+2] << 16) | (data[pos+3] << 24);
        pos += 4;
        return v;
    }
    
    double readF64() {
        double v;
        memcpy(&v, &data[pos], 8);
        pos += 8;
        return v;
    }
    
    void skip(size_t count) { pos += count; }
    
    std::string readByteSizeString(int maxLen) {
        int len = readU8();
        std::string s(reinterpret_cast<char*>(&data[pos]), std::min(len, maxLen));
        skip(maxLen);
        return s;
    }
    
    std::string readIntByteSizeString() {
        int totalSize = readI32();
        if (totalSize <= 0) return "";
        int actualLen = readU8();
        std::string s(reinterpret_cast<char*>(&data[pos]), actualLen);
        skip(totalSize - 1);
        return s;
    }
    
    std::string readIntSizeString() {
        int len = readI32();
        if (len <= 0) return "";
        std::string s(reinterpret_cast<char*>(&data[pos]), len);
        pos += len;
        return s;
    }
    
    void readVersion() {
        std::string version = readByteSizeString(30);
        std::cout << "Version: " << version << "\n";
        if (version.find("5.00") != std::string::npos) {
            versionMinor = 0;
        } else if (version.find("5.10") != std::string::npos) {
            versionMinor = 10;
        }
        std::cout << "Parsed as: GP5." << versionMinor << "\n";
    }
    
    void readInfo() {
        std::string title = readIntByteSizeString();
        std::string subtitle = readIntByteSizeString();
        std::string artist = readIntByteSizeString();
        std::string album = readIntByteSizeString();
        std::string words = readIntByteSizeString();
        std::string music = readIntByteSizeString();
        std::string copyright = readIntByteSizeString();
        std::string tab = readIntByteSizeString();
        std::string instructions = readIntByteSizeString();
        
        int noticeCount = readI32();
        for (int i = 0; i < noticeCount; i++) {
            readIntByteSizeString();
        }
        
        std::cout << "Title: " << title << "\n";
        std::cout << "Artist: " << artist << "\n";
    }
    
    void readLyrics() {
        int track = readI32();
        for (int i = 0; i < 5; i++) {
            readI32();  // from bar
            readIntSizeString();  // lyrics
        }
        std::cout << "Lyrics track: " << track << "\n";
    }
    
    void readRSEMasterEffect() {
        if (versionMinor > 0) {
            readI32();  // volume
            readI32();  // unknown
            for (int i = 0; i < 11; i++) readI8();  // equalizer
            std::cout << "Read RSE master effect (GP5.1+)\n";
        } else {
            std::cout << "Skipped RSE master effect (GP5.0)\n";
        }
    }
    
    void readPageSetup() {
        int width = readI32();
        int height = readI32();
        std::cout << "Page size: " << width << "x" << height << "\n";
        
        // Margins
        readI32(); readI32(); readI32(); readI32();
        // Score size
        readI32();
        // Flags
        readI16();
        
        // 10 strings
        for (int i = 0; i < 10; i++) {
            readIntByteSizeString();
        }
    }
    
    void readDirections() {
        for (int i = 0; i < 19; i++) {
            readI16();
        }
    }
    
    void readMidiChannels() {
        for (int i = 0; i < 64; i++) {
            readI32();  // instrument
            readU8(); readU8();  // volume, balance
            readU8(); readU8();  // chorus, reverb
            readU8(); readU8();  // phaser, tremolo
            skip(2);
        }
    }
    
    void readMeasureHeaders(int count) {
        std::cout << "\n=== Reading " << count << " measure headers ===\n";
        for (int i = 0; i < count; i++) {
            if (i > 0) skip(1);
            
            uint8_t flags = readU8();
            
            if (flags & 0x01) readU8();  // numerator
            if (flags & 0x02) readU8();  // denominator
            if (flags & 0x08) readU8();  // repeat close
            if (flags & 0x20) {
                readIntByteSizeString();  // marker
                readI32();  // color
            }
            if (flags & 0x40) {
                readI8(); readI8();  // key
            }
            if (flags & 0x10) readU8();  // repeat alternative
            if (flags & 0x03) {
                skip(4);  // time sig beams
            }
            if ((flags & 0x10) == 0) skip(1);
            readU8();  // triplet feel
            
            if (i < 10) {
                std::cout << "  Header " << (i+1) << ": flags=0x" << std::hex << (int)flags << std::dec 
                          << ", pos=" << pos << "\n";
            }
        }
    }
    
    void readRSEInstrument() {
        readI32();  // instrument
        readI32();  // unknown
        readI32();  // soundBank
        if (versionMinor == 0) {
            readI16();  // effectNumber
            skip(1);
        } else {
            readI32();  // effectNumber
        }
    }
    
    void readTrackRSE() {
        readU8();  // humanize
        readI32(); readI32(); readI32();  // 3 unknowns
        skip(12);  // more unknown
        readRSEInstrument();
        
        if (versionMinor > 0) {
            for (int i = 0; i < 4; i++) readI8();  // equalizer
            readIntByteSizeString();  // effect
            readIntByteSizeString();  // effect category
        }
    }
    
    void readTracks(int count, int measureCount) {
        std::cout << "\n=== Reading " << count << " tracks ===\n";
        
        for (int i = 0; i < count; i++) {
            // GP5.0: skip 1 for every track
            // GP5.1+: skip 1 only for first track
            if (i == 0 || versionMinor == 0) {
                skip(1);
            }
            
            uint8_t flags1 = readU8();
            std::string name = readByteSizeString(40);
            int stringCount = readI32();
            
            // 7 tuning ints
            for (int s = 0; s < 7; s++) readI32();
            
            readI32();  // port
            readI32();  // channel
            readI32();  // effect channel
            readI32();  // fret count
            readI32();  // capo
            
            // Color
            skip(4);
            
            // Flags2
            readI16();
            
            // Auto accentuation
            readU8();
            
            // Bank
            readU8();
            
            // Track RSE
            readTrackRSE();
            
            std::cout << "  Track " << (i+1) << ": " << name << " (" << stringCount << " strings), pos=" << pos << "\n";
        }
        
        // Skip after tracks
        if (versionMinor == 0) {
            skip(2);
        } else {
            skip(1);
        }
    }
    
    void readNote(int stringNum) {
        size_t notePos = pos;
        uint8_t flags = readU8();
        
        int noteType = 0;
        int fret = 0;
        
        if (flags & 0x20) {
            noteType = readU8();
        }
        if (flags & 0x10) {
            readI8();  // velocity
        }
        if (flags & 0x20) {
            fret = readI8();
        }
        if (flags & 0x80) {
            readI8(); readI8();  // fingering
        }
        if (flags & 0x01) {
            readF64();  // duration percent
        }
        
        // flags2 always
        uint8_t flags2 = readU8();
        
        // Note effects (0x08)
        if (flags & 0x08) {
            // Read note effects
            uint8_t ef1 = readU8();
            uint8_t ef2 = readU8();
            
            if (ef1 & 0x01) {
                // Bend
                readU8();  // type
                readI32();  // value
                int points = readI32();
                skip(points * 6);
            }
            if (ef1 & 0x10) {
                // Grace
                readU8();  // fret
                readU8();  // velocity
                readU8();  // transition
                readU8();  // duration
                readU8();  // flags
            }
            if (ef2 & 0x04) {
                // Tremolo picking
                readU8();
            }
            if (ef2 & 0x08) {
                // Slide
                readI8();
            }
            if (ef2 & 0x10) {
                // Harmonic
                readU8();  // type
                // May have more data depending on type
            }
            if (ef2 & 0x20) {
                // Trill
                readU8();  // fret
                readU8();  // period
            }
        }
        
        std::cout << "      Note: str=" << stringNum << " fret=" << fret 
                  << " flags=0x" << std::hex << (int)flags << std::dec 
                  << " pos=" << notePos << "\n";
    }
    
    int readBeat(int measureNum, int trackNum, int voiceNum, int beatNum) {
        size_t beatPos = pos;
        uint8_t flags = readU8();
        
        int status = 0;
        if (flags & 0x40) {
            status = readU8();
            if (status == 0) {
                // Empty beat
                std::cout << "    Beat " << beatNum << ": EMPTY, pos=" << beatPos << "\n";
                return 0;
            }
        }
        
        int8_t duration = readI8();
        
        if (flags & 0x20) {
            readI32();  // tuplet
        }
        
        if (flags & 0x02) {
            // Chord diagram - skip it
            bool newFormat = (readU8() != 0);
            if (newFormat) {
                skip(16);  // sharp/blank/root/type/extension/bass/dim7/add/name?
                std::string chordName = readByteSizeString(21);
                skip(4);  // blank
                readI32();  // fifth
                readI32();  // ninth
                readI32();  // eleventh
                readI32();  // first fret
                for (int s = 0; s < 7; s++) readI32();  // frets
                readU8();  // barre count
                // Could have more for barres
                skip(5);
                for (int f = 0; f < 7; f++) readI8();  // fingering
                readU8();  // display fingering
            } else {
                // Old format
                skip(25);  // header
                std::string name = readByteSizeString(34);
                readI32();  // first fret
                for (int s = 0; s < 6; s++) readI32();  // frets
                skip(36);  // more data
            }
        }
        
        if (flags & 0x04) {
            readIntByteSizeString();  // text
        }
        
        if (flags & 0x08) {
            // Beat effects
            uint8_t be1 = readU8();
            if (be1 & 0x20) {
                uint8_t be2 = readU8();
                if (be2 == 0) {
                    // Tremolo bar
                    readU8();  // type
                    readI32();  // value
                    int points = readI32();
                    skip(points * 6);
                } else {
                    readI32();  // slap effect value
                }
            }
            if (be1 & 0x40) {
                readU8(); readU8();  // stroke
            }
            if (be1 & 0x04) {
                // Natural harmonic
                readU8();
            }
            if (be1 & 0x02) {
                // Pickstroke
                readU8();
            }
        }
        
        if (flags & 0x10) {
            // Mix table
            int8_t instrument = readI8();
            // RSE instrument
            readRSEInstrument();
            if (versionMinor == 0) skip(1);
            
            int8_t volume = readI8();
            int8_t balance = readI8();
            int8_t chorus = readI8();
            int8_t reverb = readI8();
            int8_t phaser = readI8();
            int8_t tremolo = readI8();
            std::string tempoName = readIntByteSizeString();
            int32_t tempo = readI32();
            
            if (volume >= 0) readU8();
            if (balance >= 0) readU8();
            if (chorus >= 0) readU8();
            if (reverb >= 0) readU8();
            if (phaser >= 0) readU8();
            if (tremolo >= 0) readU8();
            if (tempo >= 0) {
                readU8();
                if (versionMinor > 0) readU8();
            }
            
            readU8();  // flags
            
            if (versionMinor > 0) {
                readIntByteSizeString();  // wah effect
                readIntByteSizeString();  // wah category
            }
        }
        
        uint8_t stringFlags = readU8();
        int noteCount = 0;
        
        for (int s = 6; s >= 0; s--) {
            if (stringFlags & (1 << s)) {
                int stringNum = 6 - s + 1;
                readNote(stringNum);
                noteCount++;
            }
        }
        
        // flags2
        int16_t flags2 = readI16();
        if (flags2 & 0x0800) {
            readU8();  // break secondary
        }
        
        std::cout << "    Beat " << beatNum << ": dur=" << (int)duration 
                  << " notes=" << noteCount << " flags=0x" << std::hex << (int)flags 
                  << " strFlags=0x" << (int)stringFlags << std::dec 
                  << " pos=" << beatPos << "\n";
        
        return (status != 2) ? 1 : 0;  // non-empty
    }
    
    void readVoice(int measureNum, int trackNum, int voiceNum) {
        int beatCount = readI32();
        std::cout << "  Voice " << voiceNum << ": " << beatCount << " beats\n";
        
        for (int b = 0; b < beatCount; b++) {
            readBeat(measureNum, trackNum, voiceNum, b + 1);
        }
    }
    
    void readMeasures(int measureCount, int trackCount) {
        std::cout << "\n=== Reading measures ===\n";
        
        for (int m = 0; m < measureCount; m++) {
            if (m >= 5 && m <= 8) {  // Focus on measures 6-9
                std::cout << "\n--- Measure " << (m + 1) << " ---\n";
            }
            
            for (int t = 0; t < trackCount; t++) {
                if (m >= 5 && m <= 8 && t < 3) {
                    std::cout << " Track " << (t + 1) << ":\n";
                }
                
                // 2 voices
                for (int v = 0; v < 2; v++) {
                    if (m >= 5 && m <= 8 && t < 3) {
                        readVoice(m + 1, t + 1, v + 1);
                    } else {
                        // Skip this voice quietly
                        int beatCount = readI32();
                        for (int b = 0; b < beatCount; b++) {
                            size_t savedPos = pos;
                            try {
                                readBeatQuiet();
                            } catch (...) {
                                std::cerr << "Error at measure " << (m+1) 
                                          << ", track " << (t+1) 
                                          << ", voice " << (v+1) 
                                          << ", beat " << (b+1) 
                                          << ", pos=" << savedPos << "\n";
                                throw;
                            }
                        }
                    }
                }
                
                // Line break byte
                readU8();
            }
        }
    }
    
    void readBeatQuiet() {
        uint8_t flags = readU8();
        
        if (flags & 0x40) {
            if (readU8() == 0) return;  // empty
        }
        
        readI8();  // duration
        
        if (flags & 0x20) readI32();  // tuplet
        
        if (flags & 0x02) {
            // Chord
            if (readU8() != 0) {
                skip(16);
                readByteSizeString(21);
                skip(4);
                readI32(); readI32(); readI32(); readI32();
                for (int s = 0; s < 7; s++) readI32();
                int barreCount = readU8();
                skip(5 + barreCount * 5);
                for (int f = 0; f < 7; f++) readI8();
                readU8();
            } else {
                skip(25);
                readByteSizeString(34);
                readI32();
                for (int s = 0; s < 6; s++) readI32();
                skip(36);
            }
        }
        
        if (flags & 0x04) readIntByteSizeString();
        
        if (flags & 0x08) {
            uint8_t be1 = readU8();
            if (be1 & 0x20) {
                uint8_t be2 = readU8();
                if (be2 == 0) {
                    readU8();
                    readI32();
                    int pts = readI32();
                    skip(pts * 6);
                } else {
                    readI32();
                }
            }
            if (be1 & 0x40) { readU8(); readU8(); }
            if (be1 & 0x04) readU8();
            if (be1 & 0x02) readU8();
        }
        
        if (flags & 0x10) {
            readI8();
            readRSEInstrument();
            if (versionMinor == 0) skip(1);
            
            int8_t vol = readI8();
            int8_t bal = readI8();
            int8_t cho = readI8();
            int8_t rev = readI8();
            int8_t pha = readI8();
            int8_t tre = readI8();
            readIntByteSizeString();
            int32_t tempo = readI32();
            
            if (vol >= 0) readU8();
            if (bal >= 0) readU8();
            if (cho >= 0) readU8();
            if (rev >= 0) readU8();
            if (pha >= 0) readU8();
            if (tre >= 0) readU8();
            if (tempo >= 0) {
                readU8();
                if (versionMinor > 0) readU8();
            }
            
            readU8();
            
            if (versionMinor > 0) {
                readIntByteSizeString();
                readIntByteSizeString();
            }
        }
        
        uint8_t strFlags = readU8();
        for (int s = 6; s >= 0; s--) {
            if (strFlags & (1 << s)) {
                readNoteQuiet();
            }
        }
        
        int16_t f2 = readI16();
        if (f2 & 0x0800) readU8();
    }
    
    void readNoteQuiet() {
        uint8_t flags = readU8();
        
        if (flags & 0x20) readU8();
        if (flags & 0x10) readI8();
        if (flags & 0x20) readI8();
        if (flags & 0x80) { readI8(); readI8(); }
        if (flags & 0x01) readF64();
        
        readU8();  // flags2
        
        if (flags & 0x08) {
            uint8_t ef1 = readU8();
            uint8_t ef2 = readU8();
            
            if (ef1 & 0x01) {
                readU8();
                readI32();
                int pts = readI32();
                skip(pts * 6);
            }
            if (ef1 & 0x10) skip(5);
            if (ef2 & 0x04) readU8();
            if (ef2 & 0x08) readI8();
            if (ef2 & 0x10) {
                uint8_t ht = readU8();
                if (ht == 2) skip(3);  // artificial
                else if (ht == 3) readU8();  // tapped
            }
            if (ef2 & 0x20) { readU8(); readU8(); }
        }
    }
    
    void analyze() {
        readVersion();
        readInfo();
        readLyrics();
        readRSEMasterEffect();
        readPageSetup();
        
        std::string tempoName = readIntByteSizeString();
        int tempo = readI32();
        std::cout << "Tempo: " << tempo << " (" << tempoName << ")\n";
        
        if (versionMinor > 0) {
            readU8();  // hide tempo
        }
        
        readI8();  // key
        readI32();  // octave
        
        readMidiChannels();
        readDirections();
        
        int reverb = readI32();
        int measureCount = readI32();
        int trackCount = readI32();
        
        std::cout << "Measures: " << measureCount << ", Tracks: " << trackCount << "\n";
        
        readMeasureHeaders(measureCount);
        readTracks(trackCount, measureCount);
        
        std::cout << "\n=== Position after tracks: " << pos << " ===\n";
        
        readMeasures(measureCount, trackCount);
        
        std::cout << "\n=== Analysis complete. Final pos: " << pos << " ===\n";
    }
};

int main(int argc, char* argv[]) {
    const char* filename = "D:\\GitHub\\NewProject\\test_partial.gp5";
    
    GP5Dump dump;
    if (!dump.load(filename)) {
        std::cerr << "Failed to load: " << filename << "\n";
        return 1;
    }
    
    try {
        dump.analyze();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
