import struct
import guitarpro.models as gp
from guitarpro.iobase import GPFileBase

# Manually parse the file to check what channels array looks like

with open(r'D:\GitHub\NewProject\Recording.gp5', 'rb') as f:
    data = f.read()

# MidiChannels starten bei Position 442 (nach TempoInfo)
pos = 442

print("=== Parsing MidiChannels (64 channels x 12 bytes each) ===")

# Parse all 64 channels like PyGuitarPro does
channels = []

for port in range(4):
    for channelIndex in range(16):
        channel = gp.MidiChannel()
        channel.channel = len(channels)
        channel.effectChannel = len(channels)
        
        # Read channel data (12 bytes)
        instrument = struct.unpack('<i', data[pos:pos+4])[0]
        volume = data[pos+4]
        balance = data[pos+5]
        chorus = data[pos+6]
        reverb = data[pos+7]
        phaser = data[pos+8]
        tremolo = data[pos+9]
        # 2 padding bytes
        
        pos += 12
        
        # Convert from "short" format
        def toChannelShort(d):
            return min(max((d << 3) - 1, -1), 32767) + 1
        
        channel.instrument = instrument
        channel.volume = toChannelShort(volume)
        channel.balance = toChannelShort(balance)
        channel.chorus = toChannelShort(chorus)
        channel.reverb = toChannelShort(reverb)
        channel.phaser = toChannelShort(phaser)
        channel.tremolo = toChannelShort(tremolo)
        # channel.bank = gp.MidiChannel.DEFAULT_BANK
        
        channels.append(channel)

print(f"Total channels parsed: {len(channels)}")
print(f"MidiChannels end at position: {pos}")
print()

# Print first few channels
for i, ch in enumerate(channels[:5]):
    print(f"Channel {i}: instrument={ch.instrument}, volume={ch.volume}, balance={ch.balance}")

print()

# Now test readChannel function
# From gp3.py:
# def readChannel(self, channels):
#     channelId = self.readI32()
#     effectChannel = self.readI32()
#     # ...
#     index = channelId - 1
#     if 0 <= index < len(channels):
#         return channels[index]
#     return None

print("=== Test readChannel ===")
channelId = 1  # From track data
effectChannel = 2

index = channelId - 1
print(f"channelId = {channelId}, index = {index}")
print(f"len(channels) = {len(channels)}")
print(f"0 <= {index} < {len(channels)} ? {0 <= index < len(channels)}")

if 0 <= index < len(channels):
    ch = channels[index]
    print(f"Result: MidiChannel with instrument={ch.instrument}")
else:
    print("Result: None")
