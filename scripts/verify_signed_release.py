#!/usr/bin/env python3
"""Read-only, secret-safe verification of externally signed ESP32 build evidence.

This does not build, sign, flash, enroll eFuses, or authorize production release.
The two input builds and public key must come from a trusted release process.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import json
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Callable

from check_production_security import classify, enabled_options

ESPTOOL_VERSION = "4.11.0"
VERIFICATION_TOOLS = {"esptool": ESPTOOL_VERSION, "cryptography": "50.0.1"}
INPUTS = ("firmware.bin", "bootloader.bin", "partitions.bin", "sdkconfig", "build-flags.txt", "dependencies.lock")
LIMITS = {"firmware.bin": 0x200000, "bootloader.bin": 0x7000}


class EvidenceError(ValueError):
    pass


def read_bounded(path: Path, maximum: int) -> bytes:
    with path.open("rb") as stream:
        data = stream.read(maximum + 1)
    if not data or len(data) > maximum:
        raise EvidenceError("artifact is empty or exceeds profile capacity")
    return data


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_signature(image: Path, public_key: Path) -> None:
    if any(importlib.metadata.version(name) != version for name, version in VERIFICATION_TOOLS.items()):
        raise EvidenceError("install the pinned production verification toolchain")
    from cryptography.hazmat.primitives.serialization import load_pem_public_key
    from cryptography.hazmat.primitives.asymmetric.rsa import RSAPublicKey
    key = load_pem_public_key(public_key.read_bytes())
    if not isinstance(key, RSAPublicKey) or key.key_size != 3072:
        raise EvidenceError("ESP32 Secure Boot v2 requires the approved RSA-3072 public key")
    result = subprocess.run(
        [sys.executable, "-m", "espsecure", "verify_signature", "--version", "2",
         "--keyfile", str(public_key.resolve()), str(image.resolve())],
        capture_output=True, timeout=60, check=False,
    )
    if result.returncode != 0:
        # Never echo tool stdout/stderr, key paths, configuration, or bundles.
        raise EvidenceError("Secure Boot v2 signature verification failed")


def verify(first: Path, second: Path, signed: Path, public_key: Path,
           source_commit: str, project: str, security_version: int,
           verifier: Callable[[Path, Path], None] = verify_signature) -> dict[str, object]:
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        raise EvidenceError("source commit must be an immutable full SHA")
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,31}", project) or not 0 <= security_version <= 32:
        raise EvidenceError("invalid project or security-version policy")
    key = read_bounded(public_key, 8192)
    if b"PRIVATE" in key or not key.startswith(b"-----BEGIN PUBLIC KEY-----"):
        raise EvidenceError("only a public verification key is accepted")
    inputs: dict[str, bytes] = {}
    for name in INPUTS:
        left = read_bounded(first / name, LIMITS.get(name, 1024 * 1024))
        right = read_bounded(second / name, LIMITS.get(name, 1024 * 1024))
        if left != right:
            raise EvidenceError("independent build inputs are not byte-identical")
        inputs[name] = left
    # Reject contradictory/repeated SDK entries instead of accepting any '=y'.
    entries: set[str] = set()
    for line in inputs["sdkconfig"].decode("utf-8").splitlines():
        match = re.match(r"(?:# )?(CONFIG_[A-Z0-9_]+)(?:=| is not set)", line)
        if match:
            if match[1] in entries:
                raise EvidenceError("duplicate SDK configuration entry")
            entries.add(match[1])
    flags = inputs["build-flags.txt"].decode("utf-8")
    if not classify(first / "sdkconfig", flags, external_signing=True)["sdk_security_ready"]:
        raise EvidenceError("SDK or runtime security gate is not ready")
    options = enabled_options(first / "sdkconfig")
    if "CONFIG_SECURE_BOOT" not in options or "CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK" in options:
        raise EvidenceError("unsupported secure-boot or automatic anti-rollback policy")
    reproducible = "CONFIG_APP_REPRODUCIBLE_BUILD" in options or (
        "CONFIG_APP_COMPILE_TIME_DATE" not in options and "CONFIG_COMPILER_HIDE_PATHS_MACROS" in options
    )
    if not reproducible:
        raise EvidenceError("reproducible-build SDK controls are missing")
    # Fixed secure-profile template; do not silently enlarge an inactive slot or
    # adopt a factory image/partition migration through an OTA artifact.
    app = inputs["firmware.bin"]
    if len(app) < 288 or app[0] != 0xE9 or struct.unpack_from("<H", app, 12)[0] != 0:
        raise EvidenceError("application target is not the supported ESP32 image")
    if struct.unpack_from("<II", app, 32) != (0xABCD5432, security_version):
        raise EvidenceError("application security version does not match policy")
    expected_project = project.encode("ascii").ljust(32, b"\0")
    if app[80:112] != expected_project:
        raise EvidenceError("application project does not match policy")
    partitions = [
        (1, 2, 0x9000, 0x4000, b"nvs", 0), (1, 4, 0xD000, 0x1000, b"nvs_keys", 1),
        (1, 0, 0xE000, 0x2000, b"otadata", 0), (0, 0x10, 0x10000, 0x200000, b"app0", 0),
        (0, 0x11, 0x210000, 0x200000, b"app1", 0), (1, 0x82, 0x410000, 0xBF0000, b"littlefs", 1),
    ]
    table = b"".join(struct.pack("<HBBII16sI", 0x50AA, *entry) for entry in partitions)
    table += b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(table).digest()
    if inputs["partitions.bin"] != table.ljust(0xC00, b"\xff"):
        raise EvidenceError("partition table does not match encrypted NVS/dual-slot profile")
    artifacts = {}
    for name in LIMITS:
        data = read_bounded(signed / name, LIMITS[name])
        unsigned = inputs[name]
        padded = unsigned + b"\xff" * (-len(unsigned) % 4096)
        if len(data) != len(padded) + 4096 or data[:-4096] != padded:
            raise EvidenceError("signed image differs from the reproduced unsigned input")
        # Verify immutable private snapshots, not paths that can change between
        # the content comparison, signature check, and manifest hashing.
        with tempfile.TemporaryDirectory(prefix="apexi-signed-evidence-") as directory:
            snapshot = Path(directory)
            (snapshot / "image.bin").write_bytes(data)
            (snapshot / "public.pem").write_bytes(key)
            verifier(snapshot / "image.bin", snapshot / "public.pem")
        artifacts[name] = {"sha256": sha(data), "bytes": len(data), "signature_verified": True}
    return {
        "schema_version": 1,
        "classification": "candidate-evidence-only",
        "production_approved": False,
        "source_provenance_verified": False,
        "source_commit_claim": source_commit,
        "project": project,
        "security_version": security_version,
        "public_key_sha256": sha(key),
        "verification_tools": VERIFICATION_TOOLS.copy(),
        "reproduced_input_sha256": {name: sha(value) for name, value in inputs.items()},
        "signed_artifacts": artifacts,
        "remaining_gates": ["trusted build provenance", "secure SDK integration", "signer authorization",
                            "hardware identity/eFuse enrollment", "signed/unsigned OTA and rollback rehearsal",
                            "authenticated delivery and owner recovery authorization",
                            "encrypted LittleFS compatibility and qualification"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("first-build", "second-build", "signed", "public-key"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--project", required=True)
    parser.add_argument("--security-version", type=int, required=True)
    args = parser.parse_args()
    try:
        result = verify(args.first_build, args.second_build, args.signed, args.public_key,
                        args.source_commit, args.project, args.security_version)
    except EvidenceError as error:
        print(f"signed-release: {error}", file=sys.stderr)
        return 1
    except Exception:
        print("signed-release: evidence verification failed", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
