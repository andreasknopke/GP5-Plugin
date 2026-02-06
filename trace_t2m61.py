#!/usr/bin/env python3
"""Deep dive into T2 M61 - the exact measure where parsing fails."""

import guitarpro
import struct

BEFORE = r'D:\GitHub\NewProject\before.gp5'
AFTER = r'D:\GitHub\NewProject\After.gp5'
REFERENCE = r'D:\GitHub\NewProject\reference_pyguitarpro.gp5'

# Parse original and get track 2 measure 61 details
song = guitarpro.parse(BEFORE)
guitarpro.write(song, REFERENCE)

print("=== Original Track 2 (index 2), Measures 57-65 ===")
track2 = song.tracks[2]
for mi in range(56, 65):
    m = track2.measures[mi]
    for vi, voice in enumerate(m.voices):
        beats = voice.beats
        if not beats:
            continue
        beat_details = []
        for bi, beat in enumerate(beats):
            status = beat.status.name
            dur = beat.duration.value
            notes = [(n.string, n.value, n.type.name) for n in beat.notes]
            effects = []
            if beat.effect.chord:
                effects.append("chord")
            if beat.effect.mixTableChange:
                effects.append("mixTable")
            if beat.text:
                effects.append(f"text='{beat.text.value}'")
            for n in beat.notes:
                if n.effect.bend:
                    effects.append(f"bend(s{n.string})")
                if n.effect.slides:
                    effects.append(f"slide(s{n.string})")
                if n.effect.harmonic:
                    effects.append(f"harmonic(s{n.string})")
                if n.effect.grace:
                    effects.append(f"grace(s{n.string})")
                if n.effect.trill:
                    effects.append(f"trill(s{n.string})")
                if n.effect.letRing:
                    effects.append(f"letRing(s{n.string})")
                if n.effect.palmMute:
                    effects.append(f"palmMute(s{n.string})")
                if n.type == guitarpro.models.NoteType.tie:
                    effects.append(f"tie(s{n.string})")
            
            detail = f"B{bi}:{status} d={dur} n={notes}"
            if effects:
                detail += f" fx={effects}"
            beat_details.append(detail)
        
        print(f"  M{mi+1} V{vi+1}: {len(beats)} beats")
        for d in beat_details:
            print(f"    {d}")

# Now let's check what our writer SHOULD be producing for these measures
# by examining the convertToTabTrack output
print("\n\n=== Comparing ref and after byte data for T2 around M61 ===")

with open(REFERENCE, 'rb') as f:
    ref = f.read()
with open(AFTER, 'rb') as f:
    aft = f.read()

# Get positions from the trace (from previous run)
# T1 M61: aft starts at 17553, 39 bytes, ends at 17592
# So T2 M61 starts at 17592 in aft
# T2 M61: ref starts at 28204

# Let me verify by finding the actual measure start positions
from guitarpro import gp3, gp5

def get_all_measure_positions(filepath):
    """Get stream position for each measure"""
    original_readMeasure = gp5.GP5File.readMeasure
    positions = {}
    
    def patched(self, measure):
        t = measure.track.number - 1
        m = measure.header.number - 1
        pos = self.data.tell()
        try:
            original_readMeasure(self, measure)
            end = self.data.tell()
            positions[(t, m)] = (pos, end)
        except:
            positions[(t, m)] = (pos, self.data.tell(), 'ERROR')
            raise
    
    gp5.GP5File.readMeasure = patched
    try:
        guitarpro.parse(filepath)
    except:
        pass
    finally:
        gp5.GP5File.readMeasure = original_readMeasure
    return positions

ref_pos = get_all_measure_positions(REFERENCE)
aft_pos = get_all_measure_positions(AFTER)

# T2 M61 (index 2, index 60)
key = (2, 60)
print(f"\nT2 M61 positions:")
if key in ref_pos:
    rp = ref_pos[key]
    print(f"  REF: {rp[0]} to {rp[1]} ({rp[1]-rp[0]} bytes)")
    # Show the raw bytes
    print(f"  REF bytes: {ref[rp[0]:rp[1]].hex(' ')}")
if key in aft_pos:
    ap = aft_pos[key]
    if len(ap) == 3:
        print(f"  AFT: starts at {ap[0]}, error at {ap[1]}")
        # Show bytes from start to start+150
        print(f"  AFT bytes: {aft[ap[0]:ap[0]+150].hex(' ')}")
    else:
        print(f"  AFT: {ap[0]} to {ap[1]} ({ap[1]-ap[0]} bytes)")
        print(f"  AFT bytes: {aft[ap[0]:ap[1]].hex(' ')}")

# Also check T2 M58 (aft is bigger: 191 vs 186)
print(f"\n\nT2 M58 (aft 191b vs ref 186b):")
key58 = (2, 57)
if key58 in ref_pos and key58 in aft_pos:
    rp = ref_pos[key58]
    ap = aft_pos[key58]
    print(f"  REF: {rp[1]-rp[0]} bytes: {ref[rp[0]:rp[1]].hex(' ')}")
    print(f"  AFT: {ap[1]-ap[0]} bytes: {aft[ap[0]:ap[1]].hex(' ')}")

# Let me also check M1 T0 as a simple case
print(f"\n\nT0 M1 (simple case, ref=27 aft=21):")
key01 = (0, 0)
if key01 in ref_pos and key01 in aft_pos:
    rp = ref_pos[key01]
    ap = aft_pos[key01]
    print(f"  REF: {rp[1]-rp[0]} bytes: {ref[rp[0]:rp[1]].hex(' ')}")
    print(f"  AFT: {ap[1]-ap[0]} bytes: {aft[ap[0]:ap[1]].hex(' ')}")
