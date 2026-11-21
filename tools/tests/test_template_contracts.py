"""Unsupported template assignments stay disabled; public default construction works."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    code = r'''#include <type_traits>
#include "Nullable.hpp"
#include "RenderAPISupport.h"
static_assert(!std::is_copy_assignable_v<CNullable<int>>);
static_assert(!std::is_move_assignable_v<CNullable<int>>);
struct ProbeDevice;
struct ProbeDevice {using Type=int;struct Storage {
    void AddResource(CDeviceResourceBase<ProbeDevice>*);void RemoveResource(CDeviceResourceBase<ProbeDevice>*);};};
using ProbeResource=CDeviceResource<ProbeDevice>;
static_assert(!std::is_assignable_v<CDeviceResourceBase<ProbeDevice>&,CDeviceResourceBase<ProbeDevice>&>);
static_assert(!std::is_assignable_v<CDeviceResourceBase<ProbeDevice>&,CDeviceResourceBase<ProbeDevice>&&>);
static_assert(!std::is_move_constructible_v<CTrackableDevice<ProbeDevice>>);
static_assert(!std::is_copy_assignable_v<CTrackableDevice<ProbeDevice>>);
static_assert(!std::is_move_assignable_v<CTrackableDevice<ProbeDevice>>);
int wmain(int argc,wchar_t** argv){if(argc!=3||std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0)return 2;
    static_assert(std::is_default_constructible_v<CResourceTracker<ProbeResource>>);
    CResourceTracker<ProbeResource> tracker;
    return tracker.GetTrackerSet().empty()?0:1;}
'''
    run, env = native.build(code, "template-contracts")
    result = native.execute(run, env, "templates")
    print(f"ARTIFACTS {run}")
    return int(not result)


if __name__ == "__main__":
    raise SystemExit(main())
