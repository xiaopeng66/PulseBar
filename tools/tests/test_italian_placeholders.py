import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read_entries(language):
    entries = {}
    for line in (ROOT / "PulseBar/language" / f"{language}.ini").read_text(encoding="utf-8-sig").splitlines():
        match = re.match(r'\s*([A-Z0-9_]+)\s*=\s*"(.*)"\s*$', line)
        if match:
            entries[match.group(1)] = match.group(2)
    return entries


class TestItalianPlaceholders(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.english = read_entries("English")
        cls.italian = read_entries("Italian")

    def test_update_dialog_uses_unicode_string_format_arguments(self):
        for key in ("IDS_UPDATE_AVLIABLE", "IDS_UPDATE_AVLIABLE" + chr(50)):
            with self.subTest(key=key):
                self.assertEqual(self.italian[key].count("%s"), self.english[key].count("%s"))
                self.assertNotIn("%S", self.italian[key])

    def test_connection_diagnostics_keep_runtime_replacement_tokens(self):
        expected = {
            "IDS_INSUFFICIENT_BUFFER": {"cnt"},
            "IDS_CONNECTION_NUM_CHANGED": {"before", "after", "cnt"},
            "IDS_CONNECTION_NOT_MATCH": {"cnt"},
        }
        for key, tokens in expected.items():
            with self.subTest(key=key):
                found = set(re.findall(r"<%([^%]+)%>", self.italian[key]))
                for token in tokens:
                    self.assertIn(token, found)


if __name__ == "__main__":
    unittest.main()
