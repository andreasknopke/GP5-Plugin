#!/usr/bin/env python3
"""Complete GP5 structure parsing following PyGuitarPro's exact format."""

import struct

def read_file(path):
    with open(path, "rb") as f:
        return f.read()

class GP5Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0
    
    def read_byte(self):
        val = self.data[self.pos]
        self.pos += 1
        return val
    
    def read_sbyte(self):
        val = struct.unpack_from('b', self.data, self.pos)[0]
        self.pos += 1
        return val
    
    def read_bool(self):
        return self.read_byte() != 0
    
    def read_short(self):
        val = struct.unpack_from('<h', self.data, self.pos)[0]
        self.pos += 2
        return val
    
    def read_int(self):
        val = struct.unpack_from('<i', self.data, self.pos)[0]
        self.pos += 4
        return val
    
    def read_bytes(self, count):
        result = self.data[self.pos:self.pos+count]
        self.pos += count
        return result
    
    def skip(self, count):
        self.pos += count
    
    def read_byte_size_string(self, size):
        length = self.read_byte()
        s = self.read_bytes(size)
        return s[:length].decode('latin-1', errors='replace')
    
    def read_int_byte_size_string(self):
        buf_size = self.read_int()
        str_len = self.read_byte()
        s = self.read_bytes(buf_size - 1) if buf_size > 0 else b""
        return s[:str_len].decode('latin-1', errors='replace')
    
    def read_int_size_string(self):
        length = self.read_int()
        if length <= 0:
            return ""
        s = self.read_bytes(length)
        return s.decode('latin-1', errors='replace')

