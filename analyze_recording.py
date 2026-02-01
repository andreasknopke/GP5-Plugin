#!/usr/bin/env python3
"""
Complete GP5 file analyzer - analyzes Recording.gp5 including measures.
"""

import struct
import os

def read_byte(f):
    data = f.read(1)
    if len(data) == 0:
        raise EOFError("Unexpected end of file")
    return struct.unpack('<B', data)[0]

def read_signed_byte(f):
    return struct.unpack('<b', f.read(1))[0]

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

def analyze_full_gp5(filename):
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
        read_int_byte_size_string(f)  # subtitle
        read_int_byte_size_string(f)  # artist
        read_int_byte_size_string(f)  # album
        read_int_byte_size_string(f)  # words
        read_int_byte_size_string(f)  # music
        read_int_byte_size_string(f)  # copyright
        read_int_byte_size_string(f)  # tab
        read_int_byte_size_string(f)  # instructions
        notice_count = read_int(f)
        for i in range(notice_count):
            read_int_byte_size_string(f)
        
        # LYRICS
        print(f"[{f.tell():5d}] LYRICS")
        read_int(f)  # track choice
        for _ in range(5):
            read_int(f)  # starting measure
            lyrics_len = read_int(f)
            if lyrics_len > 0:
                f.read(lyrics_len)
        
        # PAGE SETUP
        print(f"[{f.tell():5d}] PAGE SETUP")
        for _ in range(7):
            read_int(f)
        read_byte(f)  # flags1
        read_byte(f)  # flags2
        for _ in range(10):
            read_int_byte_size_string(f)
        
        # TEMPO
        print(f"[{f.tell():5d}] TEMPO")
        read_int_byte_size_string(f)  # tempo name
        tempo = read_int(f)
        print(f"         Tempo: {tempo}")
        
        # KEY
        read_signed_byte(f)  # key
        read_int(f)  # octave
        
        # MIDI CHANNELS
        print(f"[{f.tell():5d}] MIDI CHANNELS")
        for _ in range(64):
            read_int(f)  # program
            for _ in range(8):
                read_byte(f)
        
        # DIRECTIONS
        print(f"[{f.tell():5d}] DIRECTIONS")
        for _ in range(19):
            read_short(f)
        
        # MASTER REVERB
        print(f"[{f.tell():5d}] MASTER REVERB")
        read_int(f)
        
        # COUNTS
        print(f"[{f.tell():5d}] COUNTS")
        measure_count = read_int(f)
        track_count = read_int(f)
        print(f"         Measures: {measure_count}, Tracks: {track_count}")
        
        # MEASURE HEADERS
        print(f"[{f.tell():5d}] MEASURE HEADERS")
        for m in range(measure_count):
            if m > 0:
                read_byte(f)  # placeholder
            
            flags = read_byte(f)
            
            if flags & 0x01:
                read_byte(f)  # numerator
            if flags & 0x02:
                read_byte(f)  # denominator
            if flags & 0x08:
                read_byte(f)  # repeat close
            if flags & 0x20:
                read_int_byte_size_string(f)  # marker title
                for _ in range(4):
                    read_byte(f)  # color
            if flags & 0x40:
                read_signed_byte(f)  # key root
                read_signed_byte(f)  # key type
            if flags & 0x10:
                read_byte(f)  # repeat alt
            if flags & 0x03:
                for _ in range(4):
                    read_byte(f)  # beams
            if (flags & 0x10) == 0:
                read_byte(f)  # placeholder
            read_byte(f)  # triplet feel
        
        # TRACKS
        print(f"[{f.tell():5d}] TRACKS")
        for t in range(track_count):
            track_start = f.tell()
            
            # Placeholder for first track or GP5.0.0
            read_byte(f)
            
            # flags1
            flags1 = read_byte(f)
            
            # name (1 + 40 bytes)
            name = read_byte_size_string(f, 40)
            print(f"         Track {t+1}: '{name}' flags1={flags1:02x}")
            
            # strings
            num_strings = read_int(f)
            print(f"                  Strings: {num_strings}")
            for _ in range(7):
                read_int(f)
            
            # port, channel, effect channel
            read_int(f)
            read_int(f)
            read_int(f)
            
            # fret count, offset
            read_int(f)
            read_int(f)
            
            # color
            for _ in range(4):
                read_byte(f)
            
            # flags2 (short)
            read_short(f)
            
            # auto accentuation, bank
            read_byte(f)
            read_byte(f)
            
            # Track RSE
            read_byte(f)  # humanize
            read_int(f)
            read_int(f)
            read_int(f)
            
            # 12 placeholder bytes
            for _ in range(12):
                read_byte(f)
            
            # RSE Instrument
            read_int(f)  # instrument
            read_int(f)  # unknown
            read_int(f)  # soundBank
            
            # GP5.0.0: effectNumber (short) + placeholder(1)
            read_short(f)
            read_byte(f)
            
            print(f"         Track end at: {f.tell()}")
        
        # 2 placeholder bytes after all tracks (GP5.0.0)
        p1 = read_byte(f)
        p2 = read_byte(f)
        print(f"[{f.tell():5d}] Post-track placeholders: {p1:02x} {p2:02x}")
        
        # MEASURES
        print(f"[{f.tell():5d}] MEASURES")
        
        for m in range(measure_count):
            measure_start = f.tell()
            print(f"\n  Measure {m+1} at position {measure_start}:")
            
            for t in range(track_count):
                # Voice 1
                beat_count_v1 = read_int(f)
                print(f"    Track {t+1} Voice 1: {beat_count_v1} beats")
                
                for b in range(beat_count_v1):
                    beat_start = f.tell()
                    flags = read_byte(f)
                    print(f"      Beat {b+1} at {beat_start}: flags={flags:02x}", end="")
                    
                    # beatStatus (if rest flag 0x40)
                    if flags & 0x40:
                        status = read_byte(f)
                        print(f" status={status}", end="")
                    
                    # duration
                    duration = read_signed_byte(f)
                    print(f" dur={duration}", end="")
                    
                    # tuplet
                    if flags & 0x20:
                        tuplet = read_int(f)
                        print(f" tuplet={tuplet}", end="")
                    
                    # chord (flags & 0x02)
                    if flags & 0x02:
                        print(f" CHORD!", end="")
                        # Skip chord data - complex
                        break
                    
                    # text (flags & 0x04)
                    if flags & 0x04:
                        text = read_int_byte_size_string(f)
                        print(f" text='{text}'", end="")
                    
                    # beat effects (flags & 0x08)
                    if flags & 0x08:
                        ef1 = read_byte(f)
                        ef2 = read_byte(f)
                        print(f" effects={ef1:02x},{ef2:02x}", end="")
                        if ef1 & 0x20:  # slap
                            read_byte(f)
                        if ef1 & 0x40:  # stroke
                            read_byte(f)
                            read_byte(f)
                        if ef2 & 0x04:  # tremolo bar
                            # read bend data
                            read_byte(f)
                            read_int(f)
                            num_points = read_int(f)
                            for _ in range(num_points):
                                read_int(f)
                                read_int(f)
                                read_byte(f)
                    
                    # mix table (flags & 0x10)
                    if flags & 0x10:
                        print(f" MIXTABLE!", end="")
                        # Skip - complex
                        break
                    
                    # notes (if not rest)
                    if (flags & 0x40) == 0:
                        string_bits = read_byte(f)
                        notes = []
                        for s in range(7):
                            if string_bits & (1 << (6 - s)):
                                # read note
                                nflags = read_byte(f)
                                ntype = 1
                                if nflags & 0x20:
                                    ntype = read_byte(f)
                                vel = 6
                                if nflags & 0x10:
                                    vel = read_signed_byte(f)
                                fret = 0
                                if nflags & 0x20:
                                    fret = read_signed_byte(f)
                                if nflags & 0x80:
                                    read_byte(f)  # left finger
                                    read_byte(f)  # right finger
                                if nflags & 0x01:
                                    # durationPercent (double = 8 bytes)
                                    f.read(8)
                                # flags2
                                read_byte(f)
                                # note effects
                                if nflags & 0x08:
                                    nef1 = read_byte(f)
                                    nef2 = read_byte(f)
                                    if nef1 & 0x01:  # bend
                                        read_byte(f)
                                        read_int(f)
                                        num_pts = read_int(f)
                                        for _ in range(num_pts):
                                            read_int(f)
                                            read_int(f)
                                            read_byte(f)
                                    if nef1 & 0x10:  # grace note
                                        for _ in range(5):
                                            read_byte(f)
                                    if nef2 & 0x04:  # tremolo picking
                                        read_byte(f)
                                    if nef2 & 0x08:  # slide
                                        read_byte(f)
                                    if nef2 & 0x10:  # harmonic
                                        htype = read_byte(f)
                                        if htype == 2:  # artificial
                                            read_byte(f)
                                            read_byte(f)
                                            read_byte(f)
                                        elif htype == 3:  # tapped
                                            read_byte(f)
                                    if nef2 & 0x20:  # trill
                                        read_byte(f)
                                        read_byte(f)
                                notes.append(f"s{s+1}f{fret}")
                        print(f" notes={notes}", end="")
                    
                    # flags2 (short) - GP5 beat end
                    flags2 = read_short(f)
                    print(f" flags2={flags2:04x}")
                
                # Voice 2
                beat_count_v2 = read_int(f)
                if beat_count_v2 > 0:
                    print(f"    Track {t+1} Voice 2: {beat_count_v2} beats")
                    # Would need to read all voice 2 beats too
                    for b in range(beat_count_v2):
                        # Similar to voice 1...
                        pass
            
            # linebreak
            linebreak = read_byte(f)
            print(f"    LineBreak: {linebreak}")
            
            # Safety limit
            if m >= 3:
                print(f"\n... stopping after {m+1} measures for brevity")
                break
        
        print(f"\n[{f.tell():5d}] Current position")
        print(f"Remaining bytes: {os.path.getsize(filename) - f.tell()}")

if __name__ == "__main__":
    analyze_full_gp5("D:/GitHub/NewProject/Recording.gp5")
