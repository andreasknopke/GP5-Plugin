import struct
with open(r'D:\GitHub\NewProject\Recording.gp5', 'rb') as f:
    data = f.read()

# Position 1474 is start of measures
pos = 1474

print('=== Measure 1 Raw Data ===')
print(f'Bytes 1474-1510: {data[1474:1510].hex()}')
print()

# Voice 1 beat count
v1_beats = struct.unpack('<i', data[pos:pos+4])[0]
print(f'pos {pos}: Voice 1 beat count = {v1_beats}')
pos += 4

# For each beat in voice 1
for b in range(v1_beats):
    beat_start = pos
    flags = data[pos]
    pos += 1
    print(f'pos {beat_start}: Beat {b+1} flags = 0x{flags:02x}')
    
    # Rest flag (0x40) means empty beat status byte follows
    if flags & 0x40:
        status = data[pos]
        pos += 1
        print(f'  Empty beat status: {status}')
    
    # Duration byte always present
    duration = data[pos]
    pos += 1
    print(f'  Duration: {duration}')
    
    # Tuplet (0x20)
    if flags & 0x20:
        tuplet = struct.unpack('<i', data[pos:pos+4])[0]
        pos += 4
        print(f'  Tuplet: {tuplet}')
    
    # Chord (0x02) - skip for now
    # Text (0x04) - skip for now
    
    # Beat effects (0x08)
    if flags & 0x08:
        ef1 = data[pos]
        pos += 1
        ef2 = data[pos]
        pos += 1
        print(f'  Beat effects: 0x{ef1:02x} 0x{ef2:02x}')
        # Parse effects based on flags...
    
    # MixTableChange (0x10)
    if flags & 0x10:
        print(f'  Has MixTableChange!')
    
    # String flags - only if not rest
    if not (flags & 0x40):
        string_flags = data[pos]
        pos += 1
        print(f'  String flags: 0x{string_flags:02x}')
        # Notes...
    
    # GP5 flags2 (short)
    flags2 = struct.unpack('<h', data[pos:pos+2])[0]
    pos += 2
    print(f'  Flags2: 0x{flags2:04x}')

print(f'\nAfter Voice 1, pos = {pos}')

# Voice 2 beat count
v2_beats = struct.unpack('<i', data[pos:pos+4])[0]
print(f'Voice 2 beat count at pos {pos}: {v2_beats}')
pos += 4

# LineBreak
print(f'LineBreak at pos {pos}: {data[pos]}')
