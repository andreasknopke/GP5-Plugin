import struct

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

print("Analyzing measure headers...")
print("=" * 60)

# Find measure count position
# From earlier: pos 1252 has measureCount=16, trackCount=1

# The measure headers should be BEFORE the tracks
# Let me find where measure headers start

# After reading the header info, we have:
# - measureCount (int) at 1252
# - trackCount (int) at 1256
# So measure headers should be at ~1260

# But wait, measure headers are read BEFORE measureCount/trackCount
# Let me trace the GP5 readSong flow:

# 1. readVersion (31 bytes) -> pos 31
# 2. readClipboard -> nothing in 5.0
# 3. readInfo -> variable
# 4. readLyrics -> variable  
# 5. readRSEMasterEffect -> variable
# 6. readPageSetup -> variable
# 7. readIntByteSizeString (tempo name) -> variable
# 8. readI32 (tempo) -> 4 bytes
# 9. If >5.0: readBool (hideTempo)
# 10. readI8 (key) -> 1 byte
# 11. readI32 (octave) -> 4 bytes
# 12. readMidiChannels -> 64*17 = 1088 bytes
# 13. readDirections -> 19*2 = 38 bytes
# 14. readMasterReverb -> 4 bytes
# 15. readI32 (measureCount) -> 4 bytes
# 16. readI32 (trackCount) -> 4 bytes
# 17. readMeasureHeaders (measureCount times)
# 18. readTracks (trackCount times)
# 19. readMeasures

# So measure headers come after the measureCount/trackCount
# Position 1260 should be start of measure headers

pos = 1260

print(f"Reading measure headers starting at pos {pos}")
print()

for m in range(16):
    header_start = pos
    
    # First measure has no previous, others have 1-byte placeholder
    if m > 0:
        placeholder = data[pos]
        pos += 1
    
    # Flags
    flags = data[pos]
    pos += 1
    
    flag_names = []
    if flags & 0x01: flag_names.append("timeSigNum")
    if flags & 0x02: flag_names.append("timeSigDen")
    if flags & 0x04: flag_names.append("repeat")
    if flags & 0x08: flag_names.append("altEnding")
    if flags & 0x10: flag_names.append("marker")
    if flags & 0x20: flag_names.append("keySig")
    if flags & 0x40: flag_names.append("doubleBar")
    
    print(f"Measure Header {m+1} at {header_start}: flags=0x{flags:02x} [{', '.join(flag_names)}]")
    
    # Time signature numerator
    if flags & 0x01:
        num = data[pos]
        pos += 1
        print(f"  Time sig numerator: {num}")
    
    # Time signature denominator  
    if flags & 0x02:
        den = data[pos]
        pos += 1
        print(f"  Time sig denominator: {den}")
    
    # If flags & 0x03 (both set) for GP5: read beams
    if (flags & 0x03) == 0x03:
        beams = data[pos:pos+4]
        pos += 4
        print(f"  Beams: {list(beams)}")
    
    # Repeat end  
    if flags & 0x04:
        repeat = data[pos]
        pos += 1
        print(f"  Repeat: {repeat}")
    
    # Alternate ending
    if flags & 0x08:
        altEnd = data[pos]
        pos += 1
        print(f"  Alt ending: {altEnd}")
    
    # Marker
    if flags & 0x10:
        # IntByteSizeString
        str_total = struct.unpack_from('<i', data, pos)[0]
        pos += 4
        str_len = data[pos]
        pos += 1
        marker_name = data[pos:pos+str_len].decode('latin1', errors='replace')
        pos += str_total - 1  # total includes length byte
        
        # Color (4 bytes)
        color = data[pos:pos+4]
        pos += 4
        print(f"  Marker: '{marker_name}'")
    
    # Key signature
    if flags & 0x20:
        key = struct.unpack_from('<b', data, pos)[0]
        minor = data[pos+1]
        pos += 2
        print(f"  Key sig: {key}, minor={minor}")
    
    # GP5 specific: triplet feel
    # Read after placeholder byte if present
    triplet = data[pos]
    pos += 1
    print(f"  Triplet feel: {triplet}")

print(f"\nAfter measure headers, pos: {pos}")
print(f"Track data should start around here")

# Now let's see what's at this position
print(f"\nBytes at {pos}: {' '.join(f'{data[pos+i]:02x}' for i in range(30))}")
