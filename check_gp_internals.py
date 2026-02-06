#!/usr/bin/env python3
"""Check PyGuitarPro internals"""
import guitarpro
from guitarpro import gp5, gp3
import inspect

# Check what attributes the file object uses
print("GP5File methods:")
for name in dir(gp5.GP5File):
    if 'stream' in name.lower() or 'tell' in name.lower() or 'read' == name[:4]:
        print(f"  {name}")

# Check the base class
print("\nGPFileBase attributes:")
src = inspect.getsource(gp5.GP5File.__init__) if hasattr(gp5.GP5File, '__init__') else "no init"
print(src[:500])

print("\n\nGP3File readVoice source:")
print(inspect.getsource(gp3.GP3File.readVoice)[:500])

# Check what readI32 uses internally
print("\n\nreadI32/readU8:")
from guitarpro import iobase
print(inspect.getsource(iobase.GPFileBase.readI32)[:300])
print(inspect.getsource(iobase.GPFileBase.readU8)[:300])
