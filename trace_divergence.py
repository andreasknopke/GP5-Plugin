#!/usr/bin/env python3
"""Find where After.gp5 diverges from a known-good reference."""

import guitarpro
import traceback

BEFORE = r'D:\GitHub\NewProject\before.gp5'
AFTER = r'D:\GitHub\NewProject\After.gp5'
REFERENCE = r'D:\GitHub\NewProject\reference_pyguitarpro.gp5'

# Re-save reference
song = guitarpro.parse(BEFORE)
guitarpro.write(song, REFERENCE)

with open(REFERENCE, 'rb') as f:
    ref = f.read()
with open(AFTER, 'rb') as f:
    aft = f.read()

print(f"REF: {len(ref)} bytes")
print(f"AFT: {len(aft)} bytes")

# Find first byte divergence
min_len = min(len(ref), len(aft))
first_diff = None
for i in range(min_len):
    if ref[i] != aft[i]:
        first_diff = i
        break

if first_diff is not None:
    print(f"\nFirst divergence at byte {first_diff} (0x{first_diff:x})")
    ctx = 16
    s = max(0, first_diff - ctx)
    e = min(min_len, first_diff + ctx)
    print(f"REF[{s}:{e}]: {ref[s:e].hex(' ')}")
    print(f"AFT[{s}:{e}]: {aft[s:e].hex(' ')}")
    
    # Try to find what the bytes around first_diff represent
    # Search for the version string position in both files
    print(f"\nREF[{first_diff}] = 0x{ref[first_diff]:02x} ({ref[first_diff]})")
    print(f"AFT[{first_diff}] = 0x{aft[first_diff]:02x} ({aft[first_diff]})")
else:
    print(f"\nFiles identical up to min length ({min_len})")

# Try to parse After.gp5 with full traceback
print(f"\n=== Parsing After.gp5 ===")
try:
    after_song = guitarpro.parse(AFTER)
    print("SUCCESS!")
    print(f"  Title: {after_song.title}")
    print(f"  Tracks: {len(after_song.tracks)}")
    print(f"  Measures: {len(after_song.measureHeaders)}")
    
    # Compare structure
    for i, t in enumerate(after_song.tracks):
        print(f"  Track {i}: '{t.name}' strings={len(t.strings)}")
        
except Exception as e:
    print(f"PARSE ERROR: {e}")
    traceback.print_exc()

# Also check: what is the version string in After.gp5?
print(f"\n=== Version strings ===")
# GP5 version is at the start: 1 byte length + string
ref_vlen = ref[0]
ref_ver = ref[1:1+ref_vlen].decode('latin-1')
print(f"REF version: '{ref_ver}' (padded to 31 bytes)")

aft_vlen = aft[0]
aft_ver = aft[1:1+aft_vlen].decode('latin-1')
print(f"AFT version: '{aft_ver}' (padded to 31 bytes)")

# Show the song info section too
# After version (31 bytes), comes info strings
# Each is: 4 bytes int (total), 1 byte length, then string
print(f"\n=== Info strings in After.gp5 ===")
import struct
pos = 31
labels = ['title', 'subtitle', 'artist', 'album', 'words', 'music', 'copyright', 'tab', 'instructions']
for label in labels:
    total = struct.unpack_from('<i', aft, pos)[0]
    pos += 4
    slen = aft[pos]
    pos += 1
    s = aft[pos:pos+slen].decode('latin-1', errors='replace')
    pos += slen
    print(f"  {label}: '{s}'")

# Now count notice lines
notice_count = struct.unpack_from('<i', aft, pos)[0]
pos += 4
print(f"  notice_lines: {notice_count}")
for i in range(notice_count):
    total = struct.unpack_from('<i', aft, pos)[0]
    pos += 4
    slen = aft[pos]
    pos += 1
    s = aft[pos:pos+slen].decode('latin-1', errors='replace')
    pos += slen
    print(f"    '{s}'")
