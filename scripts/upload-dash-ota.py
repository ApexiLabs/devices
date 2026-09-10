#!/usr/bin/env python3
"""Upload the prebuilt Dash application without exposing the password in argv."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
from urllib.request import ProxyHandler, build_opener


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", help="Dash station IP or hostname")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    binary = root / ".pio/build/dash-waveshare-s3-128/firmware.bin"
    partitions = root / ".pio/build/dash-waveshare-s3-128/partitions.bin"
    secrets = (root / "include/AppSecrets.h").read_text()
    match = re.search(r'^\s*#define\s+APEXI_OTA_PASSWORD\s+"([^"\n]+)"', secrets, re.M)
    if not match:
        raise ValueError("APEXI_OTA_PASSWORD must be configured locally")
    # Check the actual generated partition table, including binary padding.
    import struct
    slots = []
    table = partitions.read_bytes()
    for offset in range(0, len(table) - 31, 32):
        magic, kind, subtype, address, size = struct.unpack_from("<HBBII", table, offset)
        if magic == 0x50AA and kind == 0 and subtype in (0x10, 0x11):
            slots.append(size)
    if len(slots) != 2 or binary.stat().st_size > min(slots):
        raise ValueError("Dash image does not fit both generated OTA slots")
    opener = build_opener(ProxyHandler({}))
    with opener.open(f"http://{args.target}/api/status", timeout=5) as response:
        status = json.load(response)
    if status.get("device") != "APEXI-DASH" or not status.get("otaReady"):
        raise ValueError("Target is not an OTA-ready Apexi Dash")
    tool = root / ".platformio/packages/framework-arduinoespressif32/tools/espota.py"
    spec = importlib.util.spec_from_file_location("dash_espota", tool)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.main(["--ip", args.target, "--auth", match.group(1), "--file", str(binary)])


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"Dash OTA: {error}", file=sys.stderr)
        sys.exit(1)
