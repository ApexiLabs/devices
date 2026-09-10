# Qualification Result Templates

These CSV files are blank schemas for APE-85 physical evidence. Copy them into `fixtures/qualification/results/<hardware-revision>/<assembly-serial>/<run-id>/` or attach them to a release/issue. Do not edit the templates with measured values.

- `calibration-points-template.csv`: one row per applied current point, including ascending and descending sweeps.
- `soak-observations-template.csv`: periodic snapshots and event rows during reboot, outage, and 24–48 hour soak runs.
- `manufacturing-checklist-template.csv`: one row per inspection or functional check.

Use ISO 8601 UTC timestamps. Preserve failed and repeated rows. Never store secrets, bearer values, Wi-Fi passwords, Cloudflare credentials, or MQTT passwords in these records.

Run `python3 scripts/validate_qualification_records.py` to validate the checked-in templates, or pass copied result files to validate their headers and controlled fields. Duplicate header names, truncated rows, and surplus columns fail validation. Header-only templates remain valid, and CSV quoting allows commas inside fields. This validates record shape, not truth, calibration accuracy, or hardware acceptance.
