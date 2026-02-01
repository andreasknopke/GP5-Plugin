import struct

# Compare the measure data structure between a working and broken file

def find_measures_start(data):
    """Find where measure data starts by looking for pattern after track data"""
    # Track data ends with some bytes, then measure data starts with beat counts
    # For now, manually specify based on file analysis
    return None

# Load both files
with open('D:/GitHub/NewProject/test_partial.gp5', 'rb') as f:
    ref_data = f.read()

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    my_data = f.read()

print(f"Reference file: {len(ref_data)} bytes")
print(f"My file: {len(my_data)} bytes")
print()

# For the reference file, let's find where measures start
# First, find measureCount and trackCount
for pos in range(len(ref_data)-8):
    val1 = struct.unpack_from('<i', ref_data, pos)[0]
    val2 = struct.unpack_from('<i', ref_data, pos+4)[0]
    # Looking for reasonable measure and track counts
    if val1 == 156 and val2 == 6:  # 156 measures, 6 tracks
        print(f"Reference: measureCount={val1}, trackCount={val2} at pos {pos}")
        break

# For my file
for pos in range(len(my_data)-8):
    val1 = struct.unpack_from('<i', my_data, pos)[0]
    val2 = struct.unpack_from('<i', my_data, pos+4)[0]
    if val1 == 16 and val2 == 1:
        print(f"My file: measureCount={val1}, trackCount={val2} at pos {pos}")
        break

print()
print("=" * 60)
print("Comparing first measure structure")
print("=" * 60)

# My file measures start at 1474
my_pos = 1474

# Reference file - need to find it
# Let's search for a pattern: beat count (small int) followed by flags byte
# For GP5, after track data there's placeholder bytes

# Actually, let me just compare the beat structure format
# In my file, I write:
# - beat count (int)
# - for each beat: flags, [status], duration, [tuplet], [effects], [notes], flags2

# Let's look at what I write for a rest beat
print("\nMy file - Measure 1 (rest beat):")
print(f"Position {my_pos}")
print(f"Bytes: {' '.join(f'{my_data[my_pos+i]:02x}' for i in range(20))}")

# Parse it
v1_beats = struct.unpack_from('<i', my_data, my_pos)[0]
my_pos += 4
print(f"Voice 1 beats: {v1_beats}")

for b in range(v1_beats):
    flags = my_data[my_pos]
    my_pos += 1
    print(f"  Beat {b+1}: flags=0x{flags:02x}")
    
    if flags & 0x40:  # Rest
        status = my_data[my_pos]
        my_pos += 1
        print(f"    Status: {status}")
    
    dur = struct.unpack_from('<b', my_data, my_pos)[0]
    my_pos += 1
    print(f"    Duration: {dur}")
    
    if flags & 0x20:
        my_pos += 4
    if flags & 0x08:
        my_pos += 2  # Simplified
    if not (flags & 0x40):
        # Notes
        pass
    
    # Flags2
    f2 = struct.unpack_from('<h', my_data, my_pos)[0]
    my_pos += 2
    print(f"    Flags2: 0x{f2:04x}")

# Voice 2
v2_beats = struct.unpack_from('<i', my_data, my_pos)[0]
my_pos += 4
print(f"Voice 2 beats: {v2_beats}")

# LineBreak
lb = my_data[my_pos]
my_pos += 1
print(f"LineBreak: {lb}")

print(f"\nAfter measure 1: position {my_pos}")

# Now let's see measure 2
print("\n" + "=" * 60)
print(f"My file - Measure 2:")
print(f"Position {my_pos}")
print(f"Bytes: {' '.join(f'{my_data[my_pos+i]:02x}' for i in range(20))}")

v1_beats = struct.unpack_from('<i', my_data, my_pos)[0]
my_pos += 4
print(f"Voice 1 beats: {v1_beats}")

# But PyGuitarPro says measure 2 has 6 beats...
# That's impossible if v1_beats = 1

# Let me check what PyGuitarPro is actually reading
# The error says "beat 6" - maybe it's reading garbage as beat count?

# Let me trace where PyGuitarPro might be
# If it misses one byte somewhere, it would read wrong data

print("\n" + "=" * 60)
print("Checking for off-by-one errors:")
print("=" * 60)

# What if the issue is with how I write the placeholder bytes after tracks?
# GP5.0 requires 2 placeholder bytes after tracks
# Let me check what's just before position 1474

print(f"\nBytes before measures (1464-1474):")
for i in range(1464, 1475):
    print(f"  {i}: 0x{my_data[i]:02x}")
