with open(r'D:\GitHub\NewProject\Recording.gp5', 'rb') as f:
    data = f.read()

import struct

# Track bei 1329
print('=== Track Raw Bytes ===')
pos = 1329

# Byte 0: blank
print(f'pos {pos}: blank = 0x{data[pos]:02x}')
pos += 1

# Byte 1: flags1
print(f'pos {pos}: flags1 = 0x{data[pos]:02x}')
pos += 1

# Bytes 2-42: name (1 length + 40 chars)
name_len = data[pos]
print(f'pos {pos}: name_len = {name_len}')
pos += 1
name = data[pos:pos+name_len].decode('latin-1')
print(f'pos {pos}: name = "{name}"')
pos += 40  # Name field is 40 bytes after length byte, so total = 40 (after length byte already consumed)

# Now at position - but wait!
# readByteSizeString(40) does:
# 1. Read 1 byte length
# 2. Read 40 bytes
# So total is 41 bytes, NOT 40!

# Let me recalculate
print()
print('=== Recalculate ===')
# Start: 1329
# blank: 1 byte -> 1330
# flags1: 1 byte -> 1331
# name_len: 1 byte -> 1332
# name: 40 bytes (including the length already read!) 
# NO! readByteSizeString(40) reads length THEN 40 bytes!
# So: 1331 + 1 (length) + 40 (chars) = 1372

pos = 1372
print(f'After name (readByteSizeString(40)), pos should be 1372')
print(f'Actual pos: {pos}')
print(f'Bytes at pos: {data[pos:pos+4].hex()}')

# My code writes the name as:
# writeByte(name_len) + for 40 bytes, either char or 0
# So total 41 bytes for name

# BUT PyGuitarPro readByteSizeString(40) reads:
# 1 byte length + 40 bytes content
# So 41 bytes total

# So pos 1372 should have string_count
string_count = struct.unpack('<i', data[1372:1376])[0]
print(f'String count at 1372: {string_count}')

# Wait! 671088640 = 0x28000000
# In hex: 00 00 00 28 in big-endian would be 40 (0x28)
# But little-endian 28 00 00 00 = 40!
# So if data is 28 00 00 00, that's correct for 40 strings... wait no, should be 6 strings

# Let me check what's at 1371-1380
print()
print('=== Bytes 1371-1380 ===')
print(f'{data[1371:1381].hex()}')
print(f'Position 1371: 0x{data[1371]:02x}')
print(f'Position 1372-1375: {data[1372:1376].hex()}')

# Oh wait! My code writes the name differently!
# Let me check what I write for name:
# writeByteSizeString does NOT exist, I use writeByte + loop
# My writeTrack function...

print()
print('=== My track writing code ===')
print('I write: blank (1) + flags1 (1) + name_len (1) + 40 name bytes (40) = 43 bytes')
print('But readByteSizeString(40) reads: length (1) + 40 chars = 41 bytes')
print('So there is a mismatch!')
