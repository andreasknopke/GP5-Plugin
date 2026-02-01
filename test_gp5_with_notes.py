#!/usr/bin/env python3
"""
Test GP5 file structure by creating a file with multiple measures and notes.
This simulates what the C++ GP5Writer should produce.
"""

import struct
import os
import guitarpro

def write_byte(f, val):
    f.write(struct.pack('<B', val & 0xFF))

def write_short(f, val):
    f.write(struct.pack('<h', val))

def write_int(f, val):
    f.write(struct.pack('<i', val))

def write_bool(f, val):
    write_byte(f, 1 if val else 0)

def write_byte_size_string(f, s, max_len):
    """Write 1 byte length + max_len bytes (padded with zeros)"""
    write_byte(f, len(s))
    for i in range(max_len):
        if i < len(s):
            write_byte(f, ord(s[i]))
        else:
            write_byte(f, 0)

def write_int_byte_size_string(f, s):
    """Write int(len+1) + byte(len) + string bytes"""
    write_int(f, len(s) + 1)
    write_byte(f, len(s))
    for c in s:
        write_byte(f, ord(c))

def write_int_size_string(f, s):
    """Write int(len) + string bytes"""
    write_int(f, len(s))
    for c in s:
        write_byte(f, ord(c))

def create_gp5_with_notes(filename, num_measures=4):
    """Create a GP5 file with actual notes."""
    with open(filename, 'wb') as f:
        # ===== VERSION =====
        write_byte_size_string(f, "FICHIER GUITAR PRO v5.00", 30)
        
        # ===== SONG INFO =====
        write_int_byte_size_string(f, "Test Song with Notes")  # title
        write_int_byte_size_string(f, "")  # subtitle
        write_int_byte_size_string(f, "Test Artist")  # artist
        write_int_byte_size_string(f, "")  # album
        write_int_byte_size_string(f, "")  # words
        write_int_byte_size_string(f, "")  # music
        write_int_byte_size_string(f, "")  # copyright
        write_int_byte_size_string(f, "GP5 Writer Test")  # tab
        write_int_byte_size_string(f, "")  # instructions
        write_int(f, 0)  # notice count
        
        # ===== LYRICS =====
        write_int(f, 0)  # track choice
        for _ in range(5):
            write_int(f, 0)  # starting measure
            write_int(f, 0)  # empty string (int size)
        
        # ===== PAGE SETUP =====
        write_int(f, 210)  # width
        write_int(f, 297)  # height
        write_int(f, 10)   # margin left
        write_int(f, 10)   # margin right
        write_int(f, 15)   # margin top
        write_int(f, 10)   # margin bottom
        write_int(f, 100)  # score size proportion
        write_byte(f, 0xFF)  # flags1
        write_byte(f, 0x01)  # flags2
        
        # 10 header/footer strings
        write_int_byte_size_string(f, "%TITLE%")
        write_int_byte_size_string(f, "%SUBTITLE%")
        write_int_byte_size_string(f, "%ARTIST%")
        write_int_byte_size_string(f, "%ALBUM%")
        write_int_byte_size_string(f, "Words by %WORDS%")
        write_int_byte_size_string(f, "Music by %MUSIC%")
        write_int_byte_size_string(f, "Words & Music by %WORDSMUSIC%")
        write_int_byte_size_string(f, "Copyright %COPYRIGHT%")
        write_int_byte_size_string(f, "All Rights Reserved - International Copyright Secured")
        write_int_byte_size_string(f, "Page %N%/%P%")
        
        # ===== TEMPO =====
        write_int_byte_size_string(f, "")  # tempo name
        write_int(f, 120)  # tempo
        
        # ===== KEY =====
        write_byte(f, 0)  # key (signed byte)
        write_int(f, 0)   # octave
        
        # ===== MIDI CHANNELS =====
        for port in range(4):
            for channel in range(16):
                write_int(f, 25 if channel != 9 else 0)  # program
                write_byte(f, 13)  # volume
                write_byte(f, 8)   # balance
                write_byte(f, 0)   # chorus
                write_byte(f, 0)   # reverb
                write_byte(f, 0)   # phaser
                write_byte(f, 0)   # tremolo
                write_byte(f, 0)   # blank1
                write_byte(f, 0)   # blank2
        
        # ===== DIRECTIONS =====
        for _ in range(19):
            write_short(f, -1)
        
        # ===== MASTER REVERB =====
        write_int(f, 0)
        
        # ===== MEASURE/TRACK COUNTS =====
        write_int(f, num_measures)  # measures
        write_int(f, 1)  # 1 track
        
        # ===== MEASURE HEADERS =====
        for m in range(num_measures):
            if m > 0:
                write_byte(f, 0)  # placeholder before non-first measures
            
            if m == 0:
                flags = 0x03  # time signature
            else:
                flags = 0x00  # no changes
            
            write_byte(f, flags)
            
            if flags & 0x01:
                write_byte(f, 4)  # numerator
            if flags & 0x02:
                write_byte(f, 4)  # denominator
            
            # Beams (if time sig changed)
            if flags & 0x03:
                write_byte(f, 2)
                write_byte(f, 2)
                write_byte(f, 2)
                write_byte(f, 2)
            
            # Placeholder if no repeat alternative
            if (flags & 0x10) == 0:
                write_byte(f, 0)
            
            # Triplet feel
            write_byte(f, 0)
        
        # ===== TRACKS =====
        # Placeholder for first track (GP5.0.0)
        write_byte(f, 0)
        
        # Track flags1
        write_byte(f, 0x08)  # isVisible
        
        # Track name
        write_byte_size_string(f, "Track 1", 40)
        
        # Number of strings
        write_int(f, 6)
        
        # 7 string tunings
        tuning = [64, 59, 55, 50, 45, 40, 0]
        for t in tuning:
            write_int(f, t)
        
        # Port, Channel, Effect channel
        write_int(f, 1)
        write_int(f, 1)
        write_int(f, 2)
        
        # Fret count, Offset
        write_int(f, 24)
        write_int(f, 0)
        
        # Color
        write_byte(f, 255)
        write_byte(f, 0)
        write_byte(f, 0)
        write_byte(f, 0)
        
        # Flags2 (short)
        write_short(f, 0x0003)
        
        # Auto accentuation, Bank
        write_byte(f, 0)
        write_byte(f, 0)
        
        # Track RSE
        write_byte(f, 0)    # humanize
        write_int(f, 0)     # unknown
        write_int(f, 0)     # unknown
        write_int(f, 100)   # unknown
        
        # 12 placeholder bytes
        for _ in range(12):
            write_byte(f, 0)
        
        # RSE Instrument
        write_int(f, -1)    # instrument
        write_int(f, 0)     # unknown
        write_int(f, 0)     # soundBank
        
        # GP5.0.0: effectNumber (short) + placeholder(1)
        write_short(f, 0)
        write_byte(f, 0)
        
        # Placeholder after all tracks (2 bytes for GP5.0.0)
        write_byte(f, 0)
        write_byte(f, 0)
        
        # ===== MEASURES =====
        for m in range(num_measures):
            # Voice 1: 4 beats (quarter notes with different frets)
            write_int(f, 4)  # beat count
            
            for beat_num in range(4):
                # Beat flags
                flags = 0x00  # normal beat (not rest)
                write_byte(f, flags)
                
                # Duration (quarter note = 0)
                write_byte(f, 0)
                
                # Notes (string bits: 6 strings, bit 6 = string 1, etc.)
                # Let's play one note per beat, cycling through strings
                string_idx = beat_num % 6
                string_bits = 1 << (6 - string_idx)
                write_byte(f, string_bits)
                
                # Note
                note_flags = 0x20 | 0x10  # has type/fret + has velocity
                write_byte(f, note_flags)
                
                # Note type
                write_byte(f, 1)  # normal
                
                # Velocity (dynamic)
                write_byte(f, 6)  # mezzoforte
                
                # Fret
                fret = (m * 4 + beat_num) % 12 + 1  # frets 1-12
                write_byte(f, fret)
                
                # flags2 (byte) for note
                write_byte(f, 0)
                
                # flags2 (short) for beat - GP5 specific
                write_short(f, 0)
            
            # Voice 2: empty
            write_int(f, 0)
            
            # Line break
            write_byte(f, 0)
    
    return os.path.getsize(filename)

def verify_file(filename):
    try:
        song = guitarpro.parse(filename)
        print(f"SUCCESS: PyGuitarPro can read the file!")
        print(f"  Title: {song.title}")
        print(f"  Artist: {song.artist}")
        print(f"  Tracks: {len(song.tracks)}")
        print(f"  Measures: {len(song.tracks[0].measures)}")
        
        # Print some notes
        for m_idx, measure in enumerate(song.tracks[0].measures[:2]):
            print(f"  Measure {m_idx + 1}:")
            for beat in measure.voices[0].beats[:4]:
                notes = [f"s{n.string}f{n.value}" for n in beat.notes]
                print(f"    Beat: {notes}")
        
        return True
    except Exception as e:
        print(f"FAILED: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    test_file = "D:/GitHub/NewProject/test_with_notes.gp5"
    size = create_gp5_with_notes(test_file, num_measures=4)
    print(f"Created: {test_file} ({size} bytes)")
    verify_file(test_file)
