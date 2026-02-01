#!/usr/bin/env python3
"""
Fix MIDI files that have Format 1 with only 1 track.
This converts them to Format 0 which is the proper format for single-track files.

Usage: python fix_midi_format.py input.mid [output.mid]
If no output file is specified, the input file will be overwritten.
"""

import sys
import struct

def fix_midi_format(input_file, output_file=None):
    if output_file is None:
        output_file = input_file
    
    with open(input_file, 'rb') as f:
        data = bytearray(f.read())
    
    # Verify MIDI header
    if data[0:4] != b'MThd':
        print(f"Error: {input_file} is not a valid MIDI file")
        return False
    
    # Read header
    header_len = struct.unpack('>I', data[4:8])[0]
    if header_len != 6:
        print(f"Warning: Unexpected header length: {header_len}")
    
    format_type = struct.unpack('>H', data[8:10])[0]
    num_tracks = struct.unpack('>H', data[10:12])[0]
    ppq = struct.unpack('>H', data[12:14])[0]
    
    print(f"Input file: {input_file}")
    print(f"  Format: {format_type}")
    print(f"  Tracks: {num_tracks}")
    print(f"  PPQ: {ppq}")
    
    # Count actual MTrk chunks
    actual_tracks = data.count(b'MTrk')
    print(f"  Actual MTrk headers: {actual_tracks}")
    
    # Check if fix is needed
    if format_type == 0:
        print("File is already Format 0, no fix needed.")
        return True
    
    if format_type == 1 and num_tracks == 1:
        print("\nFixing: Converting Format 1 -> Format 0")
        # Change format from 1 to 0
        data[8:10] = struct.pack('>H', 0)
        
        with open(output_file, 'wb') as f:
            f.write(data)
        
        print(f"Fixed file written to: {output_file}")
        return True
    
    if format_type == 1 and num_tracks > 1:
        print("Format 1 with multiple tracks is valid, no fix needed.")
        return True
    
    print(f"Unexpected format configuration: Format {format_type} with {num_tracks} tracks")
    return False


def verify_end_of_track(filename):
    """Verify that each track has a proper End of Track marker"""
    with open(filename, 'rb') as f:
        data = f.read()
    
    num_tracks = struct.unpack('>H', data[10:12])[0]
    
    # Find all MTrk chunks and check for End of Track
    pos = 14  # Start after header
    for i in range(num_tracks):
        if data[pos:pos+4] != b'MTrk':
            print(f"Error: Expected MTrk at position {pos}")
            return False
        
        track_len = struct.unpack('>I', data[pos+4:pos+8])[0]
        track_data = data[pos+8:pos+8+track_len]
        
        # Check for End of Track marker (FF 2F 00) at the end
        # Note: There's a delta-time before it, so we look for it near the end
        if b'\xff\x2f\x00' in track_data[-10:]:
            print(f"Track {i}: End of Track marker OK")
        else:
            print(f"Track {i}: WARNING - End of Track marker not found at expected position!")
            # Search for it anywhere in the track
            eot_pos = track_data.find(b'\xff\x2f\x00')
            if eot_pos >= 0:
                print(f"  Found at offset {eot_pos} (expected near {track_len-3})")
            else:
                print(f"  End of Track marker MISSING!")
                return False
        
        pos += 8 + track_len
    
    return True


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    print("=" * 60)
    if fix_midi_format(input_file, output_file):
        print("\n" + "=" * 60)
        print("Verifying End of Track markers...")
        verify_end_of_track(output_file or input_file)
    else:
        sys.exit(1)
