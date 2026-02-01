with open(r'D:\GitHub\NewProject\Recording.gp5', 'rb') as f:
    data = f.read()

# Check version string
print('=== Version bytes (0-31) ===')
print(f'Hex: {data[0:31].hex()}')
print(f'Length byte: {data[0]}')
print(f'String: {repr(data[1:31])}')
print()

# The version string is 24 chars: FICHIER GUITAR PRO v5.00
version = 'FICHIER GUITAR PRO v5.00'
print(f'Version string length: {len(version)}')

# Should be: 1 byte length (24) + 30 bytes (string + padding) = 31 bytes
print()
print('=== Check position 30 and beyond ===')
print(f'Byte 30: 0x{data[30]:02x}')
print(f'Bytes 30-50: {data[30:50].hex()}')
print(f'As text: {repr(data[30:50])}')

# First IntByteSizeString at position 30 should be:
# - 4 bytes: total length (string_len + 1)
# - 1 byte: string length
# - N bytes: string
print()
print('=== First IntByteSizeString (title) at position 30 ===')
import struct
total_len = struct.unpack('<i', data[30:34])[0]
print(f'Total length (4 bytes): {total_len}')
if total_len > 0:
    str_len = data[34]
    print(f'String length (1 byte): {str_len}')
    if str_len > 0:
        s = data[35:35+str_len]
        print(f'String: {repr(s)}')
else:
    print('Empty string (total_len <= 0)')
