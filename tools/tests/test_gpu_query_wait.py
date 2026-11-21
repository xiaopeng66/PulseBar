"""Compile and exercise the production GPU event-query wait method."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    method = native.method(native.source("D3D10Support1.cpp"),
                           "HRESULT CD3D10DrawCallWaiter::Wait() const noexcept")
    code = r'''#include <cstdint>
#include <chrono>
#include <string>
#include <thread>
using HRESULT = long;
constexpr HRESULT S_OK = 0;
constexpr HRESULT S_FALSE = 1;
constexpr HRESULT E_FAIL = static_cast<HRESULT>(0x80004005L);
struct ID3D10Query { HRESULT next = S_FALSE; HRESULT GetData(void*, unsigned, unsigned) {return next;} };
class CD3D10DrawCallWaiter { public: HRESULT Wait() const noexcept; ID3D10Query* m_p_query{}; };
'''+method+r'''

int wmain(int argc, wchar_t** argv) {
    if (argc != 3 || std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT) != 0) return 2;
    const auto mode = std::wstring(argv[1]);
    ID3D10Query query{}; CD3D10DrawCallWaiter waiter; waiter.m_p_query = &query;
    if (mode == L"success") {
        query.next = S_OK;
        return waiter.Wait() == S_OK ? 0 : 1;
    }
    if (mode == L"failure") {
        query.next = E_FAIL;
        return waiter.Wait() == E_FAIL ? 0 : 1;
    }
    if (mode == L"timeout") {
        const auto start = std::chrono::steady_clock::now();
        query.next = S_FALSE;
        const HRESULT result = waiter.Wait();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return result == S_FALSE && elapsed >= std::chrono::milliseconds(1400)
            && elapsed < std::chrono::milliseconds(1800) ? 0 : 1;
    }
    return 3;
}
'''
    run, env = native.build(code, "gpu-query-wait")
    outcomes = [native.execute(run, env, mode) for mode in ("success", "failure", "timeout")]
    print(f"ARTIFACTS {run}")
    return int(not all(outcomes))


if __name__ == "__main__":
    raise SystemExit(main())
