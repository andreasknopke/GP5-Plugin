import struct

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

print(f"File size: {len(data)} bytes")

# Find measure start position - need to parse properly
# Let's manually step through from position 1474 (measure start from previous debug)

pos = 1474
print(f"\n=== Starting at position {pos} ===")

# Parse Measure 1
print("\n--- Measure 1 ---")

# Voice 1
v1_beats = struct.unpack_from('<i', data, pos)[0]
print(f"pos {pos}: Voice 1 beat count = {v1_beats}")
pos += 4

for b in range(v1_beats):
    flags = data[pos]
    print(f"  Beat {b+1} at pos {pos}: flags = 0x{flags:02x}")
    pos += 1
    
    if flags & 0x40:  # Rest
        empty_status = data[pos]
        print(f"    Empty status: {empty_status}")
        pos += 1
    
    # Duration
    duration = struct.unpack_from('<b', data, pos)[0]
    print(f"    Duration: {duration}")
    pos += 1
    
    # Chord (flags & 0x02)
    if flags & 0x02:
        print(f"    Has chord at pos {pos}")
        # Skip chord parsing for now
        
    # Text (flags & 0x04)
    if flags & 0x04:
        print(f"    Has text")
        
    # Effects (flags & 0x08)
    if flags & 0x08:
        print(f"    Has effects")
        # Read effect flags
        effect_flags1 = data[pos]
        effect_flags2 = data[pos+1]
        print(f"    Effect flags: 0x{effect_flags1:02x} 0x{effect_flags2:02x}")
        pos += 2
        
        # Parse based on flags...
        if effect_flags1 & 0x20:  # Stroke
            stroke_down = data[pos]
            stroke_up = data[pos+1]
            print(f"    Stroke: down={stroke_down}, up={stroke_up}")
            pos += 2
        if effect_flags2 & 0x04:  # Tremolo bar
            print(f"    Has tremolo bar")
            # bendValue
            pos += 4
        if effect_flags2 & 0x08:  # Pickstroke
            pickstroke = data[pos]
            print(f"    Pickstroke: {pickstroke}")
            pos += 1
    
    # Tuplet (flags & 0x20)
    if flags & 0x20:
        tuplet = struct.unpack_from('<i', data, pos)[0]
        print(f"    Tuplet: {tuplet}")
        pos += 4
    
    # Notes (if not rest)
    if not (flags & 0x40):
        string_bits = data[pos]
        print(f"    String bits: 0x{string_bits:02x}")
        pos += 1
        
        # Count notes
        note_count = bin(string_bits).count('1')
        for n in range(note_count):
            note_flags = data[pos]
            print(f"      Note {n+1} flags: 0x{note_flags:02x}")
            pos += 1
            
            if note_flags & 0x20:  # Accentuated
                accentuated = data[pos]
                print(f"        Accentuated: {accentuated}")
                pos += 1
            
            if note_flags & 0x01:  # Duration/percent
                duration_percent = data[pos]
                tuplet_val = data[pos+1]
                print(f"        Duration%: {duration_percent}, Tuplet: {tuplet_val}")
                pos += 2
            
            if note_flags & 0x10:  # Dynamic
                dynamic = data[pos]
                print(f"        Dynamic: {dynamic}")
                pos += 1
                
            # Note type
            note_type = data[pos]
            print(f"        Note type: {note_type}")
            pos += 1
            
            if note_flags & 0x02:  # Duration
                print(f"        Has duration info")
                pos += 2  # Skip 2 bytes
            
            if note_type in [1, 2]:  # Tied or regular
                fret = struct.unpack_from('<b', data, pos)[0]
                print(f"        Fret: {fret}")
                pos += 1
                
            if note_flags & 0x80:  # Fingering
                left_finger = data[pos]
                right_finger = data[pos+1]
                print(f"        Fingering: L={left_finger}, R={right_finger}")
                pos += 2
            
            if note_flags & 0x08:  # Effects
                print(f"        Has note effects")
                # Skip effect parsing - complex
                
    # Flags2 at end of beat
    flags2 = struct.unpack_from('<h', data, pos)[0]
    print(f"    Flags2: 0x{flags2:04x}")
    pos += 2

print(f"\nAfter Voice 1: pos = {pos}")

# Voice 2
v2_beats = struct.unpack_from('<i', data, pos)[0]
print(f"pos {pos}: Voice 2 beat count = {v2_beats}")
pos += 4

# LineBreak
linebreak = data[pos]
print(f"pos {pos}: LineBreak = {linebreak}")
pos += 1

print(f"\n--- Measure 2 starts at pos {pos} ---")

# Parse Measure 2
print("\n--- Measure 2 ---")

# Voice 1
v1_beats = struct.unpack_from('<i', data, pos)[0]
print(f"pos {pos}: Voice 1 beat count = {v1_beats}")
pos += 4

for b in range(min(v1_beats, 10)):  # Limit to 10 beats
    flags = data[pos]
    print(f"  Beat {b+1} at pos {pos}: flags = 0x{flags:02x}")
    
    # Check for chord flag
    if flags & 0x02:
        print(f"    *** HAS CHORD FLAG! This is the problem! ***")
    
    pos += 1
    
    if flags & 0x40:  # Rest
        empty_status = data[pos]
        print(f"    Empty status: {empty_status}")
        pos += 1
    
    # Duration
    duration = struct.unpack_from('<b', data, pos)[0]
    print(f"    Duration: {duration}")
    pos += 1
    
    # Effects (flags & 0x08)
    if flags & 0x08:
        effect_flags1 = data[pos]
        effect_flags2 = data[pos+1]
        print(f"    Effect flags: 0x{effect_flags1:02x} 0x{effect_flags2:02x}")
        pos += 2
        
        if effect_flags1 & 0x20:  # Stroke
            pos += 2
        if effect_flags2 & 0x04:  # Tremolo bar
            pos += 4
        if effect_flags2 & 0x08:  # Pickstroke
            pos += 1
    
    # Tuplet (flags & 0x20)
    if flags & 0x20:
        tuplet = struct.unpack_from('<i', data, pos)[0]
        print(f"    Tuplet: {tuplet}")
        pos += 4
    
    # Notes (if not rest)
    if not (flags & 0x40):
        string_bits = data[pos]
        print(f"    String bits: 0x{string_bits:02x}")
        pos += 1
        
        note_count = bin(string_bits).count('1')
        for n in range(note_count):
            note_flags = data[pos]
            print(f"      Note {n+1} flags: 0x{note_flags:02x}")
            pos += 1
            
            if note_flags & 0x20:  # Accentuated
                pos += 1
            
            if note_flags & 0x01:  # Duration/percent
                pos += 2
            
            if note_flags & 0x10:  # Dynamic
                pos += 1
                
            # Note type
            note_type = data[pos]
            print(f"        Note type: {note_type}")
            pos += 1
            
            if note_flags & 0x02:  # Duration
                pos += 2
            
            if note_type in [1, 2]:
                fret = struct.unpack_from('<b', data, pos)[0]
                print(f"        Fret: {fret}")
                pos += 1
                
            if note_flags & 0x80:  # Fingering
                pos += 2
            
            if note_flags & 0x08:  # Effects
                print(f"        Has note effects - complex parsing needed")
                
    # Flags2
    flags2 = struct.unpack_from('<h', data, pos)[0]
    print(f"    Flags2: 0x{flags2:04x}")
    pos += 2

print(f"\nCurrent position: {pos}")
