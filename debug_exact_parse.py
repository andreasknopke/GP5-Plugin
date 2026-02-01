import struct

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

print(f"File size: {len(data)}")
print("=" * 70)

# According to the error, it fails at "measure 2, voice 1, beat 6"
# Trying to read a chord, but getting a huge number

# Let me carefully parse measure 2 beat by beat according to EXACT GP5 format
# GP5 format from PyGuitarPro source:

# readBeat (gp3.py):
# 1. flags = readU8()
# 2. if flags & 0x40: beatStatus = readI8() (rest/empty)
# 3. duration = readDuration()  <-- This is signed byte
# 4. if flags & 0x20: readI32() (tuplet)
# 5. if flags & 0x02: readChord()
# 6. if flags & 0x04: readText()
# 7. if flags & 0x08: readBeatEffects()
# 8. if flags & 0x10: readMixTableChange()
# 9. if not (flags & 0x40): readNotes()
# Then GP5 adds: flags2 = readI16()

# Let me parse very carefully

pos = 1474

def show_pos(label):
    print(f"  [{pos}] {label}")

for m in range(3):  # Parse first 3 measures
    print(f"\n=== MEASURE {m+1} at pos {pos} ===")
    
    v1_count = struct.unpack_from('<i', data, pos)[0]
    pos += 4
    print(f"Voice 1 beats: {v1_count}")
    
    for b in range(v1_count):
        print(f"\n  --- Beat {b+1} ---")
        beat_start = pos
        
        flags = data[pos]
        pos += 1
        
        flag_names = []
        if flags & 0x01: flag_names.append("dotted")
        if flags & 0x02: flag_names.append("CHORD")
        if flags & 0x04: flag_names.append("text")
        if flags & 0x08: flag_names.append("effects")
        if flags & 0x10: flag_names.append("mixChange")
        if flags & 0x20: flag_names.append("tuplet")
        if flags & 0x40: flag_names.append("rest")
        
        print(f"  pos {beat_start}: flags=0x{flags:02x} [{', '.join(flag_names)}]")
        
        # Rest/empty status
        if flags & 0x40:
            status = data[pos]
            pos += 1
            print(f"  pos {pos-1}: beatStatus={status}")
        
        # Duration (signed byte: -2=whole, -1=half, 0=quarter, 1=8th, 2=16th, 3=32nd)
        duration = struct.unpack_from('<b', data, pos)[0]
        pos += 1
        print(f"  pos {pos-1}: duration={duration}")
        
        # Tuplet
        if flags & 0x20:
            tuplet = struct.unpack_from('<i', data, pos)[0]
            pos += 4
            print(f"  pos {pos-4}: tuplet={tuplet}")
        
        # CHORD
        if flags & 0x02:
            print(f"  pos {pos}: *** CHORD DATA ***")
            chord_indicator = data[pos]
            print(f"    First byte: {chord_indicator}")
            if chord_indicator == 0:
                # Old chord format
                pos += 1
                name_total_len = struct.unpack_from('<i', data, pos)[0]
                print(f"    Old chord, name length: {name_total_len}")
            break
        
        # Text
        if flags & 0x04:
            text_len = struct.unpack_from('<i', data, pos)[0]
            pos += 4
            text_byte_len = data[pos]
            pos += 1
            text = data[pos:pos+text_byte_len].decode('latin1', errors='replace')
            pos += text_len - 1  # total len includes byte length prefix
            print(f"  pos {pos}: text='{text}'")
        
        # Beat effects (GP3/4/5)
        if flags & 0x08:
            ef1 = data[pos]
            ef2 = data[pos+1]
            pos += 2
            print(f"  pos {pos-2}: effects flags1=0x{ef1:02x} flags2=0x{ef2:02x}")
            
            # Effect flags1:
            # 0x01 = vibrato
            # 0x02 = wide vibrato
            # 0x04 = natural harmonic
            # 0x08 = artificial harmonic
            # 0x10 = fade in
            # 0x20 = stroke (GP4+)
            # 0x40 = (GP3 only: rasgueado)
            # 0x80 = (unused)
            
            # Effect flags2 (GP4+):
            # 0x01 = rasgueado
            # 0x02 = pickstroke (GP4+)
            # 0x04 = tremolo bar
            # 0x08 = pickstroke direction (GP5)
            
            if ef1 & 0x20:  # Stroke
                stroke_down = data[pos]
                stroke_up = data[pos+1]
                pos += 2
                print(f"    Stroke: down={stroke_down}, up={stroke_up}")
            
            if ef2 & 0x04:  # Tremolo bar
                bend_type = data[pos]
                pos += 1
                bend_value = struct.unpack_from('<i', data, pos)[0]
                pos += 4
                num_points = struct.unpack_from('<i', data, pos)[0]
                pos += 4
                pos += num_points * 8  # Each point: position(4) + value(4)
                print(f"    TremoloBar: type={bend_type}, value={bend_value}, points={num_points}")
            
            if ef2 & 0x08:  # Pickstroke (GP5)
                pickstroke = data[pos]
                pos += 1
                print(f"    Pickstroke: {pickstroke}")
        
        # Mix table change
        if flags & 0x10:
            print(f"  pos {pos}: *** MIX TABLE CHANGE ***")
            # Complex structure, skip for now
            break
        
        # Notes (if not rest)
        if not (flags & 0x40):
            string_bits = data[pos]
            pos += 1
            note_count = bin(string_bits).count('1')
            print(f"  pos {pos-1}: stringBits=0x{string_bits:02x} ({note_count} notes)")
            
            for n in range(note_count):
                note_start = pos
                nflags = data[pos]
                pos += 1
                
                print(f"    Note {n+1}: flags=0x{nflags:02x}", end="")
                
                # GP5 Note flags:
                # 0x01 = duration percent (GP5)
                # 0x02 = heavy accent
                # 0x04 = ghost note
                # 0x08 = has note effects
                # 0x10 = velocity
                # 0x20 = note type + fret
                # 0x40 = accentuated
                # 0x80 = fingering
                
                if nflags & 0x20:  # Note type
                    ntype = data[pos]
                    pos += 1
                    print(f" type={ntype}", end="")
                
                if nflags & 0x10:  # Velocity
                    vel = struct.unpack_from('<b', data, pos)[0]
                    pos += 1
                    print(f" vel={vel}", end="")
                
                if nflags & 0x20:  # Fret
                    fret = struct.unpack_from('<b', data, pos)[0]
                    pos += 1
                    print(f" fret={fret}", end="")
                
                if nflags & 0x80:  # Fingering
                    left = struct.unpack_from('<b', data, pos)[0]
                    right = struct.unpack_from('<b', data, pos+1)[0]
                    pos += 2
                    print(f" fingering=({left},{right})", end="")
                
                if nflags & 0x01:  # Duration percent (GP5 only)
                    dur_pct = struct.unpack_from('<d', data, pos)[0]
                    pos += 8
                    print(f" dur%={dur_pct}", end="")
                
                # GP5: flags2 byte (always)
                nflags2 = data[pos]
                pos += 1
                print(f" flags2=0x{nflags2:02x}", end="")
                
                # Note effects
                if nflags & 0x08:
                    ef1 = data[pos]
                    ef2 = data[pos+1]
                    pos += 2
                    print(f" effects=0x{ef1:02x},0x{ef2:02x}", end="")
                    
                    if ef1 & 0x01:  # Bend
                        bend_type = data[pos]
                        bend_val = struct.unpack_from('<i', data, pos+1)[0]
                        num_pts = struct.unpack_from('<i', data, pos+5)[0]
                        pos += 9 + num_pts * 8
                        print(f" bend({bend_type},{bend_val},{num_pts}pts)", end="")
                    
                    if ef1 & 0x10:  # Grace (GP5)
                        pos += 5
                        print(f" grace", end="")
                    
                    if ef2 & 0x04:  # Tremolo picking
                        pos += 1
                        print(f" tremPick", end="")
                    
                    if ef2 & 0x08:  # Slide
                        slide = data[pos]
                        pos += 1
                        print(f" slide={slide}", end="")
                    
                    if ef2 & 0x10:  # Harmonic
                        harm = data[pos]
                        pos += 1
                        if harm == 2:  # Artificial
                            pos += 3
                        elif harm == 3:  # Tapped
                            pos += 1
                        print(f" harm={harm}", end="")
                    
                    if ef2 & 0x20:  # Trill
                        pos += 3
                        print(f" trill", end="")
                
                print()  # newline
        
        # GP5: Beat flags2
        beat_flags2 = struct.unpack_from('<h', data, pos)[0]
        pos += 2
        print(f"  pos {pos-2}: beat_flags2=0x{beat_flags2:04x}")
    
    # Voice 2
    v2_count = struct.unpack_from('<i', data, pos)[0]
    pos += 4
    print(f"\nVoice 2: {v2_count} beats")
    
    # LineBreak
    linebreak = data[pos]
    pos += 1
    print(f"LineBreak: {linebreak}")

print(f"\n\nFinal position: {pos}")
