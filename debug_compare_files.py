#!/usr/bin/env python3
"""
Debug: Check what PyGuitarPro is writing.
Compare test_roundtrip.gp5 (known good) with test_midi_converted.gp5 (broken)
"""

import struct

def hexdump(data, start, length, label=""):
    """Print hex dump of data section."""
    print(f"\n{label} - {length} bytes starting at {start}:")
    for i in range(0, length, 16):
        offset = start + i
        chunk = data[offset:offset+min(16, length-i)]
        hex_str = ' '.join(f'{b:02X}' for b in chunk)
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print(f"  {offset:04d}: {hex_str:48s} {ascii_str}")

def compare_files():
    # Load both files
    with open(r"D:\GitHub\NewProject\test_roundtrip.gp5", "rb") as f:
        good_data = f.read()
    
    with open(r"D:\GitHub\NewProject\test_midi_converted.gp5", "rb") as f:
        bad_data = f.read()
    
    print(f"Good file size: {len(good_data)}")
    print(f"Bad file size: {len(bad_data)}")
    
    # Compare first 100 bytes (header)
    hexdump(good_data, 0, 50, "Good file header")
    hexdump(bad_data, 0, 50, "Bad file header")
    
    # Find first difference
    for i in range(min(len(good_data), len(bad_data))):
        if good_data[i] != bad_data[i]:
            print(f"\nFirst difference at byte {i}")
            hexdump(good_data, max(0, i-16), 48, f"Good file around diff")
            hexdump(bad_data, max(0, i-16), 48, f"Bad file around diff")
            break
    else:
        print("\nNo differences found in overlapping bytes")

if __name__ == "__main__":
    compare_files()
