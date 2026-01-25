#!/usr/bin/env python3
"""Quick debug: Read track 3 (Melody) measures 40-50"""

import guitarpro

# Load the file
song = guitarpro.parse('d:/GitHub/NewProject/test_partial.gp5')

print(f"Song: {song.title} by {song.artist}")
print(f"Tracks: {len(song.tracks)}")

for i, track in enumerate(song.tracks):
    print(f"  Track {i+1}: {track.name} ({track.channel.instrument})")

# Get track 3 (index 2)
if len(song.tracks) >= 3:
    track = song.tracks[2]  # 0-indexed, so track 3 is index 2
    print(f"\n=== Track 3: {track.name} ===")
    print(f"Strings: {len(track.strings)}")
    print(f"Tuning: {[s.value for s in track.strings]}")
    
    # Measures 40-50
    for m_idx in range(39, min(50, len(track.measures))):
        measure = track.measures[m_idx]
        print(f"\n--- Measure {m_idx + 1} ---")
        
        for voice in measure.voices:
            if voice.beats:
                for beat_idx, beat in enumerate(voice.beats):
                    if beat.notes:
                        notes_str = ", ".join(f"s{n.string}:f{n.value}" for n in beat.notes)
                        effects = []
                        for n in beat.notes:
                            if n.effect.bend:
                                effects.append(f"bend")
                            if n.effect.hammer:
                                effects.append(f"hammer")
                            if n.effect.slide:
                                effects.append(f"slide")
                        effects_str = f" [{', '.join(effects)}]" if effects else ""
                        print(f"  Beat {beat_idx}: {notes_str}{effects_str}")
                        if beat.text:
                            print(f"    Text: {beat.text}")
