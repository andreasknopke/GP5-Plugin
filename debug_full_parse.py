"""
Debug: Step through PyGuitarPro parsing to see where it actually reads
"""
import struct
from io import BytesIO

# Open file
with open(r'D:\GitHub\NewProject\Recording.gp5', 'rb') as f:
    data = f.read()

# Create a file-like object to track position
class TrackedFile:
    def __init__(self, data):
        self.data = data
        self.pos = 0
    
    def read(self, n=1):
        result = self.data[self.pos:self.pos+n]
        self.pos += n
        return result
    
    def tell(self):
        return self.pos
    
    def seek(self, pos):
        self.pos = pos

f = TrackedFile(data)

def readU8():
    return struct.unpack('B', f.read(1))[0]

def readI8():
    return struct.unpack('b', f.read(1))[0]

def readI16():
    return struct.unpack('<h', f.read(2))[0]

def readI32():
    return struct.unpack('<i', f.read(4))[0]

def readBool():
    return struct.unpack('?', f.read(1))[0]

def skip(n):
    f.read(n)

def readByteSizeString(size):
    length = readU8()
    all_bytes = f.read(size)  # Read exactly 'size' bytes
    s = all_bytes[:length].decode('latin-1', errors='ignore')
    return s

def readIntByteSizeString():
    total = readI32()
    if total <= 0:
        return ''
    length = readU8()
    s = f.read(length).decode('latin-1', errors='ignore')
    return s

def readIntSizeString():
    length = readI32()
    if length <= 0:
        return ''
    return f.read(length).decode('latin-1', errors='ignore')

# Step through file like PyGuitarPro does

# 1. Version
print("=== Version ===")
pos = f.tell()
version = readByteSizeString(30)
print(f"Position {pos}: Version = '{version}'")

# 2. Info
print("\n=== Info ===")
pos = f.tell()
title = readIntByteSizeString()
print(f"Position {pos}: title = '{title}'")
subtitle = readIntByteSizeString()
print(f"  subtitle = '{subtitle}'")
artist = readIntByteSizeString()
print(f"  artist = '{artist}'")
album = readIntByteSizeString()
print(f"  album = '{album}'")
words = readIntByteSizeString()
print(f"  words = '{words}'")
music = readIntByteSizeString()
print(f"  music = '{music}'")
copyright = readIntByteSizeString()
print(f"  copyright = '{copyright}'")
tab = readIntByteSizeString()
print(f"  tab = '{tab}'")
instructions = readIntByteSizeString()
print(f"  instructions = '{instructions}'")
notice_lines = readI32()
print(f"  notice_lines = {notice_lines}")
for i in range(notice_lines):
    readIntByteSizeString()
print(f"Info ends at: {f.tell()}")

# 3. Lyrics
print("\n=== Lyrics ===")
pos = f.tell()
trackChoice = readI32()
print(f"Position {pos}: trackChoice = {trackChoice}")
for i in range(5):
    startMeasure = readI32()
    lyrics = readIntSizeString()
print(f"Lyrics ends at: {f.tell()}")

# 4. RSE Master Effect (GP5)
print("\n=== RSE Master Effect ===")
pos = f.tell()
# Skip RSE master effect for GP5.0 - it's empty
# Looks like GP5.0 has no RSE master effect data at all
# Let me check what's at this position
print(f"Position {pos}: next bytes = {data[pos:pos+20].hex()}")

# For GP5.0, there's no RSE master effect written
# PyGuitarPro readRSEMasterEffect for v5.0.0 might be empty
# Let's continue as if there's no RSE data

# 5. Page Setup
print("\n=== Page Setup ===")
pos = f.tell()
print(f"Position {pos}: starting page setup")
width = readI32()
height = readI32()
print(f"  Page size: {width} x {height}")
left = readI32()
right = readI32()
top = readI32()
bottom = readI32()
print(f"  Margins: L={left}, R={right}, T={top}, B={bottom}")
scoreSizeProp = readI32()
print(f"  Score size: {scoreSizeProp}")
headerFooter = readI16()
print(f"  Header/footer flags: 0x{headerFooter:04x}")
# 10 strings for header/footer
for i in range(10):
    s = readIntByteSizeString()
print(f"Page setup ends at: {f.tell()}")

# 6. Tempo info
print("\n=== Tempo Info ===")
pos = f.tell()
tempoName = readIntByteSizeString()
print(f"Position {pos}: tempoName = '{tempoName}'")
tempo = readI32()
print(f"Tempo = {tempo}")

# For version > 5.0.0, there's hideTempo bool
# Our version is 5.0.0, so no hideTempo

key = readI8()
print(f"Key = {key}")
octave = readI32()
print(f"Octave = {octave}")
print(f"Tempo info ends at: {f.tell()}")

# 7. MIDI Channels
print("\n=== MIDI Channels ===")
pos = f.tell()
print(f"Position {pos}: starting MIDI channels (64 x 12 = 768 bytes)")
skip(768)
print(f"MIDI channels end at: {f.tell()}")

# 8. Directions (GP5)
print("\n=== Directions ===")
pos = f.tell()
print(f"Position {pos}: starting directions (19 x 2 = 38 bytes)")
for i in range(19):
    readI16()
print(f"Directions end at: {f.tell()}")

# 9. Master Reverb
print("\n=== Master Reverb ===")
pos = f.tell()
reverb = readI32()
print(f"Position {pos}: reverb = {reverb}")

# 10. Measure and Track counts
print("\n=== Counts ===")
pos = f.tell()
measureCount = readI32()
print(f"Position {pos}: measureCount = {measureCount}")
trackCount = readI32()
print(f"TrackCount = {trackCount}")
print(f"Counts end at: {f.tell()}")

# 11. Measure Headers
print("\n=== Measure Headers ===")
pos = f.tell()
print(f"Position {pos}: starting measure headers")

previous_time_beams = [2, 2, 2, 2]

for m in range(measureCount):
    header_start = f.tell()
    
    if m > 0:
        skip(1)  # blank byte
    
    flags = readU8()
    
    if flags & 0x01:
        readI8()  # numerator
    if flags & 0x02:
        readI8()  # denominator
    if flags & 0x08:
        readI8()  # repeat close
    if flags & 0x20:
        # marker
        readIntByteSizeString()
        skip(4)  # color
    if flags & 0x40:
        readI8()  # key
        readI8()  # type
    if flags & 0x10:
        readU8()  # repeat alternative
    if flags & 0x03:
        skip(4)  # beams
    if (flags & 0x10) == 0:
        skip(1)  # blank
    readU8()  # triplet feel

print(f"Measure headers end at: {f.tell()}")

# 12. Tracks!
print("\n=== Tracks ===")
pos = f.tell()
print(f"Position {pos}: starting tracks (should be 1329)")
print(f"Byte at pos: 0x{data[pos]:02x}")
print(f"Next 10 bytes: {data[pos:pos+10].hex()}")
