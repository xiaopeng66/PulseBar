import os
import tempfile
from pathlib import Path


def configured_root(repository=None):
    repository = Path(repository or Path(__file__).resolve().parents[2]).resolve()
    configured = os.environ.get("PULSEBAR_TEST_ROOT")
    root = Path(configured) if configured else Path(tempfile.gettempdir()) / "pulsebar-regression"
    root = root.resolve()
    if root == repository or root in repository.parents or repository in root.parents:
        raise ValueError("PULSEBAR_TEST_ROOT must be outside and separate from the repository")
    return root


def run_root(name, repository=None):
    return configured_root(repository) / name
