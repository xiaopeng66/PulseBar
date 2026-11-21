import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "support", Path(__file__).with_name("native_test_support.py")
)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    code = r'''#include <cstdio>
#include "NumericVersion.h"

int main() {
    auto newer=IsNewerNumericVersion(L"1.10.0",L"1.9.9");
    if (!newer || !*newer) return 1;
    auto equal=IsNewerNumericVersion(L"01.002.0",L"1.2");
    if (!equal || *equal) return 2;
    auto huge=IsNewerNumericVersion(L"999999999999999999999999999999.0",L"2.0");
    if (!huge || !*huge) return 3;
    for (const auto invalid : {L"",L"1..2",L"1.",L".1",L"v1.2",L"1-beta"})
        if (IsNewerNumericVersion(invalid,L"1.0")) return 4;
    if (IsNewerNumericVersion(L"1.0",L"1.x")) return 5;
    std::puts("numeric version tests passed");
}
'''
    run, env = native.build(code, "numeric-versions")
    return int(not native.execute(run, env, "versions"))


if __name__ == "__main__":
    raise SystemExit(main())
