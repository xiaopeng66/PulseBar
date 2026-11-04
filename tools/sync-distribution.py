#!/usr/bin/env python3
"""Check/sync PulseBar EXE and authoritative external languages (Windows only).

No arguments: read-only check; exit 0 = current, 1 = drift, 2 = invalid/failed.
--write: verify every source/target, preflight sharing locks, make immutable
SHA-256 preimage backups, stage bytes on the same volume, then atomically replace
individual files. This is NOT an atomic multi-file release transaction: a disk
or sharing failure stops immediately and reports any already completed paths.
Close PulseBar normally before a write; this tool never kills processes.
DELETE-access preflight checks sharing permissions, NOT whether an executable
image is unloaded. A running image can pass preflight and still fail at replace;
that failure reports STOP, retains backups, and never forces a rename or retries.

The only authoritative language directory is src/PulseBar/language.
Full mode updates languages in existing known Bin outputs, portable, installer,
and the Debug working directory; only selected distributions receive the Release
EXE. --distribution portable / --distribution installer selects distribution roots
(repeatable, default: both); unselected roots are neither read nor written, and
cannot be reintroduced by --quarantine or --debug-cwd. Known build-output and
Debug-CWD language synchronization stays enabled. --build-output instead updates
languages only, never EXEs or distributions, and is mutually exclusive with
--distribution (invalid combinations fail before any filesystem inspection).
Debug loads CWD/language (default CWD: src, used by the build wrappers); Release
loads module-directory/language. Visual Studio's src/TrafficMonitor working
directory already uses the authoritative files. Use --debug-cwd for a different
project-local Debug working directory, or launch from the synced module folder.

Extra INIs and old EXEs block a write unless each exact project-relative path
is supplied with --quarantine. Only obsolete language files or PulseBar / legacy
TrafficMonitor EXE names qualify; no globs, directory deletion, config/history,
plugins, DLLs, PDBs, licenses, registry, startup settings, or icon cache changes.
"""
from __future__ import annotations

import argparse
from contextlib import ExitStack
import configparser
import ctypes
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import stat
import sys
import uuid


WORKSPACE = Path("E:/AI/Zcode")
TEMP_ROOT = WORKSPACE / "tmp/traffic-monitor-branding"
DEFAULT_BACKUP = WORKSPACE / "backups/traffic-monitor/20261103-branding"
LANGUAGES = (
    "English.ini", "French.ini", "German.ini", "Hebrew.ini", "Hungarian.ini",
    "Italian.ini", "Japanese.ini", "Korean.ini", "Polish.ini",
    "Portuguese_Brazilian.ini", "Russian.ini", "Simplified_Chinese.ini",
    "Slovenian.ini", "Spanish.ini", "Spanish_Spain.ini",
    "Traditional_Chinese.ini", "Turkish.ini", "Vietnamese.ini",
)
ICON_KEYS = (
    "IDS_PULSEBAR_ICON_BLUE", "IDS_PULSEBAR_ICON_LIGHT", "IDS_PULSEBAR_ICON_DARK",
)
BUILD_OUTPUTS = tuple(
    PurePosixPath("src/Bin") / platform / configuration
    for platform in ("", "x64", "ARM64EC")
    for configuration in ("Release", "Debug", "Release (lite)", "Debug (lite)")
)
REPARSE_POINT = 0x400


