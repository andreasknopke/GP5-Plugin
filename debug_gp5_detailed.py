import struct

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

print(f"File size: {len(data)} bytes")

# Let's look at the raw bytes around where the error occurs
# Error: measure 2, voice 1, beat 6

# Start position for measures
pos = 1474

def read_int(pos):
    return struct.unpack_from('<i', data, pos)[0], pos + 4

def read_byte(pos):
    return data[pos], pos + 1

def read_signed_byte(pos):
    return struct.unpack_from('<b', data, pos)[0], pos + 1

def read_short(pos):
    return struct.unpack_from('<h', data, pos)[0], pos + 2

print("\n=== Parsing Measures Following PyGuitarPro Structure ===")

# Measure 1
print("\n--- Measure 1 ---")
v1_count, pos = read_int(pos)
print(f"Voice 1 beats: {v1_count}")

for b in range(v1_count):
    start_pos = pos
    flags, pos = read_byte(pos)
    print(f"  Beat {b+1} at {start_pos}: flags=0x{flags:02x}", end="")
    
    flag_desc = []
    if flags & 0x01: flag_desc.append("dotted")
    if flags & 0x02: flag_desc.append("CHORD")
    if flags & 0x04: flag_desc.append("text")
    if flags & 0x08: flag_desc.append("effects")
    if flags & 0x10: flag_desc.append("mixChange")
    if flags & 0x20: flag_desc.append("tuplet")
    if flags & 0x40: flag_desc.append("rest")
    print(f" [{', '.join(flag_desc)}]")
    
    if flags & 0x40:  # Rest
        status, pos = read_byte(pos)
        print(f"    Empty status: {status}")
    
    duration, pos = read_signed_byte(pos)
    print(f"    Duration: {duration}")
    
    if flags & 0x20:  # Tuplet
        tuplet, pos = read_int(pos)
        print(f"    Tuplet: {tuplet}")
    
    if flags & 0x02:  # Chord - this is the problem!
        print(f"    CHORD DATA EXPECTED HERE!")
    
    if flags & 0x04:  # Text
        print(f"    TEXT DATA EXPECTED HERE!")
    
    if flags & 0x08:  # Effects
        ef1, pos = read_byte(pos)
        ef2, pos = read_byte(pos)
        print(f"    Effect flags: 0x{ef1:02x} 0x{ef2:02x}")
        
        if ef1 & 0x20:  # Stroke
            pos += 2
        if ef2 & 0x04:  # Tremolo bar
            pos += 4
        if ef2 & 0x08:  # Pickstroke
            pos += 1
    
    if flags & 0x10:  # Mix table change
        print(f"    MIX TABLE CHANGE DATA EXPECTED HERE!")
    
    if not (flags & 0x40):  # Has notes
        string_bits, pos = read_byte(pos)
        print(f"    String bits: 0x{string_bits:02x}")
        
        note_count = bin(string_bits).count('1')
        for n in range(note_count):
            nflags, pos = read_byte(pos)
            print(f"      Note {n+1}: flags=0x{nflags:02x}")
            
            if nflags & 0x20:  # Accentuated
                acc, pos = read_byte(pos)
                print(f"        Accent: {acc}")
            
            if nflags & 0x01:  # Duration percent
                pos += 2
            
            if nflags & 0x10:  # Velocity
                vel, pos = read_signed_byte(pos)
                print(f"        Velocity: {vel}")
            
            if nflags & 0x20:  # Type and fret
                ntype, pos = read_byte(pos)
                print(f"        Note type: {ntype}")
                
            if nflags & 0x02:  # Time duration
                pos += 2
                
            if nflags & 0x20:  # Fret value
                fret, pos = read_signed_byte(pos)
                print(f"        Fret: {fret}")
            
            # GP5: flags2 byte
            nflags2, pos = read_byte(pos)
            
            if nflags & 0x80:  # Fingering
                pos += 2
            
            if nflags & 0x08:  # Note effects
                ef1, pos = read_byte(pos)
                ef2, pos = read_byte(pos)
                print(f"        Note effect flags: 0x{ef1:02x} 0x{ef2:02x}")
                
                if ef1 & 0x01:  # Bend
                    bend_type, pos = read_byte(pos)
                    bend_val, pos = read_int(pos)
                    num_points, pos = read_int(pos)
                    print(f"        Bend: type={bend_type}, val={bend_val}, points={num_points}")
                    pos += num_points * 8  # Each point is 8 bytes
    
    # Flags2 at end of beat
    flags2, pos = read_short(pos)
    print(f"    Beat flags2: 0x{flags2:04x}")

# Voice 2
v2_count, pos = read_int(pos)
print(f"Voice 2 beats: {v2_count}")

# LineBreak  
linebreak, pos = read_byte(pos)
print(f"LineBreak: {linebreak}")

print(f"\n=== End of Measure 1, position: {pos} ===")

# Now let's dump raw bytes of measures 2-3
print("\n=== Raw bytes from position", pos, "to", pos+100, "===")
for i in range(min(100, len(data) - pos)):
    if i % 20 == 0:
        print(f"\n{pos+i:4d}: ", end="")
    print(f"{data[pos+i]:02x} ", end="")
print()
