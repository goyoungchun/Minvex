"""Convert the pinned LTC reference coefficients to canonical IEEE binary16."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path, help="fit/results/ltc.js from the pinned upstream revision")
parser.add_argument("--output", type=Path, default=root / "third_party/ltc/Tables.h")
args = parser.parse_args()
manifest = json.loads((root / "third_party/versions.json").read_text(encoding="utf-8-sig"))["ltc"]
raw = args.source.read_bytes()
if hashlib.sha256(raw).hexdigest() != manifest["sourceSha256"]:
    raise SystemExit("LTC reference source checksum mismatch")
arrays = []
for name in ("g_ltc_1", "g_ltc_2"):
    match = re.search(r"var\s+" + name + r"\s*=\s*(\[.*?\]);", raw.decode(), re.S)
    if not match:
        raise SystemExit(f"Missing {name}")
    values = json.loads(re.sub(r",\s*\]", "]", match.group(1)))
    if len(values) != 64 * 64 * 4 or any(not abs(v) <= 65504 for v in values):
        raise SystemExit("Unexpected LTC table dimensions or coefficient range")
    arrays.append([struct.unpack("<H", struct.pack("<e", v))[0] for v in values])
text = (
    f"// LTC coefficients from selfshadow/ltc_code revision {manifest['revision']}.\n"
    "// Binary16 conversion of fit/results/ltc.js; see LICENSE and the SIGGRAPH 2016 paper.\n"
    "#pragma once\n#include <cstdint>\nnamespace lv::detail {\n"
)
for name, data in zip(("LtcMatrix", "LtcAmplitude"), arrays):
    text += f"inline constexpr std::uint16_t {name}[16384]={{\n"
    text += "\n".join(",".join(hex(v) for v in data[i:i + 16]) + "," for i in range(0, len(data), 16))
    text += "\n};\n"
text += "}\n"
args.output.write_text(text, encoding="utf-8", newline="\n")
print("Generated 64 KiB of canonical binary16 LTC coefficients")
