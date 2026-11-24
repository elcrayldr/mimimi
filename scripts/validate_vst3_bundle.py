#!/usr/bin/env python3
"""Sanity-check the architecture and required resources in a Windows VST3 bundle."""
import pathlib
import struct
import sys

bundle = pathlib.Path(sys.argv[1])
if not bundle.is_dir() or bundle.suffix.lower() != ".vst3":
    raise SystemExit(f"Not a VST3 bundle directory: {bundle}")

modules = sorted((bundle / "Contents" / "x86_64-win").glob("*.vst3"))
if len(modules) != 1:
    raise SystemExit(f"Expected exactly one x86_64-win VST3 module, found {len(modules)}")

module = modules[0]
with module.open("rb") as stream:
    dos = stream.read(64)
    if len(dos) < 64 or dos[:2] != b"MZ":
        raise SystemExit("VST3 module is not a valid Windows PE image (missing MZ header)")
    pe_offset = struct.unpack_from("<I", dos, 0x3C)[0]
    stream.seek(pe_offset)
    header = stream.read(6)
    if len(header) != 6 or header[:4] != b"PE\0\0":
        raise SystemExit("VST3 module has an invalid PE signature")
    machine = struct.unpack_from("<H", header, 4)[0]

if machine != 0x8664:
    raise SystemExit(f"Expected AMD64/x64 PE machine 0x8664, got 0x{machine:04x}")

preset_dir = bundle / "Contents" / "Resources" / "FactoryPresets"
presets = sorted(preset_dir.glob("*.craypreset")) if preset_dir.is_dir() else []
if len(presets) < 8:
    raise SystemExit(f"Expected at least 8 factory presets, found {len(presets)}")

print(f"Validated x64 PE module: {module.name}")
print(f"Validated factory preset bundle entries: {len(presets)}")
