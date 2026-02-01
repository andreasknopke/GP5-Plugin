import struct

def parse_measures(filepath, start_pos, num_measures):
    """Parse measures and voices like PyGuitarPro"""
    with open(filepath, 'rb') as f:
        f.seek(start_pos)
        
        for m in range(num_measures):
            print(f'\n=== Measure {m+1} at position {f.tell()} ===')
            
            # Each measure has 2 voices
            for v in range(2):
                beat_count = struct.unpack('<i', f.read(4))[0]
                print(f'  Voice {v+1}: {beat_count} beats')
                
                for b in range(beat_count):
                    beat_start = f.tell()
                    flags = struct.unpack('<B', f.read(1))[0]
                    print(f'    Beat {b+1} at {beat_start}: flags=0x{flags:02x}', end='')
                    
                    # 0x40 = rest
                    if flags & 0x40:
                        status = struct.unpack('<B', f.read(1))[0]
                        print(f' status={status}', end='')
                    
                    # duration
                    duration = struct.unpack('<b', f.read(1))[0]
                    print(f' dur={duration}', end='')
                    
                    # 0x20 = tuplet
                    if flags & 0x20:
                        tuplet = struct.unpack('<i', f.read(4))[0]
                        print(f' tuplet={tuplet}', end='')
                    
                    # 0x02 = chord
                    if flags & 0x02:
                        print(' [chord - not implemented]', end='')
                        return  # bail
                    
                    # 0x04 = text
                    if flags & 0x04:
                        print(' [text - not implemented]', end='')
                        return  # bail
                    
                    # 0x08 = beat effects
                    if flags & 0x08:
                        effect_flags1 = struct.unpack('<B', f.read(1))[0]
                        effect_flags2 = struct.unpack('<B', f.read(1))[0]
                        print(f' effect(0x{effect_flags1:02x},0x{effect_flags2:02x})', end='')
                        
                        # effect_flags1 & 0x20 = Tapping/slapping/popping
                        if effect_flags1 & 0x20:
                            tap = struct.unpack('<B', f.read(1))[0]
                            print(f' tap={tap}', end='')
                        
                        # effect_flags2 & 0x04 = TremoloBar
                        if effect_flags2 & 0x04:
                            # Skip bend points (complex)
                            num_points = struct.unpack('<i', f.read(4))[0]
                            for _ in range(num_points):
                                f.read(4+4+1)  # position, value, vibrato
                            print(f' tremolo({num_points}pts)', end='')
                        
                        # effect_flags1 & 0x01 = Vibrato
                        # effect_flags1 & 0x02 = Wide vibrato
                        # effect_flags2 & 0x01 = Rasgueado (bool)
                        if effect_flags2 & 0x01:
                            print(' rasgueado', end='')
                        
                        # effect_flags2 & 0x02 = Pickstroke
                        if effect_flags2 & 0x02:
                            pickstroke = struct.unpack('<B', f.read(1))[0]
                            print(f' pickstroke={pickstroke}', end='')
                    
                    # 0x10 = mix table change
                    if flags & 0x10:
                        print(' [mix table - not implemented]', end='')
                        return  # bail
                    
                    # Notes (if not rest or empty)
                    if not (flags & 0x40):  # Not a rest
                        note_flags = struct.unpack('<B', f.read(1))[0]
                        note_count = bin(note_flags).count('1')
                        print(f' noteFlags=0x{note_flags:02x}({note_count}notes)', end='')
                        
                        for s in range(7):
                            if note_flags & (1 << (6-s)):
                                # Read note
                                nflags = struct.unpack('<B', f.read(1))[0]
                                
                                # 0x20 = accentuatedNote or noteType
                                if nflags & 0x20:
                                    note_type = struct.unpack('<B', f.read(1))[0]
                                
                                # 0x04 = ghost note (no extra read)
                                
                                # 0x01 = duration percent
                                if nflags & 0x01:
                                    duration_percent = struct.unpack('<B', f.read(1))[0]
                                    duration_percent2 = struct.unpack('<B', f.read(1))[0]
                                
                                # 0x10 = velocity
                                if nflags & 0x10:
                                    velocity = struct.unpack('<b', f.read(1))[0]
                                
                                # fret (always read)
                                fret = struct.unpack('<b', f.read(1))[0]
                                
                                # 0x80 = fingering
                                if nflags & 0x80:
                                    left_finger = struct.unpack('<B', f.read(1))[0]
                                    right_finger = struct.unpack('<B', f.read(1))[0]
                                
                                # Note flags2 (for GP5)
                                nflags2 = struct.unpack('<B', f.read(1))[0]
                                
                                # 0x08 = note effects
                                if nflags & 0x08:
                                    # Read note effects (complex)
                                    ne_flags1 = struct.unpack('<B', f.read(1))[0]
                                    ne_flags2 = struct.unpack('<B', f.read(1))[0]
                                    
                                    # Many possible effects...
                                    if ne_flags1 & 0x01:  # bend
                                        num_points = struct.unpack('<i', f.read(4))[0]
                                        for _ in range(num_points):
                                            f.read(4+4+1)
                                    if ne_flags1 & 0x04:  # grace note
                                        f.read(4)  # fret, velocity, transition, duration
                                    if ne_flags1 & 0x10:  # let ring (bool)
                                        pass
                                    if ne_flags2 & 0x01:  # staccato (bool)
                                        pass
                                    if ne_flags2 & 0x02:  # palm mute (bool)
                                        pass
                                    if ne_flags2 & 0x04:  # tremolo picking
                                        f.read(1)
                                    if ne_flags2 & 0x08:  # slide
                                        f.read(1)
                                    if ne_flags2 & 0x10:  # harmonic
                                        f.read(1)  # harmonic type
                                    if ne_flags2 & 0x20:  # trill
                                        f.read(3)  # fret + period + duration
                    
                    # Beat flags2 (short)
                    flags2 = struct.unpack('<H', f.read(2))[0]
                    print(f' flags2=0x{flags2:04x}')
            
            # Line break byte after each measure
            linebreak = struct.unpack('<B', f.read(1))[0]
            print(f'  LineBreak: {linebreak}')
        
        print(f'\n=== Done at position {f.tell()} ===')

# Compare both files
print('='*60)
print('Recording.gp5 (16 measures)')
print('='*60)
parse_measures('D:/GitHub/NewProject/Recording.gp5', 1466, 16)  # All 16
