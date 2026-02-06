#!/usr/bin/env python3
"""Compare bytes of individual measures between reference and After.gp5"""

import guitarpro
from guitarpro import gp5

BEFORE = r'D:\GitHub\NewProject\before.gp5'
AFTER = r'D:\GitHub\NewProject\After.gp5'
REFERENCE = r'D:\GitHub\NewProject\reference_pyguitarpro.gp5'

song = guitarpro.parse(BEFORE)
guitarpro.write(song, REFERENCE)

with open(REFERENCE, 'rb') as f:
    ref = f.read()
with open(AFTER, 'rb') as f:
    aft = f.read()

def get_measure_positions(filepath):
    original = gp5.GP5File.readMeasure
    positions = {}
    def patched(self, measure):
        t = measure.track.number - 1
        m = measure.header.number - 1
        pos = self.data.tell()
        try:
            original(self, measure)
            positions[(t, m)] = (pos, self.data.tell())
        except:
            positions[(t, m)] = (pos, self.data.tell(), 'ERROR')
            raise
    gp5.GP5File.readMeasure = patched
    try:
        guitarpro.parse(filepath)
    except:
        pass
    finally:
        gp5.GP5File.readMeasure = original
    return positions

ref_pos = get_measure_positions(REFERENCE)
aft_pos = get_measure_positions(AFTER)

# Compare bytes for specific measures
def show_measure(label, data, start, end):
    chunk = data[start:end]
    print(f"  {label} [{start}:{end}] ({end-start}b): {chunk.hex(' ')}")

# Annotate a measure's bytes
def annotate_measure(data, start, end):
    """Parse a GP5 measure from raw bytes and annotate."""
    import struct
    pos = start
    result = []
    
    for voice_num in range(2):
        beat_count = struct.unpack_from('<i', data, pos)[0]
        result.append(f"    V{voice_num+1} beats={beat_count} @{pos}")
        pos += 4
        
        for b in range(beat_count):
            bstart = pos
            flags = data[pos]; pos += 1
            flag_str = []
            if flags & 0x01: flag_str.append("dotted")
            if flags & 0x02: flag_str.append("chord")
            if flags & 0x04: flag_str.append("text")
            if flags & 0x08: flag_str.append("effects")
            if flags & 0x10: flag_str.append("mixTable")
            if flags & 0x20: flag_str.append("tuplet")
            if flags & 0x40: flag_str.append("rest/empty")
            
            status = None
            if flags & 0x40:
                status = data[pos]; pos += 1
                
            dur = struct.unpack_from('b', data, pos)[0]; pos += 1
            
            if flags & 0x20:
                tuplet = struct.unpack_from('<i', data, pos)[0]; pos += 4
            
            # Skip chord, text, effects, mixTable (complex - just note positions)
            if flags & 0x02:
                result.append(f"      B{b}: HAS CHORD (complex, stopping annotation)")
                break
            if flags & 0x04:
                result.append(f"      B{b}: HAS TEXT (complex)")
                break
            if flags & 0x08:
                # Beat effects
                ef1 = data[pos]; ef2 = data[pos+1]; pos += 2
                if ef1 & 0x20: pos += 1  # slap
                if ef1 & 0x40: pos += 2  # stroke
                if ef2 & 0x04:  # tremolo bar
                    pos += 1  # type
                    pos += 4  # value
                    n = struct.unpack_from('<i', data, pos)[0]; pos += 4
                    pos += n * 9  # points
            if flags & 0x10:
                result.append(f"      B{b}: HAS MIXTABLE (complex)")
                break
            
            # String flags
            sf = data[pos]; pos += 1
            note_strings = []
            for s in range(6, -1, -1):
                if sf & (1 << s):
                    note_strings.append(7 - s)
                    # Read note
                    nflags = data[pos]; pos += 1
                    if nflags & 0x20:
                        nt = data[pos]; pos += 1  # noteType
                    if nflags & 0x10:
                        vel = data[pos]; pos += 1  # velocity
                    if nflags & 0x20:
                        fret = struct.unpack_from('b', data, pos)[0]; pos += 1  # fret
                    if nflags & 0x80:
                        pos += 2  # fingering
                    if nflags & 0x01:
                        pos += 8  # durationPercent (double)
                        pos += 1  # swapAccent
                    # flags2
                    nf2 = data[pos]; pos += 1
                    if nflags & 0x08:
                        # Note effects  
                        nef1 = data[pos]; nef2 = data[pos+1]; pos += 2
                        if nef1 & 0x01:  # bend
                            pos += 1 + 4  # type + value
                            n = struct.unpack_from('<i', data, pos)[0]; pos += 4
                            pos += n * 9
                        if nef1 & 0x10:  # grace
                            pos += 5  # GP5: fret + vel + trans + dur + flags
                        if nef2 & 0x04: pos += 1  # tremolo picking
                        if nef2 & 0x08: pos += 1  # slide
                        if nef2 & 0x10: pos += 1  # harmonic
                        if nef2 & 0x20: pos += 2  # trill
            
            # GP5 flags2 (short)
            pos += 2
            
            status_str = f" status={status}" if status is not None else ""
            result.append(f"      B{b}: flags=0x{flags:02x}({','.join(flag_str)}){status_str} dur={dur} strings={note_strings} ({pos-bstart}b)")
    
    # Linebreak
    if pos < end:
        lb = data[pos]; pos += 1
        result.append(f"    Linebreak={lb} @{pos-1}")
    
    if pos != end:
        result.append(f"    WARNING: pos={pos} but end={end} (diff={end-pos})")
    
    return '\n'.join(result)

# Show a few specific measures
measures_to_check = [(0, 0), (0, 4), (2, 57), (2, 59), (2, 60)]

for t, m in measures_to_check:
    key = (t, m)
    print(f"\n=== T{t} M{m+1} ===")
    
    if key in ref_pos:
        rp = ref_pos[key]
        rsize = rp[1] - rp[0] if len(rp) == 2 else rp[1] - rp[0]
        show_measure("REF", ref, rp[0], rp[1] if len(rp) == 2 else rp[1])
        try:
            print(annotate_measure(ref, rp[0], rp[1]))
        except Exception as e:
            print(f"  REF annotation error: {e}")
    
    if key in aft_pos:
        ap = aft_pos[key]
        end = ap[1] if len(ap) == 2 else ap[1]
        show_measure("AFT", aft, ap[0], end)
        try:
            print(annotate_measure(aft, ap[0], end))
        except Exception as e:
            print(f"  AFT annotation error: {e}")