class SyncError(Exception):
    pass


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def guarded_path(value: str | Path) -> Path:
    """Validate lexically BEFORE resolving; resolving would hide a junction."""
    raw = str(value).replace("\\", "/")
    windows = PureWindowsPath(raw)
    if not windows.is_absolute() or raw.startswith("/") or raw.startswith("//"):
        raise SyncError(f"expected an absolute Windows workspace path: {value}")
    parts = raw.split("/")[1:]
    if any(part in (".", "..") or not part or part.endswith((".", " "))
           or re.search(r'[<>:"|?*]', part) for part in parts):
        raise SyncError(f"noncanonical path (traversal/alias/glob forbidden): {value}")
    path = Path(raw)
    if not path.is_relative_to(WORKSPACE):
        raise SyncError(f"path is outside E:/AI/Zcode workspace: {value}")
    # Include workspace ancestors: E:/AI itself could have been redirected.
    for current in (*reversed(path.parents), path):
        try:
            info = current.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & REPARSE_POINT:
            raise SyncError(f"symlink/junction/reparse point forbidden: {current}")
        if current != path and not stat.S_ISDIR(info.st_mode):
            raise SyncError(f"non-directory ancestor: {current}")
    return path


def directory(path: Path, required: bool = False) -> bool:
    guarded_path(path)
    if not path.exists():
        if required:
            raise SyncError(f"missing directory: {path}")
        return False
    if not path.is_dir():
        raise SyncError(f"expected directory: {path}")
    return True


def make_directory(path: Path) -> None:
    guarded_path(path)
    if directory(path):
        return
    make_directory(path.parent)
    guarded_path(path)
    path.mkdir()
    directory(path, required=True)


@dataclass(frozen=True)
class Image:
    path: Path
    data: bytes
    sha256: str
    identity: tuple[int, int, int, int]


def shared_read_bytes(path: Path) -> bytes:
    # Python's default CRT reader omits FILE_SHARE_DELETE. A preflight DELETE
    # handle would therefore conflict with our own re-read before replacement.
    # Keep the protection handle open and explicitly allow it in this reader.
    from ctypes import wintypes
    import msvcrt
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = (
        wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE,
    )
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
    kernel.CloseHandle.restype = wintypes.BOOL
    handle = kernel.CreateFileW(str(path), 0x80000000, 1 | 2 | 4, None, 3, 0x200080, None)
    if handle == ctypes.c_void_p(-1).value:
        raise SyncError(f"cannot read file: {path} (WinError {ctypes.get_last_error()})")
    try:
        fd = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
    except OSError:
        kernel.CloseHandle(handle)
        raise
    with os.fdopen(fd, "rb") as stream:
        return stream.read()


def image(path: Path, required: bool = True) -> Image | None:
    guarded_path(path)
    try:
        before = path.stat()
    except FileNotFoundError:
        if required:
            raise SyncError(f"missing file: {path}")
        return None
    if not stat.S_ISREG(before.st_mode) or before.st_nlink != 1:
        raise SyncError(f"expected regular single-link file: {path}")
    data = shared_read_bytes(path)
    guarded_path(path)
    after = path.stat()
    identity = lambda info: (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns)
    if identity(before) != identity(after) or len(data) != after.st_size:
        raise SyncError(f"file changed while being read: {path}")
    return Image(path, data, digest(data), identity(after))


def unchanged(path: Path, expected: Image | None) -> None:
    current = image(path, required=False)
    if (current is None) != (expected is None) or (
        current is not None and expected is not None
        and (current.sha256 != expected.sha256 or current.identity != expected.identity)
    ):
        raise SyncError(f"file changed since inspection; retry check: {path}")


def authoritative_languages(project: Path) -> dict[str, Image]:
    source = project / "src/PulseBar/language"
    directory(source, required=True)
    found = {entry.name for entry in source.iterdir() if entry.suffix.lower() == ".ini"}
    if found != set(LANGUAGES):
        raise SyncError(f"authoritative manifest must contain exactly 18 languages; "
                        f"missing={sorted(set(LANGUAGES) - found)}, extra={sorted(found - set(LANGUAGES))}")
    result = {}
    for name in LANGUAGES:
        item = image(source / name)
        assert item is not None
        if not item.data.startswith(b"\xef\xbb\xbf") or b"\n" in item.data.replace(b"\r\n", b""):
            raise SyncError(f"authoritative language must retain UTF-8 BOM and CRLF: {item.path}")
        ini = configparser.ConfigParser(interpolation=None, strict=False)
        ini.read_string(item.data.decode("utf-8-sig"))
        if ini.get("text", "IDS_TRAFFICMONITOR", fallback="").strip('"') != "PulseBar":
            raise SyncError(f"authoritative product name is not PulseBar: {item.path}")
        if "PulseBar" not in ini.get("text", "IDS_TRAFFICMONITOR_PLUGIN_NITIFICATION", fallback=""):
            raise SyncError(f"missing PulseBar plugin notification name: {item.path}")
        labels = [ini.get("text", key, fallback="").strip('"') for key in ICON_KEYS]
        if len(set(labels)) != 3 or not all(label.startswith("PulseBar ") for label in labels):
            raise SyncError(f"missing/distinct PulseBar blue/light/dark icon labels: {item.path}")
        result[name] = item
    return result


