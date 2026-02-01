import io
import struct

# Manually replicate PyGuitarPro's parsing to find the exact issue

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

class DebugStream:
    def __init__(self, data):
        self.data = data
        self.pos = 0
        
    def read(self, n):
        result = self.data[self.pos:self.pos+n]
        self.pos += n
        return result
    
    def tell(self):
        return self.pos
    
    def seek(self, pos):
        self.pos = pos

stream = DebugStream(data)

def readU8():
    return struct.unpack('<B', stream.read(1))[0]

def readI8():
    return struct.unpack('<b', stream.read(1))[0]

def readI16():
    return struct.unpack('<h', stream.read(2))[0]

def readI32():
    return struct.unpack('<i', stream.read(4))[0]

def readByteSizeString(maxLen):
    length = readU8()
    s = stream.read(length).decode('latin1', errors='replace')
    stream.read(maxLen - length)  # Skip padding
    return s

def readIntByteSizeString():
    total = readI32()
    if total <= 0:
        return ''
    length = readU8()
    s = stream.read(length).decode('latin1', errors='replace')
    stream.read(total - length - 1)  # Skip padding
    return s

# Skip to measure data start
stream.seek(1474)

print("Starting measure parsing at position 1474")
print("=" * 70)

measure_num = 0
beat_global = 0

try:
    for m in range(16):  # 16 measures
        measure_num = m + 1
        measure_start = stream.tell()
        
        print(f"\n=== MEASURE {measure_num} at pos {measure_start} ===")
        
        for voice_num in range(2):  # 2 voices
            voice_start = stream.tell()
            beat_count = readI32()
            
            print(f"  Voice {voice_num+1} at pos {voice_start}: {beat_count} beats")
            
            if beat_count < 0 or beat_count > 100:
                print(f"    ERROR: Invalid beat count!")
                raise ValueError(f"Invalid beat count: {beat_count}")
            
            for b in range(beat_count):
                beat_global += 1
                beat_start = stream.tell()
                
                flags = readU8()
                
                flag_names = []
                if flags & 0x01: flag_names.append("dotted")
                if flags & 0x02: flag_names.append("CHORD")
                if flags & 0x04: flag_names.append("text")
                if flags & 0x08: flag_names.append("effects")
                if flags & 0x10: flag_names.append("mixChange")
                if flags & 0x20: flag_names.append("tuplet")
                if flags & 0x40: flag_names.append("rest")
                
                print(f"    Beat {b+1} (global {beat_global}) at pos {beat_start}: flags=0x{flags:02x} [{', '.join(flag_names)}]")
                
                # Rest/empty status
                if flags & 0x40:
                    status = readI8()
                    print(f"      beatStatus={status}")
                
                # Duration
                duration = readI8()
                print(f"      duration={duration}")
                
                # Tuplet
                if flags & 0x20:
                    tuplet = readI32()
                    print(f"      tuplet={tuplet}")
                
                # Chord
                if flags & 0x02:
                    print(f"      CHORD at pos {stream.tell()}")
                    chord_byte = readU8()
                    print(f"        First byte: {chord_byte}")
                    if chord_byte == 0:
                        # Old chord format
                        print("        Old chord format")
                        name = readIntByteSizeString()
                        print(f"        Name: '{name}'")
                    else:
                        print("        New chord format - complex")
                    raise ValueError("Chord found!")
                
                # Text
                if flags & 0x04:
                    text = readIntByteSizeString()
                    print(f"      text='{text}'")
                
                # Beat effects
                if flags & 0x08:
                    ef1 = readU8()
                    ef2 = readU8()
                    print(f"      effects: flags1=0x{ef1:02x} flags2=0x{ef2:02x}")
                    
                    if ef1 & 0x20:  # Stroke
                        stream.read(2)
                    if ef2 & 0x04:  # Tremolo bar
                        readU8()  # type
                        readI32()  # value
                        n = readI32()  # points
                        stream.read(n * 8)
                    if ef2 & 0x08:  # Pickstroke
                        readU8()
                
                # Mix table change
                if flags & 0x10:
                    print(f"      MIX TABLE CHANGE at pos {stream.tell()}")
                    raise ValueError("Mix table change found!")
                
                # Notes
                if not (flags & 0x40):
                    string_bits = readU8()
                    note_count = bin(string_bits).count('1')
                    print(f"      stringBits=0x{string_bits:02x} ({note_count} notes)")
                    
                    for n in range(note_count):
                        note_start = stream.tell()
                        nflags = readU8()
                        
                        print(f"        Note {n+1} at {note_start}: flags=0x{nflags:02x}", end="")
                        
                        if nflags & 0x20:  # Type
                            ntype = readU8()
                            print(f" type={ntype}", end="")
                        
                        if nflags & 0x10:  # Velocity
                            vel = readI8()
                            print(f" vel={vel}", end="")
                        
                        if nflags & 0x20:  # Fret
                            fret = readI8()
                            print(f" fret={fret}", end="")
                        
                        if nflags & 0x80:  # Fingering
                            stream.read(2)
                            print(f" fingering", end="")
                        
                        if nflags & 0x01:  # Duration percent (GP5)
                            stream.read(8)
                            print(f" durPct", end="")
                        
                        # flags2 (GP5)
                        nflags2 = readU8()
                        print(f" flags2=0x{nflags2:02x}", end="")
                        
                        # Note effects
                        if nflags & 0x08:
                            e1 = readU8()
                            e2 = readU8()
                            print(f" effects=0x{e1:02x},0x{e2:02x}", end="")
                            
                            if e1 & 0x01:  # Bend
                                readU8()
                                readI32()
                                np = readI32()
                                stream.read(np * 8)
                            if e1 & 0x10:  # Grace
                                stream.read(5)
                            if e2 & 0x04:  # Tremolo picking
                                readU8()
                            if e2 & 0x08:  # Slide
                                readU8()
                            if e2 & 0x10:  # Harmonic
                                ht = readU8()
                                if ht == 2:
                                    stream.read(3)
                                elif ht == 3:
                                    stream.read(1)
                            if e2 & 0x20:  # Trill
                                stream.read(3)
                        
                        print()
                
                # Beat flags2 (GP5)
                beat_flags2 = readI16()
                print(f"      beat_flags2=0x{beat_flags2:04x}")
        
        # LineBreak
        linebreak = readU8()
        print(f"  LineBreak: {linebreak}")

except Exception as e:
    print(f"\n\nERROR at position {stream.tell()}: {e}")
    print(f"Bytes around error: {' '.join(f'{data[stream.tell()-5+i]:02x}' for i in range(20))}")

print(f"\n\nFinal position: {stream.tell()}")
print(f"File size: {len(data)}")
