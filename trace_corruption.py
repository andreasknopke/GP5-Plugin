#!/usr/bin/env python3
"""Instrument PyGuitarPro to find exact byte position of corruption in After.gp5"""

import guitarpro
from guitarpro import gp3, gp5, models as gp

AFTER = r'D:\GitHub\NewProject\After.gp5'

# Monkey-patch the stream to log positions
class TracingGP5(gp5.GP5File):
    def readMeasure(self, measure):
        track_idx = measure.track.number - 1
        measure_idx = measure.header.number - 1
        self._current_track = track_idx
        self._current_measure = measure_idx
        try:
            super().readMeasure(measure)
        except Exception as e:
            pos = self.stream.tell()
            print(f"  ERROR at stream pos {pos} (0x{pos:x}): {e}")
            # Print surrounding bytes
            self.stream.seek(pos - 16)
            before_bytes = self.stream.read(32)
            print(f"  Bytes around error: {before_bytes.hex(' ')}")
            raise

    def readVoice(self, start, voice):
        pos = self.stream.tell()
        beats = self.readI32()
        track_idx = getattr(self, '_current_track', -1)
        measure_idx = getattr(self, '_current_measure', -1)
        voice_idx = voice.number
        
        if track_idx == 3 and measure_idx >= 58 and measure_idx <= 62:
            print(f"  T{track_idx} M{measure_idx+1} V{voice_idx}: {beats} beats at pos {pos} (0x{pos:x})")
        
        for beat_number in range(beats):
            try:
                start += self.readBeat(start, voice)
            except Exception as e:
                pos2 = self.stream.tell()
                print(f"    Beat {beat_number} ERROR at pos {pos2}: {e}")
                raise
        return voice

# Also trace track 3 from measure 1 to find where things go wrong
class DeepTracingGP5(gp5.GP5File):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._last_good_pos = 0
        self._error_track = -1
        self._error_measure = -1
    
    def readMeasure(self, measure):
        track_idx = measure.track.number - 1
        measure_idx = measure.header.number - 1
        self._current_track = track_idx
        self._current_measure = measure_idx
        start_pos = self.stream.tell()
        
        try:
            super().readMeasure(measure)
            end_pos = self.stream.tell()
            measure_bytes = end_pos - start_pos
            
            # Log every measure for track where error happens
            if track_idx <= 3 and measure_idx >= 55 and measure_idx <= 65:
                print(f"  T{track_idx} M{measure_idx+1}: {measure_bytes} bytes [{start_pos}-{end_pos}]")
            
            self._last_good_pos = end_pos
        except Exception as e:
            self._error_track = track_idx
            self._error_measure = measure_idx
            raise

# Parse with tracing
print("=== Parsing After.gp5 with tracing ===")
try:
    stream = open(AFTER, 'rb')
    parser = DeepTracingGP5(stream)
    song = parser.readSong()
    print("SUCCESS!")
except Exception as e:
    print(f"\nFINAL ERROR: {e}")
    print(f"Last good pos: {parser._last_good_pos}")
finally:
    stream.close()

# Now also trace what the reference file looks like at the same position
print("\n=== Same region in reference ===")
REFERENCE = r'D:\GitHub\NewProject\reference_pyguitarpro.gp5'
import guitarpro
ref_song = guitarpro.parse(r'D:\GitHub\NewProject\before.gp5')
guitarpro.write(ref_song, REFERENCE)

try:
    stream2 = open(REFERENCE, 'rb')
    parser2 = DeepTracingGP5(stream2)
    song2 = parser2.readSong()
    print("REF parse SUCCESS!")
except Exception as e:
    print(f"REF ERROR: {e}")
finally:
    stream2.close()
