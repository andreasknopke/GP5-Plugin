#!/usr/bin/env python3
"""
Test: Read test_partial.gp5 and write it back.
If PyGuitarPro can read the output, we have a correct GP5 writer.
"""

import guitarpro

# Step 1: Parse the test file
print("Step 1: Parsing test_partial.gp5...")
song = guitarpro.parse(r"D:\GitHub\NewProject\test_partial.gp5")

print(f"  Title: {song.title}")
print(f"  Artist: {song.artist}")
print(f"  Tempo: {song.tempo}")
print(f"  Tracks: {len(song.tracks)}")
print(f"  Measures per track: {len(song.tracks[0].measures)}")

# Print first track info
track = song.tracks[0]
print(f"\n  Track 1: {track.name}")
print(f"    Strings: {len(track.strings)}")
print(f"    Frets: {track.fretCount}")

# Step 2: Write it back
print("\nStep 2: Writing to test_roundtrip.gp5...")
guitarpro.write(song, r"D:\GitHub\NewProject\test_roundtrip.gp5")

# Step 3: Parse the output to verify
print("\nStep 3: Verifying test_roundtrip.gp5...")
song2 = guitarpro.parse(r"D:\GitHub\NewProject\test_roundtrip.gp5")

print(f"  Title: {song2.title}")
print(f"  Artist: {song2.artist}")
print(f"  Tempo: {song2.tempo}")
print(f"  Tracks: {len(song2.tracks)}")
print(f"  Measures per track: {len(song2.tracks[0].measures)}")

# Compare
if song.title == song2.title and len(song.tracks) == len(song2.tracks):
    print("\n✓ Round-trip successful! PyGuitarPro can write valid GP5 files.")
else:
    print("\n✗ Round-trip failed!")

# Step 4: Print file sizes
import os
orig_size = os.path.getsize(r"D:\GitHub\NewProject\test_partial.gp5")
new_size = os.path.getsize(r"D:\GitHub\NewProject\test_roundtrip.gp5")
print(f"\nFile sizes: Original={orig_size}, New={new_size}")
