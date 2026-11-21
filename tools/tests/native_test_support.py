"""Small bounded native regression helpers; all generated files stay in workspace tmp."""
from pathlib import Path
import os
import subprocess
import tempfile
from test_paths import configured_root

SOURCE = Path(__file__).resolve().parents[2] / "PulseBar"
RUN_ROOT = configured_root(SOURCE.parent)


def find_vcvars():
    configured = os.environ.get("PULSEBAR_VCVARS")
    candidates = [Path(configured)] if configured else []
    program_files = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = program_files / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not configured and vswhere.is_file():
        result = subprocess.run(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
            capture_output=True, text=True, encoding="utf-8", timeout=10,
        )
        if result.returncode == 0 and result.stdout.strip():
            candidates.append(Path(result.stdout.strip()) / "VC/Auxiliary/Build/vcvars64.bat")
    if not configured:
        for edition in ("BuildTools", "Enterprise", "Professional", "Community"):
            candidates.append(program_files / "Microsoft Visual Studio/2022" / edition /
                              "VC/Auxiliary/Build/vcvars64.bat")
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError("Visual Studio 2022 vcvars64.bat was not found; set PULSEBAR_VCVARS")


def source(name):
    path = SOURCE / name
    if name in {"HistoryTrafficFile.cpp", "HistoryTrafficFile.h"}:
        return path.read_bytes().decode("gbk")
    return path.read_text(encoding="utf-8-sig")


def method(text, signature):
    start = text.index(signature)
    pos = text.index("{", start)
    depth = 0
    state = "code"
    quote = ""
    while pos < len(text):
        c = text[pos]
        nxt = text[pos:pos + 2]
        if state == "line":
            if c == "\n":
                state = "code"
        elif state == "block":
            if nxt == "*/":
                state = "code"
                pos += 1
        elif state == "string":
            if c == "\\":
                pos += 1
            elif c == quote:
                state = "code"
        elif nxt == "//":
            state = "line"
            pos += 1
        elif nxt == "/*":
            state = "block"
            pos += 1
        elif c in "\"'":
            quote = c
            state = "string"
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
        pos += 1
    raise ValueError(f"Unterminated method: {signature}")


def no_includes(text):
    return "\n".join(line for line in text.splitlines()
                     if not line.startswith(("#include", "#pragma once")))


def build(code, name):
    RUN_ROOT.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix=name + "-", dir=RUN_ROOT))
    cpp = run / "test.cpp"
    root_literal = (str(RUN_ROOT) + "\\").replace("\\", "\\\\").replace('"', '\\"')
    cpp.write_text(f'#define PULSEBAR_TEST_ROOT L"{root_literal}"\n' + code,
                   encoding="utf-8")
    script = run / "build.cmd"
    script.write_text(f'''@echo off
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
    call "{find_vcvars()}" >nul
if errorlevel 1 exit /b %errorlevel%
cd /d "{run}"
cl /nologo /EHsc /std:c++20 /W4 /utf-8 /Od /I"{SOURCE}" test.cpp /Fe:test.exe
exit /b %errorlevel%
''', encoding="utf-8")
    env = os.environ.copy()
    env.update(PYTHONUTF8="1", PYTHONIOENCODING="utf-8", TEMP=str(run), TMP=str(run))
    compiled = subprocess.run(["cmd.exe", "/d", "/c", str(script)], cwd=run, env=env,
                              capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
    (run / "build.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr)
        raise RuntimeError(f"Native fixture did not compile: {run}")
    return run, env


def execute(run, env, mode, timeout=5):
    try:
        result = subprocess.run([str(run / "test.exe"), mode, str(run)], cwd=run, env=env,
                                capture_output=True, text=True, encoding="utf-8", errors="replace",
                                timeout=timeout)
    except subprocess.TimeoutExpired:
        message = f"FAIL {mode}: exceeded {timeout}s bounded completion\n"
        (run / f"{mode}.log").write_text(message, encoding="utf-8")
        print(message, end="")
        return False
    output = result.stdout + result.stderr
    (run / f"{mode}.log").write_text(output + f"EXIT {result.returncode}\n", encoding="utf-8")
    print(output, end="")
    return result.returncode == 0
