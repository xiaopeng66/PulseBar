import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "tools" / "tests" / "test_display_text_repair.py"


class TestModuleImportSafety(unittest.TestCase):
    def test_display_text_repair_import_has_no_execution_side_effects(self):
        code = (
            "import importlib.util, sys; "
            "spec = importlib.util.spec_from_file_location('repair_test', sys.argv[1]); "
            "module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)"
        )
        result = subprocess.run(
            [sys.executable, "-c", code, str(MODULE)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
