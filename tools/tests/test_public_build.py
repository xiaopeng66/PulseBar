"""Public build post-processing in fresh clones; compiler/sync boundaries are inert.

Only the vcvars/MSBuild invocations are replaced in mirror copies of the batch
wrappers. The real post-build tail and language helper run; no executable is built,
read or launched, and the private workspace synchronizer is a fail-fast sentinel.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
RUN_ROOT = Path("E:/AI/Zcode/tmp/pulsebar-fix-20261105")


def snapshot(root):
    return {path.relative_to(root).as_posix():
            (path.read_bytes(), path.stat().st_mtime_ns) if path.is_file() else None
            for path in sorted(root.rglob("*"))}


class PublicBuildLanguages(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        RUN_ROOT.mkdir(parents=True, exist_ok=True)
        cls.run_root = Path(tempfile.mkdtemp(prefix="public-build-", dir=RUN_ROOT))
        print(f"ARTIFACTS {cls.run_root.as_posix()}", flush=True)

    def setUp(self):
        self.area = self.run_root / self._testMethodName
        self.area.mkdir()
        self.clone = self.area / "checkout with spaces"
        self.source = self.clone / "PulseBar/language"
        self.source.mkdir(parents=True)
        self.expected = {p.name: p.read_bytes() for p in (ROOT / "PulseBar/language").glob("*.ini")}
        self.assertEqual(len(self.expected), 18)
        for name, data in self.expected.items():
            (self.source / name).write_bytes(data)
        tools = self.clone / "tools"
        tools.mkdir()
        helper = ROOT / "tools/stage-languages.py"
        if helper.is_file():
            shutil.copyfile(helper, tools / helper.name)
        (tools / "sync-distribution.py").write_text(
            'print("FORBIDDEN_WORKSPACE_SYNC")\nraise SystemExit(73)\n', encoding="utf-8")
        self.caller = self.area / "caller"
        self.caller.mkdir()
        self.env = os.environ.copy()
        self.env.update(TEMP=str(self.caller), TMP=str(self.caller), PYTHONUTF8="1", PYTHONIOENCODING="utf-8")
        self.sentinels = []
        for relative in ("portable/keep.txt", "installer/keep.txt", "config.ini", "history_traffic.dat"):
            path = self.area / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"private sentinel; must not be read or changed by staging")
            self.sentinels.append(path)
        self.sentinel_before = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.sentinels}
        self.calls = 0

    def tearDown(self):
        self.assertEqual({p: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.sentinels},
                         self.sentinel_before)

    def invoke(self, command):
        self.calls += 1
        result = subprocess.run(command, cwd=self.caller, env=self.env, capture_output=True,
                                text=True, encoding="utf-8", timeout=30)
        (self.area / f"call-{self.calls}.log").write_text(
            result.stdout + result.stderr + f"\nEXIT {result.returncode}\n", encoding="utf-8")
        return result

    def wrapper(self, name, build_exit=0):
        text = (ROOT / name).read_text(encoding="utf-8")
        stubs = self.area / "compiler stubs"
        stubs.mkdir(exist_ok=True)
        for stub, code in (("vcvars.cmd", 0), ("msbuild.cmd", build_exit)):
            (stubs / stub).write_text(f"@echo off\nexit /b {code}\n", encoding="utf-8")
        text, vcvars = re.subn(r'^call "[^"\n]*vcvars64\.bat"[^\n]*$',
                              lambda _: f'call "{stubs / "vcvars.cmd"}"', text, flags=re.M)
        text, compiler = re.subn(r'^msbuild ([^\n]*)$',
                                lambda m: f'call "{stubs / "msbuild.cmd"}" {m[1]}', text, flags=re.M)
        self.assertEqual((vcvars, compiler), (1, 1), "review changed compiler seam before running")
        text = re.sub(r'^python ', lambda _: f'"{sys.executable}" ', text, flags=re.M)
        mirror = self.clone / name
        mirror.write_text(text, encoding="utf-8")
        return self.invoke(["cmd.exe", "/d", "/c", str(mirror)])

    def stage(self, output="Bin/x64/Release"):
        helper = self.clone / "tools/stage-languages.py"
        self.assertTrue(helper.is_file(), "public builds need a repository-local language staging helper")
        return self.invoke([sys.executable, "-X", "utf8", "-I", "-B", str(helper),
                            "--build-output", str(output)])

    def assert_languages(self, root):
        self.assertEqual({p.name: p.read_bytes() for p in (root / "language").glob("*.ini")},
                         self.expected)

    def test_release_wrapper_works_without_workspace_sync_or_container_src_layout(self):
        result = self.wrapper("build-sln.bat")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("FORBIDDEN_WORKSPACE_SYNC", result.stdout)
        self.assert_languages(self.clone / "Bin/x64/Release")
        self.assertFalse((self.clone / "language").exists())
        self.assertEqual({p.name: p.read_bytes() for p in self.source.glob("*.ini")}, self.expected)

    def test_debug_wrapper_stages_module_languages_and_the_documented_working_directory(self):
        result = self.wrapper("build-debug.bat")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_languages(self.clone / "Bin/x64/Debug")
        self.assert_languages(self.clone)
        self.assertNotIn("FORBIDDEN_WORKSPACE_SYNC", result.stdout)

    def test_failed_build_preserves_exit_code_and_does_not_stage_anything(self):
        result = self.wrapper("build-sln.bat", build_exit=37)
        self.assertEqual(result.returncode, 37, result.stdout + result.stderr)
        self.assertFalse((self.clone / "Bin").exists())
        self.assertFalse((self.clone / "language").exists())
        self.assertNotIn("FORBIDDEN_WORKSPACE_SYNC", result.stdout)

    def test_post_build_language_failure_propagates_without_partial_output(self):
        (self.source / "French.ini").unlink()
        result = self.wrapper("build-sln.bat")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("manifest", result.stderr)
        self.assertFalse((self.clone / "Bin").exists())

    def test_all_platforms_and_configurations_preserve_exact_bytes_and_are_idempotent(self):
        for platform in ("", "x64", "ARM64EC"):
            for config in ("Release", "Debug", "Release (lite)", "Debug (lite)"):
                output = Path("Bin") / platform / config
                with self.subTest(output=str(output)):
                    result = self.stage(output)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assert_languages(self.clone / output)
                    if config.startswith("Debug"):
                        self.assert_languages(self.clone)
        before = snapshot(self.clone)
        result = self.stage()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(self.clone), before, "current languages must not be rewritten")

    def test_absolute_output_and_windows_case_variants_keep_debug_cwd_languages(self):
        result = self.stage(self.clone / "Bin/x64/debug")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_languages(self.clone / "Bin/x64/Debug")
        self.assert_languages(self.clone)

    def test_invalid_source_manifest_or_encoding_fails_before_any_write(self):
        for mode in ("missing", "extra", "invalid-encoding"):
            with self.subTest(mode=mode):
                path = self.source / ("Unexpected.ini" if mode == "extra" else "French.ini")
                if mode == "missing":
                    path.unlink()
                else:
                    path.write_bytes(b"not a canonical UTF-8 language")
                before = snapshot(self.clone)
                result = self.stage()
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(snapshot(self.clone), before)
                if mode == "extra":
                    path.unlink()
                else:
                    path.write_bytes(self.expected[path.name])

    def test_directory_target_is_rejected_before_creating_other_languages(self):
        destination = self.clone / "Bin/x64/Release/language/French.ini"
        destination.mkdir(parents=True)
        before = snapshot(self.clone)
        result = self.stage()
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(snapshot(self.clone), before)

    def test_directory_reparse_target_is_rejected_before_writing_outside_the_clone(self):
        outside = self.area / "outside-language"
        outside.mkdir()
        sentinel = outside / "keep.txt"
        sentinel.write_bytes(b"must remain")
        output = self.clone / "Bin/x64/Release"
        output.mkdir(parents=True)
        link = output / "language"
        self.addCleanup(lambda: link.rmdir() if link.exists() else None)
        junction_script = self.area / "make-junction.cmd"
        junction_script.write_text(
            f'@echo off\nchcp 65001 >nul\nmklink /J "{link}" "{outside}" >nul 2>&1\n'
            'exit /b %errorlevel%\n', encoding="utf-8")
        result = self.invoke(["cmd.exe", "/d", "/c", str(junction_script)])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        try:
            result = self.stage()
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("reparse point", result.stderr)
            self.assertEqual(list(outside.iterdir()), [sentinel])
            self.assertEqual(sentinel.read_bytes(), b"must remain")
        finally:
            link.rmdir()

    def test_output_outside_known_bin_directories_cannot_write_any_language(self):
        for output in (self.area / "portable", self.clone / "PulseBar", "Bin/../portable",
                       "Bin/x64/Release:alias", "Bin/x64/Release."):
            with self.subTest(output=str(output)):
                before = snapshot(self.clone)
                result = self.stage(output)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(snapshot(self.clone), before)
                self.assertFalse((self.area / "portable/language").exists())

    def test_unknown_destination_ini_is_not_deleted_or_overwritten(self):
        destination = self.clone / "Bin/x64/Release/language"
        destination.mkdir(parents=True)
        (destination / "config.ini").write_bytes(b"user data")
        before = snapshot(self.clone)
        result = self.stage()
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(snapshot(self.clone), before)

    def test_hardlinked_target_is_rejected_before_replacing_an_alias(self):
        destination = self.clone / "Bin/x64/Release/language"
        destination.mkdir(parents=True)
        alias = self.area / "alias.ini"
        alias.write_bytes(b"must remain")
        target = destination / "French.ini"
        os.link(alias, target)
        self.addCleanup(lambda: target.unlink(missing_ok=True))
        before = snapshot(self.clone)
        result = self.stage()
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(alias.read_bytes(), b"must remain")
        self.assertEqual(snapshot(self.clone), before)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    unittest.main(verbosity=2)
