from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ExeLicenseMetadataTests(unittest.TestCase):
    def test_version_resource_declares_project_license(self):
        text = (ROOT / "PulseBar" / "PulseBar.rc").read_text(encoding="utf-16")
        self.assertIn('VALUE "LegalCopyright", "Based on TrafficMonitor, licensed under Anti-996 License"', text)
        self.assertNotIn("GNU GPL v3", text)


if __name__ == "__main__":
    unittest.main()
