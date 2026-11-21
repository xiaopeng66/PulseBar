import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "support", Path(__file__).with_name("native_test_support.py")
)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    code = r'''#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#include "UpdateCheckGate.h"

int main() {
    std::atomic_bool active{};
    if (!TryBeginUpdateCheck(active) || TryBeginUpdateCheck(active)) return 1;
    FinishUpdateCheck(active);
    if (active || !TryBeginUpdateCheck(active)) return 2;
    FinishUpdateCheck(active);

    constexpr int contenders=48;
    std::atomic<int> ready{}, attempts{}, finished{}, admitted{};
    std::atomic_bool start{};
    std::vector<std::thread> threads;
    for (int i=0;i<contenders;++i) threads.emplace_back([&] {
        ++ready;
        while (!start.load()) std::this_thread::yield();
        ++attempts;
        if (!TryBeginUpdateCheck(active)) { ++finished; return; }
        ++admitted;
        while (attempts.load() != contenders) std::this_thread::yield();
        while (finished.load() != contenders-1) std::this_thread::yield();
        FinishUpdateCheck(active);
    });
    while (ready != contenders) std::this_thread::yield();
    start=true;
    for (auto& thread:threads) thread.join();
    if (admitted != 1 || finished != contenders-1 || active) return 3;
    std::puts("update-check gate tests passed");
}
'''
    run, env = native.build(code, "update-check-gate")
    return int(not native.execute(run, env, "gate"))


if __name__ == "__main__":
    raise SystemExit(main())
