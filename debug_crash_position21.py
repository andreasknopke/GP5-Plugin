#!/usr/bin/env python3
"""
Debug-Script: Analysiert Track 3 (Melody) bei Position 21
um den Cubase-Crash zu finden.

Der Crash passiert bei: 0xFFFFFFFFFFFFFFFF = -1 als Pointer
Das deutet auf einen ungültigen Index hin (stringIndex = -1 oder fret = -1)
"""

import guitarpro
import sys

def analyze_track_detailed(track, track_name, start_measure=0, end_measure=None):
    """Analysiert einen Track im Detail"""
    print(f"\n{'='*60}")
    print(f"Track: {track_name}")
    print(f"{'='*60}")
    
    # Tuning info
    print(f"\nTuning ({len(track.strings)} strings):")
    for i, s in enumerate(track.strings):
        print(f"  String {i}: MIDI {s.value} (Note: {midi_to_note(s.value)})")
    
    if end_measure is None:
        end_measure = len(track.measures)
    
    total_beats = 0
    beat_position = 0.0  # In quarter notes from song start
    
    for m_idx in range(min(end_measure, len(track.measures))):
        measure = track.measures[m_idx]
        measure_start_beat = beat_position
        
        for voice_idx, voice in enumerate(measure.voices):
            if not voice.beats:
                continue
                
            for beat_idx, beat in enumerate(voice.beats):
                total_beats += 1
                
                # Calculate beat duration
                duration = get_duration_in_beats(beat)
                
                # Show details for measures around position 21
                # Position 21 could be beat position or measure
                show_this = (m_idx >= start_measure - 2 and m_idx <= start_measure + 5)
                
                if show_this or beat.notes:
                    if beat.notes:
                        for note in beat.notes:
                            # Check for problematic values
                            string_idx = note.string - 1  # GP uses 1-based
                            fret = note.value
                            
                            problems = []
                            if string_idx < 0:
                                problems.append(f"INVALID STRING INDEX: {string_idx}")
                            if string_idx >= len(track.strings):
                                problems.append(f"STRING OUT OF RANGE: {string_idx} >= {len(track.strings)}")
                            if fret < 0:
                                problems.append(f"INVALID FRET: {fret}")
                            if fret > 30:
                                problems.append(f"FRET TOO HIGH: {fret}")
                            
                            # Calculate MIDI note
                            if string_idx >= 0 and string_idx < len(track.strings):
                                tuning_value = track.strings[string_idx].value
                                midi_note = tuning_value + fret
                                if midi_note < 0 or midi_note >= 128:
                                    problems.append(f"MIDI NOTE OUT OF RANGE: {midi_note}")
                            else:
                                midi_note = -1
                                problems.append("CANNOT CALCULATE MIDI NOTE")
                            
                            effects = get_note_effects(note)
                            
                            status = "*** PROBLEM ***" if problems else ""
                            if show_this or problems:
                                print(f"\nMeasure {m_idx+1}, Beat {beat_idx} (pos ~{beat_position:.2f})")
                                print(f"  Note: string={note.string} (idx={string_idx}), fret={fret}")
                                print(f"  MIDI Note: {midi_note} ({midi_to_note(midi_note)})")
                                print(f"  Duration: {duration:.3f} beats (value={beat.duration.value}, dotted={beat.duration.isDotted})")
                                if effects:
                                    print(f"  Effects: {', '.join(effects)}")
                                if problems:
                                    for p in problems:
                                        print(f"  !!! {p} !!!")
                                if note.effect.bend:
                                    print(f"  Bend: {note.effect.bend}")
                
                beat_position += duration
        
    print(f"\n{'='*60}")
    print(f"Total beats in track: {total_beats}")
    print(f"Total beat positions: {beat_position:.2f}")
    print(f"{'='*60}")

def get_duration_in_beats(beat):
    """Calculate beat duration in quarter notes"""
    # duration.value: 1=whole, 2=half, 4=quarter, 8=eighth, etc.
    base_duration = 4.0 / beat.duration.value if beat.duration.value > 0 else 1.0
    
    if beat.duration.isDotted:
        base_duration *= 1.5
    
    if beat.duration.tuplet:
        # Tuplet: n notes in the time of m
        enters = beat.duration.tuplet.enters
        times = beat.duration.tuplet.times
        if enters > 0:
            base_duration *= times / enters
    
    return base_duration

def get_note_effects(note):
    """Get list of note effects"""
    effects = []
    if note.effect.bend:
        effects.append("bend")
    if note.effect.hammer:
        effects.append("hammer-on")
    if hasattr(note.effect, 'slide') and note.effect.slide:
        effects.append(f"slide")
    if note.effect.vibrato:
        effects.append("vibrato")
    if note.effect.harmonic:
        effects.append(f"harmonic")
    if note.effect.accentuatedNote:
        effects.append("accent")
    if note.effect.heavyAccentuatedNote:
        effects.append("heavy-accent")
    if note.effect.ghostNote:
        effects.append("ghost")
    if note.effect.palmMute:
        effects.append("palm-mute")
    if note.type.value == 3:  # Dead note
        effects.append("dead")
    if note.type.value == 2:  # Tied
        effects.append("tied")
    return effects

def midi_to_note(midi):
    """Convert MIDI note number to note name"""
    if midi < 0 or midi >= 128:
        return "INVALID"
    notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    octave = (midi // 12) - 1
    note = notes[midi % 12]
    return f"{note}{octave}"

def main():
    print("Loading test_partial.gp5...")
    song = guitarpro.parse('d:/GitHub/NewProject/test_partial.gp5')
    
    print(f"\nSong: {song.title} by {song.artist}")
    print(f"Tempo: {song.tempo} BPM")
    print(f"Tracks: {len(song.tracks)}")
    
    for i, track in enumerate(song.tracks):
        print(f"  Track {i+1}: {track.name}")
    
    # Analyze Track 3 (Melody) - index 2
    if len(song.tracks) >= 3:
        track = song.tracks[2]
        
        # Focus on measures around position 21
        # Check first 30 measures to find any issues
        analyze_track_detailed(track, "Track 3 (Melody)", start_measure=18, end_measure=30)
        
        # Also dump raw data for first few measures
        print("\n\n=== RAW DATA DUMP: First 5 measures ===")
        for m_idx in range(min(5, len(track.measures))):
            measure = track.measures[m_idx]
            print(f"\nMeasure {m_idx+1}:")
            for voice_idx, voice in enumerate(measure.voices):
                print(f"  Voice {voice_idx}: {len(voice.beats)} beats")
                for beat_idx, beat in enumerate(voice.beats):
                    notes_info = []
                    for n in beat.notes:
                        notes_info.append(f"str{n.string}:fret{n.value}")
                    print(f"    Beat {beat_idx}: {notes_info if notes_info else 'REST'} dur={beat.duration.value}")
    else:
        print("Track 3 not found!")

if __name__ == "__main__":
    main()
