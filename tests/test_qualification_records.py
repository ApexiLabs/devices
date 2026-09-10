import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_qualification_records",
    ROOT / "scripts" / "validate_qualification_records.py",
)
validator = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(validator)


class QualificationRecordTests(unittest.TestCase):
    def test_ragged_rows_are_rejected_for_every_schema(self):
        for kind, (template, _) in validator.SCHEMAS.items():
            header = (validator.TEMPLATE_DIR / template).read_text()
            width = len(next(csv.reader([header.strip()])))
            for row in ("truncated\n", ",".join([""] * (width + 1)) + "\n"):
                with self.subTest(kind=kind, row=row):
                    with tempfile.TemporaryDirectory() as directory:
                        path = Path(directory) / f"{kind}-result.csv"
                        path.write_text(header + row)
                        self.assertIn("row width", validator.validate(path)[0])

    def test_quoted_commas_are_valid(self):
        template = validator.TEMPLATE_DIR / "manufacturing-checklist-template.csv"
        fields = next(csv.reader([template.read_text().strip()]))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manufacturing-result.csv"
            with path.open("w", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=fields)
                writer.writeheader()
                writer.writerow({"result": "not_run", "notes": "awaiting fixture, operator"})
            self.assertEqual([], validator.validate(path))

    def test_checked_in_templates_match_their_schemas(self):
        for template_name, _required in validator.SCHEMAS.values():
            self.assertEqual(
                [], validator.validate(validator.TEMPLATE_DIR / template_name)
            )

    def test_invalid_result_and_sweep_are_rejected(self):
        template = validator.TEMPLATE_DIR / "calibration-points-template.csv"
        with template.open(newline="", encoding="utf-8") as handle:
            fieldnames = next(csv.reader(handle))

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "calibration-invalid.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=fieldnames)
                writer.writeheader()
                writer.writerow({"result": "maybe", "sweep_direction": "sideways"})

            errors = validator.validate(path)

        self.assertEqual(2, len(errors))
        self.assertTrue(any("invalid result" in error for error in errors))
        self.assertTrue(any("invalid sweep_direction" in error for error in errors))

    def test_missing_required_columns_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "soak-incomplete.csv"
            path.write_text("run_id,result\n", encoding="utf-8")
            errors = validator.validate(path)

        self.assertEqual(1, len(errors))
        self.assertIn("missing columns", errors[0])


if __name__ == "__main__":
    unittest.main()
