"""CCommon::RepairMisdecodedText：把「UTF-8 被按 ANSI 解码」写坏的配置文本还原。

用例：
- 乱码的「内存: 」→ 正确「内存: 」；
- 本来正确的中文（GBK 字节不是合法 UTF-8）保持原样；
- ASCII 原样；
- 乱码的字体名「微软雅黑」→ 正确值。
"""
from pathlib import Path
import importlib.util
import sys

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("repair_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)

body = native.method(native.source("Common.cpp"), "std::wstring CCommon::RepairMisdecodedText(")

code = r'''#include <windows.h>
#include <cstdio>
#include <string>
struct CCommon {
    static std::wstring RepairMisdecodedText(const std::wstring& text);
};
''' + native.no_includes(body) + r'''
static int checks{}, failures{};
static void Check(bool ok, const char* label) {
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
}
static std::wstring W(const wchar_t* text) { return std::wstring(text); }
int wmain() {
    const std::wstring bad_memory = L"\u9350\u546d\u74e8\u003a\u0020";
    const std::wstring good_memory = L"\u5185\u5b58\u003a\u0020";
    const std::wstring bad_font = L"\u5bf0\ue1bf\u848b\u95c6\u5474\u7ca6";
    const std::wstring good_font = L"\u5fae\u8f6f\u96c5\u9ed1";

    Check(CCommon::RepairMisdecodedText(bad_memory) == good_memory, "mojibake-memory-restored");
    Check(CCommon::RepairMisdecodedText(good_memory) == good_memory, "correct-chinese-untouched");
    Check(CCommon::RepairMisdecodedText(W(L"CPU: ")) == W(L"CPU: "), "ascii-untouched");
    Check(CCommon::RepairMisdecodedText(W(L"")) == W(L""), "empty-untouched");
    Check(CCommon::RepairMisdecodedText(bad_font) == good_font, "mojibake-font-restored");
    std::printf("CHECKS %d PASS %d FAIL %d\n", checks, checks - failures, failures);
    return failures ? 1 : 0;
}
'''

run, env = native.build(code, "display-text-repair")
exit_code = native.execute(run, env, "repair")
print("ARTIFACTS", run.as_posix())

# 丢失字节的乱码无法还原，必须能被识别出来以退回默认文案
lost = native.method(native.source("SettingsHelper.cpp"), "bool HasLostByteMojibake(")
code2 = r'''#include <cstdio>
#include <string>
''' + native.no_includes(lost) + r'''
static int checks{}, failures{};
static void Check(bool ok, const char* label) {
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
}
int wmain() {
    Check(HasLostByteMojibake(L"\u922b? ", L"\u2191"), "lost-byte-mojibake-detected");
    Check(!HasLostByteMojibake(L"CPU: ", L"CPU: "), "ascii-not-flagged");
    Check(!HasLostByteMojibake(L"\u5185\u5b58: ", L"\u5185\u5b58: "), "correct-chinese-not-flagged");
    std::printf("CHECKS %d PASS %d FAIL %d\n", checks, checks - failures, failures);
    return failures ? 1 : 0;
}
'''
run2, env2 = native.build(code2, "display-text-lost-byte")
exit2 = native.execute(run2, env2, "lost")
print("ARTIFACTS", run2.as_posix())
sys.exit(0 if (exit_code and exit2) else 1)
