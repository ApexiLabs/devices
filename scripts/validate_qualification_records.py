#!/usr/bin/env python3
"""Validate APE-85 qualification CSV shape without asserting physical results."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE_DIR = ROOT / "fixtures" / "qualification"

SCHEMAS = {
    "calibration": (
        "calibration-points-template.csv",
        {"run_id", "recorded_at_utc", "hardware_revision", "assembly_serial", "firmware_sha256", "channel_id", "sweep_direction", "reference_current_ma", "expected_engineering_value", "reported_engineering_value", "tolerance_engineering", "result"},
    ),
    "soak": (
        "soak-observations-template.csv",
        {"run_id", "recorded_at_utc", "hardware_revision", "assembly_serial", "firmware_sha256", "event_type", "reset_count", "timestamp_regression_count", "queue_dropped_records", "records_generated", "records_server_accepted", "result"},
    ),
    "manufacturing": (
        "manufacturing-checklist-template.csv",
        {"inspection_id", "recorded_at_utc", "hardware_revision", "assembly_serial", "firmware_sha256", "check_id", "check_description", "result"},
    ),
}

ALLOWED_RESULTS = {"pass", "fail", "not_run", "not_applicable"}
ALLOWED_SWEEPS = {"ascending", "descending"}


def schema_for(path: Path) -> tuple[str, set[str]]:
    for kind, (template_name, required) in SCHEMAS.items():
        if path.name == template_name or kind in path.name.lower():
            return kind, required
    raise ValueError(f"cannot infer record type from filename: {path}")


def validate(path: Path) -> list[str]:
    errors: list[str] = []
    kind, required = schema_for(path)
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        fields = set(reader.fieldnames or [])
        if len(fields) != len(reader.fieldnames or []):
            errors.append(f"{path}: duplicate column names")
        missing = sorted(required - fields)
        if missing:
            errors.append(f"{path}: missing columns: {', '.join(missing)}")
            return errors
        for line_number, row in enumerate(reader, start=2):
            if None in row or any(value is None for value in row.values()):
                errors.append(f"{path}:{line_number}: row width does not match header")
                continue
            result = (row.get("result") or "").strip().lower()
            if result and result not in ALLOWED_RESULTS:
                errors.append(f"{path}:{line_number}: invalid result {result!r}")
            if kind == "calibration":
                sweep = (row.get("sweep_direction") or "").strip().lower()
                if sweep and sweep not in ALLOWED_SWEEPS:
                    errors.append(f"{path}:{line_number}: invalid sweep_direction {sweep!r}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="*", type=Path)
    args = parser.parse_args()
    files = args.files or [TEMPLATE_DIR / value[0] for value in SCHEMAS.values()]
    errors: list[str] = []
    for path in files:
        try:
            errors.extend(validate(path))
        except (OSError, ValueError) as exc:
            errors.append(str(exc))
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"qualification-records: ok ({len(files)} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