@dataclass(frozen=True)
class Change:
    path: Path
    before: Image | None
    source: Image | None  # None means explicitly allowlisted quarantine.


@dataclass
class Plan:
    project: Path
    backup_root: Path
    stage_parent: Path
    changes: list[Change]
    sources: list[Image]
    unlisted: list[str]

    def relative(self, path: Path) -> str:
        return path.relative_to(self.project).as_posix()

    def backup_path(self, change: Change) -> Path:
        assert change.before is not None
        project_id = digest(self.project.as_posix().lower().encode("utf-8"))[:12]
        relative = change.path.relative_to(self.project)
        return (self.backup_root / f"{self.project.name}-{project_id}" / relative.parent
                / f"{relative.name}.{change.before.sha256}.bak")


def obsolete_executable(name: str) -> bool:
    return bool(re.fullmatch(r"(?:PulseBar(?:\.[A-Za-z0-9_.-]+)?\.exe\.old|"
                             r"TrafficMonitor(?:\.exe|(?:\.[A-Za-z0-9_.-]+)?\.exe\.old))",
                             name, re.IGNORECASE))


def build_plan(options: argparse.Namespace) -> Plan:
    if os.name != "nt":
        raise SyncError("this workspace synchronizer requires Windows")
    project = guarded_path(options.project_root)
    if project == WORKSPACE or project == TEMP_ROOT or project.parent == TEMP_ROOT:
        raise SyncError("project root must be a dedicated project directory (fixtures: tmp/context/run/project)")
    directory(project, required=True)
    fixture = project.is_relative_to(TEMP_ROOT)
    backup_default = project.parent / "backups" if fixture else DEFAULT_BACKUP
    backup_root = guarded_path(options.backup_root or backup_default)
    if backup_root.is_relative_to(project) or project.is_relative_to(backup_root):
        raise SyncError("backup root must not overlap the project")
    if not (backup_root.is_relative_to(WORKSPACE / "backups") or backup_root.is_relative_to(TEMP_ROOT)):
        raise SyncError("backup root must be under workspace/backups or tmp/traffic-monitor-branding")
    if fixture and not backup_root.is_relative_to(project.parent):
        raise SyncError("a controlled temporary project must keep backups in its own temporary area")
    directory(backup_root)
    print(f"BACKUP-ROOT {backup_root.as_posix()}")
    # A --project-root fixture also stages entirely inside its controlled area.
    stage_parent = project.parent if fixture else TEMP_ROOT
    guarded_path(stage_parent)
    languages = authoritative_languages(project)
    known_outputs = [project / Path(relative) for relative in BUILD_OUTPUTS]
    if options.build_output:
        outputs = [guarded_path(value) for value in options.build_output]
        for output in outputs:
            if output not in known_outputs:
                raise SyncError(f"--build-output must select a known src/Bin build directory: {output}")
            directory(output)
    else:
        outputs = [output for output in known_outputs if directory(output)]
    outputs = list(dict.fromkeys(outputs))
    distribution_names = ("portable", "installer")
    selected_names = set(options.distribution or distribution_names)
    distribution_roots = [] if options.build_output else [
        project / name for name in distribution_names if name in selected_names
    ]
    excluded_roots = [project / name for name in distribution_names if name not in selected_names]
    roots = outputs + distribution_roots
    for root in roots:
        directory(root)
    language_dirs = [root / "language" for root in roots]
    debug = any(output.name.startswith("Debug") for output in outputs)
    if options.debug_cwd and not debug:
        raise SyncError("--debug-cwd needs a selected/existing Debug build output")
    if debug:
        debug_cwd = Path(options.debug_cwd or project / "src")
        if any(debug_cwd.is_relative_to(root) for root in excluded_roots):
            raise SyncError("Debug CWD is outside the selected distribution scope")
        debug_cwd = guarded_path(options.debug_cwd or project / "src")
        if not debug_cwd.is_relative_to(project) or any(
            part.lower() in ("plugins", ".git") for part in debug_cwd.relative_to(project).parts
        ):
            raise SyncError("Debug CWD must be project-local, outside plugins and .git")
        directory(debug_cwd)
        language_dirs.append(debug_cwd / "language")
        print(f"DEBUG-CWD {debug_cwd.as_posix()} (Debug uses CWD/language, not module directory)")
    source_dir = project / "src/PulseBar/language"
    language_dirs = [path for path in dict.fromkeys(language_dirs) if path != source_dir]
    for path in language_dirs:
        directory(path)
    plan = Plan(project, backup_root, stage_parent, [], list(languages.values()), [])

    allowlisted: set[Path] = set()
    for raw in options.quarantine:
        # Project-relative forward-slash names only, no drive/ADS/normalization.
        if "\\" in raw or re.search(r'[:*?<>|]', raw) or raw.startswith("/") or any(
            part in ("", ".", "..") for part in raw.split("/")
        ):
            raise SyncError(f"--quarantine requires an exact project-relative path: {raw}")
        selected = project / raw
        protected_name = selected.name.lower() in ("config.ini", "global_cfg.ini") or \
            selected.name.lower().startswith(("history_traffic", "plugin"))
        allowed_language = selected.parent in language_dirs and selected.suffix.lower() == ".ini" \
            and selected.name.lower() not in {name.lower() for name in LANGUAGES} and not protected_name
        allowed_exe = selected.parent in roots and obsolete_executable(selected.name)
        if not (allowed_language or allowed_exe):
            raise SyncError(f"not an obsolete language or EXE allowlist candidate: {raw}")
        # Reject excluded roots lexically before any target-path inspection.
        allowlisted.add(guarded_path(selected))

    def compare(target: Path, source: Image) -> None:
        previous = image(target, required=False)  # Read every target BEFORE deciding to replace.
        status = "MISSING" if previous is None else "OK" if previous.sha256 == source.sha256 else "STALE"
        print(f"{status} {plan.relative(target)} source_sha256={source.sha256}"
              + (f" target_sha256={previous.sha256}" if previous else ""))
        if status != "OK":
            plan.changes.append(Change(target, previous, source))

    # EXEs first. A sharing failure never upgrades that application's languages first.
    if not options.build_output:
        release = image(project / "src/Bin/x64/Release/PulseBar.exe")
        assert release is not None
        if not release.data.startswith(b"MZ"):
            raise SyncError(f"Release EXE does not have an MZ header: {release.path}")
        plan.sources.append(release)
        for root in distribution_roots:
            compare(root / "PulseBar.exe", release)
    for path in language_dirs:
        for name in LANGUAGES:
            compare(path / name, languages[name])

    extras: list[Path] = []
    for path in language_dirs:
        if directory(path):
            extras.extend(entry for entry in sorted(path.iterdir())
                          if entry.suffix.lower() == ".ini" and entry.name not in LANGUAGES)
    for root in roots:
        if directory(root):
            for entry in sorted(root.iterdir()):
                if obsolete_executable(entry.name) or entry.name.lower().endswith(".exe.old"):
                    extras.append(entry)
                elif entry.suffix.lower() in (".exe", ".ico", ".bmp"):
                    item = image(entry)
                    assert item is not None
                    print(f"INVENTORY {plan.relative(entry)} bytes={len(item.data)} sha256={item.sha256}")
    for entry in dict.fromkeys(extras):
        previous = image(entry)
        assert previous is not None
        selected = entry in allowlisted
        print(f"EXTRA-{'ALLOWLISTED' if selected else 'UNLISTED'} {plan.relative(entry)} "
              f"sha256={previous.sha256}")
        if selected:
            plan.changes.append(Change(entry, previous, None))
        else:
            plan.unlisted.append(plan.relative(entry))
    return plan


