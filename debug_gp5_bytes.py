#!/usr/bin/env python3
"""
Debug GP5 file structure by analyzing byte positions.
"""

import struct
import os

def read_byte(f):
    return struct.unpack('<B', f.read(1))[0]

def read_short(f):
    return struct.unpack('<h', f.read(2))[0]

def read_int(f):
    return struct.unpack('<i', f.read(4))[0]

def read_byte_size_string(f, max_len):
    length = read_byte(f)
    s = f.read(max_len).decode('latin-1').rstrip('\x00')
    return s[:length]

def read_int_byte_size_string(f):
    total_len = read_int(f)
    if total_len == 0:
        return ""
    str_len = read_byte(f)
    return f.read(str_len).decode('latin-1')

def read_int_size_string(f):
    str_len = read_int(f)
    if str_len == 0:
        return ""
    return f.read(str_len).decode('latin-1')

def analyze_gp5(filename):
    with open(filename, 'rb') as f:
        print(f"=== Analyzing {filename} ===")
        print(f"File size: {os.path.getsize(filename)} bytes")
        print()
        
        # VERSION
        print(f"[{f.tell():5d}] VERSION")
        version = read_byte_size_string(f, 30)
        print(f"         Version: '{version}'")
        
        # SONG INFO
        print(f"[{f.tell():5d}] SONG INFO")
        title = read_int_byte_size_string(f)
        print(f"         Title: '{title}'")
        subtitle = read_int_byte_size_string(f)
        print(f"         Subtitle: '{subtitle}'")
        artist = read_int_byte_size_string(f)
        print(f"         Artist: '{artist}'")
        album = read_int_byte_size_string(f)
        print(f"         Album: '{album}'")
        words = read_int_byte_size_string(f)
        print(f"         Words: '{words}'")
        music = read_int_byte_size_string(f)
        print(f"         Music: '{music}'")
        copyright = read_int_byte_size_string(f)
        print(f"         Copyright: '{copyright}'")
        tab = read_int_byte_size_string(f)
        print(f"         Tab: '{tab}'")
        instructions = read_int_byte_size_string(f)
        print(f"         Instructions: '{instructions}'")
        notice_count = read_int(f)
        print(f"         Notice count: {notice_count}")
        for i in range(notice_count):
            notice = read_int_byte_size_string(f)
            print(f"         Notice[{i}]: '{notice}'")
        
        # LYRICS
        print(f"[{f.tell():5d}] LYRICS")
        track_choice = read_int(f)
        print(f"         Track choice: {track_choice}")
        for i in range(5):
            start = read_int(f)
            lyrics_len = read_int(f)
            lyrics = f.read(lyrics_len).decode('latin-1') if lyrics_len > 0 else ""
            print(f"         Line[{i}]: start={start}, len={lyrics_len}")
        
        # PAGE SETUP
        print(f"[{f.tell():5d}] PAGE SETUP")
        width = read_int(f)
        height = read_int(f)
        print(f"         Size: {width}x{height}")
        margin_left = read_int(f)
        margin_right = read_int(f)
        margin_top = read_int(f)
        margin_bottom = read_int(f)
        print(f"         Margins: L={margin_left} R={margin_right} T={margin_top} B={margin_bottom}")
        score_size = read_int(f)
        print(f"         Score size: {score_size}")
        flags1 = read_byte(f)
        flags2 = read_byte(f)
        print(f"         Flags: {flags1:02x} {flags2:02x}")
        
        # 10 header strings
        for i in range(10):
            s = read_int_byte_size_string(f)
            print(f"         Header[{i}]: '{s[:30]}...' (len={len(s)})")
        
        # TEMPO
        print(f"[{f.tell():5d}] TEMPO")
        tempo_name = read_int_byte_size_string(f)
        print(f"         Tempo name: '{tempo_name}'")
        tempo = read_int(f)
        print(f"         Tempo: {tempo}")
        
        # KEY (note: hideTempo only for GP5.1+)
        print(f"[{f.tell():5d}] KEY")
        key = struct.unpack('<b', f.read(1))[0]
        print(f"         Key: {key}")
        octave = read_int(f)
        print(f"         Octave: {octave}")
        
        # MIDI CHANNELS
        print(f"[{f.tell():5d}] MIDI CHANNELS (64 x 12 bytes = 768)")
        for port in range(4):
            for channel in range(16):
                program = read_int(f)
                volume = read_byte(f)
                balance = read_byte(f)
                chorus = read_byte(f)
                reverb = read_byte(f)
                phaser = read_byte(f)
                tremolo = read_byte(f)
                blank1 = read_byte(f)
                blank2 = read_byte(f)
        print(f"[{f.tell():5d}] (after MIDI channels)")
        
        # DIRECTIONS
        print(f"[{f.tell():5d}] DIRECTIONS (19 x 2 bytes = 38)")
        directions = [read_short(f) for _ in range(19)]
        print(f"         Directions: {directions[:5]}...")
        
        # MASTER REVERB
        print(f"[{f.tell():5d}] MASTER REVERB")
        master_reverb = read_int(f)
        print(f"         Master reverb: {master_reverb}")
        
        # MEASURE/TRACK COUNTS
        print(f"[{f.tell():5d}] COUNTS")
        measure_count = read_int(f)
        track_count = read_int(f)
        print(f"         Measures: {measure_count}, Tracks: {track_count}")
        
        # MEASURE HEADERS
        print(f"[{f.tell():5d}] MEASURE HEADERS")
        for m in range(measure_count):
            pos = f.tell()
            # For GP5: if not first measure, skip 1 byte placeholder
            if m > 0:
                placeholder = read_byte(f)
                print(f"         [{pos:5d}] Measure {m+1} placeholder: {placeholder:02x}")
            
            flags = read_byte(f)
            print(f"         [{f.tell()-1:5d}] Measure {m+1} flags: {flags:02x}")
            
            if flags & 0x01:
                numerator = read_byte(f)
                print(f"                  Numerator: {numerator}")
            if flags & 0x02:
                denominator = read_byte(f)
                print(f"                  Denominator: {denominator}")
            if flags & 0x08:
                repeat_close = read_byte(f)
                print(f"                  Repeat close: {repeat_close}")
            if flags & 0x20:
                marker_title = read_int_byte_size_string(f)
                r = read_byte(f)
                g = read_byte(f)
                b = read_byte(f)
                a = read_byte(f)
                print(f"                  Marker: '{marker_title}' RGB({r},{g},{b})")
            if flags & 0x40:
                key_root = struct.unpack('<b', f.read(1))[0]
                key_type = struct.unpack('<b', f.read(1))[0]
                print(f"                  Key sig: {key_root}/{key_type}")
            if flags & 0x10:
                repeat_alt = read_byte(f)
                print(f"                  Repeat alt: {repeat_alt}")
            
            # If time sig changed (flags & 0x03)
            if flags & 0x03:
                beams = [read_byte(f) for _ in range(4)]
                print(f"                  Beams: {beams}")
            
            # Placeholder if no repeat alt
            if (flags & 0x10) == 0:
                placeholder = read_byte(f)
                print(f"                  Placeholder: {placeholder:02x}")
            
            triplet_feel = read_byte(f)
            print(f"                  Triplet feel: {triplet_feel}")
        
        # TRACKS
        print(f"[{f.tell():5d}] TRACKS")
        # ... rest of parsing would continue here
        
        print(f"\n[{f.tell():5d}] Current position after headers")
        print(f"Remaining bytes: {os.path.getsize(filename) - f.tell()}")

if __name__ == "__main__":
    # First analyze the Recording.gp5 file
    print("="*60)
    print("RECORDING.GP5 (C++ Writer output)")
    print("="*60)
    analyze_gp5("D:/GitHub/NewProject/Recording.gp5")
