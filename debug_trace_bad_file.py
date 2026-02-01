#!/usr/bin/env python3
"""
Debug: Trace through the bad GP5 file to find the issue location.
"""

import struct

def read_file(path):
    with open(path, "rb") as f:
        return f.read()

class GP5Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0
    
    def read_byte(self):
        val = self.data[self.pos]
        self.pos += 1
        return val
    
    def read_sbyte(self):
        val = struct.unpack_from('b', self.data, self.pos)[0]
        self.pos += 1
        return val
    
    def read_bool(self):
        return self.read_byte() != 0
    
    def read_short(self):
        val = struct.unpack_from('<h', self.data, self.pos)[0]
        self.pos += 2
        return val
    
    def read_int(self):
        val = struct.unpack_from('<i', self.data, self.pos)[0]
        self.pos += 4
        return val
    
    def read_bytes(self, count):
        result = self.data[self.pos:self.pos+count]
        self.pos += count
        return result
    
    def skip(self, count):
        self.pos += count
    
    def read_byte_size_string(self, size):
        length = self.read_byte()
        s = self.read_bytes(size)
        return s[:length].decode('latin-1', errors='replace')
    
    def read_int_byte_size_string(self):
        buf_size = self.read_int()
        str_len = self.read_byte()
        s = self.read_bytes(buf_size - 1) if buf_size > 0 else b""
        return s[:str_len].decode('latin-1', errors='replace')
    
    def read_int_size_string(self):
        length = self.read_int()
        if length <= 0:
            return ""
        s = self.read_bytes(length)
        return s.decode('latin-1', errors='replace')
    
    def hexdump(self, start, length):
        for i in range(0, length, 16):
            offset = start + i
            chunk = self.data[offset:offset+min(16, length-i)]
            hex_str = ' '.join(f'{b:02X}' for b in chunk)
            print(f"  {offset:04d}: {hex_str}")

# Load file
data = read_file(r"D:\GitHub\NewProject\test_midi_converted.gp5")
r = GP5Reader(data)

print(f"File size: {len(data)}")

# Skip to measure data - need to trace through the header first
# Version
version_len = r.read_byte()
version = r.read_bytes(30)
print(f"Version: {version[:version_len].decode('latin-1')}")

# Score info
title = r.read_int_byte_size_string()
subtitle = r.read_int_byte_size_string()
artist = r.read_int_byte_size_string()
album = r.read_int_byte_size_string()
words = r.read_int_byte_size_string()
music = r.read_int_byte_size_string()
copyright_info = r.read_int_byte_size_string()
tab = r.read_int_byte_size_string()
instructions = r.read_int_byte_size_string()
print(f"Title: '{title}'")

# Notice
notice_count = r.read_int()
for i in range(notice_count):
    r.read_int_byte_size_string()
print(f"After notices: pos={r.pos}")

# Lyrics
lyrics_track = r.read_int()
for i in range(5):
    bar = r.read_int()
    text = r.read_int_size_string()
print(f"After lyrics: pos={r.pos}")

# Master effect  
r.skip(4)  # volume
r.skip(4)  # reverb + eq placeholder
# Wait - GP5.0.0 might have different structure
# Let me check what bytes are here
print(f"At pos {r.pos}:")
r.hexdump(r.pos, 32)

# Page setup
page_width = r.read_int()
page_height = r.read_int()
margin_left = r.read_int()
margin_right = r.read_int()
margin_top = r.read_int()
margin_bottom = r.read_int()
score_size = r.read_int()
header_footer_flags = r.read_short()
print(f"Page: {page_width}x{page_height}, flags={header_footer_flags:04X}")
print(f"After page dims: pos={r.pos}")

# Header/footer strings (11)
for i in range(11):
    s = r.read_int_byte_size_string()
print(f"After HF strings: pos={r.pos}")

# Tempo
tempo_name = r.read_int_byte_size_string()
tempo = r.read_int()
print(f"Tempo: {tempo}")
print(f"After tempo: pos={r.pos}")

# Key signature
key = r.read_sbyte()
r.skip(3)  # padding
octave = r.read_int()
print(f"Key: {key}, Octave: {octave}")
print(f"After key: pos={r.pos}")

# MIDI channels (64 * 12 bytes = 768 bytes)
r.skip(768)
print(f"After MIDI channels: pos={r.pos}")

# Musical directions (19 * 2 = 38 bytes)
r.skip(38)
print(f"After directions: pos={r.pos}")

# Master reverb
reverb = r.read_int()
print(f"Master reverb: {reverb}")
print(f"After reverb: pos={r.pos}")

# Measure count and track count
measure_count = r.read_int()
track_count = r.read_int()
print(f"Measure count: {measure_count}, Track count: {track_count}")
print(f"After counts: pos={r.pos}")

# Measure headers
for m in range(measure_count):
    if m > 0:
        r.skip(1)  # placeholder before each header after the first
    
    flags = r.read_byte()
    
    if flags & 0x01:  # numerator
        r.read_byte()
    if flags & 0x02:  # denominator
        r.read_byte()
    if flags & 0x08:  # repeat end
        r.read_byte()
    if flags & 0x20:  # marker
        r.read_int_byte_size_string()
        r.read_int()  # color
    if flags & 0x40:  # key signature
        r.read_sbyte()
        r.read_sbyte()
    if flags & 0x10:  # alt ending
        r.read_byte()
    if flags & 0x03:  # time sig changed - beams
        r.skip(4)
    if flags & 0x10 == 0:
        r.skip(1)  # placeholder
    
    # triplet feel
    r.read_byte()
    
    if m < 3 or m >= measure_count - 2:
        print(f"  Measure header {m+1}: flags=0x{flags:02X}")

