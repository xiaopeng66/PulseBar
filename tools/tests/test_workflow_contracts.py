import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class TestWorkflowContracts(unittest.TestCase):
    def test_actions_are_pinned_to_full_commit_shas(self):
        workflow = (ROOT / ".github/workflows/main.yml").read_text(encoding="utf-8")
        actions = re.findall(r"(?m)^\s*-?\s*uses:\s*([^\s]+)", workflow)
        self.assertTrue(actions)
        for action in actions:
            with self.subTest(action=action):
                self.assertRegex(action, r"^[^@]+@[0-9a-f]{40}$")

    def test_pull_requests_run_the_native_regression_suite(self):
        workflow = (ROOT / ".github/workflows/main.yml").read_text(encoding="utf-8")
        self.assertRegex(workflow, r"(?m)^  pull_request:")
        self.assertRegex(workflow, r"(?m)^  push:\n    branches: \[main\]")
        self.assertIn("python -X utf8 -B -m unittest discover -s tools/tests", workflow)
        for build in ("x64_build", "x86_build", "arm64ec_build"):
            self.assertRegex(workflow, rf"(?m)^  {build}:\n    needs: regression")


if __name__ == "__main__":
    unittest.main()
