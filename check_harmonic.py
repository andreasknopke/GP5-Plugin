#!/usr/bin/env python3
"""Check harmonic format in PyGuitarPro"""
import inspect
from guitarpro import gp5, gp4, gp3

# Check readHarmonic
print("=== GP5 readHarmonic ===")
if hasattr(gp5.GP5File, 'readHarmonic'):
    print(inspect.getsource(gp5.GP5File.readHarmonic))
elif hasattr(gp4.GP4File, 'readHarmonic'):
    print(inspect.getsource(gp4.GP4File.readHarmonic))

# Check writeHarmonic 
print("\n=== GP5 writeHarmonic ===")
if hasattr(gp5.GP5File, 'writeHarmonic'):
    print(inspect.getsource(gp5.GP5File.writeHarmonic))
elif hasattr(gp4.GP4File, 'writeHarmonic'):
    print(inspect.getsource(gp4.GP4File.writeHarmonic))

# Also check grace note writing
print("\n=== GP5 writeGrace ===")
for cls in [gp5.GP5File, gp4.GP4File, gp3.GP3File]:
    if hasattr(cls, 'writeGrace'):
        print(f"Found in {cls.__name__}:")
        print(inspect.getsource(cls.writeGrace))
        break

# Check writeNoteEffects to see ALL data written
print("\n=== GP5 writeNoteEffects ===")
for cls in [gp5.GP5File, gp4.GP4File]:
    if hasattr(cls, 'writeNoteEffects'):
        print(f"Found in {cls.__name__}:")
        print(inspect.getsource(cls.writeNoteEffects))
        break
