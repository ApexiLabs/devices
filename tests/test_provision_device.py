import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

MODULE_PATH = Path(__file__).parents[1] / "scripts" / "provision_device.py"
SPEC = importlib.util.spec_from_file_location("provision_device", MODULE_PATH)
provision_device = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(provision_device)


def bundle(device_id="mda-aabbccddeeff"):
    return {
        "expected_device_id": device_id,
        "friendly_name": "Workshop logger",
        "hardware_revision": "esp32-wroom-32",
        "provisioned_at": "2026-09-05T00:00:00+00:00",
        "wifi": {"ssid": "network", "password": "password123"},
        "ota_password": "unique-ota-password",
        "upload": {
            "protocol": "https",
            "host": "app-dev.apexilabs.com",
            "port": 443,
            "cloudflare_client_id": "client.access",
            "cloudflare_client_secret": "secret",
            "app_device_token": "token",
            "app_token_subject": f"logger:{device_id}",
        },
    }


class ProvisionDeviceTests(unittest.TestCase):
    def test_serial_interruptions_preserve_reservation_and_redact_errors(self):
        for fault in ("write", "short_write", "flush", "read"):
            for reprovision in (False, True):
                with self.subTest(fault=fault, reprovision=reprovision):
                    class Connection:
                        identified = False

                        def __enter__(self):
                            return self

                        def __exit__(self, *_):
                            pass

                        def write(self, data):
                            if fault == "write":
                                raise OSError("driver echoed password123")
                            return len(data) - 1 if fault == "short_write" else len(data)

                        def flush(self):
                            if fault == "flush":
                                raise OSError("driver echoed password123")

                        def readline(self):
                            if not self.identified:
                                self.identified = True
                                return b"DEVICE_ID=mda-aabbccddeeff\n"
                            raise OSError("driver echoed password123")

                    with tempfile.TemporaryDirectory() as directory:
                        path = Path(directory) / "inventory.json"
                        if reprovision:
                            provision_device.reserve_identity(path, "mda-aabbccddeeff", bundle(), False)
                        with patch.dict("sys.modules", {"serial": SimpleNamespace(Serial=lambda *a, **kw: Connection())}), patch.object(provision_device.time, "sleep"):
                            with self.assertRaises(provision_device.ProvisioningIndeterminate) as error:
                                provision_device.provision_serial("fake", bundle(), path, reprovision)
                        self.assertNotIn("password123", str(error.exception))
                        self.assertEqual(json.loads(path.read_text())["devices"][0]["status"], "indeterminate")
                        self.assertNotIn("password123", path.read_text())
                        with self.assertRaisesRegex(provision_device.ProvisioningError, "indeterminate"):
                            provision_device.reserve_identity(path, "mda-aabbccddeeff", bundle(), False)

    def test_serial_acceptance_commits_but_rejection_preserves_reservation(self):
        for accepted, reprovision in ((True, False), (True, True), (False, False), (False, True)):
            with self.subTest(accepted=accepted, reprovision=reprovision):
                class Connection:
                    lines = iter([
                        b"DEVICE_ID=mda-aabbccddeeff\n",
                        b"PROVISIONING_RESULT=accepted\n" if accepted else b"PROVISIONING_RESULT=rejected private-device-text\n",
                    ])

                    def __enter__(self): return self
                    def __exit__(self, *_): pass
                    def write(self, data): return len(data)
                    def flush(self): pass
                    def readline(self): return next(self.lines)

                with tempfile.TemporaryDirectory() as directory:
                    path = Path(directory) / "inventory.json"
                    if reprovision:
                        provision_device.reserve_identity(path, "mda-aabbccddeeff", bundle(), False)
                    with patch.dict("sys.modules", {"serial": SimpleNamespace(Serial=lambda *a, **kw: Connection())}), patch.object(provision_device.time, "sleep"):
                        if accepted:
                            provision_device.provision_serial("fake", bundle(), path, reprovision)
                            self.assertEqual(json.loads(path.read_text())["devices"][0]["status"], "committed")
                        else:
                            with self.assertRaises(provision_device.ProvisioningIndeterminate) as error:
                                provision_device.provision_serial("fake", bundle(), path, reprovision)
                            self.assertNotIn("private-device-text", str(error.exception))
                            self.assertEqual(json.loads(path.read_text())["devices"][0]["status"], "indeterminate")
                            with self.assertRaisesRegex(provision_device.ProvisioningError, "indeterminate"):
                                provision_device.reserve_identity(path, "mda-aabbccddeeff", bundle(), False)

    def test_accepts_identity_bound_bundle(self):
        validated = provision_device.validate_bundle(bundle(), "mda-aabbccddeeff")
        self.assertEqual(validated["expected_device_id"], "mda-aabbccddeeff")
        self.assertFalse(validated["remote_management_enabled"])

        managed = bundle()
        managed["remote_management_enabled"] = True
        self.assertTrue(
            provision_device.validate_bundle(managed, "mda-aabbccddeeff")[
                "remote_management_enabled"
            ]
        )

    def test_rejects_blank_malformed_and_mismatched_identity(self):
        with self.assertRaises(provision_device.ProvisioningError):
            provision_device.validate_bundle({}, "mda-aabbccddeeff")
        with self.assertRaises(provision_device.ProvisioningError):
            provision_device.validate_bundle(bundle(), "mda-INVALID")
        with self.assertRaises(provision_device.ProvisioningError):
            provision_device.validate_bundle(bundle(), "mda-000000000001")
        wrong_subject = bundle()
        wrong_subject["upload"]["app_token_subject"] = "mda-000000000001"
        with self.assertRaises(provision_device.ProvisioningError):
            provision_device.validate_bundle(wrong_subject, "mda-aabbccddeeff")
        unknown_protocol = bundle()
        unknown_protocol["upload"]["protocol"] = "ftp"
        with self.assertRaises(provision_device.ProvisioningError):
            provision_device.validate_bundle(unknown_protocol, "mda-aabbccddeeff")
        malformed_port = bundle()
        malformed_port["upload"]["port"] = "443"
        with self.assertRaisesRegex(provision_device.ProvisioningError, "upload.port"):
            provision_device.validate_bundle(malformed_port, "mda-aabbccddeeff")
        non_string_secret = bundle()
        non_string_secret["ota_password"] = ["not", "text"]
        with self.assertRaisesRegex(provision_device.ProvisioningError, "ota_password"):
            provision_device.validate_bundle(non_string_secret, "mda-aabbccddeeff")
        blank_timestamp = bundle()
        blank_timestamp["provisioned_at"] = ""
        with self.assertRaisesRegex(provision_device.ProvisioningError, "provisioned_at"):
            provision_device.validate_bundle(blank_timestamp, "mda-aabbccddeeff")
        invalid_management = bundle()
        invalid_management["remote_management_enabled"] = "yes"
        with self.assertRaisesRegex(provision_device.ProvisioningError, "remote_management_enabled"):
            provision_device.validate_bundle(invalid_management, "mda-aabbccddeeff")

    def test_duplicate_inventory_fails_closed_and_contains_no_secrets(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            validated = provision_device.validate_bundle(bundle(), "mda-aabbccddeeff")
            provision_device.reserve_identity(path, "mda-aabbccddeeff", validated, False)
            inventory_text = path.read_text()
            self.assertNotIn("password123", inventory_text)
            self.assertNotIn("client.access", inventory_text)
            self.assertNotIn("token", inventory_text)
            with self.assertRaises(provision_device.ProvisioningError):
                provision_device.reserve_identity(path, "mda-aabbccddeeff", validated, False)
            provision_device.reserve_identity(path, "mda-aabbccddeeff", validated, True)
            self.assertEqual(len(json.loads(path.read_text())["devices"]), 1)

    def test_duplicate_is_rejected_before_delivery(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            provision_device.commission_identity(
                path, "mda-aabbccddeeff", bundle(), False, lambda _: None
            )
            delivered = []
            with self.assertRaises(provision_device.ProvisioningError):
                provision_device.commission_identity(
                    path,
                    "mda-aabbccddeeff",
                    bundle(),
                    False,
                    lambda payload: delivered.append(payload),
                )
            self.assertEqual(delivered, [])

    def test_failed_delivery_rolls_back_pending_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"

            def fail(_):
                raise provision_device.ProvisioningError("device rejected record")

            with self.assertRaises(provision_device.ProvisioningError):
                provision_device.commission_identity(
                    path, "mda-aabbccddeeff", bundle(), False, fail
                )
            self.assertEqual(json.loads(path.read_text())["devices"], [])

    def test_ambiguous_delivery_retains_indeterminate_reservation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"

            def lose_acknowledgement(_):
                raise provision_device.ProvisioningIndeterminate("timeout")

            with self.assertRaises(provision_device.ProvisioningIndeterminate):
                provision_device.commission_identity(
                    path,
                    "mda-aabbccddeeff",
                    bundle(),
                    False,
                    lose_acknowledgement,
                )
            entry = json.loads(path.read_text())["devices"][0]
            self.assertEqual(entry["status"], "indeterminate")

    def test_crash_left_pending_is_reported_as_indeterminate(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(
                json.dumps(
                    {
                        "devices": [
                            {
                                "device_id": "mda-aabbccddeeff",
                                "friendly_name": "Workshop logger",
                                "hardware_revision": "esp32-wroom-32",
                                "provisioned_at": "2026-09-05T00:00:00+00:00",
                                "status": "pending",
                            }
                        ]
                    }
                )
            )
            with self.assertRaisesRegex(provision_device.ProvisioningError, "indeterminate"):
                provision_device.commission_identity(
                    path, "mda-aabbccddeeff", bundle(), False, lambda _: None
                )
            self.assertEqual(json.loads(path.read_text())["devices"][0]["status"], "indeterminate")


if __name__ == "__main__":
    unittest.main()
