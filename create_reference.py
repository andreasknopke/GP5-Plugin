import guitarpro as gp
import io

# Create a minimal GP5 file with PyGuitarPro
song = gp.Song()
song.title = 'Test'
song.artist = 'Test'
song.tempo = 120

# Create measure header first
header = gp.MeasureHeader()
header.tempo.value = 120
header.timeSignature = gp.TimeSignature()
song.measureHeaders.append(header)

# Create a track - needs song as arg
track = gp.Track(song)
track.name = 'Track 1'
track.channel = gp.MidiChannel()
track.channel.channel = 0
track.channel.effectChannel = 1
track.strings = [gp.GuitarString(i+1, [64, 59, 55, 50, 45, 40][i]) for i in range(6)]

# Create measure
measure = gp.Measure(header, track)

# Create voice
voice = gp.Voice(measure)

# Create a simple beat with one note
beat = gp.Beat(voice)
beat.start = 960
beat.duration = gp.Duration()
beat.duration.value = 4

# Add a note
note = gp.Note(beat)
note.value = 5
note.velocity = gp.Velocities.default
note.string = 1
note.type = gp.NoteType.normal
beat.notes.append(note)

voice.beats.append(beat)
measure.voices.append(voice)

# Add empty voice 2
voice2 = gp.Voice(measure)
measure.voices.append(voice2)

track.measures.append(measure)
song.tracks.append(track)

# Write to file
gp.write(song, 'D:/GitHub/NewProject/reference.gp5', version=(5, 0, 0))
print("Reference file created")

# Parse it back to verify
song2 = gp.parse('D:/GitHub/NewProject/reference.gp5')
print(f'Parsed back: {len(song2.tracks)} tracks, {len(song2.tracks[0].measures)} measures')

# Get file size
import os
size = os.path.getsize('D:/GitHub/NewProject/reference.gp5')
print(f'File size: {size} bytes')
