#!/usr/bin/env python3
"""Instrument PyGuitarPro to find exact byte position of corruption in After.gp5"""

import guitarpro
from guitarpro import gp3, gp5, models as gp

AFTER = r'D:\GitHub\NewProject\After.gp5'
BEFORE = r'D:\GitHub\NewProject\before.gp5'
REFERENCE = r'D:\GitHub\NewProject\reference_pyguitarpro.gp5'

# Re-save reference
ref_song = guitarpro.parse(BEFORE)
guitarpro.write(ref_song, REFERENCE)

def trace_parse(filepath, label):
    """Parse a GP5 file with position tracing."""
    print(f"\n=== Tracing {label} ===")
    
    original_readMeasure = gp5.GP5File.readMeasure
    original_readVoice = gp3.GP3File.readVoice
    
    measure_positions = {}
    
    def patched_readMeasure(self, measure):
        track_idx = measure.track.number - 1
        measure_idx = measure.header.number - 1
        start_pos = self.data.tell()
        
        try:
            original_readMeasure(self, measure)
            end_pos = self.data.tell()
            measure_bytes = end_pos - start_pos
            
            key = (track_idx, measure_idx)
            measure_positions[key] = (start_pos, end_pos, measure_bytes)
            
        except Exception as e:
            end_pos = self.data.tell()
            print(f"  T{track_idx} M{measure_idx+1}: ERROR at pos {end_pos} (0x{end_pos:x})")
            raise
    
    gp5.GP5File.readMeasure = patched_readMeasure
    
    try:
        song = guitarpro.parse(filepath)
        print(f"  Parse SUCCESS! Tracks={len(song.tracks)} Measures={len(song.measureHeaders)}")
        return song, measure_positions
    except Exception as e:
        print(f"  Parse FAILED: {e}")
        return None, measure_positions
    finally:
        gp5.GP5File.readMeasure = original_readMeasure

ref_result, ref_positions = trace_parse(REFERENCE, "REFERENCE")
aft_result, aft_positions = trace_parse(AFTER, "AFTER")

# Find first size mismatch
print(f"\n=== Measure size mismatches ===")
mismatch_count = 0
for m in range(156):
    for t in range(6):
        key = (t, m)
        ref_p = ref_positions.get(key)
        aft_p = aft_positions.get(key)
        if ref_p and aft_p:
            if ref_p[2] != aft_p[2]:
                mismatch_count += 1
                if mismatch_count <= 30:
                    print(f"  T{t} M{m+1}: ref={ref_p[2]}b (at {ref_p[0]}) aft={aft_p[2]}b (at {aft_p[0]}) diff={aft_p[2]-ref_p[2]:+d}")
        elif ref_p and not aft_p:
            if mismatch_count <= 30:
                print(f"  T{t} M{m+1}: ref={ref_p[2]}b aft=MISSING (parse failed before this)")
            mismatch_count += 1

print(f"\nTotal mismatches: {mismatch_count}")

# Show the error area in detail
print(f"\n=== Around error (T3 M61) ===")
for m in range(55, 66):
    for t in range(6):
        key = (t, m)
        ref_p = ref_positions.get(key)
        aft_p = aft_positions.get(key)
        ref_s = f"ref={ref_p[2]}b@{ref_p[0]}" if ref_p else "ref=N/A"
        aft_s = f"aft={aft_p[2]}b@{aft_p[0]}" if aft_p else "aft=N/A"
        marker = " <<<" if (ref_p and aft_p and ref_p[2] != aft_p[2]) else ""
        marker = " *** ERROR ***" if (ref_p and not aft_p) else marker
        print(f"  T{t} M{m+1}: {ref_s} {aft_s}{marker}")

# Also, let's examine the cumulative offset
print(f"\n=== Cumulative offset (ref_pos - aft_pos) ===")
prev_offset = None
for m in range(156):
    for t in range(6):
        key = (t, m)
        ref_p = ref_positions.get(key)
        aft_p = aft_positions.get(key)
        if ref_p and aft_p:
            offset = ref_p[0] - aft_p[0]
            if prev_offset is None or offset != prev_offset:
                print(f"  T{t} M{m+1}: ref@{ref_p[0]} - aft@{aft_p[0]} = {offset} bytes ahead")
                prev_offset = offset
