#!/usr/bin/env python3
"""Stage the 18 repository languages beside a known Bin output.

This public build helper derives its checkout from this file, never from CWD or
an author's workspace. It only copies INIs into a known Bin/.../language directory;
Debug also stages checkout/language, matching the build wrappers' working directory.
It never reads EXEs/DLLs, synchronizes distributions, deletes extra files, or changes
user settings. All inputs/targets are checked before writes; replacement is atomic
per file, not a multi-file transaction. Failures return 2 and remain visible to CI.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import stat
import sys
import tempfile

ROOT = Path(__file__).absolute().parents[1]
LANGUAGES = (
    "English.ini", "French.ini", "German.ini", "Hebrew.ini", "Hungarian.ini",
    "Italian.ini", "Japanese.ini", "Korean.ini", "Polish.ini",
    "Portuguese_Brazilian.ini", "Russian.ini", "Simplified_Chinese.ini",
    "Slovenian.ini", "Spanish.ini", "Spanish_Spain.ini",
    "Traditional_Chinese.ini", "Turkish.ini", "Vietnamese.ini",
)
OUTPUTS = tuple(ROOT / "Bin" / platform / configuration
                for platform in ("", "x64", "ARM64EC")
                for configuration in ("Release", "Debug", "Release (lite)", "Debug (lite)"))


def safe_path(path: Path) -> None:
    # Do not resolve first: that would hide a symlink/junction in the input.
    if not path.is_relative_to(ROOT):
        raise ValueError(f"path must remain in this checkout: {path}")
    for current in (*reversed(path.parents), path):
        try:
            info = current.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError(f"symlink/junction/reparse point forbidden: {current}")
        if current != path and not stat.S_ISDIR(info.st_mode):
            raise ValueError(f"non-directory ancestor: {current}")


def build_output(value: str) -> Path:
    raw = value.replace("\\", "/")
    path = Path(raw)
    parts = raw.split("/")
    if path.is_absolute():
        parts = parts[1:]  # Drive on Windows, root slash on POSIX.
    if any(part in ("", ".", "..") or part.endswith((".", " "))
           or any(char in part for char in '<>:"|?*') for part in parts):
        raise ValueError(f"noncanonical build output: {value}")
    if not path.is_absolute():
        path = ROOT / path
    canonical = next((candidate for candidate in OUTPUTS if candidate == path), None)
    if canonical is None:
        raise ValueError(f"build output must be a known Bin directory in this checkout: {value}")
    safe_path(canonical)
    return canonical


def read_file(path: Path, required: bool = True) -> bytes | None:
    safe_path(path)
    try:
        info = path.stat()
    except FileNotFoundError:
        if required:
            raise
        return None
    if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
        raise ValueError(f"expected regular single-link file: {path}")
    return path.read_bytes()


def stage(output: Path) -> int:
    source = ROOT / "PulseBar/language"
    safe_path(source)
    found = {entry.name for entry in source.iterdir() if entry.suffix.lower() == ".ini"}
    if found != set(LANGUAGES):
        raise ValueError(f"language manifest must contain exactly 18 languages; "
                         f"missing={sorted(set(LANGUAGES) - found)}, extra={sorted(found - set(LANGUAGES))}")
    languages = {}
    for name in LANGUAGES:
        data = read_file(source / name)
        assert data is not None
        data.decode("utf-8-sig", errors="strict")
        if not data.startswith(b"\xef\xbb\xbf") or b"\n" in data.replace(b"\r\n", b""):
            raise ValueError(f"language must retain UTF-8 BOM and CRLF: {name}")
        languages[name] = data
    destinations = [output / "language"]
    if output.name.startswith("Debug"):
        destinations.append(ROOT / "language")
    changes = []
    for directory in destinations:
        safe_path(directory)
        if directory.exists():
            extras = [entry.name for entry in directory.iterdir()
                      if entry.suffix.lower() == ".ini" and entry.name not in LANGUAGES]
            if extras:
                raise ValueError(f"unexpected destination INIs; no files deleted: {directory}: {extras}")
        for name, data in languages.items():
            target = directory / name
            if read_file(target, required=False) != data:
                changes.append((target, data))
    # Preflight completed for every source and destination. Only rebuildable language
    # outputs can be touched; failures never trigger deletion/rollback of user files.
    for target, data in changes:
        safe_path(target)
        target.parent.mkdir(parents=True, exist_ok=True)
        safe_path(target)
        descriptor, temporary_name = tempfile.mkstemp(prefix=".pulsebar-language-", suffix=".tmp", dir=target.parent)
        temporary = Path(temporary_name)
        try:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            safe_path(target)
            os.replace(temporary, target)
        finally:
            if temporary.exists():
                temporary.unlink()
    print(f"LANGUAGES source=18 directories={len(destinations)} copied={len(changes)}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-output", required=True,
                        help="known Bin directory, absolute or relative to this checkout")
    options = parser.parse_args(argv)
    try:
        return stage(build_output(options.build_output))
    except (OSError, ValueError, UnicodeError) as error:
        print(f"FAIL {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    raise SystemExit(main())
