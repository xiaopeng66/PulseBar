import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from test_paths import configured_root


class TestNativeTestRoot(unittest.TestCase):
    def test_defaults_to_system_temp_outside_the_repository(self):
        with patch.dict(os.environ, {}, clear=True):
            root = configured_root()
        repository = Path(__file__).resolve().parents[2]
        self.assertEqual(root, (Path(tempfile.gettempdir()) / "pulsebar-regression").resolve())
        self.assertFalse(root == repository or repository in root.parents)
        self.assertFalse(root in repository.parents)

    def test_honors_an_explicit_external_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            chosen = Path(temporary) / "native-tests"
            with patch.dict(os.environ, {"PULSEBAR_TEST_ROOT": str(chosen)}):
                self.assertEqual(configured_root(), chosen.resolve())

    def test_rejects_roots_inside_or_above_the_repository(self):
        repository = Path(__file__).resolve().parents[2]
        for unsafe in (repository / "tmp", repository.parent):
            with self.subTest(path=unsafe), patch.dict(
                os.environ, {"PULSEBAR_TEST_ROOT": str(unsafe)}
            ):
                with self.assertRaises(ValueError):
                    configured_root()


if __name__ == "__main__":
    unittest.main()
