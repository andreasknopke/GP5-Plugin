#!/usr/bin/env python3
"""
Simulate what the C++ writer would produce with the fixes:
- Voice 2: 1 empty beat (instead of 0)
- Harmonic: correct extra bytes for artificial/tapped
- Velocity: only write when != 95

Parse the original file, then check if the fixes would make the output correct.
"""

import guitarpro
import struct

BEFORE = r'D:\GitHub\NewProject\before.gp5'
AFTER = r'D:\GitHub\NewProject\After.gp5'
REFERENCE = r'D:\GitHub\NewProject\reference_pyguitarpro.gp5'

# Parse original
song = guitarpro.parse(BEFORE)

# Count bytes that were wrong
print("=== Analysis of fixes needed ===")

# 1. Voice 2 fix: Each measure for each track needs 6 extra bytes (was 4 bytes for writeInt(0), now 10 bytes)
num_measures = len(song.measureHeaders)
num_tracks = len(song.tracks)
voice2_fix_bytes = num_measures * num_tracks * 6  # 6 extra bytes per measure per track
print(f"Voice 2 fix: +{voice2_fix_bytes} bytes ({num_measures} measures × {num_tracks} tracks × 6 bytes)")

# 2. Velocity fix: count notes with default velocity (95) that were getting velocity written
# When we stop writing velocity for default (95), we save 1 byte per such note
notes_with_default_vel = 0
notes_with_custom_vel = 0
for track in song.tracks:
    for measure in track.measures:
        for voice in measure.voices:
            for beat in voice.beats:
                for note in beat.notes:
                    if note.velocity == 95:
                        notes_with_default_vel += 1
                    else:
                        notes_with_custom_vel += 1

print(f"Velocity fix: default(95)={notes_with_default_vel} notes (save 1 byte each)")
print(f"              custom vel={notes_with_custom_vel} notes")
print(f"              Net velocity fix: -{notes_with_default_vel} bytes")

# 3. Harmonic fix: count harmonics that need extra bytes
artificial_harmonics = 0
tapped_harmonics = 0
for track in song.tracks:
    for measure in track.measures:
        for voice in measure.voices:
            for beat in voice.beats:
                for note in beat.notes:
                    if note.effect.harmonic:
                        h = note.effect.harmonic
                        if isinstance(h, guitarpro.models.ArtificialHarmonic):
                            artificial_harmonics += 1
                        elif isinstance(h, guitarpro.models.TappedHarmonic):
                            tapped_harmonics += 1

harmonic_fix = artificial_harmonics * 3 + tapped_harmonics * 1
print(f"Harmonic fix: {artificial_harmonics} artificial (+3 bytes each) + {tapped_harmonics} tapped (+1 byte each) = +{harmonic_fix} bytes")

# Total adjustment
total_fix = voice2_fix_bytes - notes_with_default_vel + harmonic_fix
print(f"\nTotal byte adjustment from fixes: +{total_fix}")

# Compare expected sizes
with open(REFERENCE, 'rb') as f:
    ref = f.read()
with open(AFTER, 'rb') as f:
    aft = f.read()

print(f"\nReference: {len(ref)} bytes")
print(f"After (current): {len(aft)} bytes") 
print(f"After + fixes: ~{len(aft) + total_fix} bytes")
print(f"Diff from ref: {len(aft) + total_fix - len(ref)} bytes")

# The remaining diff would be: chords, text, mixTable, grace notes, etc.
# Let's count those
print(f"\n=== Remaining data not written by our writer ===")
chord_count = 0
text_count = 0
mixtable_count = 0
grace_count = 0
trill_count = 0
tremolo_count = 0
duration_percent = 0
fingering_count = 0
palm_mute_count = 0

for track in song.tracks:
    for measure in track.measures:
        for voice in measure.voices:
            for beat in voice.beats:
                if beat.effect.chord:
                    chord_count += 1
                if beat.text:
                    text_count += 1
                if beat.effect.mixTableChange:
                    mixtable_count += 1
                for note in beat.notes:
                    if note.effect.grace:
                        grace_count += 1
                    if note.effect.trill:
                        trill_count += 1
                    if note.effect.tremoloPicking:
                        tremolo_count += 1
                    if abs(note.durationPercent - 1.0) >= 0.001:
                        duration_percent += 1
                    if note.effect.palmMute:
                        palm_mute_count += 1

print(f"  Chords: {chord_count}")
print(f"  Texts: {text_count}")
print(f"  MixTable changes: {mixtable_count}")
print(f"  Grace notes: {grace_count}")
print(f"  Trills: {trill_count}")
print(f"  Tremolo picking: {tremolo_count}")
print(f"  Duration percent != 1.0: {duration_percent}")
print(f"  Palm mute: {palm_mute_count}")
