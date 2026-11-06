"""Exception cleanup for the production stack-object draw factory.

MFC/D3D objects are replaced only with counted, polymorphic lifetime boundaries.
The actual current factory header/implementation are compiled. Legacy placement
construction and the new ownership-aware construction follow their respective
production call contracts; both must release every fully constructed object.
"""
from pathlib import Path
import importlib.util
import json

spec = importlib.util.spec_from_file_location('factory_native', Path(__file__).with_name('native_test_support.py'))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    header = native.source('DrawCommonFactory.h')
    implementation = native.source('DrawCommonFactory.cpp')
    aware = 'EmplaceDrawBuffer' in header and 'EmplaceDrawCommon' in header
    code = r'''
#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <cstdio>
#define TRAFFICMONITOR_ERROR_STR(x) x
struct IDrawCommon { virtual ~IDrawCommon() = default; };
struct IDrawBuffer { virtual ~IDrawBuffer() = default; };
int alive{}, destroyed{}, failures_marked{};
struct CDrawCommon : IDrawCommon { CDrawCommon(){++alive;} ~CDrawCommon(){--alive;++destroyed;} };
struct CTaskBarDlgDrawCommon : IDrawCommon { CTaskBarDlgDrawCommon(){++alive;} ~CTaskBarDlgDrawCommon(){--alive;++destroyed;} };
struct CDrawDoubleBufferCached : IDrawBuffer { CDrawDoubleBufferCached(){++alive;} ~CDrawDoubleBufferCached(){--alive;++destroyed;} };
struct CTaskBarDlgDrawBuffer : IDrawBuffer { CTaskBarDlgDrawBuffer(){++alive;} ~CTaskBarDlgDrawBuffer(){--alive;++destroyed;} };
struct CTaskBarDlgDrawBufferUseDComposition : IDrawBuffer { CTaskBarDlgDrawBufferUseDComposition(){++alive;} ~CTaskBarDlgDrawBufferUseDComposition(){--alive;++destroyed;} };
template<class... Ts> using AlignedUnionStorage = typename std::aligned_union<0, Ts...>::type;
template<class T> void Destroy(T* p) { p->~T(); }
template<class T, class... Args> void EmplaceAt(T* p, Args&&... args) { ::new(p) T(std::forward<Args>(args)...); }
namespace DrawCommonHelper {
enum class RenderType { DEFAULT, D2D1, D2D1_WITH_DCOMPOSITION };
void MarkFrameSubmitFailed(long) {++failures_marked;}
}
constexpr long E_FAIL=-1;
'''
    code += native.no_includes(header)+'\n'+native.no_includes(implementation)+'\n'
    code += '\n#define OWNERSHIP_AWARE '+('1' if aware else '0')+'\n'
    code += r'''
int main() {
    int failures{};
    for (int where=1; where<=2; ++where) {
        alive=destroyed=failures_marked=0;
        try {
            AllInvolvedDrawCommonObjectsStorage storage;
            GetInterfaceFromAllInvolvedDrawCommonObjects(storage, DrawCommonHelper::RenderType::DEFAULT,
                {{DrawCommonHelper::RenderType::DEFAULT,[&](IDrawBuffer* b, IDrawCommon* d) {
#if OWNERSHIP_AWARE
                    storage.EmplaceDrawBuffer<CDrawDoubleBufferCached>();
#else
                    EmplaceAt(static_cast<CDrawDoubleBufferCached*>(b));
#endif
                    if (where==1) throw std::runtime_error("injected-after-buffer-construction");
#if OWNERSHIP_AWARE
                    storage.EmplaceDrawCommon<CDrawCommon>();
#else
                    EmplaceAt(static_cast<CDrawCommon*>(d));
#endif
                    throw std::runtime_error("injected-after-draw-construction");
                }}});
        } catch (const std::runtime_error&) {}
        const bool ok=alive==0 && destroyed==where;
        std::printf("%s injected-stage=%d alive=%d destroyed=%d\n",ok?"PASS":"FAIL",where,alive,destroyed);
        failures+=!ok;
    }
    return failures?1:0;
}
'''
    run, env = native.build(code, 'draw-factory-exception')
    print('ARTIFACTS',run.as_posix())
    passed = native.execute(run,env,'cleanup')
    print('PASS' if passed else 'FAIL','fully-constructed-render-objects-are-destroyed-on-initializer-failure')
    return int(not passed)


if __name__=='__main__':
    raise SystemExit(main())
