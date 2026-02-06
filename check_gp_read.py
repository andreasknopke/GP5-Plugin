#!/usr/bin/env python3
"""Check PyGuitarPro read internals"""
import inspect
from guitarpro import iobase
print(inspect.getsource(iobase.GPFileBase.read)[:500])
print("\n---\n")
print(inspect.getsource(iobase.GPFileBase.__init__)[:500])

# Check data type
print("\n---\n")
print("GPFileBase attributes:")
for name, val in vars(iobase.GPFileBase).items():
    if not name.startswith('__'):
        print(f"  {name}: {type(val).__name__}")