print(f"After measure headers: pos={r.pos}")

# Tracks
for t in range(track_count):
    track_start = r.pos
    if t == 0:
        r.skip(1)  # placeholder for first track in GP5.0.0
    
    flags1 = r.read_byte()
    track_name = r.read_byte_size_string(40)
    print(f"  Track {t+1}: '{track_name}' flags=0x{flags1:02X} at {track_start}")
    
    # Strings
    num_strings = r.read_int()
    for s in range(7):
        r.read_int()
    
    # Port, channel, effects, frets, capo, color
    r.skip(6 * 4)
    
    # GP5 track flags2
    flags2 = r.read_short()
    
    # Auto accentuation
    r.read_byte()
    
    # Bank
    r.read_int()
    
    # Track RSE (GP5.0.0 simplified)
    # humanize
    r.read_byte()
    r.skip(12)  # 3 ints
    r.skip(12)  # unknown
    
    # RSE instrument
    r.skip(4 * 3)  # instrument, unknown, soundBank
    r.skip(3)  # effectNumber(2) + placeholder(1)

print(f"After tracks: pos={r.pos}")

# Extra placeholder after tracks
r.skip(2)
print(f"After track placeholder: pos={r.pos}")

# Now measure data
print("\n=== MEASURE DATA ===")

for m in range(min(measure_count, 25)):
    measure_start = r.pos
    print(f"\nMeasure {m+1} at pos {r.pos}:")
    
    for t in range(track_count):
        for voice in range(2):
            beat_count_pos = r.pos
            beat_count = r.read_int()
            print(f"  Track {t+1} Voice {voice+1}: {beat_count} beats at pos {beat_count_pos}")
            
            for b in range(beat_count):
                beat_start = r.pos
                beat_flags = r.read_byte()
                
                # Beat status
                if beat_flags & 0x40:
                    beat_status = r.read_byte()
                
                # Duration
                duration = r.read_sbyte()
                
                # Tuplet
                if beat_flags & 0x20:
                    tuplet = r.read_int()
                
                # Chord - this is where the error happens!
                if beat_flags & 0x02:
                    print(f"    *** CHORD FLAG SET at beat {b+1}! pos={beat_start} ***")
                    # This is wrong - there should be no chord
                    r.hexdump(beat_start, 32)
                    print(f"    ERROR: Unexpected chord flag!")
                    exit(1)
                
                # Text
                if beat_flags & 0x04:
                    r.read_int_byte_size_string()
                
                # Beat effects
                if beat_flags & 0x08:
                    effect1 = r.read_byte()
                    effect2 = r.read_byte()
                    if effect1 & 0x20:
                        r.read_byte()  # slap
                    if effect2 & 0x04:
                        # Tremolo bar
                        r.read_byte()
                        r.read_int()
                        points = r.read_int()
                        r.skip(points * 9)
                    if effect1 & 0x40:
                        r.read_byte()
                        r.read_byte()
                    if effect2 & 0x02:
                        r.read_byte()
                        r.read_byte()
                
                # Mix table
                if beat_flags & 0x10:
                    # Skip mix table - complex
                    print(f"    *** MIX TABLE at beat {b+1} ***")
                
                # Notes (if not rest)
                if not (beat_flags & 0x40):
                    string_bits = r.read_byte()
                    note_count = bin(string_bits).count('1')
                    for n in range(note_count):
                        note_flags = r.read_byte()
                        if note_flags & 0x20:
                            r.read_byte()  # type
                        if note_flags & 0x10:
                            r.read_sbyte()  # velocity
                        if note_flags & 0x20:
                            r.read_sbyte()  # fret
                        if note_flags & 0x80:
                            r.skip(2)  # fingering
                        if note_flags & 0x01:
                            r.skip(8)  # duration percent
                        if note_flags & 0x08:
                            # Note effects
                            ne1 = r.read_byte()
                            ne2 = r.read_byte()
                            if ne1 & 0x01:
                                # Bend
                                r.read_byte()
                                r.read_int()
                                pts = r.read_int()
                                r.skip(pts * 9)
                            if ne1 & 0x08:
                                r.skip(5)  # grace
                            if ne2 & 0x02:
                                r.read_byte()  # harmonic
                            if ne2 & 0x04:
                                r.skip(2)  # trill
                            if ne2 & 0x08:
                                r.read_byte()  # slide
                        # flags2
                        r.read_byte()
                
                # Beat flags2
                r.read_short()
                
                if b < 3 and m < 3:
                    print(f"    Beat {b+1}: flags=0x{beat_flags:02X} at {beat_start}")
    
    # Line break
    r.read_byte()
    
    if m == 21:
        print(f"\n*** Approaching error point ***")
        print(f"Position after measure 22: {r.pos}")
        r.hexdump(r.pos, 64)

print(f"\nFinal position: {r.pos}")
print(f"File size: {len(data)}")
