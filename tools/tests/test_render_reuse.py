"""Compile production resource-reuse decisions with small native COM-pointer doubles."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    source = native.source("TaskBarDlgDrawCommon.cpp")
    d2d_device_handler = native.method(
        source, "bool CD2D1DeviceContextWindowSupport::CD2D1DeviceContextHelper::HandleDeviceRecreationIfNecessary()")
    d3d_device_handler = native.method(
        source, "bool CD2D1DeviceContextWindowSupport::CD3DHelper::HandleDeviceRecreationIfNecessary()")
    rebind = native.method(native.source("TaskBarDlgDrawCommon.h"), "void RebindRenderTarget(")
    code = r'''#include <utility>
#include <string>
namespace Microsoft { namespace WRL {
template<class T> struct ComPtr { T* value{}; T* Get() const { return value; } ComPtr& operator=(ComPtr other) { value=other.value; return *this; } };
}}
struct ID2D1RenderTarget {};
class CD2D1DeviceContextWindowSupport {
public:
    class CD2D1DeviceContextHelper { public: bool m_need_recreate=false; int resets=0; void Reinitialize(){++resets;} bool HandleDeviceRecreationIfNecessary(); };
    class CD3DHelper { public: bool m_need_recreate=false; bool HandleDeviceRecreationIfNecessary(); };
};
template<class ResType> class CacheProbe {
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> m_p_render_target{};
public:
    int recreations{};
    void RecreateAllResources(){++recreations;}
'''+rebind+r'''
};
'''+d2d_device_handler+d3d_device_handler+r'''
int wmain(int argc,wchar_t** argv) {
    if(argc!=3 || std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0) return 2;
    CacheProbe<int> cache; ID2D1RenderTarget first{}, second{};
    cache.RebindRenderTarget({&first}); cache.RebindRenderTarget({&first});
    if(cache.recreations!=1) return 1;
    cache.RebindRenderTarget({&second}); if(cache.recreations!=2) return 1;
    CD2D1DeviceContextWindowSupport::CD2D1DeviceContextHelper d2d;
    if(d2d.HandleDeviceRecreationIfNecessary() || d2d.resets!=0) return 1;
    d2d.m_need_recreate=true;
    if(!d2d.HandleDeviceRecreationIfNecessary() || d2d.resets!=1) return 1;
    if(d2d.HandleDeviceRecreationIfNecessary() || d2d.resets!=1) return 1;
    CD2D1DeviceContextWindowSupport::CD3DHelper d3d;
    if(d3d.HandleDeviceRecreationIfNecessary()) return 1;
    d3d.m_need_recreate=true;
    if(!d3d.HandleDeviceRecreationIfNecessary() || d3d.HandleDeviceRecreationIfNecessary()) return 1;
    return 0;
}
'''
    run, env = native.build(code, "render-reuse")
    outcome = native.execute(run, env, "all")
    print(f"ARTIFACTS {run}")
    return int(not outcome)


if __name__ == "__main__":
    raise SystemExit(main())
