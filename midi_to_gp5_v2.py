#!/usr/bin/env python3
"""
Convert a MIDI file to GP5 format using PyGuitarPro.
Simplified version - create basic beats without complex effects.
"""

import mido
import guitarpro as gp
from collections import defaultdict

def midi_to_gp5(midi_path: str, output_path: str):
    """Convert MIDI file to GP5."""
    
    # Load MIDI file
    mid = mido.MidiFile(midi_path)
    print(f"MIDI file: {midi_path}")
    print(f"  Type: {mid.type}")
    print(f"  Ticks per beat: {mid.ticks_per_beat}")
    print(f"  Tracks: {len(mid.tracks)}")
    
    # Find tempo
    tempo = 120  # default
    time_sig_num = 4
    time_sig_den = 4
    
    for track in mid.tracks:
        for msg in track:
            if msg.type == 'set_tempo':
                tempo = round(60000000 / msg.tempo)
                print(f"  Tempo: {tempo} BPM")
            elif msg.type == 'time_signature':
                time_sig_num = msg.numerator
                time_sig_den = msg.denominator
                print(f"  Time Signature: {time_sig_num}/{time_sig_den}")
    
    # Parse MIDI notes by track
    print("\nParsing MIDI tracks...")
    for i, track in enumerate(mid.tracks):
        note_count = 0
        for msg in track:
            if msg.type == 'note_on' and msg.velocity > 0:
                note_count += 1
        if note_count > 0:
            print(f"  Track {i}: '{track.name}' - {note_count} notes")
    
    # Get the first non-empty MIDI track
    midi_track = None
    midi_track_name = "Track 1"
    for i, track in enumerate(mid.tracks):
        note_count = sum(1 for msg in track if msg.type == 'note_on' and msg.velocity > 0)
        if note_count > 0:
            midi_track = track
            midi_track_name = track.name if track.name else f"Track {i}"
            print(f"\nUsing MIDI track: '{midi_track_name}'")
            break
    
    if midi_track is None:
        print("No notes found in MIDI file!")
        return
    
    # Collect all notes with absolute timing
    notes = []
    abs_time = 0
    active_notes = {}  # pitch -> (start_time, velocity)
    
    for msg in midi_track:
        abs_time += msg.time
        
        if msg.type == 'note_on' and msg.velocity > 0:
            active_notes[msg.note] = (abs_time, msg.velocity)
        elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
            if msg.note in active_notes:
                start_time, velocity = active_notes.pop(msg.note)
                duration = abs_time - start_time
                notes.append({
                    'pitch': msg.note,
                    'start': start_time,
                    'duration': duration,
                    'velocity': velocity
                })
    
    print(f"Collected {len(notes)} notes")
    
    if not notes:
        print("No notes to convert!")
        return
    
    # Sort notes by start time
    notes.sort(key=lambda n: (n['start'], n['pitch']))
    
    # Calculate measures
    ticks_per_beat = mid.ticks_per_beat
    ticks_per_measure = ticks_per_beat * time_sig_num * (4 // time_sig_den)
    
    max_time = max(n['start'] + n['duration'] for n in notes)
    num_measures = int((max_time // ticks_per_measure) + 1)
    print(f"Number of measures needed: {num_measures}")
    
    # Create a fresh song from scratch
    song = gp.Song()
    song.title = "MIDI Import"
    song.artist = "Converted from MIDI"
    song.tempo = tempo
    
    # Get the track and configure it
    gp_track = song.tracks[0]
    gp_track.name = midi_track_name[:40]
    
    # Standard guitar tuning (high E to low E: 64, 59, 55, 50, 45, 40)
    gp_track.strings = [
        gp.GuitarString(1, 64),  # E4
        gp.GuitarString(2, 59),  # B3
        gp.GuitarString(3, 55),  # G3
        gp.GuitarString(4, 50),  # D3
        gp.GuitarString(5, 45),  # A2
        gp.GuitarString(6, 40),  # E2
    ]
    
    # Add measures (song starts with 1)
    for _ in range(num_measures - 1):
        song.newMeasure()
    
    print(f"Created {len(gp_track.measures)} measures")
    
    # Group notes by measure
    notes_by_measure = defaultdict(list)
    for note in notes:
        measure_num = int(note['start'] // ticks_per_measure)
        if measure_num < num_measures:
            notes_by_measure[measure_num].append(note)
    
    # Process each measure
    for measure_idx, measure in enumerate(gp_track.measures):
        measure_start = measure_idx * ticks_per_measure
        measure_notes = notes_by_measure.get(measure_idx, [])
        
        voice = measure.voices[0]
        voice.beats.clear()
        
        if not measure_notes:
            # Empty measure - whole rest
            rest_beat = gp.Beat(voice)
            rest_beat.status = gp.BeatStatus.rest
            rest_beat.duration.value = 1  # whole note
            voice.beats.append(rest_beat)
        else:
            # Group notes by start time
            notes_by_time = defaultdict(list)
            for note in measure_notes:
                rel_time = note['start'] - measure_start
                notes_by_time[rel_time].append(note)
            
            # Create beats
            for rel_time in sorted(notes_by_time.keys()):
                time_notes = notes_by_time[rel_time]
                
                beat = gp.Beat(voice)
                beat.duration.value = 4  # quarter note
                beat.status = gp.BeatStatus.normal
                
                for note_data in time_notes:
                    pitch = note_data['pitch']
                    velocity = note_data['velocity']
                    
                    # Find best string/fret for this pitch
                    best_string = None
                    best_fret = None
                    
                    for string_idx, guitar_string in enumerate(gp_track.strings):
                        fret = pitch - guitar_string.value
                        if 0 <= fret <= 24:
                            if best_string is None or fret < best_fret:
                                best_string = string_idx + 1
                                best_fret = fret
                    
                    if best_string is not None:
                        note = gp.Note(beat)
                        note.string = best_string
                        note.value = best_fret
                        note.velocity = velocity
                        note.type = gp.NoteType.normal
                        beat.notes.append(note)
                
                if beat.notes:
                    voice.beats.append(beat)
            
            if not voice.beats:
                rest_beat = gp.Beat(voice)
                rest_beat.status = gp.BeatStatus.rest
                rest_beat.duration.value = 1
                voice.beats.append(rest_beat)
    
    # Write as GP5
    print(f"\nWriting to {output_path}...")
    gp.write(song, output_path, version=(5, 0, 0))
    
    # Check file size
    import os
    size = os.path.getsize(output_path)
    print(f"Output file size: {size} bytes")
    
    # Try to verify by parsing it back
    print("\nVerifying output file...")
    try:
        song2 = gp.parse(output_path)
        print(f"  Title: {song2.title}")
        print(f"  Tracks: {len(song2.tracks)}")
        print(f"  Measures: {len(song2.tracks[0].measures)}")
        
        total_notes = 0
        for measure in song2.tracks[0].measures:
            for voice in measure.voices:
                for beat in voice.beats:
                    total_notes += len(beat.notes)
        print(f"  Total notes: {total_notes}")
        print("\n✓ MIDI to GP5 conversion successful!")
    except Exception as e:
        print(f"  Verification failed: {e}")
        print("\n⚠ File was written but could not be verified")

if __name__ == "__main__":
    midi_to_gp5(
        r"D:\GitHub\NewProject\test_partial.mid",
        r"D:\GitHub\NewProject\test_midi_converted.gp5"
    )
