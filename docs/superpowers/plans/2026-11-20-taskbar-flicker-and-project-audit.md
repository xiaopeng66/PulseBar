# Taskbar Flicker Fix and Full Project Audit Plan

> **For agentic workers:** Execute the requested work in this session, with each change verified before moving to the next phase.

**Goal:** Stop periodic taskbar-panel recreation caused by a false embed-failure result, and record a numbered, evidence-based review of project-wide quality and security findings.

**Architecture:** Correct the Win32 `SetParent` result handling in the existing taskbar-window initialization path and add a regression contract beside the existing HWND lifecycle tests. Review all tracked text/source/configuration files using ten complete repository passes with distinct review lenses, then validate candidate findings and write them to one numbered report. Exclude binary assets from code-reading passes and mark any static-only or environment-dependent conclusions explicitly.

**Tech Stack:** C++/MFC, Win32 HWND APIs, Python `unittest`, PowerShell, Git.

**Scope:** `PulseBar/`, `OpenHardwareMonitorApi/`, `PluginDemo/`, `include/`, `tools/`, root build/release scripts, solution/project files, and tracked documentation/configuration. Do not modify installed PulseBar, publish a release, or change distribution binaries.

---

### Task 1: Reproduce the embed-result bug with a regression contract

**Files:**
- Modify: `tools/tests/test_window_lifecycle_guards.py`
- Read: `tools/tests/native_test_support.py`

- [ ] Add a test requiring `SetLastError(ERROR_SUCCESS)` before `SetParent`, failure classification that distinguishes a legitimate null previous parent from an API failure, and verification that the resulting parent equals the requested taskbar parent.
- [ ] Run only the new test and confirm it fails on the current implementation for the expected missing contract.

### Task 2: Fix the HWND embed result handling

**Files:**
- Modify: `PulseBar/TaskBarDlg.cpp`
- Verify callers: `CPulseBarDlg::OpenTaskBarWnd`, the 10-second retry block in `CPulseBarDlg::OnTimer`, and fallback positioning in `CTaskBarDlg::AdjustWindowPos`.

- [ ] Clear LastError before `SetParent`.
- [ ] Treat a null previous-parent result as failure only when LastError reports a failure; independently reject a resulting `GA_PARENT` different from the requested parent.
- [ ] Preserve the actual failure code and existing fallback behavior.
- [ ] Run the regression contract, lifecycle tests, and relevant native/build checks.

### Task 3: Ten repository-wide review passes

**Files:**
- Review: all tracked text/source/configuration files (binary assets excluded and counted separately).
- Create: `reports/2026-11-20-project-audit.md`

- [ ] Run each pass from the first tracked reviewable file through the last, reading every file completely. Use separate lenses for correctness/state, ownership/resources, security/trust boundaries, error handling, parsing/storage, concurrency/window lifecycle, UI/accessibility/platform compatibility, build/dependency/release supply chain, tests/coverage, and a final cross-file recheck.
- [ ] Validate every finding against the code and evidence; number findings in severity order, include file/line, impact, evidence, and recommended action.
- [ ] Clearly mark review limitations and issues requiring runtime/environment verification.

### Task 4: Final verification and handoff

- [ ] Run focused lifecycle tests and the available broader automated suite/build.
- [ ] Inspect the final diff and confirm the local install and GitHub release state were not changed.
- [ ] Report the confirmed root cause, fix, test evidence, and audit findings.
