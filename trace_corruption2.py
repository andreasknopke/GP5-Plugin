#!/usr/bin/env python3
"""Instrument PyGuitarPro to find exact byte position of corruption in After.gp5"""

import guitarpro
from guitarpro import gp3, gp5, models as gp
import io

AFTER = r'D:\GitHub\NewProject\After.gp5'
BEFORE = r'D:\GitHub\NewProject\before.gp5'
REFERENCE = r'D:\GitHub\NewProject\reference_pyguitarpro.gp5'

# Re-save reference
ref_song = guitarpro.parse(BEFORE)
guitarpro.write(ref_song, REFERENCE)

def trace_parse(filepath, label):
    """Parse a GP5 file with position tracing around the error area."""
    print(f"\n=== Tracing {label}: {filepath} ===")
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Use the normal guitarpro.parse but monkey-patch the readMeasure
    original_readMeasure = gp5.GP5File.readMeasure
    original_readVoice = gp3.GP3File.readVoice
    original_readBeat = gp5.GP5File.readBeat
    
    measure_positions = {}
    
    def patched_readMeasure(self, measure):
        track_idx = measure.track.number - 1
        measure_idx = measure.header.number - 1
        start_pos = self.stream.tell()
        
        try:
            original_readMeasure(self, measure)
            end_pos = self.stream.tell()
            measure_bytes = end_pos - start_pos
            
            key = (track_idx, measure_idx)
            measure_positions[key] = (start_pos, end_pos, measure_bytes)
            
            # Log around error area
            if measure_idx >= 55 and measure_idx <= 65:
                print(f"  T{track_idx} M{measure_idx+1}: {measure_bytes} bytes [{start_pos}-{end_pos}]")
            
        except Exception as e:
            end_pos = self.stream.tell()
            print(f"  T{track_idx} M{measure_idx+1}: ERROR at pos {end_pos} (0x{end_pos:x})")
            print(f"    Error: {e}")
            # Context bytes  
            ctx_start = max(0, end_pos - 32)
            print(f"    Bytes before error [{ctx_start}:{end_pos}]: {data[ctx_start:end_pos].hex(' ')}")
            print(f"    Bytes after error [{end_pos}:{end_pos+32}]: {data[end_pos:end_pos+32].hex(' ')}")
            raise
    
    def patched_readVoice(self, start, voice):
        pos = self.stream.tell()
        beats = self.readI32()
        
        # Check for suspicious beat count  
        if beats > 100 or beats < 0:
            print(f"    WARNING: suspicious beat count {beats} at pos {pos}")
        
        for beat_number in range(beats):
            try:
                start += self.readBeat(start, voice)
            except Exception:
                pos2 = self.stream.tell()
                print(f"    Beat {beat_number} error at pos {pos2}")
                raise
        return voice
    
    gp5.GP5File.readMeasure = patched_readMeasure
    gp3.GP3File.readVoice = patched_readVoice
    
    try:
        song = guitarpro.parse(filepath)
        print(f"  Parse SUCCESS! Tracks={len(song.tracks)} Measures={len(song.measureHeaders)}")
        return song, measure_positions
    except Exception as e:
        print(f"  Parse FAILED: {e}")
        return None, measure_positions
    finally:
        gp5.GP5File.readMeasure = original_readMeasure
        gp3.GP3File.readVoice = original_readVoice

ref_result, ref_positions = trace_parse(REFERENCE, "REFERENCE")
aft_result, aft_positions = trace_parse(AFTER, "AFTER")

# Compare positions where both succeeded
print(f"\n=== Position comparison (measures 55-65) ===")
for m in range(55, 66):
    for t in range(6):
        key = (t, m)
        ref_p = ref_positions.get(key)
        aft_p = aft_positions.get(key)
        if ref_p and aft_p:
            diff = aft_p[2] - ref_p[2]  # byte size difference
            if diff != 0:
                print(f"  T{t} M{m+1}: ref={ref_p[2]}b aft={aft_p[2]}b DIFF={diff:+d}")
        elif ref_p and not aft_p:
            print(f"  T{t} M{m+1}: ref={ref_p[2]}b aft=ERROR")

# Also compare earlier measures to find first difference
print(f"\n=== First size mismatch (checking all measures) ===")
first_mismatch = None
for m in range(156):
    for t in range(6):
        key = (t, m)
        ref_p = ref_positions.get(key)
        aft_p = aft_positions.get(key)
        if ref_p and aft_p:
            if ref_p[2] != aft_p[2]:
                if first_mismatch is None:
                    first_mismatch = (t, m)
                    print(f"  FIRST: T{t} M{m+1}: ref={ref_p[2]}b (at {ref_p[0]}) aft={aft_p[2]}b (at {aft_p[0]}) diff={aft_p[2]-ref_p[2]:+d}")
                    # Show next few
                elif m < first_mismatch[1] + 5:
                    print(f"         T{t} M{m+1}: ref={ref_p[2]}b aft={aft_p[2]}b diff={aft_p[2]-ref_p[2]:+d}")

if first_mismatch is None:
    print("  No size mismatches found in individual measures!")
