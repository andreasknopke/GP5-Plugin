import struct

with open(r'D:\GitHub\NewProject\Recording.gp5', 'rb') as f:
    data = f.read()

# Track data startet bei 1329
pos = 1329

# Blank byte
blank = data[pos]
print(f'pos {pos}: blank = 0x{blank:02x}')
pos += 1

# flags1 (for GP5)
flags1 = data[pos]
print(f'pos {pos}: flags1 = 0x{flags1:02x}')
pos += 1

# Track name (40 bytes ByteSizeString)
name_len = data[pos]
print(f'pos {pos}: name_len = {name_len}')
pos += 1
name = data[pos:pos+name_len].decode('latin-1')
print(f'pos {pos}: name = "{name}"')
pos += 40 - 1  # Skip rest of 40 bytes minus the length byte already read

# String count
string_count = struct.unpack('<i', data[pos:pos+4])[0]
print(f'pos {pos}: string_count = {string_count}')
pos += 4

# 7 tunings (int each)
tunings = []
for i in range(7):
    t = struct.unpack('<i', data[pos:pos+4])[0]
    tunings.append(t)
    pos += 4
print(f'pos {pos-28}: tunings = {tunings}')

# Port
port = struct.unpack('<i', data[pos:pos+4])[0]
print(f'pos {pos}: port = {port}')
pos += 4

# Channel (IMPORTANT!)
channel = struct.unpack('<i', data[pos:pos+4])[0]
print(f'pos {pos}: channel = {channel}')
pos += 4

# Effect channel
effect_channel = struct.unpack('<i', data[pos:pos+4])[0]
print(f'pos {pos}: effect_channel = {effect_channel}')
pos += 4

print()
print('=== Channel lookup ===')
print(f'readChannel reads: channel={channel}, effectChannel={effect_channel}')

# PyGuitarPro readChannel:
# index = channelId - 1
# if 0 <= index < len(channels):
#   return channels[index]
# else:
#   return None

index = channel - 1
print(f'index = {channel} - 1 = {index}')
print(f'channels array has 64 entries (0-63)')
print(f'0 <= {index} < 64 ? {0 <= index < 64}')

if 0 <= index < 64:
    print('Channel lookup SHOULD succeed!')
else:
    print('Channel lookup FAILS!')
