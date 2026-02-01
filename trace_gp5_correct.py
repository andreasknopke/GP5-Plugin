"""
Detaillierter Trace, wie PyGuitarPro die Recording.gp5 liest
"""
import struct

def read_byte(f):
    return struct.unpack('<B', f.read(1))[0]

def read_sbyte(f):
    return struct.unpack('<b', f.read(1))[0]

def read_short(f):
    return struct.unpack('<h', f.read(2))[0]

def read_ushort(f):
    return struct.unpack('<H', f.read(2))[0]

def read_int(f):
    return struct.unpack('<i', f.read(4))[0]

def trace_measures(filepath, measures_start, num_measures):
    with open(filepath, 'rb') as f:
        f.seek(measures_start)
        
        beat_global = 0
        
        for m in range(num_measures):
            print(f'\n=== Measure {m+1} at pos {f.tell()} ===')
            
            for v in range(2):
                beat_count = read_int(f)
                print(f'  Voice {v+1}: {beat_count} beats (pos {f.tell()-4})')
                
                for b in range(beat_count):
                    beat_global += 1
                    beat_pos = f.tell()
                    flags = read_byte(f)
                    print(f'    Beat {b+1} (global {beat_global}) at pos {beat_pos}: flags=0x{flags:02x}', end='')
                    
                    # 0x40 = rest
                    if flags & 0x40:
                        status = read_byte(f)
                        print(f' REST status={status}', end='')
                    
                    duration = read_sbyte(f)
                    print(f' dur={duration}', end='')
                    
                    # 0x20 = tuplet
                    if flags & 0x20:
                        tuplet = read_int(f)
                        print(f' tuplet={tuplet}', end='')
                    
                    # 0x02 = chord - NOT IMPLEMENTED
                    if flags & 0x02:
                        print(' [CHORD NOT IMPL]')
                        return
                    
                    # 0x04 = text - NOT IMPLEMENTED
                    if flags & 0x04:
                        print(' [TEXT NOT IMPL]')
                        return
                    
                    # 0x08 = beat effects
                    if flags & 0x08:
                        ef1 = read_byte(f)
                        ef2 = read_byte(f)
                        print(f' effects(0x{ef1:02x},0x{ef2:02x})', end='')
                        
                        # Parse effect flags
                        if ef1 & 0x20:  # Tapping/slapping/popping
                            f.read(1)
                        if ef2 & 0x04:  # Tremolo bar
                            num_points = read_int(f)
                            for _ in range(num_points):
                                f.read(4+4+1)
                        if ef2 & 0x02:  # Pickstroke
                            f.read(1)
                    
                    # 0x10 = mix table change - NOT IMPLEMENTED
                    if flags & 0x10:
                        print(' [MIX TABLE NOT IMPL]')
                        return
                    
                    # PyGuitarPro ALWAYS calls readNotes() which reads stringFlags
                    # This is the key difference!
                    string_flags = read_byte(f)
                    note_count = bin(string_flags).count('1')
                    print(f' stringFlags=0x{string_flags:02x}({note_count})', end='')
                    
                    # Read notes
                    for s in range(7):
                        if string_flags & (1 << (6-s)):
                            nflags = read_byte(f)
                            
                            # 0x20 = note type
                            if nflags & 0x20:
                                read_byte(f)
                            
                            # 0x01 = duration percent
                            if nflags & 0x01:
                                f.read(2)
                            
                            # 0x10 = velocity
                            if nflags & 0x10:
                                read_sbyte(f)
                            
                            # fret (always)
                            fret = read_sbyte(f)
                            
                            # 0x80 = fingering
                            if nflags & 0x80:
                                f.read(2)
                            
                            # GP5: note flags2
                            nflags2 = read_byte(f)
                            
                            # 0x08 = note effects
                            if nflags & 0x08:
                                ne_f1 = read_byte(f)
                                ne_f2 = read_byte(f)
                                
                                if ne_f1 & 0x01:  # bend
                                    num_pts = read_int(f)
                                    for _ in range(num_pts):
                                        f.read(4+4+1)
                                if ne_f1 & 0x04:  # grace note
                                    f.read(4)
                                if ne_f2 & 0x04:  # tremolo picking
                                    f.read(1)
                                if ne_f2 & 0x08:  # slide
                                    f.read(1)
                                if ne_f2 & 0x10:  # harmonic
                                    f.read(1)
                                if ne_f2 & 0x20:  # trill
                                    f.read(3)
                            
                            print(f' [s{s+1}:f{fret}]', end='')
                    
                    # GP5: flags2 (short)
                    flags2 = read_ushort(f)
                    print(f' f2=0x{flags2:04x}')
            
            # Line break
            lb = read_byte(f)
            print(f'  LineBreak: {lb}')
        
        print(f'\n=== Done at pos {f.tell()} ===')

# Now trace
print('='*60)
print('Recording.gp5 - Tracing with PyGuitarPro logic')
print('='*60)
trace_measures('D:/GitHub/NewProject/Recording.gp5', 1466, 16)
