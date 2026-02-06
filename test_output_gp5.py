#!/usr/bin/env python3
"""Test if a GP5 file is readable and analyze its structure."""

import sys
import guitarpro

def analyze_file(filepath):
    print(f"Analyzing: {filepath}")
    print("=" * 60)
    
    try:
        song = guitarpro.parse(filepath)
        print(f"✓ File loaded successfully!")
        print(f"Title: {song.title}")
        print(f"Artist: {song.artist}")
        print(f"Tempo: {song.tempo}")
        print(f"Number of tracks: {len(song.tracks)}")
        print(f"Number of measure headers: {len(song.measureHeaders)}")
        
        for i, track in enumerate(song.tracks):
            print(f"\nTrack {i+1}: {track.name}")
            print(f"  Strings: {track.strings}")
            print(f"  Channel: {track.channel.channel}")
            print(f"  Measures: {len(track.measures)}")
            
            # Count notes
            total_notes = 0
            for measure in track.measures:
                for voice in measure.voices:
                    for beat in voice.beats:
                        for note in beat.notes:
                            total_notes += 1
            
            print(f"  Total notes: {total_notes}")
            
            # Show first few beats with notes
            notes_shown = 0
            for m_idx, measure in enumerate(track.measures):
                if notes_shown > 5:
                    break
                for voice in measure.voices:
                    for beat in voice.beats:
                        if beat.notes:
                            print(f"  Measure {m_idx+1}: {len(beat.notes)} notes - ", end="")
                            for note in beat.notes:
                                print(f"str{note.string}:fret{note.value} ", end="")
                            print()
                            notes_shown += 1
                            if notes_shown > 5:
                                break
        
        return True
        
    except Exception as e:
        print(f"✗ Error loading file: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_output_gp5.py <file.gp5>")
        sys.exit(1)
    
    analyze_file(sys.argv[1])
