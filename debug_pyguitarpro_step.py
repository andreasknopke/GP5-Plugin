#!/usr/bin/env python3
"""Step through PyGuitarPro's parsing to find where it diverges."""

import struct

# Read the file
with open(r"D:\GitHub\NewProject\Recording.gp5", "rb") as f:
    data = f.read()

print(f"File size: {len(data)}")

# Let's trace exactly what PyGuitarPro does
# First, let's find the measure data start position

# Search for the version string
version_start = 0
version_len = struct.unpack_from('<I', data, version_start)[0]
print(f"Version length at 0: {version_len}")
version_str = data[4:4+version_len].decode('latin-1')
print(f"Version: {version_str}")

# According to GP5 format, after version (31 bytes = 4 + 30 - 1 for null)
# But let's follow the actual structure

# PyGuitarPro uses a stream-based reader - let me check what happens
# by looking at the code's expectations

# Let me read PyGuitarPro's source for the exact read order
# For now, let me compare byte-by-byte with a working GP5 file

pos = 0

def read_byte():
    global pos
    val = data[pos]
    pos += 1
    return val

def read_int():
    global pos
    val = struct.unpack_from('<i', data, pos)[0]
    pos += 4
    return val

def read_short():
    global pos
    val = struct.unpack_from('<h', data, pos)[0]
    pos += 2
    return val

def read_string(size, length=None):
    global pos
    if length is None:
        length = size
    s = data[pos:pos+size]
    pos += size
    return s[:length]

def read_byte_size_string(size):
    global pos
    length = data[pos]
    pos += 1
    s = data[pos:pos+size]
    pos += size
    return s[:length].decode('latin-1', errors='replace')

def read_int_size_string():
    global pos
    length = struct.unpack_from('<i', data, pos)[0]
    pos += 4
    if length <= 0:
        return ""
    s = data[pos:pos+length]
    pos += length
    return s.decode('latin-1', errors='replace')

def read_int_byte_size_string():
    global pos
    # Read 4-byte int (total buffer size including length byte)
    buffer_size = struct.unpack_from('<i', data, pos)[0]
    pos += 4
    # Read 1-byte string length
    str_len = data[pos]
    pos += 1
    # Read the string content
    s = data[pos:pos+buffer_size-1] if buffer_size > 0 else b""
    pos += buffer_size - 1 if buffer_size > 0 else 0
    return s[:str_len].decode('latin-1', errors='replace')

# Read header according to GP5 format
print("\n=== HEADER ===")
version_len = read_int()
print(f"Version length: {version_len}")
version = read_string(30)
print(f"Version: {version[:version_len]}")

# Score information (all int-byte-size strings)
print("\n=== SCORE INFO ===")
title = read_int_byte_size_string()
print(f"Title: '{title}' (pos={pos})")
subtitle = read_int_byte_size_string()
print(f"Subtitle: '{subtitle}' (pos={pos})")
artist = read_int_byte_size_string()
print(f"Artist: '{artist}' (pos={pos})")
album = read_int_byte_size_string()
print(f"Album: '{album}' (pos={pos})")

# GP5 has words (lyricist) before music
words = read_int_byte_size_string()
print(f"Words: '{words}' (pos={pos})")

music = read_int_byte_size_string()
print(f"Music: '{music}' (pos={pos})")
copyright_info = read_int_byte_size_string()
print(f"Copyright: '{copyright_info}' (pos={pos})")
tab = read_int_byte_size_string()
print(f"Tab: '{tab}' (pos={pos})")
instructions = read_int_byte_size_string()
print(f"Instructions: '{instructions}' (pos={pos})")

# Notice count (number of notice lines)
notice_count = read_int()
print(f"Notice count: {notice_count} (pos={pos})")
for i in range(notice_count):
    notice = read_int_byte_size_string()
    print(f"  Notice {i}: '{notice}'")

# Triplet feel (byte for GP5)
# But first let's check - GP5 might have lyrics here
print(f"\nBefore lyrics pos: {pos}")

# GP5: tripletFeel (byte)
# No wait - GP5 has lyrics track and lyrics text here
lyrics_track = read_int()
print(f"Lyrics track: {lyrics_track} (pos={pos})")

# Then 5 lyric lines, each with startFromBar (int) + text (int-size string)
for i in range(5):
    bar = read_int()
    lyric_text = read_int_size_string()
    print(f"Lyric line {i}: bar={bar}, text='{lyric_text[:30] if lyric_text else ''}...' (pos={pos})")

# RSE master effect (GP5.1+)
print(f"\nBefore page setup pos: {pos}")

# Page setup
page_setup_pos = pos
print(f"Page setup starts at: {pos}")
# In GP5, page setup is: pageFormat + string fields
# pageFormat: pageWidth(int), pageHeight(int), leftMargin(int), rightMargin(int), topMargin(int), bottomMargin(int), scoreSizeProportion(int)
# Then a bunch of string flags

# Skip through page setup - this is complex
# Let me just look for the measure/track counts around pos 1252
print(f"\nSearching for measure count around pos 1252...")
test_pos = 1248
for i in range(20):
    val = struct.unpack_from('<i', data, test_pos + i)[0]
    if 1 <= val <= 100:
        next_val = struct.unpack_from('<i', data, test_pos + i + 4)[0]
        if 1 <= next_val <= 100:
            print(f"  At pos {test_pos+i}: {val}, {next_val}")

# Let's also check what's at position 1252
print(f"\nAt pos 1252: {struct.unpack_from('<i', data, 1252)[0]}")
print(f"At pos 1256: {struct.unpack_from('<i', data, 1256)[0]}")

# Let me trace the page setup more carefully
pos = page_setup_pos

# Actually, let's just skip to where we know measure data should be and compare
# Skip to position 1258 and try to parse measure headers
print(f"\n=== CHECKING MEASURE HEADERS ===")
pos = 1260

print(f"Starting measure header parse at pos {pos}")
for m in range(16):
    header_pos = pos
    header_flags = read_byte()
    print(f"Measure {m+1} header at {header_pos}: flags=0x{header_flags:02X}")
    
    if header_flags & 0x01:  # numerator
        num = read_byte()
        print(f"  numerator: {num}")
    if header_flags & 0x02:  # denominator  
        denom = read_byte()
        print(f"  denominator: {denom}")
    if header_flags & 0x04:  # repeat start
        pass
    if header_flags & 0x08:  # repeat end
        repeats = read_byte()
        print(f"  repeat count: {repeats}")
    if header_flags & 0x10:  # alt ending
        alt = read_byte()
        print(f"  alt ending: {alt}")
    if header_flags & 0x20:  # marker
        marker_name = read_int_byte_size_string()
        marker_color = read_int()  # RGB color
        print(f"  marker: '{marker_name}'")
    if header_flags & 0x40:  # key signature
        key_sig = read_byte()
        key_type = read_byte()
        print(f"  key sig: {key_sig}, type: {key_type}")
    if header_flags & 0x80:  # double bar
        pass
    
    # GP5 has triplet feel at end of measure header
    if header_flags & 0x03:  # If time sig changed, read triplet feel
        pass
    
    # Actually in GP5, there's always triplet feel byte after flags (or after optional data)
    triplet = read_byte()
    print(f"  triplet feel: {triplet}")

print(f"\nAfter measure headers: pos={pos}")
print(f"Next bytes: {' '.join(f'{b:02X}' for b in data[pos:pos+20])}")
