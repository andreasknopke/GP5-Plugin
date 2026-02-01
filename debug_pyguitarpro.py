#!/usr/bin/env python3
"""Debug PyGuitarPro parsing step by step."""

import struct
import io

# Read Recording.gp5
with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

print(f"File size: {len(data)} bytes")

# Create a wrapper to track position
class DebugStream:
    def __init__(self, data):
        self.stream = io.BytesIO(data)
        
    def read(self, n):
        return self.stream.read(n)
    
    def tell(self):
        return self.stream.tell()
    
    def seek(self, pos):
        return self.stream.seek(pos)
    
    def readU8(self):
        return struct.unpack('<B', self.read(1))[0]
    
    def readI8(self):
        return struct.unpack('<b', self.read(1))[0]
    
    def readI16(self):
        return struct.unpack('<h', self.read(2))[0]
    
    def readI32(self):
        return struct.unpack('<i', self.read(4))[0]

MEASURES_START = 1466

stream = DebugStream(data)
stream.seek(MEASURES_START)

print(f"\n=== Starting measure parsing at position {MEASURES_START} ===")

num_measures = 16
num_tracks = 1

for measure_idx in range(num_measures):
    measure_start = stream.tell()
    print(f"\n--- Measure {measure_idx + 1} (pos {measure_start}) ---")
    
    for track_idx in range(num_tracks):
        for voice_idx in range(2):
            voice_start = stream.tell()
            beat_count = stream.readI32()
            print(f"  Voice {voice_idx + 1}: {beat_count} beats (pos {voice_start})")
            
            for beat_idx in range(beat_count):
                beat_start = stream.tell()
                flags = stream.readU8()
                
                is_rest = bool(flags & 0x40)
                has_duration = bool(flags & 0x20)
                has_effect = bool(flags & 0x08)
                has_mix_change = bool(flags & 0x10)
                has_text = bool(flags & 0x04)
                has_chord = bool(flags & 0x02)
                is_dotted = bool(flags & 0x01)
                
                status = 0
                if is_rest:
                    status = stream.readU8()
                    if status not in (0, 1, 2):
                        print(f"    !!! Beat {beat_idx + 1} at pos {beat_start}: flags=0x{flags:02x}, INVALID status={status} (0x{status:02x})")
                        print(f"    Context bytes: {list(data[beat_start:beat_start+30])}")
                        exit(1)
                
                duration = stream.readI8()
                
                if has_duration:
                    tuplet = stream.readI32()
                
                if has_chord:
                    print(f"      chord - not implemented")
                    exit(1)
                    
                if has_text:
                    text_len = stream.readI32()
                    text = stream.read(text_len)
                
                if has_effect:
                    eff1 = stream.readU8()
                    eff2 = stream.readU8()
                    if eff1 & 0x20:
                        stream.read(2)
                    if eff2 & 0x04:
                        stream.readU8()
                        stream.readI32()
                        num_pts = stream.readI32()
                        stream.read(num_pts * 9)
                    if eff1 & 0x01:
                        stream.readU8()
                
                if has_mix_change:
                    print(f"      mix change - not implemented")
                    exit(1)
                
                if not is_rest or status == 1:
                    string_flags = stream.readU8()
                    note_count = bin(string_flags).count('1')
                    
                    for string_num in range(7):
                        if string_flags & (1 << (6 - string_num)):
                            note_flags = stream.readU8()
                            
                            has_type = bool(note_flags & 0x20)
                            has_velocity = bool(note_flags & 0x10)
                            has_fingering = bool(note_flags & 0x80)
                            has_duration_percent = bool(note_flags & 0x01)
                            has_note_effect = bool(note_flags & 0x08)
                            
                            if has_type:
                                stream.readU8()
                            if has_velocity:
                                stream.readI8()
                            if has_type:  # fret uses same flag as type
                                stream.readI8()
                            if has_fingering:
                                stream.read(2)
                            if has_duration_percent:
                                stream.readI8()
                            
                            stream.readU8()  # flags2
                            
                            if has_note_effect:
                                neff1 = stream.readU8()
                                neff2 = stream.readU8()
                                # Skip effects based on flags
                                if neff1 & 0x01:  # bend
                                    stream.readU8()
                                    stream.readI32()
                                    num_pts = stream.readI32()
                                    stream.read(num_pts * 9)
                                if neff1 & 0x10:  # grace
                                    stream.read(4)
                                if neff2 & 0x04:  # harmonic
                                    stream.readU8()
                                if neff2 & 0x08:  # trill
                                    stream.read(3)
                                if neff1 & 0x02:  # hammer
                                    pass
                                if neff2 & 0x01:  # let ring
                                    pass
                                if neff1 & 0x04:  # slide
                                    stream.readI8()
                                if neff2 & 0x02:  # staccato
                                    pass
                
                flags2 = stream.readI16()
                print(f"    Beat {beat_idx + 1} @ {beat_start}: flags=0x{flags:02x} rest={is_rest} status={status} dur={duration} flags2=0x{flags2:04x}")
    
    line_break = stream.readU8()

print(f"\n=== Finished at position {stream.tell()} (file size: {len(data)}) ===")