def require_backup(change: Change, plan: Plan) -> Image | None:
    if change.before is None:
        return None
    backup = image(plan.backup_path(change), required=False)
    if backup is not None and backup.sha256 != change.before.sha256:
        raise SyncError(f"backup hash mismatch; will not overwrite: {backup.path}")
    return backup


def hold_replace_access(path: Path, stack: ExitStack) -> None:
    """Check read/delete sharing permissions; this cannot prove an image is unloaded."""
    from ctypes import wintypes
    guarded_path(path)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = (
        wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE,
    )
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
    kernel.CloseHandle.restype = wintypes.BOOL
    # Deny concurrent writers and request DELETE to detect incompatible sharing.
    # A mapped/running image may allow this handle yet reject the later replace.
    # Such a failure must STOP with backups retained, never force unload or retry.
    handle = kernel.CreateFileW(str(path), 0x80000000 | 0x10000, 1 | 4, None, 3, 0x80, None)
    if handle == ctypes.c_void_p(-1).value:
        error = ctypes.get_last_error()
        raise SyncError(f"cannot replace occupied/protected file: {path.as_posix()} (WinError {error}); "
                        "close the owner normally and retry; no process was stopped")
    stack.callback(kernel.CloseHandle, handle)


class Staging:
    """Only remove files created by this instance, never glob-delete leftovers."""
    def __init__(self, parent: Path):
        self.path = parent / ("sync-stage-" + uuid.uuid4().hex)
        self.owned: set[Path] = set()

    def __enter__(self):
        make_directory(self.path.parent)
        guarded_path(self.path)
        self.path.mkdir()  # Exclusive private directory, not a shared staging cache.
        return self

    def write(self, data: bytes) -> Path:
        path = self.path / (uuid.uuid4().hex + ".stage")
        guarded_path(path)
        with path.open("xb") as stream:
            self.owned.add(path)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        staged = image(path)
        if staged is None or staged.sha256 != digest(data):
            raise SyncError(f"staged bytes failed hash verification: {path}")
        return path

    def __exit__(self, exc_type, exc, traceback):
        for path in self.owned:
            guarded_path(path)
            if path.exists():
                path.unlink()
        guarded_path(self.path)
        self.path.rmdir()  # Non-empty/unexpected contents cause failure, never recursion.


