#!/usr/bin/env python3
"""Classify an ESP32 SDK configuration without exposing signing material."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


REQUIRED_OPTIONS = {
    "CONFIG_SECURE_BOOT": "hardware Secure Boot enforcement",
    "CONFIG_SECURE_BOOT_V2_ENABLED": "Secure Boot v2",
    "CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES": "signed application images",
    "CONFIG_SECURE_FLASH_ENC_ENABLED": "flash encryption",
    "CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE": "release-mode flash encryption",
    "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE": "bootloader rollback support",
    "CONFIG_NVS_ENCRYPTION": "encrypted NVS owner credentials",
}

FORBIDDEN_OPTIONS = {
    "CONFIG_SECURE_BOOT_ALLOW_JTAG": "JTAG under Secure Boot",
    "CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC": "ROM BASIC under Secure Boot",
    "CONFIG_SECURE_BOOT_INSECURE": "insecure Secure Boot mode",
    "CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_DEC": "UART plaintext flash decryption",
    "CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_ENC": "UART plaintext flash encryption",
    "CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_CACHE": "UART flash cache access",
}


def enabled_options(path: Path) -> set[str]:
    enabled: set[str] = set()
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and line.endswith("=y"):
            enabled.add(line[:-2])
    return enabled


def classify(path: Path, build_flags: str = "", *, external_signing: bool = False) -> dict[str, object]:
    enabled = enabled_options(path)
    missing = [option for option in REQUIRED_OPTIONS if option not in enabled]
    if external_signing:
        # Remote/HSM signing intentionally builds unsigned inputs. SDK posture
        # alone can never prove those inputs were subsequently signed.
        missing = [option for option in missing if option != "CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES"]
    insecure = [option for option in FORBIDDEN_OPTIONS if option in enabled]
    production_gate_enabled = bool(
        re.search(r"(?:^|\s)-D\s*APEXI_PRODUCTION_SECURITY_REQUIRED=1(?:\s|$)", build_flags)
    )
    encrypted_queue_qualified = bool(
        re.search(r"(?:^|\s)-D\s*APEXI_ENCRYPTED_QUEUE_QUALIFIED=1(?:\s|$)", build_flags)
    )
    return {
        "schema_version": 1,
        "sdkconfig": str(path),
        "production_eligible": not external_signing and not missing and not insecure and production_gate_enabled and encrypted_queue_qualified,
        "encrypted_queue_qualified": encrypted_queue_qualified,
        "sdk_security_ready": not missing and not insecure and production_gate_enabled,
        "external_signature_required": external_signing,
        "production_gate_enabled": production_gate_enabled,
        "required_controls": [REQUIRED_OPTIONS[option] for option in REQUIRED_OPTIONS],
        "missing_options": missing,
        "insecure_options": insecure,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("sdkconfig", type=Path)
    parser.add_argument("--require-production", action="store_true")
    parser.add_argument("--build-flags", default="")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    result = classify(args.sdkconfig, args.build_flags)
    if args.json:
        print(json.dumps(result, sort_keys=True))
    elif result["production_eligible"]:
        print("production-security: eligible")
    else:
        print("production-security: development-only")
        if not result["production_gate_enabled"]:
            print("missing: -D APEXI_PRODUCTION_SECURITY_REQUIRED=1")
        if not result["encrypted_queue_qualified"]:
            print("missing: reviewed encrypted queue qualification")
        for option in result["missing_options"]:
            print(f"missing: {option}")
        for option in result["insecure_options"]:
            print(f"insecure: {option}")
    return 1 if args.require_production and not result["production_eligible"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
