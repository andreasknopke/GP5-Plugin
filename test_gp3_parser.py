#!/usr/bin/env python3
"""
GP3 Parser Test - Verify GP3 parsing against PyGuitarPro
"""

import sys
sys.path.insert(0, 'd:/GitHub/NewProject')

import guitarpro

def analyze_gp3(filename):
    """Analyze a GP3 file using PyGuitarPro as reference"""
    print(f"\n=== Analyzing: {filename} ===\n")
    
    song = guitarpro.parse(filename)
    
    print(f"Version: {song.version}")
    print(f"Title: {song.title}")
    print(f"Artist: {song.artist}")
    print(f"Album: {song.album}")
    print(f"Tempo: {song.tempo}")
    print(f"Key: {song.key}")
    print(f"Track count: {len(song.tracks)}")
    print(f"Measure count: {len(song.measureHeaders)}")
    
    print("\n--- Tracks ---")
    for i, track in enumerate(song.tracks):
        print(f"Track {i+1}: {track.name}")
        print(f"  Strings: {len(track.strings)}")
        print(f"  Tuning: {[s.value for s in track.strings]}")
        print(f"  Is Percussion: {track.isPercussionTrack}")
        print(f"  Channel: {track.channel.channel if track.channel else 'N/A'}")
    
    print("\n--- First 5 Measure Headers ---")
    for i, header in enumerate(song.measureHeaders[:5]):
        print(f"Measure {header.number}: {header.timeSignature.numerator}/{header.timeSignature.denominator.value}")
        if header.marker:
            print(f"  Marker: {header.marker.title}")
        if header.isRepeatOpen:
            print(f"  Repeat Open")
        if header.repeatClose > 0:
            print(f"  Repeat Close: {header.repeatClose}x")
    
    print("\n--- Notes in first 10 measures (Track 1) ---")
    if song.tracks:
        track = song.tracks[0]
        for m_idx, measure in enumerate(track.measures[:10]):
            notes_in_measure = []
            for voice in measure.voices:
                for beat in voice.beats:
                    for note in beat.notes:
                        notes_in_measure.append(f"s{note.string}:f{note.value}")
            if notes_in_measure:
                print(f"  Measure {m_idx+1}: {', '.join(notes_in_measure)}")
            else:
                print(f"  Measure {m_idx+1}: (empty/rest)")
    
    return song

if __name__ == '__main__':
    filename = 'd:/GitHub/NewProject/test_partial.gp3'
    try:
        song = analyze_gp3(filename)
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
