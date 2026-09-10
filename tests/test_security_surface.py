from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class SecuritySurfaceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.web_ui = (ROOT / "src" / "WebUi.cpp").read_text(encoding="utf-8")

    def test_live_json_does_not_reference_secret_fields(self) -> None:
        start = self.web_ui.index("String WebUi::liveJson() const")
        end = self.web_ui.index("String WebUi::sensorCardsHtml() const", start)
        live_json = self.web_ui[start:end]
        for symbol in (
            "wifiPassword",
            "settingsPassword",
            "mqttPassword",
            "cloudflareAccessClientSecret",
            "appDeviceToken",
            "otaPassword",
        ):
            self.assertNotIn(symbol, live_json)

    def test_esp32_local_settings_fail_closed(self) -> None:
        self.assertIn("if (!localSettingsEnabled_)", self.web_ui)
        self.assertGreaterEqual(self.web_ui.count("server_.send(410"), 2)

    def test_rotation_status_contains_ack_but_no_bearer(self) -> None:
        live_upload = (ROOT / "src" / "LiveUpload.cpp").read_text(encoding="utf-8")
        start = live_upload.index("String LiveUpload::buildStatusJson")
        end = live_upload.index("String LiveUpload::buildSnapshotJson", start)
        status_builder = live_upload[start:end]
        self.assertIn("credential_rotation_ack", status_builder)
        self.assertNotIn("candidateBearer", status_builder)
        self.assertNotIn("activeBearer", status_builder)
        self.assertNotIn('\\"token\\"', status_builder)


if __name__ == "__main__":
    unittest.main()
