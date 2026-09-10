from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "check_production_security", ROOT / "scripts" / "check_production_security.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ProductionSecurityTests(unittest.TestCase):
    def classify(self, content: str, build_flags: str = "") -> dict[str, object]:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sdkconfig"
            path.write_text(content, encoding="utf-8")
            return MODULE.classify(path, build_flags)

    def test_all_required_controls_are_production_eligible(self) -> None:
        config = "\n".join(f"{option}=y" for option in MODULE.REQUIRED_OPTIONS)
        result = self.classify(config, "-std=gnu++17 -D APEXI_PRODUCTION_SECURITY_REQUIRED=1 -DAPEXI_ENCRYPTED_QUEUE_QUALIFIED=1")
        self.assertTrue(result["production_eligible"])
        self.assertEqual(result["missing_options"], [])

    def test_unqualified_encrypted_queue_blocks_production(self) -> None:
        config = "\n".join(f"{option}=y" for option in MODULE.REQUIRED_OPTIONS)
        result = self.classify(config, "-DAPEXI_PRODUCTION_SECURITY_REQUIRED=1")
        self.assertTrue(result["sdk_security_ready"])
        self.assertFalse(result["production_eligible"])

    def test_missing_runtime_gate_is_not_production_eligible(self) -> None:
        config = "\n".join(f"{option}=y" for option in MODULE.REQUIRED_OPTIONS)
        result = self.classify(config)
        self.assertFalse(result["production_eligible"])
        self.assertFalse(result["production_gate_enabled"])

    def test_disabled_control_fails_closed(self) -> None:
        config = "\n".join(
            f"{option}=y"
            for option in MODULE.REQUIRED_OPTIONS
            if option != "CONFIG_SECURE_BOOT_V2_ENABLED"
        )
        config += "\n# CONFIG_SECURE_BOOT_V2_ENABLED is not set\n"
        result = self.classify(config, "-D APEXI_PRODUCTION_SECURITY_REQUIRED=1")
        self.assertFalse(result["production_eligible"])
        self.assertIn("CONFIG_SECURE_BOOT_V2_ENABLED", result["missing_options"])

    def test_generic_secure_boot_does_not_substitute_for_v2(self) -> None:
        result = self.classify("CONFIG_SECURE_BOOT=y\n")
        self.assertFalse(result["production_eligible"])
        self.assertIn("CONFIG_SECURE_BOOT_V2_ENABLED", result["missing_options"])

    def test_insecure_debug_exception_fails_closed(self) -> None:
        config = "\n".join(f"{option}=y" for option in MODULE.REQUIRED_OPTIONS)
        config += "\nCONFIG_SECURE_BOOT_ALLOW_JTAG=y\n"
        result = self.classify(config, "-D APEXI_PRODUCTION_SECURITY_REQUIRED=1")
        self.assertFalse(result["production_eligible"])
        self.assertIn("CONFIG_SECURE_BOOT_ALLOW_JTAG", result["insecure_options"])

    def test_external_signing_needs_artifact_verification(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sdkconfig"
            path.write_text("\n".join(f"{name}=y" for name in MODULE.REQUIRED_OPTIONS
                                      if name != "CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES"))
            result = MODULE.classify(path, "-DAPEXI_PRODUCTION_SECURITY_REQUIRED=1", external_signing=True)
            self.assertTrue(result["sdk_security_ready"])
            self.assertTrue(result["external_signature_required"])
            self.assertFalse(result["production_eligible"])


if __name__ == "__main__":
    unittest.main()
