import struct

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

print(f"File size: {len(data)} bytes")

# Helper functions
def read_int(pos):
    return struct.unpack_from('<i', data, pos)[0], pos + 4

def read_byte(pos):
    return data[pos], pos + 1

def read_signed_byte(pos):
    return struct.unpack_from('<b', data, pos)[0], pos + 1

def read_short(pos):
    return struct.unpack_from('<h', data, pos)[0], pos + 2

# Start at measures position
pos = 1474

print("=" * 60)
print("Parsing all measures to find measure 2, beat 6")
print("=" * 60)

measure_num = 0
while pos < len(data) - 10:
    measure_num += 1
    measure_start = pos
    
    print(f"\n=== MEASURE {measure_num} (pos {pos}) ===")
    
    # Voice 1
    v1_beats, pos = read_int(pos)
    print(f"Voice 1 beat count: {v1_beats}")
    
    if v1_beats < 0 or v1_beats > 50:
        print(f"  ERROR: Invalid beat count!")
        break
    
    for beat_num in range(v1_beats):
        beat_start = pos
        flags, pos = read_byte(pos)
        
        flag_names = []
        if flags & 0x01: flag_names.append("dotted")
        if flags & 0x02: flag_names.append("CHORD")
        if flags & 0x04: flag_names.append("text")
        if flags & 0x08: flag_names.append("effects")
        if flags & 0x10: flag_names.append("mixChange")
        if flags & 0x20: flag_names.append("tuplet")
        if flags & 0x40: flag_names.append("rest")
        
        if measure_num == 2 or flags & 0x02:  # Show measure 2 or any chord beats
            print(f"  Beat {beat_num+1} (pos {beat_start}): flags=0x{flags:02x} [{', '.join(flag_names)}]")
        
        if flags & 0x40:  # Rest
            _, pos = read_byte(pos)  # Empty status
        
        # Duration
        duration, pos = read_signed_byte(pos)
        
        if flags & 0x20:  # Tuplet
            tuplet, pos = read_int(pos)
        
        if flags & 0x02:  # CHORD - this is the problem!
            print(f"    *** CHORD DATA STARTS AT POS {pos} ***")
            # Read first byte to see chord format
            chord_first, _ = read_byte(pos)
            print(f"    Chord first byte: {chord_first}")
            # If chord_first == 0, it's old chord format
            # If chord_first != 0, it's new chord format
            if chord_first == 0:
                print(f"    Old chord format detected")
                # Old chord: name is intByteSizeString
                # Let's see what bytes are there
                print(f"    Bytes at chord pos: {' '.join(f'{data[pos+i]:02x}' for i in range(20))}")
            else:
                print(f"    New chord format detected")
            break  # Stop at first chord
        
        if flags & 0x04:  # Text
            pass
        
        if flags & 0x08:  # Effects
            ef1, pos = read_byte(pos)
            ef2, pos = read_byte(pos)
            
            if ef1 & 0x20:  # Stroke
                pos += 2
            if ef2 & 0x04:  # Tremolo bar
                pos += 4
            if ef2 & 0x08:  # Pickstroke
                pos += 1
        
        if flags & 0x10:  # Mix table change - complex!
            print(f"    *** MIX TABLE CHANGE AT POS {pos} ***")
            # This is very complex, let's just dump bytes
            print(f"    Bytes: {' '.join(f'{data[pos+i]:02x}' for i in range(20))}")
            break
        
        # Notes (if not rest)
        if not (flags & 0x40):
            string_bits, pos = read_byte(pos)
            note_count = bin(string_bits).count('1')
            
            for n in range(note_count):
                nflags, pos = read_byte(pos)
                
                if nflags & 0x20:  # Type
                    _, pos = read_byte(pos)
                if nflags & 0x10:  # Velocity
                    _, pos = read_byte(pos)
                if nflags & 0x20:  # Fret
                    _, pos = read_byte(pos)
                if nflags & 0x80:  # Fingering
                    pos += 2
                if nflags & 0x01:  # Duration percent (GP5)
                    pos += 8  # F64
                
                # flags2 byte
                _, pos = read_byte(pos)
                
                if nflags & 0x08:  # Note effects
                    ef1, pos = read_byte(pos)
                    ef2, pos = read_byte(pos)
                    
                    if ef1 & 0x01:  # Bend
                        _, pos = read_byte(pos)  # type
                        _, pos = read_int(pos)  # value
                        num_pts, pos = read_int(pos)
                        pos += num_pts * 8  # points
                    if ef1 & 0x10:  # Grace
                        pos += 5  # GP5 grace is 5 bytes
                    if ef2 & 0x04:  # Tremolo picking
                        pos += 1
                    if ef2 & 0x08:  # Slide
                        pos += 1
                    if ef2 & 0x10:  # Harmonic
                        harm_type, pos = read_byte(pos)
                        if harm_type == 2:  # Artificial
                            pos += 3
                        elif harm_type == 3:  # Tapped
                            pos += 1
                    if ef2 & 0x20:  # Trill
                        pos += 3
        
        # Beat flags2 (GP5)
        _, pos = read_short(pos)
        
        if flags & 0x02:
            break
    
    if flags & 0x02:
        break
    
    # Voice 2
    v2_beats, pos = read_int(pos)
    if v2_beats != 0:
        print(f"Voice 2 beats: {v2_beats} - ERROR: Expected 0!")
        break
    
    # LineBreak
    _, pos = read_byte(pos)
    
    if measure_num >= 5:  # Stop after 5 measures
        break

print(f"\n\nFinal position: {pos}")
print(f"Remaining bytes: {len(data) - pos}")