def apply(plan: Plan) -> None:
    if plan.unlisted:
        raise SyncError("unlisted extras block write; review and pass exact --quarantine paths: "
                        + ", ".join(plan.unlisted))
    if not plan.changes:
        print("CURRENT no writes, staging, or backups needed")
        return
    completed: list[str] = []
    try:
        with ExitStack() as locks:
            target_locks: dict[Path, ExitStack] = {}
            # Complete read-only preflight BEFORE creating any directory/backup/stage.
            for source in plan.sources:
                unchanged(source.path, source)
            for change in plan.changes:
                unchanged(change.path, change.before)
                require_backup(change, plan)
                if change.before is not None:
                    target_lock = locks.enter_context(ExitStack())
                    target_locks[change.path] = target_lock
                    hold_replace_access(change.path, target_lock)
            with Staging(plan.stage_parent) as staging:
                staged: dict[Path, Path] = {}
                for change in plan.changes:
                    if change.before is not None:
                        backup = require_backup(change, plan)
                        if backup is None:
                            target = plan.backup_path(change)
                            temporary = staging.write(change.before.data)
                            make_directory(target.parent)
                            guarded_path(target)
                            # On Windows rename refuses an existing destination. Never
                            # replace an existing backup, even on a repeated write run.
                            try:
                                os.rename(temporary, target)
                            except FileExistsError:
                                pass
                            backup = require_backup(change, plan)
                            if backup is None:
                                raise SyncError(f"backup was not created: {target}")
                            print(f"BACKUP {target.as_posix()} sha256={backup.sha256}")
                    if change.source is not None:
                        staged[change.path] = staging.write(change.source.data)
                # All preimages and replacement bytes are verified before first mutation.
                for source in plan.sources:
                    unchanged(source.path, source)
                for change in plan.changes:
                    unchanged(change.path, change.before)
                    if change.before is not None and require_backup(change, plan) is None:
                        raise SyncError(f"verified backup disappeared: {change.path}")
                    # Some Windows filesystem/filter combinations refuse MoveFileEx
                    # while even a delete-sharing handle remains open. Release ONLY
                    # this target immediately before replacement and recheck it.
                    # Other targets remain protected; a new owner is reported by
                    # the atomic replace, never force-closed or retried blindly.
                    if change.path in target_locks:
                        target_locks[change.path].close()
                    unchanged(change.path, change.before)
                    if change.source is None:
                        guarded_path(change.path)
                        change.path.unlink()  # Exactly one allowlisted, backed-up file.
                        action = "QUARANTINED"
                    else:
                        make_directory(change.path.parent)
                        guarded_path(change.path)
                        temporary = staged[change.path]
                        guarded_path(temporary)
                        os.replace(temporary, change.path)
                        current = image(change.path)
                        if current is None or current.sha256 != change.source.sha256:
                            raise SyncError(f"replacement hash mismatch: {change.path}")
                        action = "COPIED"
                    completed.append(plan.relative(change.path))
                    print(f"{action} {completed[-1]}")
    except (OSError, SyncError):
        print(f"STOP completed={len(completed)}; verified backups retained; no automatic rollback/kill")
        for path in completed:
            print(f"COMPLETED {path}")
        raise


