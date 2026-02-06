#!/usr/bin/env python3
"""Check PyGuitarPro writeNote to understand velocity handling"""
import inspect
from guitarpro import gp5, gp4, gp3

for cls in [gp5.GP5File, gp4.GP4File, gp3.GP3File]:
    if hasattr(cls, 'writeNote'):
        print(f"=== {cls.__name__}.writeNote ===")
        print(inspect.getsource(cls.writeNote))
        print()

# Also check how packVelocity works
print("=== packVelocity ===")
for cls in [gp5.GP5File, gp4.GP4File, gp3.GP3File]:
    if hasattr(cls, 'packVelocity'):
        print(f"Found in {cls.__name__}:")
        print(inspect.getsource(cls.packVelocity))
        break

# Check unpackVelocity 
print("\n=== unpackVelocity ===")
for cls in [gp5.GP5File, gp4.GP4File, gp3.GP3File]:
    if hasattr(cls, 'unpackVelocity'):
        print(f"Found in {cls.__name__}:")
        print(inspect.getsource(cls.unpackVelocity))
        break

# Check what the default velocity is
from guitarpro import models as gp
print(f"\nNoteEffect default velocity: {gp.Velocity}")
for k in dir(gp.Velocity):
    if not k.startswith('_'):
        print(f"  {k} = {getattr(gp.Velocity, k)}")
