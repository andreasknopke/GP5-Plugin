#!/usr/bin/env python3
"""
Simuliert die C++ MIDI-Generation um den Crash zu finden.
Fokus: Was passiert wenn "Position jumped backwards" auftritt?

Der Crash ist bei 0xFFFFFFFFFFFFFFFF = -1 als size_t/pointer
Das bedeutet ein Iterator oder Index ist -1
"""

import guitarpro

def simulate_midi_generation():
    print("=== Simulating C++ MIDI Generation ===\n")
    
    song = guitarpro.parse('d:/GitHub/NewProject/test_partial.gp5')
    track = song.tracks[2]  # Track 3 (Melody)
    
    print(f"Track: {track.name}")
    print(f"Strings: {len(track.strings)}")
    print(f"Tuning: {[s.value for s in track.strings]}")
    print(f"Measures: {len(track.measures)}")
    
    # Simulate state like MidiExpressionEngine
    active_notes = {}  # channel -> set of notes
    pending_note_offs = []  # list of (channel, note, scheduledBeat)
    
    # Configuration
    midi_channel = 3
    transpose_offset = 0
    volume_scale = 100
    beats_per_second = 148.0 / 60.0
    
    # The key question: What does the C++ code do at position 0?
    # When "Position jumped backwards" occurs, allNotesOff is called
    # Then the position becomes 0 (or wherever DAW jumped to)
    
    print("\n=== Simulating position jump to 0 ===")
    print("This is what happens when Cubase starts playback from beginning")
    
    # Clear state (like allNotesOff)
    active_notes.clear()
    pending_note_offs.clear()
    
    current_beat = 0.0
    time_sig_num = 4
    time_sig_den = 4
    beats_per_measure = time_sig_num * (4.0 / time_sig_den)
    
    # Now simulate what processBlock does
    for measure_idx in range(min(5, len(track.measures))):
        measure = track.measures[measure_idx]
        voice = measure.voices[0]  # voice1
        
        print(f"\n--- Measure {measure_idx + 1} (voice1 has {len(voice.beats)} beats) ---")
        
        # Calculate beat positions within measure
        beat_in_measure = current_beat % beats_per_measure
        
        # Calculate which beat to process (like C++ code does)
        accumulated_beats = 0.0
        beat_index = 0
        
        for i, beat in enumerate(voice.beats):
            duration = get_duration(beat)
            if beat_in_measure >= accumulated_beats and beat_in_measure < accumulated_beats + duration:
                beat_index = i
                break
            accumulated_beats += duration
            beat_index = i
        
        # Clamp beat_index
        if len(voice.beats) > 0:
            beat_index = max(0, min(beat_index, len(voice.beats) - 1))
        else:
            print("  SKIP: No beats in voice")
            continue
        
        beat = voice.beats[beat_index]
        
        print(f"  Processing beat {beat_index}")
        print(f"  beat.duration.value = {beat.duration.value}")
        print(f"  beat.notes count = {len(beat.notes)}")
        
        for note in beat.notes:
            string_index = note.string - 1  # GP uses 1-based strings
            fret = note.value
            
            print(f"    Note: GP string={note.string} -> index={string_index}, fret={fret}")
            
            # This is what calculateMidiNote does
            if string_index < 0 or string_index >= 12:
                print(f"    !!! INVALID STRING INDEX: {string_index} !!!")
                continue
            
            if fret < 0 or fret > 30:
                print(f"    !!! INVALID FRET: {fret} !!!")
                continue
            
            tuning_size = len(track.strings)
            if tuning_size > 0 and tuning_size <= 12 and string_index < tuning_size:
                tuning_value = track.strings[string_index].value
                midi_note = tuning_value + fret + transpose_offset
                print(f"    MIDI Note: {midi_note} (tuning={tuning_value})")
                
                if midi_note < 0 or midi_note >= 128:
                    print(f"    !!! INVALID MIDI NOTE: {midi_note} !!!")
            else:
                print(f"    !!! TUNING ACCESS ERROR: idx={string_index}, size={tuning_size} !!!")
        
        # Move to next measure worth of beats
        current_beat += beats_per_measure

def get_duration(beat):
    """Get beat duration in quarter notes"""
    if beat.duration.value <= 0:
        return 1.0
    base = 4.0 / beat.duration.value
    if beat.duration.isDotted:
        base *= 1.5
    if beat.duration.tuplet:
        if beat.duration.tuplet.enters > 0:
            base *= beat.duration.tuplet.times / beat.duration.tuplet.enters
    return base

def check_all_notes():
    """Check ALL notes in track 3 for any invalid values"""
    print("\n=== Checking ALL notes in Track 3 ===\n")
    
    song = guitarpro.parse('d:/GitHub/NewProject/test_partial.gp5')
    track = song.tracks[2]
    
    problems = []
    total_notes = 0
    
    for m_idx, measure in enumerate(track.measures):
        for v_idx, voice in enumerate(measure.voices):
            for b_idx, beat in enumerate(voice.beats):
                for note in beat.notes:
                    total_notes += 1
                    string_idx = note.string - 1
                    fret = note.value
                    
                    issues = []
                    if string_idx < 0:
                        issues.append(f"string_idx={string_idx}")
                    if string_idx >= len(track.strings):
                        issues.append(f"string_idx={string_idx} >= {len(track.strings)}")
                    if fret < 0:
                        issues.append(f"fret={fret}")
                    if fret > 30:
                        issues.append(f"fret={fret} > 30")
                    
                    if issues:
                        problems.append({
                            'measure': m_idx + 1,
                            'voice': v_idx,
                            'beat': b_idx,
                            'issues': issues
                        })
    
    print(f"Total notes checked: {total_notes}")
    print(f"Problems found: {len(problems)}")
    
    for p in problems:
        print(f"  Measure {p['measure']}, Voice {p['voice']}, Beat {p['beat']}: {p['issues']}")
    
    if not problems:
        print("\nNo invalid note data found in GP5 file!")
        print("The crash must be elsewhere in the C++ code.")

def check_voice_data():
    """Check voice data structure"""
    print("\n=== Checking Voice Data Structure ===\n")
    
    song = guitarpro.parse('d:/GitHub/NewProject/test_partial.gp5')
    track = song.tracks[2]
    
    print(f"Track 3 has {len(track.measures)} measures\n")
    
    # The C++ code reads measure.voice1 which is juce::Array<GP5Beat>
    # Let's see what voices exist
    for m_idx in range(min(10, len(track.measures))):
        measure = track.measures[m_idx]
        print(f"Measure {m_idx + 1}:")
        for v_idx, voice in enumerate(measure.voices):
            beat_count = len(voice.beats)
            has_notes = sum(1 for b in voice.beats if b.notes)
            print(f"  Voice {v_idx}: {beat_count} beats, {has_notes} with notes")

if __name__ == "__main__":
    check_all_notes()
    print("\n" + "="*60 + "\n")
    check_voice_data()
    print("\n" + "="*60 + "\n")
    simulate_midi_generation()
