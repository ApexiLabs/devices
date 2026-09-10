#!/usr/bin/env python3
"""Identity-bound USB provisioning for production ESP32 loggers."""

from __future__ import annotations

import argparse
import contextlib
import datetime as dt
import fcntl
import json
import os
import re
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Callable, Iterator

DEVICE_ID_PATTERN = re.compile(r"^mda-[0-9a-f]{12}$")


class ProvisioningError(ValueError):
    pass


class ProvisioningIndeterminate(ProvisioningError):
    """The device may have committed the record but acknowledgement was lost."""


def _text(value: Any, field: str, maximum: int, minimum: int = 1) -> str:
    if not isinstance(value, str):
        raise ProvisioningError(f"{field} must be text")
    if not minimum <= len(value) <= maximum:
        raise ProvisioningError(f"{field} length must be {minimum}..{maximum}")
    if any(ord(character) < 0x20 or ord(character) == 0x7F for character in value):
        raise ProvisioningError(f"{field} contains a control character")
    return value


def validate_bundle(bundle: dict[str, Any], observed_device_id: str) -> dict[str, Any]:
    if not isinstance(bundle, dict):
        raise ProvisioningError("bundle must be a JSON object")
    if not DEVICE_ID_PATTERN.fullmatch(observed_device_id) or observed_device_id.endswith(
        "000000000000"
    ):
        raise ProvisioningError("device reported a malformed immutable ID")
    expected = bundle.get("expected_device_id")
    if expected is not None:
        _text(expected, "expected_device_id", 16)
    if expected and expected != observed_device_id:
        raise ProvisioningError("expected_device_id does not match the attached device")
    result = dict(bundle)
    result["expected_device_id"] = observed_device_id
    if "provisioned_at" not in result:
        result["provisioned_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    _text(result.get("friendly_name"), "friendly_name", 63)
    _text(result.get("hardware_revision"), "hardware_revision", 31)
    _text(result.get("provisioned_at"), "provisioned_at", 40)
    _text(result.get("ota_password"), "ota_password", 127, 12)
    remote_management = result.get("remote_management_enabled", False)
    if not isinstance(remote_management, bool):
        raise ProvisioningError("remote_management_enabled must be boolean")
    result["remote_management_enabled"] = remote_management
    wifi = result.get("wifi")
    upload = result.get("upload")
    if not isinstance(wifi, dict):
        raise ProvisioningError("wifi must be a JSON object")
    if not isinstance(upload, dict):
        raise ProvisioningError("upload must be a JSON object")
    _text(wifi.get("ssid"), "wifi.ssid", 32)
    _text(wifi.get("password"), "wifi.password", 63, 8)
    protocol = _text(upload.get("protocol"), "upload.protocol", 5)
    if protocol not in {"https", "mqtt"}:
        raise ProvisioningError("upload.protocol must be https or mqtt")
    _text(upload.get("host"), "upload.host", 127)
    port = upload.get("port")
    if isinstance(port, bool) or not isinstance(port, int) or not 1 <= port <= 65535:
        raise ProvisioningError("upload.port must be an integer from 1 to 65535")
    if protocol == "mqtt":
        mqtt_username = _text(upload.get("mqtt_username"), "upload.mqtt_username", 127)
        if mqtt_username != observed_device_id:
            raise ProvisioningError("MQTT username must equal the immutable device ID")
        _text(upload.get("mqtt_password"), "upload.mqtt_password", 127)
    else:
        _text(upload.get("cloudflare_client_id"), "upload.cloudflare_client_id", 127)
        _text(upload.get("cloudflare_client_secret"), "upload.cloudflare_client_secret", 127)
        _text(upload.get("app_device_token"), "upload.app_device_token", 511)
        app_subject = _text(upload.get("app_token_subject"), "upload.app_token_subject", 80)
        if app_subject != f"logger:{observed_device_id}":
            raise ProvisioningError("app bearer subject must equal logger:<immutable device ID>")
    return result


def _write_inventory(inventory_path: Path, inventory: dict[str, Any]) -> None:
    fd, temporary_name = tempfile.mkstemp(prefix=".inventory-", dir=inventory_path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            json.dump(inventory, handle, indent=2, sort_keys=True)
            handle.write("\n")
        os.replace(temporary_name, inventory_path)
    finally:
        if os.path.exists(temporary_name):
            os.unlink(temporary_name)


@contextlib.contextmanager
def _locked_inventory(inventory_path: Path) -> Iterator[dict[str, Any]]:
    inventory_path.parent.mkdir(parents=True, exist_ok=True)
    lock_path = inventory_path.with_suffix(inventory_path.suffix + ".lock")
    with lock_path.open("a+", encoding="utf-8") as lock:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
        inventory = {"devices": []}
        if inventory_path.exists():
            inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
        yield inventory
        fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def commission_identity(
    inventory_path: Path,
    observed_device_id: str,
    bundle: dict[str, Any],
    reprovision: bool,
    deliver: Callable[[dict[str, Any]], None],
) -> None:
    metadata = validate_bundle(bundle, observed_device_id)
    with _locked_inventory(inventory_path) as inventory:
        devices = inventory.setdefault("devices", [])
        existing = next(
            (entry for entry in devices if entry.get("device_id") == observed_device_id), None
        )
        if existing is not None and existing.get("status") == "pending":
            existing["status"] = "indeterminate"
            _write_inventory(inventory_path, inventory)
        if existing is not None and not reprovision:
            if existing.get("status") == "indeterminate":
                raise ProvisioningError(
                    f"identity {observed_device_id} has an indeterminate prior attempt; "
                    "inspect the device before using --reprovision"
                )
            raise ProvisioningError(
                f"duplicate identity {observed_device_id} already exists; "
                "use --reprovision only after ownership checks"
            )
        previous = dict(existing) if existing is not None else None
        safe_entry = {
            "device_id": observed_device_id,
            "friendly_name": metadata["friendly_name"],
            "hardware_revision": metadata["hardware_revision"],
            "provisioned_at": metadata["provisioned_at"],
            "status": "pending",
        }
        if existing is None:
            devices.append(safe_entry)
            existing = safe_entry
        else:
            existing.clear()
            existing.update(safe_entry)
        _write_inventory(inventory_path, inventory)
        try:
            deliver(metadata)
        except ProvisioningIndeterminate:
            existing["status"] = "indeterminate"
            _write_inventory(inventory_path, inventory)
            raise
        except ProvisioningError:
            if previous is None:
                devices.remove(existing)
            else:
                existing.clear()
                existing.update(previous)
            _write_inventory(inventory_path, inventory)
            raise
        except Exception:
            # A driver/write/flush/read exception is not proof that the device
            # rejected the bundle. It may already have committed owner state.
            existing["status"] = "indeterminate"
            _write_inventory(inventory_path, inventory)
            raise ProvisioningIndeterminate(
                "provisioning delivery was interrupted; inspect the device before retrying"
            ) from None
        existing["status"] = "committed"
        _write_inventory(inventory_path, inventory)


def reserve_identity(inventory_path: Path, device_id: str, metadata: dict[str, Any], reprovision: bool) -> None:
    """Compatibility helper for inventory-only validation and tests."""
    commission_identity(inventory_path, device_id, metadata, reprovision, lambda _: None)


def provision_serial(
    port: str, bundle: dict[str, Any], inventory_path: Path, reprovision: bool
) -> str:
    try:
        import serial  # type: ignore
    except ImportError as error:
        raise ProvisioningError("pyserial is required for USB provisioning") from error
    with serial.Serial(port, 115200, timeout=0.1) as connection:
        connection.dtr = False
        time.sleep(0.1)
        connection.dtr = True
        deadline = time.monotonic() + 10
        observed = ""
        while time.monotonic() < deadline:
            line = connection.readline().decode("utf-8", errors="replace").strip()
            if line.startswith("DEVICE_ID="):
                observed = line.removeprefix("DEVICE_ID=")
                break
        if not observed:
            raise ProvisioningError("logger did not report DEVICE_ID after reset")

        def deliver(validated: dict[str, Any]) -> None:
            payload = json.dumps(validated, separators=(",", ":"))
            encoded = ("APEXI_PROVISION " + payload + "\n").encode()
            if connection.write(encoded) != len(encoded):
                raise ProvisioningIndeterminate(
                    "provisioning write was incomplete; inspect the device before retrying"
                )
            connection.flush()
            acknowledgement_deadline = time.monotonic() + 5
            while time.monotonic() < acknowledgement_deadline:
                line = connection.readline().decode("utf-8", errors="replace").strip()
                if line == "PROVISIONING_RESULT=accepted":
                    return
                if line.startswith("PROVISIONING_RESULT=rejected"):
                    # Legacy firmware can reject after committing owner state
                    # without proving that its rollback succeeded.
                    raise ProvisioningIndeterminate(
                        "logger rejected provisioning but credential cleanup is unconfirmed; "
                        "inspect the device before retrying"
                    )
            raise ProvisioningIndeterminate(
                "logger acknowledgement was not received; inventory remains indeterminate"
            )

        commission_identity(inventory_path, observed, bundle, reprovision, deliver)
        return observed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", type=Path, help="secret JSON bundle; keep outside version control")
    parser.add_argument("--port", help="serial device, for example /dev/cu.usbserial-0001")
    parser.add_argument("--device-id", help="validate only, without opening a serial port")
    parser.add_argument("--inventory", type=Path, required=True, help="non-secret fleet inventory JSON")
    parser.add_argument("--reprovision", action="store_true")
    args = parser.parse_args()
    if bool(args.port) == bool(args.device_id):
        parser.error("specify exactly one of --port or --device-id")
    bundle = json.loads(args.bundle.read_text(encoding="utf-8"))
    observed = args.device_id
    if args.port:
        observed = provision_serial(args.port, bundle, args.inventory, args.reprovision)
    else:
        commission_identity(args.inventory, observed, bundle, args.reprovision, lambda _: None)
    print(f"Provisioned {observed}; non-secret inventory updated at {args.inventory}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, json.JSONDecodeError, ProvisioningError) as error:
        print(f"Provisioning failed: {error}", file=sys.stderr)
        raise SystemExit(1)
