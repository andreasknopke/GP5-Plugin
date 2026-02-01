#!/usr/bin/env python3
"""
Test GP5 file structure by creating a minimal file matching PyGuitarPro's format.
This validates our understanding of the GP5 format.
"""

import struct
import os

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

def create_minimal_gp5(filename):
    with open(filename, 'wb') as f:
        # ===== VERSION =====
        write_byte_size_string(f, "FICHIER GUITAR PRO v5.00", 30)
        
        # ===== SONG INFO =====
        write_int_byte_size_string(f, "Test Song")  # title
        write_int_byte_size_string(f, "")  # subtitle
        write_int_byte_size_string(f, "Test Artist")  # artist
        write_int_byte_size_string(f, "")  # album
        write_int_byte_size_string(f, "")  # words
        write_int_byte_size_string(f, "")  # music
        write_int_byte_size_string(f, "")  # copyright
        write_int_byte_size_string(f, "GP5 Test")  # tab
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
        
        # 10 header/footer strings (copyright is 2 strings combined)
        write_int_byte_size_string(f, "%TITLE%")
        write_int_byte_size_string(f, "%SUBTITLE%")
        write_int_byte_size_string(f, "%ARTIST%")
        write_int_byte_size_string(f, "%ALBUM%")
        write_int_byte_size_string(f, "Words by %WORDS%")
        write_int_byte_size_string(f, "Music by %MUSIC%")
        write_int_byte_size_string(f, "Words & Music by %WORDSMUSIC%")
        write_int_byte_size_string(f, "Copyright %COPYRIGHT%")  # copyright line 1
        write_int_byte_size_string(f, "All Rights Reserved - International Copyright Secured")  # copyright line 2
        write_int_byte_size_string(f, "Page %N%/%P%")  # pageNumber
        
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
        write_int(f, 1)  # 1 measure
        write_int(f, 1)  # 1 track
        
        # ===== MEASURE HEADERS =====
        # First measure: set time signature
        # GP5 readMeasureHeader: if previous is not None: skip(1) BEFORE flags
        # So for first measure (previous=None), NO placeholder before
        flags = 0x03  # numerator + denominator
        write_byte(f, flags)
        write_byte(f, 4)  # numerator
        write_byte(f, 4)  # denominator
        # Beams (because flags & 0x03, time sig changed)
        write_byte(f, 2)
        write_byte(f, 2)
        write_byte(f, 2)
        write_byte(f, 2)
        # Placeholder if flag 0x10 not set
        write_byte(f, 0)
        # Triplet feel
        write_byte(f, 0)
        
        # ===== TRACKS =====
        # Placeholder for first track (GP5.0.0)
        write_byte(f, 0)
        
        # Track flags1
        write_byte(f, 0x08)  # isVisible
        
        # Track name (1 byte len + 40 bytes)
        write_byte_size_string(f, "Track 1", 40)
        
        # Number of strings
        write_int(f, 6)
        
        # 7 string tunings
        tuning = [64, 59, 55, 50, 45, 40, 0]
        for t in tuning:
            write_int(f, t)
        
        # Port
        write_int(f, 1)
        # Channel
        write_int(f, 1)
        # Effect channel
        write_int(f, 2)
        # Fret count
        write_int(f, 24)
        # Offset (capo)
        write_int(f, 0)
        
        # Color
        write_byte(f, 255)  # R
        write_byte(f, 0)    # G
        write_byte(f, 0)    # B
        write_byte(f, 0)    # padding
        
        # Flags2 (short)
        write_short(f, 0x0003)  # tablature + notation
        
        # Auto accentuation
        write_byte(f, 0)
        # Bank
        write_byte(f, 0)
        
        # Track RSE
        write_byte(f, 0)    # humanize
        write_int(f, 0)     # unknown
        write_int(f, 0)     # unknown
        write_int(f, 100)   # unknown (PyGuitarPro uses 100)
        
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
        # Voice 1: 1 beat (quarter note rest)
        write_int(f, 1)  # beat count
        
        # Beat
        beat_flags = 0x40  # rest
        write_byte(f, beat_flags)
        write_byte(f, 0)  # beat status
        write_byte(f, 0)  # duration (quarter)
        write_short(f, 0)  # flags2
        
        # Voice 2: empty
        write_int(f, 0)
        
        # Line break
        write_byte(f, 0)
    
    print(f"Created: {filename} ({os.path.getsize(filename)} bytes)")

def verify_with_pyguitarpro(filename):
    import guitarpro
    try:
        song = guitarpro.parse(filename)
        print(f"SUCCESS: PyGuitarPro can read the file!")
        print(f"  Title: {song.title}")
        print(f"  Artist: {song.artist}")
        print(f"  Tracks: {len(song.tracks)}")
        print(f"  Measures: {len(song.tracks[0].measures)}")
        return True
    except Exception as e:
        print(f"FAILED: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    test_file = "D:/GitHub/NewProject/test_minimal.gp5"
    create_minimal_gp5(test_file)
    verify_with_pyguitarpro(test_file)
