#include "stdafx.h"
#include "DllFunctions.h"
#include <delayimp.h>

HMODULE LoadKnownSystemDll(LPCTSTR dll_name)
{
    // Accept filenames only: a path must not escape the System32 restriction.
    if (dll_name == nullptr || *dll_name == _T('\0') ||
        _tcspbrk(dll_name, _T("\\/:")) != nullptr ||
        _tcscmp(dll_name, _T(".")) == 0 || _tcscmp(dll_name, _T("..")) == 0)
        return nullptr;

    HMODULE module = ::LoadLibraryEx(dll_name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module != nullptr || ::GetLastError() != ERROR_INVALID_PARAMETER)
        return module;

    // Windows 7 without the restricted-search update rejects the flag. Keep
    // the primary DLL anchored to GetSystemDirectory, never CWD/PATH/app dir.
    TCHAR full_path[MAX_PATH + 1]{};
    const UINT length = ::GetSystemDirectory(full_path, _countof(full_path));
    const size_t name_length = _tcslen(dll_name);
    if (length == 0 || length >= _countof(full_path) ||
        name_length + length + 1 >= _countof(full_path))
        return nullptr;
    full_path[length] = _T('\\');
    _tcscpy_s(full_path + length + 1, _countof(full_path) - length - 1, dll_name);

    // On legacy loaders, dependency lookup starts in the same System32 dir.
    // Optional Windows APIs simply remain unavailable if this load fails.
    return ::LoadLibraryEx(full_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}

CDllFunctions::CDllFunctions()
{
    // shellscalingapi; absent optional APIs remain unavailable to callers.
    m_shcore_module = LoadKnownSystemDll(_T("Shcore.dll"));
    if (m_shcore_module != NULL)
    {
        m_getDpiForMonitor = (_GetDpiForMonitor)::GetProcAddress(m_shcore_module, "GetDpiForMonitor");
    }
}

CDllFunctions::~CDllFunctions()
{
    if (m_shcore_module != NULL)
    {
        FreeLibrary(m_shcore_module);
        m_shcore_module = NULL;
    }
}

HRESULT CDllFunctions::GetDpiForMonitor(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT* dpiX, UINT* dpiY)
{
    if (m_getDpiForMonitor != nullptr)
        return m_getDpiForMonitor(hmonitor, dpiType, dpiX, dpiY);
    return E_NOINTERFACE;
}

#define TRAFFICMONITOR_DEFINE_STATIC_MEMBER_IN_DLL_FUNCTIONS(member_name, ...) \
    decltype(CDllFunctions::member_name) CDllFunctions::member_name(__VA_ARGS__)

TRAFFICMONITOR_DEFINE_STATIC_MEMBER_IN_DLL_FUNCTIONS(
    D3DCompile,
    _T("d3dcompiler_47.dll"), "D3DCompile");

TRAFFICMONITOR_DEFINE_STATIC_MEMBER_IN_DLL_FUNCTIONS(
    DCompositionCreateDevice,
    _T("dcomp.dll"), "DCompositionCreateDevice");

TRAFFICMONITOR_DEFINE_STATIC_MEMBER_IN_DLL_FUNCTIONS(
    CreateDXGIFactory2,
    _T("dxgi.dll"), "CreateDXGIFactory2");

//延迟加载失败时给出明确提示，避免进程在进入代码前被系统直接终止。
extern "C" FARPROC WINAPI PulseBarDliFailureHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
    if (dliNotify == dliFailLoadLib && pdli != nullptr)
    {
        CString msg;
        msg.Format(L"缺少运行组件：%hs\n\n请重新解压完整的程序包后再运行。", pdli->szDll);
        ::MessageBoxW(nullptr, msg, L"PulseBar", MB_ICONERROR | MB_OK);
        ::ExitProcess(2);
    }
    return nullptr;
}

// 注册钩子（延迟加载失败时调用）
extern "C" const PfnDliHook __pfnDliFailureHook2 = PulseBarDliFailureHook;
