#!/usr/bin/env python3
"""
Byte-level trace of After.gp5 measure data to find where it diverges.
"""

import guitarpro
import struct
import os

BEFORE = r'D:\GitHub\NewProject\before.gp5'
AFTER = r'D:\GitHub\NewProject\After.gp5'
REFERENCE = r'D:\GitHub\NewProject\reference_pyguitarpro.gp5'

# Re-save reference
song = guitarpro.parse(BEFORE)
guitarpro.write(song, REFERENCE)

# Load reference for structure info
with open(REFERENCE, 'rb') as f:
    ref = f.read()
with open(AFTER, 'rb') as f:
    aft = f.read()

print(f"REF: {len(ref)} bytes, AFT: {len(aft)} bytes")
print(f"Tracks: {len(song.tracks)}, Measures: {len(song.measureHeaders)}")

# Helper functions
class Reader:
    def __init__(self, data, label):
        self.data = data
        self.pos = 0
        self.label = label
    
    def u8(self):
        v = self.data[self.pos]
        self.pos += 1
        return v
    
    def i8(self):
        v = struct.unpack_from('<b', self.data, self.pos)[0]
        self.pos += 1
        return v
    
    def i16(self):
        v = struct.unpack_from('<h', self.data, self.pos)[0]
        self.pos += 2
        return v
    
    def i32(self):
        v = struct.unpack_from('<i', self.data, self.pos)[0]
        self.pos += 4
        return v
    
    def skip(self, n):
        self.pos += n
    
    def ibs(self):
        """IntByteSizeString"""
        total = self.i32()
        slen = self.u8()
        s = self.data[self.pos:self.pos+slen].decode('latin-1', errors='replace')
        self.pos += slen
        return s
    
    def bs(self, maxlen):
        """ByteSizeString"""
        slen = self.u8()
        s = self.data[self.pos:self.pos+slen].decode('latin-1', errors='replace')
        self.pos += maxlen
        return s

def skip_header(r):
    """Skip to measure data section."""
    # Version
    r.bs(30)
    # Info: 9 IntByteSizeStrings
    for _ in range(9):
        r.ibs()
    # Notice
    n = r.i32()
    for _ in range(n):
        r.ibs()
    # Lyrics
    r.i32()  # track
    for _ in range(5):
        r.i32()  # start
        slen = r.i32()
        r.skip(slen)
    # Page setup: 7 ints + 1 short
    r.skip(4*4 + 4*3)
    r.i16()  # headerFooterElements
    # 11 page setup strings  
    for _ in range(11):
        r.ibs()
    # Tempo
    r.ibs()  # tempoName
    r.i32()  # tempo
    # Key + octave
    r.u8()
    r.i32()
    # MIDI channels (64)
    for _ in range(64):
        r.i32()
        r.skip(8)
    # Directions (19 shorts)
    for _ in range(19):
        r.i16()
    # Master reverb
    r.i32()
    # Counts
    measure_count = r.i32()
    track_count = r.i32()
    
    print(f"  {r.label}: measures={measure_count}, tracks={track_count} (pos={r.pos})")
    
    # Measure headers
    for m in range(measure_count):
        if m > 0:
            r.u8()  # placeholder
        flags = r.u8()
        if flags & 0x01:
            r.u8()  # numerator
        if flags & 0x02:
            r.u8()  # denominator
        if flags & 0x08:
            r.u8()  # repeat close
        if flags & 0x20:
            r.ibs()  # marker
            r.skip(4)  # color
        if flags & 0x40:
            r.skip(2)  # key sig
        if flags & 0x10:
            r.u8()  # repeat alt
        if flags & 0x03:
            r.skip(4)  # beams
        if not (flags & 0x10):
            r.u8()  # placeholder
        r.u8()  # triplet feel
    
    print(f"  {r.label}: after measure headers (pos={r.pos})")
    
    # Tracks
    for t in range(track_count):
        r.u8()  # placeholder
        r.u8()  # flags1
        name = r.bs(40)
        num_strings = r.i32()
        for _ in range(7):
            r.i32()  # tuning
        r.i32()  # port
        r.i32()  # channel
        r.i32()  # effect channel
        r.i32()  # frets
        r.i32()  # capo
        r.skip(4)  # color
        r.i16()  # flags2
        r.u8()  # auto accent
        r.u8()  # bank
        r.u8()  # humanize
        r.skip(4*3)  # 3 ints
        r.skip(12)   # 12 bytes
        r.skip(4*3)  # RSE instrument (3 ints)
        r.i16()  # effectNumber
        r.u8()   # placeholder
        if t < 2:
            print(f"    Track {t}: '{name}' strings={num_strings}")
    
    # After last track: 2 placeholder bytes for GP5.0.0
    r.u8()
    r.u8()
    
    print(f"  {r.label}: measures data starts at pos={r.pos}")
    return measure_count, track_count

ref_r = Reader(ref, "REF")
aft_r = Reader(aft, "AFT")

ref_mc, ref_tc = skip_header(ref_r)
aft_mc, aft_tc = skip_header(aft_r)

