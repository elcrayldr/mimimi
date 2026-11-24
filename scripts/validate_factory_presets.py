#!/usr/bin/env python3
"""Validate factory preset manifests before packaging the VST3 bundle."""
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET

preset_dir = pathlib.Path(sys.argv[1])
files = sorted(preset_dir.glob("*.craypreset"))
if len(files) < 8:
    raise SystemExit(f"Expected at least 8 factory presets, found {len(files)}")

# Build the valid APVTS parameter-ID set from the same declarations used by C++.
header = (pathlib.Path(__file__).resolve().parent.parent / "ParameterIDs.h").read_text(encoding="utf-8")
valid_ids = set(re.findall(r'inline constexpr const char\*\s+\w+\s*=\s*"([^"]+)"', header))
dynamic_id = re.compile(
    r'inline juce::String\s+(\w+)\(int \w+\)\s*\{\s*return "([^"]*)"\s*\+\s*juce::String\(\w+\s*\+\s*1\)\s*\+\s*"([^"]*)";'
)
for match in dynamic_id.finditer(header):
    function, prefix, suffix = match.groups()
    count = 4 if function.startswith("osc") else 2 if function.startswith("lfo") else 8
    valid_ids.update(f"{prefix}{index}{suffix}" for index in range(1, count + 1))

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
        unknown = sorted(set(params) - valid_ids)
        if unknown:
            raise ValueError(f"unknown APVTS parameter IDs: {', '.join(unknown)}")
        if len(params) != len(set(params)):
            raise ValueError("duplicate APVTS parameter IDs")
    except Exception as exc:
        errors.append(f"{path.name}: {exc}")

if errors:
    raise SystemExit("\n".join(errors))
print(f"Validated {len(files)} factory preset manifests and APVTS XML documents.")
