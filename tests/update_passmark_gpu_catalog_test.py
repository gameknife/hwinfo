import importlib.util
import json
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "update_passmark_gpu_catalog.py"
SPEC = importlib.util.spec_from_file_location("update_passmark_gpu_catalog", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
catalog = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(catalog)


class GpuInfoAliasTest(unittest.TestCase):
    def setUp(self):
        self.rows = [
            ("GeForce RTX 3050 6GB", 140, 3),
            ("GeForce RTX 3050 8GB", 150, 2),
            ("GeForce RTX 3060", 200, 1),
            ("Radeon RX 580", 90, 4),
        ]

    def test_generic_driver_name_uses_conservative_variant(self):
        self.assertEqual(catalog.match_passmark_row("NVIDIA GeForce RTX 3050", "NVIDIA", self.rows), 0)

    def test_family_name_is_too_broad(self):
        self.assertIsNone(catalog.match_passmark_row("GeForce RTX", "NVIDIA", self.rows))

    def test_vendor_prevents_cross_vendor_match(self):
        self.assertIsNone(catalog.match_passmark_row("RX 580", "NVIDIA", self.rows))

    def test_gpuinfo_response_and_alias_merge(self):
        payload = json.dumps(
            {
                "recordsFiltered": 3,
                "data": [
                    {"gpuname": "NVIDIA NVIDIA GeForce RTX 3050", "vendor": "NVIDIA", "reportcount": 20},
                    {"gpuname": "NVIDIA GeForce RTX 3050 6GB", "vendor": "NVIDIA", "reportcount": 2},
                    {"gpuname": "NVIDIA GeForce RTX 3060", "vendor": "NVIDIA", "reportcount": 10},
                ],
            }
        ).encode()
        devices = catalog.parse_gpuinfo_devices(payload)
        canonical_names, aliases, matched = catalog.merge_gpuinfo_names(self.rows, devices)
        self.assertEqual(matched, 3)
        self.assertEqual(canonical_names, {0: "NVIDIA GeForce RTX 3050", 2: "NVIDIA GeForce RTX 3060"})
        self.assertEqual(
            aliases,
            {
                0: ["GeForce RTX 3050 6GB", "NVIDIA GeForce RTX 3050 6GB"],
                2: ["GeForce RTX 3060"],
            },
        )

    def test_truncated_response_is_rejected(self):
        payload = json.dumps(
            {"recordsFiltered": 2, "data": [{"gpuname": "GPU", "vendor": None, "reportcount": 1}]}
        ).encode()
        with self.assertRaisesRegex(ValueError, "truncated"):
            catalog.parse_gpuinfo_devices(payload)


if __name__ == "__main__":
    unittest.main()