def arguments(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="read-only (default); drift exits 1")
    mode.add_argument("--write", action="store_true", help="backup, stage, and synchronize allowlisted files")
    parser.add_argument("--project-root", default=str(Path(__file__).absolute().parents[2]),
                        help="absolute Windows project path; default is derived from this script")
    parser.add_argument("--backup-root",
                        help="immutable hash backups outside project; branding backup by default, "
                             "or run/backups for a controlled tmp project")
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument("--build-output", action="append", metavar="DIRECTORY",
                       help="language-only mode for a known absolute src/Bin output; repeatable")
    scope.add_argument("--distribution", action="append", choices=("portable", "installer"),
                       help="distribution roots to sync/scan; repeatable, default both; "
                            "known Bin outputs and Debug CWD are still synchronized")
    parser.add_argument("--debug-cwd", metavar="DIRECTORY",
                        help="Debug CWD to receive language (default: project/src)")
    parser.add_argument("--quarantine", action="append", default=[], metavar="RELATIVE_FILE",
                        help="exact obsolete file to back up and remove; repeat, never use globs")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    try:
        options = arguments(argv)
        print("MODE " + ("WRITE" if options.write else "CHECK (read-only)"))
        plan = build_plan(options)
        print(f"SUMMARY changes={len(plan.changes)} unlisted_extras={len(plan.unlisted)} source_languages=18")
        if options.write:
            apply(plan)
            return 0
        return 1 if plan.changes or plan.unlisted else 0
    except (SyncError, OSError, ValueError, configparser.Error) as error:
        print(f"FAIL {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    # -I deliberately ignores PYTHON* environment variables. Also support direct
    # invocation from legacy Windows consoles without depending on locale cp936.
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    sys.exit(main())
