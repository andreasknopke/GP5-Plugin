#!/usr/bin/env python3
"""Compare Recording.gp5 structure with test_partial.gp5 to find differences."""

import struct

def read_file(path):
    with open(path, "rb") as f:
        return f.read()

# Read both files
my_data = read_file(r"D:\GitHub\NewProject\Recording.gp5")
ref_data = read_file(r"D:\GitHub\NewProject\test_partial.gp5")

print(f"My file size: {len(my_data)}")
print(f"Reference file size: {len(ref_data)}")

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

def parse_header(reader, name):
    print(f"\n{'='*60}")
    print(f"PARSING: {name}")
    print(f"{'='*60}")
    
    # Version (byte length + 30 bytes)
    version_len = reader.read_byte()
    version = reader.read_bytes(30).decode('latin-1', errors='replace')
    print(f"Version: '{version[:version_len]}' (pos now: {reader.pos})")
    
    # Score info
    title = reader.read_int_byte_size_string()
    print(f"Title: '{title}' (pos: {reader.pos})")
    
    subtitle = reader.read_int_byte_size_string()
    print(f"Subtitle: '{subtitle}' (pos: {reader.pos})")
    
    artist = reader.read_int_byte_size_string()
    print(f"Artist: '{artist}' (pos: {reader.pos})")
    
    album = reader.read_int_byte_size_string()
    print(f"Album: '{album}' (pos: {reader.pos})")
    
    words = reader.read_int_byte_size_string()
    print(f"Words: '{words}' (pos: {reader.pos})")
    
    music = reader.read_int_byte_size_string()
    print(f"Music: '{music}' (pos: {reader.pos})")
    
    copyright_info = reader.read_int_byte_size_string()
    print(f"Copyright: '{copyright_info}' (pos: {reader.pos})")
    
    tab = reader.read_int_byte_size_string()
    print(f"Tab: '{tab}' (pos: {reader.pos})")
    
    instructions = reader.read_int_byte_size_string()
    print(f"Instructions: '{instructions}' (pos: {reader.pos})")
    
    # Notice lines
    notice_count = reader.read_int()
    print(f"Notice count: {notice_count} (pos: {reader.pos})")
    for i in range(notice_count):
        notice = reader.read_int_byte_size_string()
        if i < 3:
            print(f"  Notice {i}: '{notice[:50]}...' " if len(notice) > 50 else f"  Notice {i}: '{notice}'")
    print(f"After notices: pos={reader.pos}")
    
    # Lyrics (GP5)
    lyrics_track = reader.read_int()
    print(f"Lyrics track: {lyrics_track} (pos: {reader.pos})")
    
    for i in range(5):
        bar = reader.read_int()
        text = reader.read_int_size_string()
        if text:
            print(f"Lyric {i}: bar={bar}, text='{text[:30]}...'")
        else:
            print(f"Lyric {i}: bar={bar}, text=''")
    print(f"After lyrics: pos={reader.pos}")
    
    # RSE master effect (GP5 has this before page setup)
    # In GP5.00, there might be master volume, reverb, EQ
    # Let me check what bytes follow
    print(f"Next 20 bytes: {' '.join(f'{reader.data[reader.pos+i]:02X}' for i in range(20))}")
    
    return reader.pos

# Parse my file
my_reader = GP5Reader(my_data)
my_pos = parse_header(my_reader, "Recording.gp5")

# Parse reference file  
ref_reader = GP5Reader(ref_data)
ref_pos = parse_header(ref_reader, "test_partial.gp5")

# Now compare where measure/track counts are
print(f"\n\n{'='*60}")
print("SEARCHING FOR MEASURE/TRACK COUNTS")
print(f"{'='*60}")

# My file - search from current position
print(f"\nMy file - searching from pos {my_pos}:")
for offset in range(0, 200, 4):
    test_pos = my_pos + offset
    if test_pos + 8 > len(my_data):
        break
    val1 = struct.unpack_from('<i', my_data, test_pos)[0]
    val2 = struct.unpack_from('<i', my_data, test_pos + 4)[0]
    if 1 <= val1 <= 100 and 1 <= val2 <= 50:
        print(f"  Candidate at pos {test_pos}: measureCount={val1}, trackCount={val2}")

# Reference file
print(f"\nReference file - searching from pos {ref_pos}:")
for offset in range(0, 2000, 4):
    test_pos = ref_pos + offset
    if test_pos + 8 > len(ref_data):
        break
    val1 = struct.unpack_from('<i', ref_data, test_pos)[0]
    val2 = struct.unpack_from('<i', ref_data, test_pos + 4)[0]
    # Reference might have more measures/tracks
    if 1 <= val1 <= 500 and 1 <= val2 <= 50:
        # Verify it's actually measure/track counts by checking what follows
        print(f"  Candidate at pos {test_pos}: val1={val1}, val2={val2}")
