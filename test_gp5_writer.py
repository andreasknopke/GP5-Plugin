#!/usr/bin/env python3
"""
Test script to validate GP5 file structure written by GP5Writer
Compares with a reference GP5 file to find structural issues
"""

import struct
import os

def read_int(data, pos):
    return struct.unpack('<I', data[pos:pos+4])[0], pos + 4

def read_short(data, pos):
    return struct.unpack('<h', data[pos:pos+2])[0], pos + 2

def read_byte(data, pos):
    return data[pos], pos + 1

def read_int_byte_string(data, pos):
    """Read IntByteSizeString: 4-byte total size, 1-byte string length, then string"""
    total_len, pos = read_int(data, pos)
    str_len, pos = read_byte(data, pos)
    if str_len > 0:
        s = data[pos:pos+str_len].decode('latin-1', errors='replace')
    else:
        s = ""
    return s, pos + total_len - 1  # -1 because we already read the length byte

def read_int_size_string(data, pos):
    """Read IntSizeString: 4-byte length, then string (no length byte)"""
    length, pos = read_int(data, pos)
    if length > 0:
        s = data[pos:pos+length].decode('latin-1', errors='replace')
    else:
        s = ""
    return s, pos + length

def analyze_gp5(filename, verbose=True):
    """Analyze GP5 file structure"""
    with open(filename, 'rb') as f:
        data = f.read()
    
    print(f"\n{'='*60}")
    print(f"Analyzing: {filename}")
    print(f"File size: {len(data)} bytes")
    print(f"{'='*60}")
    
    pos = 0
    
    # 1. Version (31 bytes)
    version_len = data[pos]
    pos += 1
    version = data[pos:pos+version_len].decode('ascii')
    pos = 31  # Fixed 31 bytes for version
    print(f"\n1. Version: '{version}'")
    
    # 2. Song Info
    print(f"\n2. Song Info (starting at pos {pos}):")
    for field in ['Title', 'Subtitle', 'Artist', 'Album', 'Words', 'Music', 'Copyright', 'Tab', 'Instructions']:
        s, pos = read_int_byte_string(data, pos)
        if verbose or s:
            print(f"   {field}: '{s[:50]}'" if len(s) > 50 else f"   {field}: '{s}'")
    
    notice_count, pos = read_int(data, pos)
    print(f"   Notice lines: {notice_count}")
    for i in range(notice_count):
        s, pos = read_int_byte_string(data, pos)
        if verbose:
            print(f"   Notice {i+1}: '{s[:30]}...'")
    
    # 3. Lyrics
    print(f"\n3. Lyrics (starting at pos {pos}):")
    lyrics_track, pos = read_int(data, pos)
    print(f"   Lyrics track: {lyrics_track}")
    for i in range(5):
        start_measure, pos = read_int(data, pos)
        lyrics_text, pos = read_int_size_string(data, pos)
        if verbose or lyrics_text:
            print(f"   Lyrics {i+1}: measure {start_measure}, text: '{lyrics_text[:20]}...' ({len(lyrics_text)} chars)")
    
    # 4. Page Setup
    print(f"\n4. Page Setup (starting at pos {pos}):")
    page_w, pos = read_int(data, pos)
    page_h, pos = read_int(data, pos)
    print(f"   Page size: {page_w} x {page_h}")
    margins = []
    for _ in range(4):
        m, pos = read_int(data, pos)
        margins.append(m)
    print(f"   Margins: {margins}")
    score_size, pos = read_int(data, pos)
    print(f"   Score size: {score_size}")
    hf_flags, pos = read_short(data, pos)
    print(f"   Header/footer flags: 0x{hf_flags:04X}")
    print(f"   Page setup strings:")
    for i in range(10):
        s, pos = read_int_byte_string(data, pos)
        if verbose or s:
            print(f"      [{i}]: '{s}'")
    
    # 5. Tempo
    print(f"\n5. Tempo (starting at pos {pos}):")
    tempo_name, pos = read_int_byte_string(data, pos)
    tempo_value, pos = read_int(data, pos)
    print(f"   Tempo name: '{tempo_name}'")
    print(f"   Tempo: {tempo_value} BPM")
    
    # 6. Key signature
    key, pos = read_byte(data, pos)
    octave, pos = read_int(data, pos)
    print(f"   Key: {key}, Octave: {octave}")
    
    # 7. MIDI Channels
    print(f"\n6. MIDI Channels (starting at pos {pos}):")
    print(f"   Reading 64 channels (768 bytes)...")
    midi_start = pos
    for port in range(4):
        for ch in range(16):
            instrument, pos = read_int(data, pos)
            volume, pos = read_byte(data, pos)
            balance, pos = read_byte(data, pos)
            chorus, pos = read_byte(data, pos)
            reverb, pos = read_byte(data, pos)
            phaser, pos = read_byte(data, pos)
            tremolo, pos = read_byte(data, pos)
            pos += 2  # padding
            if verbose and ch == 0 and port == 0:
                print(f"   Channel 1: instrument={instrument}, vol={volume}, bal={balance}")
    print(f"   MIDI channels end at pos {pos} (read {pos - midi_start} bytes)")
    
    # 8. Directions
    print(f"\n7. Directions (starting at pos {pos}):")
    directions_start = pos
    for i in range(19):
        d, pos = read_short(data, pos)
        if verbose and d != 0:
            print(f"   Direction {i+1}: {d}")
    print(f"   Directions end at pos {pos} (read {pos - directions_start} bytes)")
    
    # 9. Master reverb
    reverb, pos = read_int(data, pos)
    print(f"\n8. Master reverb: {reverb} (at pos {pos-4})")
    
    # 10. Measure and Track count
    measure_count, pos = read_int(data, pos)
    track_count, pos = read_int(data, pos)
    print(f"\n9. Measures: {measure_count}, Tracks: {track_count} (at pos {pos-8})")
    
    # 11. Measure Headers
    print(f"\n10. Measure Headers (starting at pos {pos}):")
    for m in range(min(measure_count, 3)):  # Show first 3
        if m > 0:
            pos += 1  # blank byte
        flags, pos = read_byte(data, pos)
        print(f"   Measure {m+1}: flags=0x{flags:02X}", end="")
        if flags & 0x01:
            num, pos = read_byte(data, pos)
            print(f", numerator={num}", end="")
        if flags & 0x02:
            denom, pos = read_byte(data, pos)
            print(f", denominator={denom}", end="")
        if flags & 0x04:
            repeat_close, pos = read_byte(data, pos)
        if flags & 0x08:
            marker, pos = read_int_byte_string(data, pos)
            # color
            pos += 4
        if flags & 0x40:
            pos += 2  # key sig
        if flags & 0x10:
            alt, pos = read_byte(data, pos)
        # beams
        if flags & 0x03:
            pos += 4  # 4 beam bytes
        # blank if no alt
        if not (flags & 0x10):
            pos += 1
        # triplet feel
        triplet, pos = read_byte(data, pos)
        print(f", triplet_feel={triplet}")
    
    if measure_count > 3:
        print(f"   ... (showing first 3 of {measure_count})")
    
    print(f"\n11. Current position: {pos}")
    print(f"    Remaining bytes: {len(data) - pos}")
    
    return True

if __name__ == "__main__":
    # Analyze our generated file
    our_file = "D:/GitHub/NewProject/Recording.gp5"
    ref_file = "D:/GitHub/NewProject/test_partial.gp5"
    
    if os.path.exists(our_file):
        try:
            analyze_gp5(our_file, verbose=True)
        except Exception as e:
            print(f"\nERROR analyzing {our_file}: {e}")
            import traceback
            traceback.print_exc()
    
    print("\n" + "="*60)
    print("REFERENCE FILE:")
    print("="*60)
    
    if os.path.exists(ref_file):
        try:
            analyze_gp5(ref_file, verbose=False)
        except Exception as e:
            print(f"\nERROR analyzing {ref_file}: {e}")
            import traceback
            traceback.print_exc()
