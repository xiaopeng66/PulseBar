"""Execute the current production frame-submission bookkeeping, without D3D.

The external HRESULT type is a scalar boundary. Production helper methods are
extracted unchanged; recursion, bounded depth and reset behavior are assertions,
not a copied implementation of those helpers.
"""
from pathlib import Path
import importlib.util
import json
import sys

spec = importlib.util.spec_from_file_location('render_test_native', Path(__file__).with_name('native_test_support.py'))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    src = native.source('TaskBarDlgDrawCommon.cpp')
    production = native.method(src, 'namespace DrawCommonHelper')
    paint = native.method(native.source('TaskBarDlg.cpp'), 'void CTaskBarDlg::OnPaint()')
    failures = []
    if 'FrameRetryDelay(' not in production:
        failures.append('bounded failure backoff policy missing')
    if 'SetTimer(TASKBAR_RENDER_RETRY_TIMER' not in paint or 'Invalidate(FALSE)' in paint[paint.index('FinishFrameSubmit'):]:
        failures.append('OnPaint must schedule a bounded retry instead of immediately invalidating itself')
    if 'if (m_in_paint)' not in paint:
        failures.append('same-window synchronous reentrant painting must be guarded')
    for signature in ('CTaskBarDlgDrawBuffer::~CTaskBarDlgDrawBuffer()', 'CTaskBarDlgDrawBufferUseDComposition::~CTaskBarDlgDrawBufferUseDComposition()'):
        if 'InvalidateRect(m_target_hwnd' in native.method(src, signature):
            failures.append('buffer destructor must leave retry scheduling to the window')
    for failure in failures:
        print('FAIL source-contract:', failure, flush=True)
    code = r'''
#include <array>
#include <cstdio>
using HRESULT = long;
constexpr HRESULT S_OK = 0, E_FAIL = -1, E_UNEXPECTED = -2;
namespace DrawCommonHelper {
struct FrameSubmitResult { bool active{}, succeeded{}; HRESULT failure_hr{S_OK}; };
void MarkFrameSubmitFailed(HRESULT) noexcept;
}
'''
    code += production
    has_retry_policy = 'FrameRetryDelay(' in production
    code += '\n#define HAS_RETRY_POLICY ' + ('1' if has_retry_policy else '0') + '\n'
    code += r'''
int main() {
    using namespace DrawCommonHelper;
    int failures{};
    const auto check = [&](bool good, const char* name) {
        std::printf("%s %s\n", good ? "PASS" : "FAIL", name);
        if (!good) ++failures;
    };
    BeginFrameSubmit(); MarkFrameSubmitFailed(E_FAIL);
    BeginFrameSubmit(); MarkFrameSubmitSucceeded();
    const auto nested = FinishFrameSubmit();
    const auto outer = FinishFrameSubmit();
    check(nested.active && nested.succeeded, "nested-frame-completes-independently");
    check(outer.active && !outer.succeeded && outer.failure_hr == E_FAIL,
          "nested-frame-does-not-clear-outer-failure");
    BeginFrameSubmit(); MarkFrameSubmitSucceeded();
    const auto fresh = FinishFrameSubmit();
    check(fresh.active && fresh.succeeded, "failed-frame-does-not-poison-next-frame");
    for (int i=0; i<64; ++i) BeginFrameSubmit();
    int active{};
    for (int i=0; i<64; ++i) active += FinishFrameSubmit().active ? 1 : 0;
    check(active == 64 && !FinishFrameSubmit().active, "bounded-depth-unwinds-without-losing-outer-frames");
#if HAS_RETRY_POLICY
    const unsigned int expected[] = {0, 100, 200, 400, 800, 1600, 1600, 0};
    bool bounded = true;
    for (unsigned int i=0; i<8; ++i) bounded = bounded && FrameRetryDelay(i) == expected[i];
    check(bounded && FrameRetryDelay(10000) == 0, "persistent-failure-retries-are-delayed-and-bounded");
#endif
    return failures ? 1 : 0;
}
'''
    run, env = native.build(code, 'render-submit-state')
    print('ARTIFACTS', run.as_posix(), flush=True)
    passed = native.execute(run, env, 'state')
    (run / 'result.json').write_text(json.dumps({'pass': passed, 'scope': __doc__}, indent=2)+'\n', encoding='utf-8')
    return int(not passed or bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
