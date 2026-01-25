#!/usr/bin/env python3
"""
GP5 File Analyzer - Find where notes start in the file
Based on PyGuitarPro structure
"""

import struct
import sys

class GP5Analyzer:
    def __init__(self, filename):
        with open(filename, 'rb') as f:
            self.data = f.read()
        self.pos = 0
        self.version = None
        self.versionTuple = (5, 0, 0)
        
    def read_byte(self):
        val = self.data[self.pos]
        self.pos += 1
        return val
    
    def read_sbyte(self):
        val = struct.unpack('b', self.data[self.pos:self.pos+1])[0]
        self.pos += 1
        return val
    
    def read_short(self):
        val = struct.unpack('<h', self.data[self.pos:self.pos+2])[0]
        self.pos += 2
        return val
    
    def read_int(self):
        val = struct.unpack('<i', self.data[self.pos:self.pos+4])[0]
        self.pos += 4
        return val
    
    def read_bool(self):
        return self.read_byte() != 0
    
    def skip(self, count):
        self.pos += count
    
    def read_byte_size_string(self, size):
        """Read string: 1 byte length + fixed size content"""
        length = self.read_byte()
        s = self.data[self.pos:self.pos+length].decode('latin-1', errors='replace')
        self.skip(size)
        return s
    
    def read_int_byte_size_string(self):
        """Read string: 4 byte total size, 1 byte actual length, then content"""
        total_size = self.read_int()
        if total_size <= 0:
            return ""
        actual_len = self.read_byte()
        s = self.data[self.pos:self.pos+actual_len].decode('latin-1', errors='replace')
        self.skip(total_size - 1)
        return s
    
    def read_int_size_string(self):
        """Read string: 4 byte length + content"""
        length = self.read_int()
        if length <= 0:
            return ""
        s = self.data[self.pos:self.pos+length].decode('latin-1', errors='replace')
        self.pos += length
        return s
    
    def read_version(self):
        """Read version string (30 bytes)"""
        version = self.read_byte_size_string(30)
        self.version = version
        print(f"Version: {version}")
        if 'FICHIER GUITAR PRO v5.00' in version:
            self.versionTuple = (5, 0, 0)
        elif 'FICHIER GUITAR PRO v5.10' in version:
            self.versionTuple = (5, 1, 0)
        print(f"Version tuple: {self.versionTuple}")
        return version
    
    def read_info(self):
        """Read song info"""
        title = self.read_int_byte_size_string()
        subtitle = self.read_int_byte_size_string()
        artist = self.read_int_byte_size_string()
        album = self.read_int_byte_size_string()
        words = self.read_int_byte_size_string()
        music = self.read_int_byte_size_string()
        copyright = self.read_int_byte_size_string()
        tab = self.read_int_byte_size_string()
        instructions = self.read_int_byte_size_string()
        
        notice_count = self.read_int()
        for _ in range(notice_count):
            self.read_int_byte_size_string()
        
        print(f"Title: {title}")
        print(f"Artist: {artist}")
        return title, artist
    
    def read_lyrics(self):
        """Read lyrics"""
        track_num = self.read_int()
        for _ in range(5):
            self.read_int()  # from bar
            self.read_int_size_string()  # lyrics text
        print(f"Lyrics track: {track_num}")
    
    def read_rse_master_effect(self):
        """Read RSE master effect - ONLY for GP5.1+"""
        if self.versionTuple > (5, 0, 0):
            # Master volume
            self.read_int()
            # Unknown
            self.read_int()
            # Equalizer (11 bands)
            for _ in range(11):
                self.read_sbyte()
            # Gain preset
            self.read_byte()
            print(f"  RSE: read master effect (GP5.1+)")
        else:
            print(f"  RSE: skipped (GP5.0)")
    
    def read_page_setup(self):
        """Read page setup"""
        # Page size
        width = self.read_int()
        height = self.read_int()
        print(f"  Page size: {width}x{height}")
        
        # Margins
        left = self.read_int()
        right = self.read_int()
        top = self.read_int()
        bottom = self.read_int()
        print(f"  Margins: L={left} R={right} T={top} B={bottom}")
        
        # Score size proportion
        score_size = self.read_int()
        print(f"  Score size: {score_size}")
        
        # Header/footer flags (2 bytes / short)
        flags = self.read_short()
        print(f"  Header/footer flags: 0x{flags:04x}")
        
        # 10 IntByteSizeStrings for page setup placeholders
        # title, subtitle, artist, album, words, music, wordsAndMusic, copyright1, copyright2, pageNumber
        for i in range(10):
            s = self.read_int_byte_size_string()
            if i < 4:  # Only print first few
                print(f"  Page string {i}: {s[:30]}..." if len(s) > 30 else f"  Page string {i}: {s}")
    
    def read_directions(self):
        """Read 19 direction markers (shorts)"""
        for _ in range(19):
            self.read_short()
    
    def read_midi_channels(self):
        """Read MIDI channels (4 ports * 16 channels)"""
        for _ in range(64):
            self.read_int()   # instrument
            self.read_byte()  # volume
            self.read_byte()  # balance
            self.read_byte()  # chorus
            self.read_byte()  # reverb
            self.read_byte()  # phaser
            self.read_byte()  # tremolo
            self.skip(2)      # padding
    
    def read_measure_headers(self, count):
        """Read measure headers"""
        print(f"\n=== Reading {count} measure headers ===")
        headers = []
        for i in range(count):
            if i > 0 or self.versionTuple != (5, 0, 0):
                if i > 0:
                    self.skip(1)  # blank byte
            
            flags = self.read_byte()
            
            header = {'number': i + 1, 'flags': flags}
            
            if flags & 0x01:
                header['numerator'] = self.read_byte()
            if flags & 0x02:
                header['denominator'] = self.read_byte()
            if flags & 0x04:
                header['repeatOpen'] = True
            if flags & 0x08:
                header['repeatClose'] = self.read_byte()
            if flags & 0x20:
                # Marker
                marker_name = self.read_int_byte_size_string()
                self.read_int()  # color R
                header['marker'] = marker_name
            if flags & 0x40:
                self.read_sbyte()  # key root
                self.read_sbyte()  # key type
            if flags & 0x10:
                self.read_byte()  # repeat alternative
            if flags & 0x80:
                header['doubleBar'] = True
            
            if flags & 0x03:
                # Time signature beams
                for _ in range(4):
                    self.read_byte()
            
            if (flags & 0x10) == 0:
                self.skip(1)  # always 0
            
            self.read_byte()  # triplet feel
            
            headers.append(header)
            if i < 10:
                print(f"  Header {i+1}: flags=0x{flags:02x}, pos={self.pos}")
        
        return headers
    
    def read_tracks(self, count, measure_count):
        """Read tracks"""
        print(f"\n=== Reading {count} tracks ===")
        tracks = []
        for i in range(count):
            if i == 0 or self.versionTuple == (5, 0, 0):
                self.skip(1)  # blank
            
            flags1 = self.read_byte()
            track = {
                'number': i + 1,
                'isPercussion': bool(flags1 & 0x01),
                'flags1': flags1
            }
            
            name = self.read_byte_size_string(40)
            track['name'] = name
            
            string_count = self.read_int()
            track['stringCount'] = string_count
            
            # Read 7 tuning values
            for _ in range(7):
                self.read_int()
            
            self.read_int()  # port
            
            # Channel
            channel_idx = self.read_int()
            self.read_int()  # channel effect
            track['channel'] = channel_idx
            
            self.read_int()  # fret count
            self.read_int()  # offset/capo
            
            # Color
            self.read_byte()
            self.read_byte()
            self.read_byte()
            self.skip(1)
            
            # Flags2
            flags2 = self.read_short()
            track['flags2'] = flags2
            
            # Auto accentuation
            self.read_byte()
            
            # Bank
            self.read_byte()
            
            # Track RSE
            self.read_track_rse()
            
            # Create measures array
            track['measures'] = [{} for _ in range(measure_count)]
            
            tracks.append(track)
            print(f"  Track {i+1}: {name} ({string_count} strings), pos={self.pos}")
        
        # Skip bytes after tracks
        if self.versionTuple == (5, 0, 0):
            self.skip(2)
        else:
            self.skip(1)
        
        return tracks
    
    def read_track_rse(self):
        """Read track RSE data - per PyGuitarPro readTrackRSE"""
        # Humanize: 1 byte
        humanize = self.read_byte()
        
        # 3 ints unknown
        for _ in range(3):
            self.read_int()
        
        # Skip 12 bytes
        self.skip(12)
        
        # Read RSE Instrument
        self.read_rse_instrument()
        
        # For GP5.1+, also read equalizer and effect
        if self.versionTuple > (5, 0, 0):
            # Equalizer (4 signed bytes)
            for _ in range(4):
                self.read_sbyte()
            # RSE instrument effect
            self.read_int_byte_size_string()  # effect name
            self.read_int_byte_size_string()  # effect category
    
    def read_rse_instrument(self):
        """Read RSE instrument data"""
        instrument = self.read_int()
        unknown = self.read_int()
        sound_bank = self.read_int()
        
        if self.versionTuple == (5, 0, 0):
            effect_number = self.read_short()
            self.skip(1)
        else:
            effect_number = self.read_int()
        
        return {'instrument': instrument, 'soundBank': sound_bank, 'effectNumber': effect_number}
    
    def read_beat(self, beat_num, measure_num, track_num, voice_num):
        """Read a single beat"""
        start_pos = self.pos
        flags = self.read_byte()
        
        beat = {
            'pos': start_pos,
            'flags': flags,
            'notes': []
        }
        
        # Status (0x40)
        if flags & 0x40:
            status = self.read_byte()
            beat['status'] = status
        
        # Duration (always)
        duration = self.read_sbyte()
        beat['duration'] = duration
        
        # Tuplet (0x20)
        if flags & 0x20:
            tuplet = self.read_int()
            beat['tuplet'] = tuplet
        
        # Chord (0x02)
        if flags & 0x02:
            self.read_chord()
        
        # Text (0x04)
        if flags & 0x04:
            text = self.read_int_byte_size_string()
            beat['text'] = text
        
        # Beat effects (0x08)
        if flags & 0x08:
            self.read_beat_effects()
        
        # Mix table (0x10)
        if flags & 0x10:
            self.read_mix_table()
        
        # String flags (always)
        string_flags = self.read_byte()
        beat['stringFlags'] = string_flags
        
        # Read notes
        for s in range(6, -1, -1):
            if string_flags & (1 << s):
                string_num = 6 - s
                note = self.read_note()
                note['string'] = string_num
                beat['notes'].append(note)
        
        # GP5 flags2
        flags2 = self.read_short()
        beat['flags2'] = flags2
        
        # Break secondary (if flag set)
        if flags2 & 0x0800:
            self.read_byte()
        
        return beat
    
    def read_note(self):
        """Read a single note"""
        start_pos = self.pos
        flags = self.read_byte()
        
        note = {
            'pos': start_pos,
            'flags': flags,
            'fret': 0
        }
        
        # Note type (0x20)
        if flags & 0x20:
            note_type = self.read_byte()
            note['type'] = note_type
        
        # Velocity (0x10)
        if flags & 0x10:
            velocity = self.read_sbyte()
            note['velocity'] = velocity
        
        # Fret (0x20)
        if flags & 0x20:
            fret = self.read_sbyte()
            note['fret'] = fret
        
        # Fingering (0x80)
        if flags & 0x80:
            self.read_sbyte()  # left
            self.read_sbyte()  # right
        
        # Duration percent (0x01)
        if flags & 0x01:
            self.skip(8)  # double
        
        # GP5: flags2 (always)
        flags2 = self.read_byte()
        note['flags2'] = flags2
        
        # Note effects (0x08)
        if flags & 0x08:
            self.read_note_effects()
        
        return note
    
    def read_note_effects(self):
        """Read note effects"""
        flags1 = self.read_byte()
        flags2 = self.read_byte()
        
        # Bend (0x01)
        if flags1 & 0x01:
            self.read_byte()   # type
            self.read_int()    # value
            point_count = self.read_int()
            for _ in range(point_count):
                self.read_int()  # position
                self.read_int()  # value
                self.read_byte() # vibrato
        
        # Grace (0x10)
        if flags1 & 0x10:
            self.read_byte()  # fret
            self.read_byte()  # velocity
            self.read_byte()  # transition
            self.read_byte()  # duration
            self.read_byte()  # flags (GP5)
        
        # Tremolo picking (0x04)
        if flags2 & 0x04:
            self.read_byte()
        
        # Slide (0x08)
        if flags2 & 0x08:
            self.read_byte()
        
        # Harmonic (0x10)
        if flags2 & 0x10:
            harmonic_type = self.read_sbyte()
            if harmonic_type == 2:  # Artificial
                self.read_byte()
                self.read_sbyte()
                self.read_byte()
            elif harmonic_type == 3:  # Tapped
                self.read_byte()
        
        # Trill (0x20)
        if flags2 & 0x20:
            self.read_byte()  # fret
            self.read_byte()  # duration
    
    def read_beat_effects(self):
        """Read beat effects"""
        flags1 = self.read_byte()
        flags2 = self.read_byte()
        
        # Slap (0x20)
        if flags1 & 0x20:
            self.read_sbyte()
        
        # Tremolo bar (0x04)
        if flags2 & 0x04:
            self.read_byte()   # type
            self.read_int()    # value
            point_count = self.read_int()
            for _ in range(point_count):
                self.read_int()
                self.read_int()
                self.read_byte()
        
        # Stroke (0x40)
        if flags1 & 0x40:
            self.read_byte()  # down
            self.read_byte()  # up
        
        # Pick stroke (0x02)
        if flags2 & 0x02:
            self.read_sbyte()
    
    def read_chord(self):
        """Read chord diagram"""
        new_format = self.read_byte()
        
        if new_format == 0:
            # Old format
            name = self.read_int_byte_size_string()
            first_fret = self.read_int()
            if first_fret > 0:
                for _ in range(6):
                    self.read_int()
        else:
            # New format (GP5)
            self.read_bool()  # sharp
            self.skip(3)
            self.read_byte()  # root
            self.read_byte()  # type
            self.read_byte()  # extension
            self.read_int()   # bass
            self.read_int()   # tonality
            self.read_bool()  # add
            self.read_byte_size_string(21)  # name
            
            # Alterations
            self.read_byte()  # fifth
            self.read_byte()  # ninth
            self.read_byte()  # eleventh
            
            self.read_int()   # base fret
            
            # Frets (7 bytes)
            for _ in range(7):
                self.read_sbyte()
            
            # Barres
            self.read_byte()  # count
            for _ in range(5):
                self.read_byte()  # frets
            for _ in range(5):
                self.read_byte()  # starts
            for _ in range(5):
                self.read_byte()  # ends
            
            # Omissions (7 bools)
            for _ in range(7):
                self.read_bool()
            
            self.skip(1)
            
            # Fingering (7 bytes)
            for _ in range(7):
                self.read_sbyte()
            
            self.read_bool()  # show fingering
    
    def read_mix_table(self):
        """Read mix table change"""
        instrument = self.read_sbyte()
        
        # RSE instrument
        self.read_int()
        self.read_int()
        self.read_int()
        
        if self.versionTuple == (5, 0, 0):
            self.read_short()
            self.skip(1)
        else:
            self.read_int()
        
        volume = self.read_sbyte()
        balance = self.read_sbyte()
        chorus = self.read_sbyte()
        reverb = self.read_sbyte()
        phaser = self.read_sbyte()
        tremolo = self.read_sbyte()
        
        tempo_name = self.read_int_byte_size_string()
        tempo = self.read_int()
        
        # Durations
        if volume >= 0:
            self.read_byte()
        if balance >= 0:
            self.read_byte()
        if chorus >= 0:
            self.read_byte()
        if reverb >= 0:
            self.read_byte()
        if phaser >= 0:
            self.read_byte()
        if tremolo >= 0:
            self.read_byte()
        if tempo >= 0:
            self.read_byte()
            if self.versionTuple >= (5, 1, 0):
                self.read_bool()  # hide tempo
        
        # Flags
        self.read_byte()
        
        # Wah
        if self.versionTuple >= (5, 1, 0):
            self.read_sbyte()
        
        # RSE instrument effect
        self.read_int_byte_size_string()
        self.read_int_byte_size_string()
    
    def read_voice(self, measure_num, track_num, voice_num):
        """Read a voice (list of beats)"""
        beat_count = self.read_int()
        
        if beat_count < 0 or beat_count > 128:
            print(f"    WARNING: Invalid beat count {beat_count} at pos {self.pos-4}")
            return []
        
        beats = []
        for b in range(beat_count):
            beat = self.read_beat(b, measure_num, track_num, voice_num)
            beats.append(beat)
        
        return beats
    
    def read_measure(self, measure_num, track_num):
        """Read a measure for one track"""
        voice1 = self.read_voice(measure_num, track_num, 0)
        voice2 = self.read_voice(measure_num, track_num, 1)
        self.read_byte()  # line break
        return voice1, voice2
    
    def analyze(self):
        """Main analysis"""
        print(f"File size: {len(self.data)} bytes\n")
        
        # Version
        self.read_version()
        print(f"After version: pos={self.pos}")
        
        # Info
        self.read_info()
        print(f"After info: pos={self.pos}")
        
        # Lyrics
        self.read_lyrics()
        print(f"After lyrics: pos={self.pos}")
        
        # RSE Master Effect
        self.read_rse_master_effect()
        print(f"After RSE: pos={self.pos}")
        
        # Page Setup
        self.read_page_setup()
        print(f"After page setup: pos={self.pos}")
        
        # Tempo
        tempo_name = self.read_int_byte_size_string()
        tempo = self.read_int()
        print(f"Tempo: {tempo} ({tempo_name})")
        
        # Hide tempo (GP5.1+)
        if self.versionTuple > (5, 0, 0):
            self.read_bool()
        
        # Key
        self.read_sbyte()  # key
        self.read_int()    # octave
        print(f"After tempo/key: pos={self.pos}")
        
        # MIDI channels
        self.read_midi_channels()
        print(f"After MIDI channels: pos={self.pos}")
        
        # Directions
        self.read_directions()
        print(f"After directions: pos={self.pos}")
        
        # Master reverb
        self.read_int()
        print(f"After reverb: pos={self.pos}")
        
        # Measure and track count
        measure_count = self.read_int()
        track_count = self.read_int()
        print(f"\nMeasures: {measure_count}, Tracks: {track_count}")
        print(f"After counts: pos={self.pos}")
        
        # Measure headers
        headers = self.read_measure_headers(measure_count)
        print(f"After measure headers: pos={self.pos}")
        
        # Tracks
        tracks = self.read_tracks(track_count, measure_count)
        print(f"After tracks: pos={self.pos}")
        
        # Now read measures
        print(f"\n=== Reading measures (looking for notes in measure 5-7) ===")
        
        for m in range(min(measure_count, 10)):  # Read first 10 measures
            print(f"\n--- Measure {m+1} at pos {self.pos} ---")
            
            for t in range(track_count):
                try:
                    start_pos = self.pos
                    voice1, voice2 = self.read_measure(m, t)
                    
                    # Show details for first track only, and measures 5-7
                    if t == 0:
                        print(f"  Track {t+1}: voice1={len(voice1)} beats, voice2={len(voice2)} beats")
                        
                        for i, beat in enumerate(voice1):
                            notes_info = ""
                            if beat['notes']:
                                notes_info = ", notes: " + ", ".join(
                                    f"s{n['string']}:f{n['fret']}" for n in beat['notes']
                                )
                            print(f"    Beat {i}: pos={beat['pos']}, flags=0x{beat['flags']:02x}, "
                                  f"dur={beat['duration']}, strFlags=0x{beat['stringFlags']:02x}{notes_info}")
                            
                            # Check for the Badd11 chord (0,4,4 on strings)
                            if beat['notes'] and m >= 4:
                                print(f"      *** FOUND NOTES at measure {m+1}! ***")
                                for n in beat['notes']:
                                    print(f"        String {n['string']}: fret {n['fret']}")
                    
                except Exception as e:
                    print(f"  ERROR at track {t+1}: {e}")
                    print(f"  Position was: {self.pos}")
                    # Try to show nearby bytes
                    print(f"  Bytes around error: {self.data[max(0,self.pos-10):self.pos+20].hex()}")
                    raise

if __name__ == '__main__':
    analyzer = GP5Analyzer('d:/GitHub/NewProject/test_partial.gp5')
    try:
        analyzer.analyze()
    except Exception as e:
        print(f"\n\nFATAL ERROR: {e}")
        import traceback
        traceback.print_exc()
