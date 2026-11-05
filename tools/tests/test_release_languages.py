"""Exercise only CI language staging in empty mirrors; never build/upload/run an EXE."""
from pathlib import Path
import fnmatch
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
RUN_ROOT = Path("E:/AI/Zcode/tmp/pulsebar-fix-20261105")


def artifact_paths(job):
    blocks = re.findall(
        r"(?ms)^    - name\s*: Upload artifact\n.*?^        path:[ \t]*\|[+-]?[ \t]*\n"
        r"((?:          [^\n]*\n)+)", job)
    if len(blocks) != 1:
        raise ValueError(f"expected one artifact path block, got {len(blocks)}")
    return [line.strip() for line in blocks[0].splitlines()]


def stage_script(job, output):
    blocks = re.findall(
        r"(?m)^    - name: Stage release languages\n      shell: pwsh\n"
        r"      run: \|[+-]?\n((?:        [^\n]*\n)+)", job)
    if not blocks:
        return None
    if len(blocks) != 1:
        raise ValueError("expected one language staging step")
    script = "\n".join(line[8:] for line in blocks[0].splitlines())
    # Execute only the audited language helper, never arbitrary workflow commands.
    # The destination must be exactly the fixture's declared EXE directory.
    expected = (
        f"python -X utf8 -I -B tools/stage-languages.py --build-output '{output}'\n"
        "if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }"
    )
    if script != expected:
        raise ValueError("unreviewed staging command: review its side effects before executing it")
    return script


def included(candidate, paths):
    return any(candidate == rule or candidate.startswith(rule.rstrip("/") + "/")
               or fnmatch.fnmatchcase(candidate, rule) for rule in paths)


def main():
    workflow = (ROOT / ".github/workflows/main.yml").read_text(encoding="utf-8").lstrip("\ufeff")
    jobs = re.split(r"(?m)^  (?=\w+_build:)", workflow)[1:]
    languages = {path.name: path.read_bytes() for path in (ROOT / "PulseBar/language").glob("*.ini")}
    if len(languages) != 18 or len(jobs) != 3:
        print("FAIL fixture: expected authoritative 18 languages and 3 build jobs")
        return 1
    shell = shutil.which("pwsh.exe") or shutil.which("powershell.exe")
    RUN_ROOT.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="ci-language-", dir=RUN_ROOT))
    failures = []
    try:
        for job in jobs:
            name = job.split(":", 1)[0]
            paths = artifact_paths(job)
            roots = [path.rsplit("/", 1)[0] for path in paths if path.endswith("/PulseBar.exe")]
            if len(roots) != 1 or not re.fullmatch(r"Bin/(?:x64/|ARM64EC/)?Release \(lite\)", roots[0]):
                raise ValueError(f"{name}: unexpected executable output path")
            output = roots[0]
            script = stage_script(job, output)
            mirror = run / name
            source = mirror / "PulseBar/language"
            source.mkdir(parents=True)
            for filename, data in languages.items():
                (source / filename).write_bytes(data)
            tools = mirror / "tools"
            tools.mkdir()
            shutil.copyfile(ROOT / "tools/stage-languages.py", tools / "stage-languages.py")
            # Intentionally no root/language, Bin, EXE, DLL, .git or live distributions.
            staged = False
            if script is not None:
                if shell is None:
                    raise ValueError("PowerShell is required for this isolated staging regression")
                environment = os.environ.copy()
                environment.update(TEMP=str(mirror), TMP=str(mirror), PYTHONUTF8="1", PYTHONIOENCODING="utf-8")
                # Pin the current interpreter rather than a Windows Store/PATH alias.
                script = "& '" + sys.executable.replace("'", "''") + "' " + script[len("python "):]
                result = subprocess.run(
                    [shell, "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", script],
                    cwd=mirror, env=environment, capture_output=True, text=True,
                    encoding="utf-8", timeout=30)
                (run / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
                if result.returncode:
                    raise ValueError(f"{name}: staging exited {result.returncode}: {result.stderr}")
                staged = True
            destination = mirror / output / "language"
            actual = {path.name: path.read_bytes() for path in destination.glob("*.ini")}
            packaged = {filename for filename in actual
                        if included(output + "/language/" + filename, paths)}
            missing = sorted(set(languages) - packaged)
            exact = actual == languages
            ok = staged and exact and not missing
            print(f"{'PASS' if ok else 'FAIL'} {name}: staging={staged} "
                  f"artifact_paths={len(paths)} staged_languages={len(actual)} "
                  f"missing_languages={len(missing)} exact_bytes={exact}")
            if not ok:
                failures.append(name)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"FAIL fixture: {error}")
        failures.append("fixture")
    print(f"ARTIFACTS {run.as_posix()}")
    return int(bool(failures))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    raise SystemExit(main())
