#!/usr/bin/env python3
"""Check the actual bytes in the GP5 file."""

import struct

# Read the file
with open(r"D:\GitHub\NewProject\Recording.gp5", "rb") as f:
    data = f.read()

print(f"File size: {len(data)}")

# First 100 bytes as hex
print("\n=== First 100 bytes ===")
for i in range(0, 100, 16):
    hex_str = ' '.join(f'{data[i+j]:02X}' for j in range(min(16, 100-i)))
    ascii_str = ''.join(chr(data[i+j]) if 32 <= data[i+j] < 127 else '.' for j in range(min(16, 100-i)))
    print(f"{i:04d}: {hex_str:48s} {ascii_str}")

# The GP5 format should start with:
# - 31 bytes for version string field (1 byte length + 30 bytes string data)
# NOT 4-byte int for length

# Check if it's the byte-length format
print("\n=== Version string (byte-length format) ===")
version_len = data[0]
print(f"Version length byte: {version_len}")
version_str = data[1:31].decode('latin-1', errors='replace')
print(f"Version string (30 bytes): '{version_str}'")

# Let's also check a reference GP5 file
try:
    with open(r"D:\GitHub\NewProject\test_partial.gp5", "rb") as f:
        ref_data = f.read()
    
    print("\n=== Reference file (test_partial.gp5) first 100 bytes ===")
    for i in range(0, 100, 16):
        hex_str = ' '.join(f'{ref_data[i+j]:02X}' for j in range(min(16, 100-i)))
        ascii_str = ''.join(chr(ref_data[i+j]) if 32 <= ref_data[i+j] < 127 else '.' for j in range(min(16, 100-i)))
        print(f"{i:04d}: {hex_str:48s} {ascii_str}")
        
    print(f"\nReference version length byte: {ref_data[0]}")
    print(f"Reference version string: '{ref_data[1:31].decode('latin-1', errors='replace')}'")
except FileNotFoundError:
    print("Reference file not found")

# The format after version (pos 31):
# Score info uses int-byte-size strings (4 byte total length, 1 byte string length, n bytes string)
print("\n=== After version (pos 31) ===")
pos = 31
for i in range(10):
    # Read int-byte-size string
    if pos + 5 > len(data):
        break
    buf_size = struct.unpack_from('<i', data, pos)[0]
    print(f"At pos {pos}: buffer_size = {buf_size}")
    if buf_size < 0 or buf_size > 1000:
        print(f"  (seems wrong, looking at raw bytes: {' '.join(f'{data[pos+j]:02X}' for j in range(10))})")
        break
    str_len = data[pos + 4]
    string_data = data[pos + 5 : pos + 4 + buf_size] if buf_size > 0 else b""
    actual_str = string_data[:str_len].decode('latin-1', errors='replace')
    print(f"  str_len = {str_len}, string = '{actual_str}'")
    pos += 4 + buf_size
