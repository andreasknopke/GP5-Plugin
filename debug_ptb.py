"""Debug script: parse Unknown.ptb and dump structure info"""
import struct, sys

def read_ptb(path):
    with open(path, 'rb') as f:
        data = f.read()
    
    # Check magic
    print(f"File size: {len(data)} bytes")
    print(f"Magic: {data[:4]}")
    
    # The header starts at offset 0
    # PowerTab file header format:
    # 4 bytes: "ptab" magic
    # 2 bytes: file version
    # 1 byte: file type (0=song, 1=lesson)
    
    offset = 0
    magic = data[offset:offset+4]
    offset += 4
    print(f"Magic bytes: {magic}")
    
    version = struct.unpack_from('<H', data, offset)[0]
    offset += 2
    print(f"Version: {version} (0x{version:04x})")
    
    file_type = data[offset]
    offset += 1
    print(f"File type: {file_type} (0=song, 1=lesson)")
    
    if file_type == 0:
        # Song header
        # Content type
        content_type = data[offset]
        offset += 1
        print(f"Content type: {content_type}")
        
        # Title (Pascal string: 1 byte length + string)
        title_len = data[offset]
        offset += 1
        title = data[offset:offset+title_len].decode('latin-1', errors='replace')
        offset += title_len
        print(f"Title: '{title}'")
        
        # Artist (Pascal string)
        artist_len = data[offset]
        offset += 1
        artist = data[offset:offset+artist_len].decode('latin-1', errors='replace')
        offset += artist_len
        print(f"Artist: '{artist}'")
    
    # Just dump bytes around guitar/staff areas to understand structure
    # Look for common patterns
    print(f"\n--- Searching for 'Standard Tuning' or instrument names ---")
    for i in range(len(data)):
        if data[i:i+8] == b'Standard':
            context = data[max(0,i-20):i+40]
            print(f"  Found 'Standard' at offset {i}: {context}")
        if data[i:i+6] == b'Guitar':
            context = data[max(0,i-10):i+30]
            print(f"  Found 'Guitar' at offset {i}")
        if data[i:i+4] == b'Bass':
            context = data[max(0,i-10):i+20]
            print(f"  Found 'Bass' at offset {i}")
        if data[i:i+8] == b'Electric':
            context = data[max(0,i-10):i+30]
            print(f"  Found 'Electric' at offset {i}")
        if data[i:i+8] == b'Acoustic':
            context = data[max(0,i-10):i+30]
            print(f"  Found 'Acoustic' at offset {i}")
        if data[i:i+8] == b'Remember':
            print(f"  Found 'Remember' at offset {i}")

if __name__ == '__main__':
    read_ptb(r'D:\GitHub\NewProject\Unknown.ptb')
