#!/usr/bin/env python3
"""
Debug: Compare the structure byte by byte between a working GP5 file
and the broken one to find exactly where PyGuitarPro's output diverges.
"""

import struct

def read_file(path):
    with open(path, "rb") as f:
        return f.read()

class GP5Reader:
    def __init__(self, data, name):
        self.data = data
        self.pos = 0
        self.name = name
    
    def read_byte(self):
        val = self.data[self.pos]
        self.pos += 1
        return val
    
    def read_sbyte(self):
        val = struct.unpack_from('b', self.data, self.pos)[0]
        self.pos += 1
        return val
    
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

def compare_readers(good, bad):
    """Compare two readers step by step."""
    
    print("=" * 70)
    print("COMPARING GP5 FILES")
    print("=" * 70)
    
    # Version
    g_vlen = good.read_byte()
    b_vlen = bad.read_byte()
    g_ver = good.read_bytes(30)
    b_ver = bad.read_bytes(30)
    print(f"Version: good='{g_ver[:g_vlen].decode()}' bad='{b_ver[:b_vlen].decode()}'")
    print(f"Position: good={good.pos} bad={bad.pos}")
    
    # Score info
    fields = ['title', 'subtitle', 'artist', 'album', 'words', 'music', 'copyright', 'tab', 'instructions']
    for field in fields:
        g_val = good.read_int_byte_size_string()
        b_val = bad.read_int_byte_size_string()
        if g_val != b_val or field == 'title':
            print(f"{field}: good='{g_val[:30]}' bad='{b_val[:30]}'")
    print(f"After info: good={good.pos} bad={bad.pos}")
    
    # Notice
    g_notice = good.read_int()
    b_notice = bad.read_int()
    print(f"Notice count: good={g_notice} bad={b_notice}")
    for i in range(g_notice):
        good.read_int_byte_size_string()
    for i in range(b_notice):
        bad.read_int_byte_size_string()
    print(f"After notices: good={good.pos} bad={bad.pos}")
    
    # Lyrics
    g_lyrics_track = good.read_int()
    b_lyrics_track = bad.read_int()
    print(f"Lyrics track: good={g_lyrics_track} bad={b_lyrics_track}")
    for i in range(5):
        good.read_int()
        good.read_int_size_string()
        bad.read_int()
        bad.read_int_size_string()
    print(f"After lyrics: good={good.pos} bad={bad.pos}")
    
    # RSE Master Effect - THIS IS WHERE IT MIGHT DIFFER
    print("\n=== RSE Master Effect ===")
    print(f"Good bytes at {good.pos}: {' '.join(f'{b:02X}' for b in good.data[good.pos:good.pos+20])}")
    print(f"Bad bytes at {bad.pos}: {' '.join(f'{b:02X}' for b in bad.data[bad.pos:bad.pos+20])}")
    
    # Volume
    g_vol = good.read_int()
    b_vol = bad.read_int()
    print(f"Master volume: good={g_vol} bad={b_vol}")
    
    # Reverb (GP5.0.0 doesn't have eq here)
    g_rev = good.read_int()
    b_rev = bad.read_int()
    print(f"Master reverb: good={g_rev} bad={b_rev}")
    print(f"After RSE: good={good.pos} bad={bad.pos}")
    
    # Page setup
    print("\n=== Page Setup ===")
    print(f"Good bytes at {good.pos}: {' '.join(f'{b:02X}' for b in good.data[good.pos:good.pos+32])}")
    print(f"Bad bytes at {bad.pos}: {' '.join(f'{b:02X}' for b in bad.data[bad.pos:bad.pos+32])}")
    
    # Page dimensions
    for name in ['width', 'height', 'left', 'right', 'top', 'bottom', 'scoreProportion']:
        g_val = good.read_int()
        b_val = bad.read_int()
        if g_val != b_val:
            print(f"  Page {name}: good={g_val} bad={b_val}")
    
    # Header/footer flags
    g_hf = good.read_short()
    b_hf = bad.read_short()
    print(f"Header/footer flags: good=0x{g_hf:04X} bad=0x{b_hf:04X}")
    print(f"After page dims: good={good.pos} bad={bad.pos}")
    
    # 11 header/footer strings
    for i in range(11):
        g_s = good.read_int_byte_size_string()
        b_s = bad.read_int_byte_size_string()
    print(f"After HF strings: good={good.pos} bad={bad.pos}")
    
    # Tempo
    g_tempo_name = good.read_int_byte_size_string()
    b_tempo_name = bad.read_int_byte_size_string()
    g_tempo = good.read_int()
    b_tempo = bad.read_int()
    print(f"Tempo: good={g_tempo} bad={b_tempo}")
    print(f"After tempo: good={good.pos} bad={bad.pos}")
    
    # Key
    g_key = good.read_sbyte()
    b_key = bad.read_sbyte()
    good.skip(3)
    bad.skip(3)
    g_octave = good.read_int()
    b_octave = bad.read_int()
    print(f"Key: good={g_key} bad={b_key}")
    print(f"After key: good={good.pos} bad={bad.pos}")
    
    # MIDI Channels
    good.skip(768)
    bad.skip(768)
    print(f"After MIDI channels: good={good.pos} bad={bad.pos}")
    
    # Directions
    good.skip(38)
    bad.skip(38)
    print(f"After directions: good={good.pos} bad={bad.pos}")
    
    # Master reverb
    g_mrev = good.read_int()
    b_mrev = bad.read_int()
    print(f"Master reverb 2: good={g_mrev} bad={b_mrev}")
    
    # Measure/track counts
    g_mc = good.read_int()
    b_mc = bad.read_int()
    g_tc = good.read_int()
    b_tc = bad.read_int()
    print(f"Measures: good={g_mc} bad={b_mc}")
    print(f"Tracks: good={g_tc} bad={b_tc}")
    print(f"After counts: good={good.pos} bad={bad.pos}")

# Load files
good = GP5Reader(read_file(r"D:\GitHub\NewProject\test_roundtrip.gp5"), "good")
bad = GP5Reader(read_file(r"D:\GitHub\NewProject\test_midi_converted.gp5"), "bad")

compare_readers(good, bad)
