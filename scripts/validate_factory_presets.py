#!/usr/bin/env python3
"""Validate factory preset manifests before packaging the VST3 bundle."""
import json
import pathlib
import sys
import xml.etree.ElementTree as ET

preset_dir = pathlib.Path(sys.argv[1])
files = sorted(preset_dir.glob("*.craypreset"))
if len(files) < 8:
    raise SystemExit(f"Expected at least 8 factory presets, found {len(files)}")

errors = []
for path in files:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        if int(data.get("schemaVersion", 0)) < 1:
            raise ValueError("missing/unsupported schemaVersion")
        xml_text = data.get("apvtsXml", "")
        if not xml_text:
            raise ValueError("missing apvtsXml")
        root = ET.fromstring(xml_text)
        if root.tag != "CRAY_LDR_PARAMETERS":
            raise ValueError(f"unexpected APVTS root: {root.tag}")
        params = [node.attrib.get("id", "") for node in root.iter("PARAM")]
        if not params:
            raise ValueError("no APVTS PARAM nodes")
        legacy = [pid for pid in params if pid.endswith("_waveform")]
        if legacy:
            raise ValueError(f"legacy oscillator parameter IDs: {', '.join(legacy)}")
        if len(params) != len(set(params)):
            raise ValueError("duplicate APVTS parameter IDs")
    except Exception as exc:
        errors.append(f"{path.name}: {exc}")

if errors:
    raise SystemExit("\n".join(errors))
print(f"Validated {len(files)} factory preset manifests and APVTS XML documents.")
