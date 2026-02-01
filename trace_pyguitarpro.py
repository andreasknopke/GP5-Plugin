import struct
import io

# Monkey patch to trace PyGuitarPro file reading
class TracingIO:
    def __init__(self, data):
        self.data = data
        self.pos = 0
        self.log = []
        
    def read(self, n=-1):
        if n == -1:
            result = self.data[self.pos:]
            self.pos = len(self.data)
        else:
            result = self.data[self.pos:self.pos+n]
            self.pos += n
        return result
    
    def seek(self, pos, whence=0):
        if whence == 0:
            self.pos = pos
        elif whence == 1:
            self.pos += pos
        elif whence == 2:
            self.pos = len(self.data) + pos
        return self.pos
    
    def tell(self):
        return self.pos

# Read the file
with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    data = f.read()

# Create a patched version that logs reads
import guitarpro.iobase as iobase

original_read = None
read_log = []

class PatchedGPFileBase:
    @classmethod
    def patch(cls, gpfile):
        original_readU8 = gpfile.readU8
        original_readI8 = gpfile.readI8
        original_readI16 = gpfile.readI16
        original_readI32 = gpfile.readI32
        
        def logged_readU8(self=gpfile):
            pos = gpfile.data.tell()
            result = original_readU8()
            read_log.append((pos, 'U8', result))
            return result
        
        def logged_readI8(self=gpfile):
            pos = gpfile.data.tell()
            result = original_readI8()
            read_log.append((pos, 'I8', result))
            return result
            
        def logged_readI16(self=gpfile):
            pos = gpfile.data.tell()
            result = original_readI16()
            read_log.append((pos, 'I16', result))
            return result
            
        def logged_readI32(self=gpfile):
            pos = gpfile.data.tell()
            result = original_readI32()
            read_log.append((pos, 'I32', result))
            return result
        
        gpfile.readU8 = logged_readU8
        gpfile.readI8 = logged_readI8
        gpfile.readI16 = logged_readI16
        gpfile.readI32 = logged_readI32

# Just trace manually where PyGuitarPro reads for beats
# Based on the traceback, the issue is in readBeat in gp3.py line 451

# Let's trace what PyGuitarPro actually does
import guitarpro

# Check BeatStatus enum
print("BeatStatus values:")
for status in guitarpro.BeatStatus:
    print(f"  {status.name} = {status.value}")

print("\n")

# Now let's manually trace reading
print("48 = 0x30 which is the ASCII '0' character")
print("This suggests reading is offset somehow")

# Let's check the exact sequence PyGuitarPro reads for beats
print("\n=== Tracing measure reading ===")

# Simulate what gp3.readBeat does for a rest
# At position 1484 (measure 2, beat 1):
# flags = 0x40 (rest)
# So it tries to read status byte

with open('D:/GitHub/NewProject/Recording.gp5', 'rb') as f:
    f.seek(1466)  # Start of measures
    
    print("Starting at position 1466 (measures)")
    
    # Let me trace through measure 1 and 2 byte by byte as PyGuitarPro would
    
    # Measure 1, Voice 1
    beat_count = struct.unpack('<i', f.read(4))[0]
    print(f"\nMeasure 1 Voice 1: beat_count={beat_count} at pos {f.tell()-4}")
    
    for b in range(beat_count):
        pos = f.tell()
        flags = struct.unpack('<B', f.read(1))[0]
        print(f"  Beat {b+1}: flags=0x{flags:02x} at pos {pos}")
        
        if flags & 0x40:  # rest
            status = struct.unpack('<B', f.read(1))[0]
            print(f"    status={status}")
        
        dur = struct.unpack('<b', f.read(1))[0]
        print(f"    duration={dur}")
        
        if flags & 0x20:  # tuplet
            tuplet = struct.unpack('<i', f.read(4))[0]
            print(f"    tuplet={tuplet}")
        
        if flags & 0x02:  # chord
            print("    [chord]")
            break
        
        if flags & 0x04:  # text
            print("    [text]")
            break
            
        if flags & 0x08:  # beat effects
            ef1 = struct.unpack('<B', f.read(1))[0]
            ef2 = struct.unpack('<B', f.read(1))[0]
            print(f"    beat effects: 0x{ef1:02x} 0x{ef2:02x}")
            # Skip parsing effects for now
            
        if flags & 0x10:  # mix table
            print("    [mix table]")
            break
        
        # Notes - only if NOT rest
        if not (flags & 0x40):
            note_flags = struct.unpack('<B', f.read(1))[0]
            print(f"    note_flags=0x{note_flags:02x}")
            
            for s in range(7):
                if note_flags & (1 << (6-s)):
                    # Read note - simplified
                    nflags = struct.unpack('<B', f.read(1))[0]
                    if nflags & 0x20:
                        note_type = struct.unpack('<B', f.read(1))[0]
                    if nflags & 0x01:
                        f.read(2)  # duration percent
                    if nflags & 0x10:
                        f.read(1)  # velocity
                    fret = struct.unpack('<b', f.read(1))[0]
                    if nflags & 0x80:
                        f.read(2)  # fingering
                    nflags2 = struct.unpack('<B', f.read(1))[0]
                    if nflags & 0x08:
                        # note effects
                        ne_flags1 = struct.unpack('<B', f.read(1))[0]
                        ne_flags2 = struct.unpack('<B', f.read(1))[0]
                        # ... more
                    print(f"      Note on string {s+1}: fret={fret}")
        
        # GP5: flags2 (short)
        flags2 = struct.unpack('<H', f.read(2))[0]
        print(f"    flags2=0x{flags2:04x}")
    
    # Measure 1, Voice 2
    beat_count = struct.unpack('<i', f.read(4))[0]
    print(f"\nMeasure 1 Voice 2: beat_count={beat_count} at pos {f.tell()-4}")
    # Should be 0
    
    # Line break
    lb = struct.unpack('<B', f.read(1))[0]
    print(f"LineBreak: {lb}")
    
    print(f"\nMeasure 1 ends at position {f.tell()}")
    
    # Now Measure 2
    # But wait - maybe the issue is that PyGuitarPro reads differently
    
    print(f"\n=== Starting Measure 2 at position {f.tell()} ===")
