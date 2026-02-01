import struct

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

# Position 1474 is where measures start
pos = 1474

print('Parsing measures with correct byte-by-byte analysis:')
print('=' * 60)

for m in range(16):  # 16 measures
    measure_start = pos
    
    # Voice 1 beat count
    v1_count = struct.unpack_from('<i', data, pos)[0]
    pos += 4
    
    print(f'Measure {m+1} at pos {measure_start}: Voice1={v1_count} beats')
    
    if v1_count < 0 or v1_count > 100:
        print(f'  ERROR: Invalid beat count!')
        print(f'  Bytes around: {" ".join(f"{data[measure_start+i]:02x}" for i in range(-4, 20))}')
        break
    
    for b in range(v1_count):
        beat_pos = pos
        flags = data[pos]
        pos += 1
        
        # Dump flags for measure 2
        if m == 1:  # Measure 2 (0-indexed)
            flag_names = []
            if flags & 0x01: flag_names.append('dotted')
            if flags & 0x02: flag_names.append('CHORD')
            if flags & 0x04: flag_names.append('text')
            if flags & 0x08: flag_names.append('effects')
            if flags & 0x10: flag_names.append('mixChange')
            if flags & 0x20: flag_names.append('tuplet')
            if flags & 0x40: flag_names.append('rest')
            print(f'    Beat {b+1} at {beat_pos}: flags=0x{flags:02x} [{",".join(flag_names)}]')
        
        if flags & 0x40:  # Rest
            pos += 1  # Empty status
        
        pos += 1  # Duration
        
        if flags & 0x20:  # Tuplet
            pos += 4
        
        if flags & 0x02:  # Chord
            print(f'    CHORD at pos {pos}!')
            chord_indicator = data[pos]
            if chord_indicator == 0:
                pos += 1
                total_len = struct.unpack_from('<i', data, pos)[0]
                print(f'      Old chord, totalLen={total_len}')
                if total_len > 1000:
                    print(f'      ERROR: Chord string length too large!')
                    break
            pos += 100
            break
        
        if flags & 0x04:  # Text
            pos += 4
        
        if flags & 0x08:  # Effects
            ef1 = data[pos]
            ef2 = data[pos+1]
            pos += 2
            if ef1 & 0x20: pos += 2
            if ef2 & 0x04: 
                pos += 1 + 4
                num_pts = struct.unpack_from('<i', data, pos)[0]
                pos += 4 + num_pts * 8
            if ef2 & 0x08: pos += 1
        
        if flags & 0x10:  # Mix table change
            print(f'    MixTableChange at pos {pos}!')
            break
        
        if not (flags & 0x40):  # Notes
            string_bits = data[pos]
            pos += 1
            note_count = bin(string_bits).count('1')
            
            for n in range(note_count):
                nflags = data[pos]
                pos += 1
                
                if nflags & 0x20: pos += 1
                if nflags & 0x10: pos += 1
                if nflags & 0x20: pos += 1
                if nflags & 0x80: pos += 2
                if nflags & 0x01: pos += 8
                pos += 1  # flags2
                
                if nflags & 0x08:
                    ef1 = data[pos]
                    ef2 = data[pos+1]
                    pos += 2
                    if ef1 & 0x01:
                        pos += 1 + 4
                        num_pts = struct.unpack_from('<i', data, pos)[0]
                        pos += 4 + num_pts * 8
                    if ef1 & 0x10: pos += 5
                    if ef2 & 0x04: pos += 1
                    if ef2 & 0x08: pos += 1
                    if ef2 & 0x10:
                        ht = data[pos]
                        pos += 1
                        if ht == 2: pos += 3
                        elif ht == 3: pos += 1
                    if ef2 & 0x20: pos += 3
        
        pos += 2  # Beat flags2
    
    # Voice 2
    v2_count = struct.unpack_from('<i', data, pos)[0]
    pos += 4
    if v2_count != 0:
        print(f'  Voice2={v2_count} beats - unexpected!')
    
    # LineBreak
    pos += 1

print(f'\nFinal pos: {pos}, file size: {len(data)}')
