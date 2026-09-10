from __future__ import annotations
import hashlib
import json
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import verify_signed_release as release
from check_production_security import REQUIRED_OPTIONS


class SignedReleaseTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        root = Path(self.directory.name)
        self.first, self.second, self.signed = (root / name for name in ("one", "two", "signed"))
        for folder in (self.first, self.second, self.signed): folder.mkdir()
        self.key = root / "public.pem"
        self.key.write_bytes(b"-----BEGIN PUBLIC KEY-----\nUNIT TEST PUBLIC PLACEHOLDER\n")
        app = bytearray(512)
        app[0] = 0xE9
        struct.pack_into("<II", app, 32, 0xABCD5432, 0)
        app[80:112] = b"apexi-logger".ljust(32, b"\0")
        rows = [(1, 2, 0x9000, 0x4000, b"nvs", 0), (1, 4, 0xD000, 0x1000, b"nvs_keys", 1),
                (1, 0, 0xE000, 0x2000, b"otadata", 0), (0, 0x10, 0x10000, 0x200000, b"app0", 0),
                (0, 0x11, 0x210000, 0x200000, b"app1", 0), (1, 0x82, 0x410000, 0xBF0000, b"littlefs", 1)]
        table = b"".join(struct.pack("<HBBII16sI", 0x50AA, *row) for row in rows)
        table += b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(table).digest()
        config = "\n".join(f"{name}=y" for name in REQUIRED_OPTIONS if name != "CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES")
        config += "\nCONFIG_COMPILER_HIDE_PATHS_MACROS=y\n"
        values = {"firmware.bin": bytes(app), "bootloader.bin": b"bootloader fixture",
                  "partitions.bin": table.ljust(0xC00, b"\xff"), "sdkconfig": config.encode(),
                  "build-flags.txt": b"-D APEXI_PRODUCTION_SECURITY_REQUIRED=1",
                  "dependencies.lock": b"immutable toolchain/dependency fixture"}
        for name, value in values.items():
            for folder in (self.first, self.second): (folder / name).write_bytes(value)
            if name in release.LIMITS:
                (self.signed / name).write_bytes(value + b"\xff" * (-len(value) % 4096) + b"\0" * 4096)
        self.calls = []

    def verify(self, verifier=None):
        if verifier is None:
            def verifier(image, key):
                self.assertNotEqual(image.parent, self.signed)
                self.assertTrue(image.exists() and key.exists())
                self.calls.append(image.read_bytes())
        return release.verify(self.first, self.second, self.signed, self.key,
                              "a" * 40, "apexi-logger", 0, verifier=verifier)

    def test_candidate_evidence_never_claims_production_approval(self):
        evidence = self.verify()
        self.assertEqual(len(self.calls), 2)
        self.assertFalse(evidence["production_approved"])
        self.assertFalse(evidence["source_provenance_verified"])
        self.assertNotIn(str(self.first), json.dumps(evidence))

    def test_build_mismatch_fails_before_signature_tool(self):
        (self.second / "firmware.bin").write_bytes(b"different")
        with self.assertRaises(release.EvidenceError): self.verify()
        self.assertFalse(self.calls)

    def test_private_key_is_rejected_without_echo(self):
        self.key.write_bytes(b"-----BEGIN PRIVATE KEY-----\nDO-NOT-PRINT\n")
        with self.assertRaises(release.EvidenceError) as error: self.verify()
        self.assertNotIn("DO-NOT-PRINT", str(error.exception))
        self.assertFalse(self.calls)

    def test_unsigned_or_modified_image_fails(self):
        (self.signed / "firmware.bin").write_bytes((self.first / "firmware.bin").read_bytes())
        with self.assertRaises(release.EvidenceError): self.verify()

    def test_signature_failure_never_emits_success(self):
        def reject(*_): raise release.EvidenceError("signature failed")
        with self.assertRaises(release.EvidenceError): self.verify(reject)

    def test_unpinned_crypto_backend_fails_before_tool_execution(self):
        for package in release.VERIFICATION_TOOLS:
            with self.subTest(package=package), patch.object(release.importlib.metadata, "version", side_effect=lambda name: "0" if name == package else release.VERIFICATION_TOOLS[name]), patch.object(release.subprocess, "run") as run:
                with self.assertRaises(release.EvidenceError):
                    release.verify_signature(self.signed / "firmware.bin", self.key)
                run.assert_not_called()

    def test_insecure_duplicate_or_unreproducible_sdk_fails(self):
        original = (self.first / "sdkconfig").read_text()
        variants = [original + "CONFIG_SECURE_BOOT_ALLOW_JTAG=y\n",
                    original + "CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=y\n",
                    original + "CONFIG_SECURE_BOOT=n\n",
                    original.replace("CONFIG_NVS_ENCRYPTION=y", "CONFIG_NVS_ENCRYPTION=n"),
                    original.replace("CONFIG_COMPILER_HIDE_PATHS_MACROS=y", "CONFIG_APP_COMPILE_TIME_DATE=y")]
        for content in variants:
            with self.subTest(content=content):
                for folder in (self.first, self.second): (folder / "sdkconfig").write_text(content)
                with self.assertRaises(release.EvidenceError): self.verify()

    def test_partition_mismatch_and_wrong_security_version_fail(self):
        for folder in (self.first, self.second):
            (folder / "partitions.bin").write_bytes(b"old unencrypted layout")
        with self.assertRaises(release.EvidenceError): self.verify()


if __name__ == "__main__": unittest.main()
