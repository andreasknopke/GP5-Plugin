#!/usr/bin/env python3
"""Check velocity defaults and packNoteFlags"""
import inspect
from guitarpro import gp5, models as gp

# Check velocity constants  
print(f"velocityIncrement = {gp.Velocities.velocityIncrement}")
print(f"minVelocity = {gp.Velocities.minVelocity}")

# List all velocity constants
for k in dir(gp.Velocities):
    if not k.startswith('_'):
        print(f"  {k} = {getattr(gp.Velocities, k)}")

# Check packNoteFlags
print("\n=== packNoteFlags ===")
for cls in [gp5.GP5File]:
    if hasattr(cls, 'packNoteFlags'):
        print(inspect.getsource(cls.packNoteFlags))
        break

# Check default note velocity
print(f"\nNote default velocity: {gp.Note().velocity}")
