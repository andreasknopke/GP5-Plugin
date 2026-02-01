import struct
import guitarpro

# Parse with error catching
try:
    song = guitarpro.parse('D:/GitHub/NewProject/Recording.gp5')
    print("SUCCESS!")
    for i, track in enumerate(song.tracks):
        print(f"Track {i+1}: {track.name}")
        for j, measure in enumerate(track.measures):
            v1_beats = len(measure.voices[0].beats) if measure.voices else 0
            print(f"  Measure {j+1}: {v1_beats} beats")
except guitarpro.models.GPException as e:
    print(f"Parse error: {e}")

# Let me understand the error better
# "measure 2, voice 1, beat 6" - but measure 2 should only have 1 beat
# Maybe the issue is that PyGuitarPro's measure numbering is based on something else

# Let me check the raw bytes at position around 1500-1600
with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

print("\n\nRaw byte analysis:")
print("=" * 60)

# The error says "beat 6" with chord parsing failing
# Let me find all positions where flags have 0x02 (chord)
pos = 1474
print(f"Starting measure scan at pos {pos}")

measure_num = 0
beat_total = 0

while pos < len(data) - 10:
    measure_num += 1
    measure_start = pos
    
    v1_count = struct.unpack_from('<i', data, pos)[0]
    if v1_count < 0 or v1_count > 100:
        print(f"Invalid beat count {v1_count} at pos {pos}")
        break
    
    pos += 4
    
    for b in range(v1_count):
        beat_total += 1
        beat_start = pos
        flags = data[pos]
        pos += 1
        
        # Check for problematic flags
        if flags & 0x02:
            print(f"CHORD flag at measure {measure_num}, beat {b+1}, pos {beat_start}")
            print(f"  Global beat number: {beat_total}")
        if flags & 0x10:
            print(f"MixTableChange flag at measure {measure_num}, beat {b+1}, pos {beat_start}")
        
        # Skip beat data (simplified)
        if flags & 0x40:
            pos += 1
        pos += 1
        if flags & 0x20:
            pos += 4
        if flags & 0x08:
            ef1 = data[pos]
            ef2 = data[pos+1]
            pos += 2
            if ef1 & 0x20: pos += 2
            if ef2 & 0x04:
                pos += 5
                n = struct.unpack_from('<i', data, pos)[0]
                pos += 4 + n * 8
            if ef2 & 0x08: pos += 1
        if not (flags & 0x40):
            sb = data[pos]
            pos += 1
            nc = bin(sb).count('1')
            for n in range(nc):
                nf = data[pos]
                pos += 1
                if nf & 0x20: pos += 2
                if nf & 0x10: pos += 1
                if nf & 0x80: pos += 2
                if nf & 0x01: pos += 8
                pos += 1
                if nf & 0x08:
                    e1 = data[pos]
                    e2 = data[pos+1]
                    pos += 2
                    if e1 & 0x01:
                        pos += 5
                        np = struct.unpack_from('<i', data, pos)[0]
                        pos += 4 + np * 8
                    if e1 & 0x10: pos += 5
                    if e2 & 0x04: pos += 1
                    if e2 & 0x08: pos += 1
                    if e2 & 0x10:
                        ht = data[pos]
                        pos += 1
                        if ht == 2: pos += 3
                        elif ht == 3: pos += 1
                    if e2 & 0x20: pos += 3
        pos += 2
    
    # Voice 2
    v2 = struct.unpack_from('<i', data, pos)[0]
    pos += 4
    # LineBreak
    pos += 1
    
    if measure_num >= 16:
        break

print(f"\nTotal beats parsed: {beat_total}")
print(f"Total measures parsed: {measure_num}")
