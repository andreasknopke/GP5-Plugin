import struct

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    # Trace exactly what PyGuitarPro would read
    f.seek(1321)
    
    print('=== Track 1 (number=1, starts at 1321) ===')
    
    # Skip 1 for track 1 (since number==1 in GP5.0.0)
    skip1 = f.read(1)
    print(f'Skip(1): {struct.unpack("<B", skip1)[0]}')
    
    # flags1
    flags1 = struct.unpack('<B', f.read(1))[0]
    print(f'flags1: {flags1} (0x{flags1:02x})')
    
    # name (1 byte length + 40 bytes)
    name_len = struct.unpack('<B', f.read(1))[0]
    name_bytes = f.read(40)
    name = name_bytes[:name_len].decode('latin-1')
    print(f'name: "{name}" (len={name_len})')
    
    # numStrings
    numStrings = struct.unpack('<i', f.read(4))[0]
    print(f'numStrings: {numStrings}')
    
    # 7 tunings
    tunings = [struct.unpack('<i', f.read(4))[0] for _ in range(7)]
    print(f'tunings: {tunings}')
    
    # port
    port = struct.unpack('<i', f.read(4))[0]
    print(f'port: {port}')
    
    # channel + effectChannel
    channel = struct.unpack('<i', f.read(4))[0]
    effectChannel = struct.unpack('<i', f.read(4))[0]
    print(f'channel: {channel}, effectChannel: {effectChannel}')
    
    # fretCount
    fretCount = struct.unpack('<i', f.read(4))[0]
    print(f'fretCount: {fretCount}')
    
    # offset
    offset = struct.unpack('<i', f.read(4))[0]
    print(f'offset: {offset}')
    
    # color (4 bytes)
    color = [struct.unpack('<B', f.read(1))[0] for _ in range(4)]
    print(f'color: {color}')
    
    # flags2 (short)
    flags2 = struct.unpack('<h', f.read(2))[0]
    print(f'flags2: {flags2} (0x{flags2:04x})')
    
    # autoAccentuation
    autoAccent = struct.unpack('<B', f.read(1))[0]
    print(f'autoAccentuation: {autoAccent}')
    
    # bank
    bank = struct.unpack('<B', f.read(1))[0]
    print(f'bank: {bank}')
    
    print(f'\nAt position {f.tell()} - now reading TrackRSE')
    
    # TrackRSE:
    # humanize
    humanize = struct.unpack('<B', f.read(1))[0]
    print(f'humanize: {humanize}')
    
    # 3 ints
    unk1 = struct.unpack('<i', f.read(4))[0]
    unk2 = struct.unpack('<i', f.read(4))[0]
    unk3 = struct.unpack('<i', f.read(4))[0]
    print(f'3 unknown ints: {unk1}, {unk2}, {unk3}')
    
    # skip(12)
    skip12 = f.read(12)
    print(f'Skip 12 bytes: {list(skip12)}')
    
    print(f'\nAt position {f.tell()} - now reading RSEInstrument')
    
    # RSEInstrument
    instrument = struct.unpack('<i', f.read(4))[0]
    print(f'instrument: {instrument}')
    
    unknown = struct.unpack('<i', f.read(4))[0]
    print(f'unknown: {unknown}')
    
    soundBank = struct.unpack('<i', f.read(4))[0]
    print(f'soundBank: {soundBank}')
    
    # GP5.0.0: effectNumber short + skip(1)
    effectNumber = struct.unpack('<h', f.read(2))[0]
    print(f'effectNumber: {effectNumber}')
    skip_after = f.read(1)
    print(f'skip(1) after effectNumber: {struct.unpack("<B", skip_after)[0]}')
    
    print(f'\nTrack 1 ends at position {f.tell()}')
    
    # After all tracks: skip(2) for GP5.0.0
    print('\n=== After all tracks (GP5.0.0 skips 2) ===')
    skip2 = f.read(2)
    print(f'skip(2): {list(skip2)}')
    print(f'Measures should start at: {f.tell()}')
    
    # Show what's next
    print('\n=== First measure bytes ===')
    for i in range(20):
        b = struct.unpack('<B', f.read(1))[0]
        print(f'[{f.tell()-1}] {b} (0x{b:02x})')