# Now trace measure data beat-by-beat
def read_beat(r, track_idx=-1, measure_idx=-1, voice_idx=-1, beat_idx=-1):
    """Read one beat and return bytes consumed. Returns None on error."""
    start_pos = r.pos
    try:
        flags = r.u8()
        
        if flags & 0x40:
            status = r.u8()
        
        duration = r.i8()
        
        if flags & 0x20:
            tuplet = r.i32()
        
        if flags & 0x02:
            # Chord - this is complex!
            r.u8()  # format
            if r.data[r.pos-1] == 0:
                # Old format
                r.ibs()
                r.skip(0)  # old chord format is simple
            else:
                # New format (GP4+)
                r.u8()  # sharp
                r.skip(3)  # blank
                r.u8()  # root
                r.u8()  # type  
                r.u8()  # extension
                r.i32()  # bass
                r.i32()  # tonality
                r.u8()  # added
                name = r.bs(20)
                r.skip(2)  # blank
                r.u8()  # fifth tonality
                r.u8()  # ninth tonality
                r.u8()  # eleventh tonality
                r.skip(0)
                # base fret
                r.i32()
                # frets (7 ints)
                for _ in range(7):
                    r.i32()
                # barres
                barre_count = r.u8()
                for _ in range(5):
                    r.u8()  # barre fret
                for _ in range(5):
                    r.u8()  # barre start
                for _ in range(5):
                    r.u8()  # barre end
                # omissions (7 bytes)
                r.skip(7)
                r.skip(1)  # blank
        
        if flags & 0x04:
            r.ibs()  # text
        
        if flags & 0x08:
            # Beat effects  
            f1 = r.u8()
            f2 = r.u8()
            if f1 & 0x20:  # slap
                r.u8()
            if f1 & 0x40:  # stroke
                r.u8()  # down
                r.u8()  # up
            if f2 & 0x04:  # tremolo bar
                r.u8()  # type
                r.i32()  # value
                n = r.i32()
                for _ in range(n):
                    r.i32()
                    r.i32()
                    r.u8()
        
        if flags & 0x10:
            # Mix table change
            instrument = r.i8()
            r.skip(16)  # RSE instrument stuff
            volume = r.i8()
            balance = r.i8()
            chorus = r.i8()
            reverb = r.i8()
            phaser = r.i8()
            tremolo = r.i8()
            r.ibs()  # tempo name
            tempo = r.i32()
            if volume >= 0: r.u8()
            if balance >= 0: r.u8()
            if chorus >= 0: r.u8()
            if reverb >= 0: r.u8()
            if phaser >= 0: r.u8()
            if tremolo >= 0: r.u8()
            if tempo >= 0:
                r.u8()  # hideTempo
                r.u8()  # tempo transition
            # GP5: RSE stuff
            r.u8()  # wah
            
        string_flags = r.u8()
        
        # Read notes for each set bit
        for s in range(6, -1, -1):
            if string_flags & (1 << s):
                # Read note
                nflags = r.u8()
                if nflags & 0x20:
                    r.u8()  # note type
                if nflags & 0x10:
                    r.u8()  # velocity (dynamic)
                if nflags & 0x20:
                    r.i8()  # fret
                if nflags & 0x80:
                    r.u8()  # left finger
                    r.u8()  # right finger
                if nflags & 0x01:
                    r.skip(8)  # duration percent + swap accent
                # flags2  
                r.u8()
                if nflags & 0x08:
                    # Note effects
                    nef1 = r.u8()
                    nef2 = r.u8()
                    if nef1 & 0x01:  # bend
                        r.u8()  # type
                        r.i32()  # value
                        n = r.i32()
                        for _ in range(n):
                            r.i32()
                            r.i32()
                            r.u8()
                    if nef1 & 0x10:  # grace note
                        r.u8()  # fret
                        r.u8()  # velocity
                        r.u8()  # transition
                        r.u8()  # duration
                        r.u8()  # flags (GP5)
                    if nef2 & 0x04:  # tremolo picking
                        r.u8()  # value
                    if nef2 & 0x08:  # slide
                        r.u8()  # type
                    if nef2 & 0x10:  # harmonic
                        r.u8()  # type
                    if nef2 & 0x20:  # trill
                        r.u8()  # fret
                        r.u8()  # period
        
        # GP5: flags2 (short)
        r.i16()
        
        return r.pos - start_pos
    except Exception as e:
        print(f"  ERROR at pos {r.pos}: {e}")
        return None

# Compare measures beat-by-beat
print(f"\n=== Comparing measure data ===")
found_error = False

for m in range(min(ref_mc, aft_mc)):
    for t in range(min(ref_tc, aft_tc)):
        ref_mstart = ref_r.pos
        aft_mstart = aft_r.pos
        
        for v in range(2):
            ref_beat_count = ref_r.i32()
            aft_beat_count = aft_r.i32()
            
            if ref_beat_count != aft_beat_count:
                print(f"  M{m+1} T{t+1} V{v+1}: BEAT COUNT DIFF! ref={ref_beat_count} aft={aft_beat_count} (ref@{ref_mstart+4} aft@{aft_mstart+4})")
                found_error = True
            
            for b in range(ref_beat_count):
                ref_bstart = ref_r.pos
                ref_size = read_beat(ref_r, t, m, v, b)
                if ref_size is None:
                    print(f"  ERROR reading REF at M{m+1} T{t+1} V{v+1} B{b}")
                    found_error = True
                    break
            
            for b in range(aft_beat_count):
                aft_bstart = aft_r.pos
                aft_size = read_beat(aft_r, t, m, v, b)
                if aft_size is None:
                    print(f"  ERROR reading AFT at M{m+1} T{t+1} V{v+1} B{b}")
                    found_error = True
                    break
            
            if found_error:
                break
        
        if not found_error:
            # Line break
            ref_r.u8()
            aft_r.u8()
        
        if found_error:
            # Show context around error
            print(f"\n  Context:")
            print(f"  REF pos={ref_r.pos}: {ref[ref_r.pos:ref_r.pos+32].hex(' ')}")
            print(f"  AFT pos={aft_r.pos}: {aft[aft_r.pos:aft_r.pos+32].hex(' ')}")
            break
    
    if found_error:
        break

if not found_error:
    print("  All measures matched structurally!")