def parse_gp5(data, name):
    r = GP5Reader(data)
    
    print(f"\n{'='*70}")
    print(f"PARSING: {name}")
    print(f"{'='*70}")
    
    # Version
    version_len = r.read_byte()
    version = r.read_bytes(30).decode('latin-1', errors='replace')
    print(f"[pos {r.pos}] Version: '{version[:version_len]}'")
    
    # Score info
    title = r.read_int_byte_size_string()
    subtitle = r.read_int_byte_size_string()
    artist = r.read_int_byte_size_string()
    album = r.read_int_byte_size_string()
    words = r.read_int_byte_size_string()
    music = r.read_int_byte_size_string()
    copyright_info = r.read_int_byte_size_string()
    tab = r.read_int_byte_size_string()
    instructions = r.read_int_byte_size_string()
    print(f"[pos {r.pos}] Title: '{title}', Artist: '{artist}'")
    
    # Notice lines
    notice_count = r.read_int()
    for i in range(notice_count):
        r.read_int_byte_size_string()
    print(f"[pos {r.pos}] Notice count: {notice_count}")
    
    # Lyrics
    lyrics_track = r.read_int()
    for i in range(5):
        bar = r.read_int()
        text = r.read_int_size_string()
    print(f"[pos {r.pos}] Lyrics track: {lyrics_track}")
    
    # Page setup - this is a complex structure
    # Let me read it field by field according to GP5 format
    
    # RSE master effect (GP5.00 might not have this, GP5.10 does)
    # Actually, GP5.00 starts with page setup directly
    
    print(f"[pos {r.pos}] === PAGE SETUP ===")
    
    # Page format settings
    pageWidth = r.read_int()
    pageHeight = r.read_int()
    marginLeft = r.read_int()
    marginRight = r.read_int()
    marginTop = r.read_int()
    marginBottom = r.read_int()
    scoreSizeProportion = r.read_int()
    print(f"[pos {r.pos}] Page: {pageWidth}x{pageHeight}, margins: L={marginLeft} R={marginRight} T={marginTop} B={marginBottom}")
    
    # Header/footer flags and fields (0x7ff = all enabled)
    headerFooterFlags = r.read_short()
    print(f"[pos {r.pos}] Header/footer flags: 0x{headerFooterFlags:04X}")
    
    # 11 strings for header/footer (always present in GP5)
    for i in range(11):
        s = r.read_int_byte_size_string()
        # print(f"  HF string {i}: '{s[:30]}...'") if len(s) > 30 else None
    print(f"[pos {r.pos}] After 11 header/footer strings")
    
    # Tempo
    tempo_name = r.read_int_byte_size_string()
    tempo = r.read_int()
    print(f"[pos {r.pos}] Tempo: '{tempo_name}' = {tempo} BPM")
    
    # GP5: hideTempo (bool)
    hideTempo = r.read_bool()
    print(f"[pos {r.pos}] Hide tempo: {hideTempo}")
    
    # Key signature (global)
    key = r.read_sbyte()
    key2 = r.read_sbyte()
    r.skip(2)  # padding
    print(f"[pos {r.pos}] Key: {key}, minor: {key2}")
    
    # Octave 8 (GP5)
    octave8 = r.read_int()
    print(f"[pos {r.pos}] Octave8: {octave8}")
    
    # MIDI channels (64 ports, 4 bytes each = 256 bytes? No, 4 channels * 16 ports)
    # Actually it's GP5: 64 channels, each with: program(int), volume(byte), balance(byte), chorus(byte), reverb(byte), phaser(byte), tremolo(byte), blank(byte), bank(int)
    # Wait, that's a lot of bytes. Let me check PyGuitarPro source
    
    # Actually GP5 format for channels is simpler:
    # For each of 4 ports, 16 channels each = 64 channels total
    # Each channel: instrument(int), volume(byte), balance(byte), chorus(byte), reverb(byte), phaser(byte), tremolo(byte), blank1(byte), blank2(byte)
    # Total per channel = 4 + 8 = 12 bytes, 64 channels = 768 bytes
    # But wait, there might also be bank info
    
    print(f"[pos {r.pos}] === MIDI CHANNELS ===")
    for port in range(4):
        for channel in range(16):
            instrument = r.read_int()
            volume = r.read_byte()
            balance = r.read_byte()
            chorus = r.read_byte()
            reverb = r.read_byte()
            phaser = r.read_byte()
            tremolo = r.read_byte()
            blank1 = r.read_byte()
            blank2 = r.read_byte()
    print(f"[pos {r.pos}] After 64 MIDI channels (768 bytes)")
    
    # Musical directions (GP5)
    # This is: cozy, double coda, etc. - a set of shorts
    # 19 direction markers, each is a short
    print(f"[pos {r.pos}] === MUSICAL DIRECTIONS ===")
    for i in range(19):
        direction = r.read_short()
    print(f"[pos {r.pos}] After 19 musical directions (38 bytes)")
    
    # Master reverb (GP5)
    masterReverb = r.read_int()
    print(f"[pos {r.pos}] Master reverb: {masterReverb}")
    
    # NOW measure count and track count
    measureCount = r.read_int()
    trackCount = r.read_int()
    print(f"[pos {r.pos}] *** MEASURE COUNT: {measureCount}, TRACK COUNT: {trackCount} ***")
    
    # Measure headers
    print(f"[pos {r.pos}] === MEASURE HEADERS ===")
    for m in range(measureCount):
        header_pos = r.pos
        flags = r.read_byte()
        
        if flags & 0x01:  # numerator
            r.read_byte()
        if flags & 0x02:  # denominator
            r.read_byte()
        if flags & 0x04:  # repeat start
            pass
        if flags & 0x08:  # repeat end
            r.read_byte()
        if flags & 0x10:  # alt ending
            r.read_byte()
        if flags & 0x20:  # marker
            r.read_int_byte_size_string()
            r.read_int()  # color
        if flags & 0x40:  # key signature change
            r.read_sbyte()
            r.read_sbyte()
        if flags & 0x80:  # double bar
            pass
        
        # Triplet feel (only if time sig changed in GP5?)
        # Actually, in GP5, triplet feel is always present
        triplet = r.read_byte()
        
        # GP5: there's also an extra byte for beam groups when time sig changes
        if flags & 0x03:  # time sig changed
            # beam groups: 4 bytes
            r.skip(4)
        
        if m < 5 or m >= measureCount - 2:
            print(f"  Measure {m+1} at {header_pos}: flags=0x{flags:02X}, triplet={triplet}")
    
    print(f"[pos {r.pos}] === TRACKS ===")
    
    # Track data
    for t in range(trackCount):
        track_start = r.pos
        track_flags = r.read_byte()
        print(f"  Track {t+1} at {track_start}: flags=0x{track_flags:02X}")
        
        # Track flags 2 (GP5)
        track_flags2 = r.read_byte()
        
        # Track name (byte-size string, 40 bytes)
        track_name = r.read_byte_size_string(40)
        print(f"    Name: '{track_name}'")
        
        # String count and tuning
        num_strings = r.read_int()
        print(f"    Strings: {num_strings}")
        for s in range(7):  # Always 7 string tunings
            tuning = r.read_int()
        
        # Port
        port = r.read_int()
        # Channel
        channel_idx = r.read_int()
        # Channel effects
        channel_effects = r.read_int()
        # Frets
        frets = r.read_int()
        # Capo
        capo = r.read_int()
        # Color
        color = r.read_int()
        print(f"    Port={port}, Channel={channel_idx}, Frets={frets}, Capo={capo}")
        
        # GP5 additional track data
        # flags2 bits determine what follows
        # There's RSE data, diagram/automations, etc.
        
        # First: RSE related (2 bytes)
        r.skip(2)
        
        # Then: diagram/automations
        r.skip(1)  # auto accentuation
        
        # Bank
        bank = r.read_int()
        
        # Human playing style
        r.skip(1)
        
        # Unknown
        r.skip(8)
        
        # Additional flags for RSE
        r.skip(1)
        
        # GP5: instrument effect, sound bank, effect number
        r.skip(4)  # instrument effect 1
        r.skip(4)  # sound bank
        r.skip(4)  # instrument effect 2
        
        # Unknown
        r.skip(9)
        
        print(f"    Track ends at pos {r.pos}")
    
    print(f"[pos {r.pos}] === MEASURE DATA ===")
    
    # Parse first few measures
    for m in range(min(3, measureCount)):
        print(f"\n  --- Measure {m+1} at pos {r.pos} ---")
        for t in range(trackCount):
            for voice in range(2):
                beat_count = r.read_int()
                print(f"    Track {t+1} Voice {voice+1}: {beat_count} beats (pos={r.pos})")
                
                for b in range(beat_count):
                    beat_start = r.pos
                    beat_flags = r.read_byte()
                    
                    # Beat status (if rest flag)
                    if beat_flags & 0x40:
                        beat_status = r.read_byte()
                    
                    # Duration
                    duration = r.read_sbyte()
                    
                    # Tuplet
                    if beat_flags & 0x20:
                        tuplet = r.read_int()
                    
                    # Chord diagram
                    if beat_flags & 0x02:
                        print(f"      Beat {b+1}: CHORD flag set! This is the problem!")
                        # Chord parsing is complex
                        chord_new_format = r.read_bool()
                        if chord_new_format:
                            # New chord format
                            sharp = r.read_bool()
                            r.skip(3)  # blank
                            root = r.read_byte()
                            chord_type = r.read_byte()
                            extension = r.read_byte()
                            bass = r.read_int()
                            tone = r.read_int()
                            r.skip(1)  # add
                            chord_name = r.read_byte_size_string(21)
                            print(f"        Chord: root={root}, type={chord_type}, name='{chord_name}'")
                            # ... lots more chord data
                            r.skip(4)  # fifth alteration, ninth alteration, eleventh alteration
                            # fret positions
                            for s in range(7):
                                fret = r.read_sbyte()
                            # barre positions
                            barre_count = r.read_byte()
                            for i in range(5):
                                barre_fret = r.read_byte()
                                barre_start = r.read_byte()
                                barre_end = r.read_byte()
                            # omissions, fingering
                            r.skip(8 + 7)  # omissions + fingering
                            show_diagram = r.read_bool()
                        else:
                            # Old chord format - simpler
                            chord_name = r.read_byte_size_string(25)
                            print(f"        Old chord format: '{chord_name}'")
                            # lots of data
                            r.skip(55)
                    
                    # Text
                    if beat_flags & 0x04:
                        text = r.read_int_byte_size_string()
                    
                    # Beat effects
                    if beat_flags & 0x08:
                        effect1 = r.read_byte()
                        effect2 = r.read_byte()
                        # Parse effects based on flags
                        if effect1 & 0x01:  # vibrato
                            pass
                        if effect1 & 0x02:  # wide vibrato
                            pass
                        if effect1 & 0x04:  # natural harmonic
                            pass
                        if effect1 & 0x08:  # artificial harmonic
                            pass
                        if effect1 & 0x10:  # fade in
                            pass
                        if effect1 & 0x20:  # let ring? or something
                            r.read_int_byte_size_string()  # slap/pop text in GP3
                        if effect2 & 0x01:  # rasgueado
                            pass
                        if effect2 & 0x02:  # pick stroke
                            r.read_byte()  # down/up
                            r.read_byte()
                        if effect1 & 0x40:  # stroke
                            r.read_byte()
                            r.read_byte()
                        if effect2 & 0x04:  # tremolo bar
                            r.read_byte()  # type
                            r.read_int()   # value
                            point_count = r.read_int()
                            for p in range(point_count):
                                r.skip(4)  # position
                                r.skip(4)  # value
                                r.skip(1)  # vibrato
                        if effect2 & 0x08:  # mix table
                            # Very complex
                            pass
                    
                    # String data (notes)
                    if not (beat_flags & 0x40):  # Not rest, has notes
                        string_bits = r.read_byte()
                        for s in range(7):
                            if string_bits & (1 << (6-s)):
                                # Read note
                                note_flags = r.read_byte()
                                if note_flags & 0x20:  # has note type
                                    note_type = r.read_byte()
                                if note_flags & 0x01:  # has duration
                                    r.skip(2)  # duration, tuplet
                                if note_flags & 0x10:  # has velocity
                                    r.read_sbyte()
                                if note_flags & 0x20:  # has fret
                                    fret = r.read_sbyte()
                                if note_flags & 0x80:  # has fingering
                                    r.read_sbyte()
                                    r.read_sbyte()
                                if note_flags & 0x02:  # has note effects
                                    # Note effects
                                    ne1 = r.read_byte()
                                    ne2 = r.read_byte()
                                    if ne1 & 0x01:  # bend
                                        r.read_byte()
                                        r.read_int()
                                        pt_count = r.read_int()
                                        r.skip(pt_count * 9)
                                    if ne1 & 0x02:  # hammer on
                                        pass
                                    if ne1 & 0x04:  # let ring
                                        pass
                                    if ne1 & 0x08:  # grace note
                                        r.skip(4)
                                    if ne1 & 0x10:  # staccato
                                        pass
                                    if ne2 & 0x01:  # palm mute
                                        pass
                                    if ne2 & 0x02:  # harmonic
                                        r.read_byte()
                                    if ne2 & 0x04:  # trill
                                        r.skip(2)
                                    if ne2 & 0x08:  # slide
                                        r.read_byte()
                                    if ne2 & 0x10:  # vibrato
                                        pass
                                # flags2 
                                r.skip(1)
                    
                    # Beat flags2
                    r.skip(2)
                    
                    if b == 0 or beat_flags & 0x02:
                        print(f"      Beat {b+1} at {beat_start}: flags=0x{beat_flags:02X}")
            
        # Line break
        line_break = r.read_byte()
    
    return r.pos

# Parse my file
my_data = read_file(r"D:\GitHub\NewProject\Recording.gp5")
my_pos = parse_gp5(my_data, "Recording.gp5")

print(f"\n\nFinal position: {my_pos}, file size: {len(my_data)}")
