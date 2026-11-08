"""Run the real host close handler with queued old/new HWND lifetime boundaries."""
from pathlib import Path
import importlib.util
spec=importlib.util.spec_from_file_location('close_native',Path(__file__).with_name('native_test_support.py'))
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)


def main():
    method=native.method(native.source('PulseBarDlg.cpp'),'afx_msg LRESULT CPulseBarDlg::OnTaskbarWndClosed(')
    code=r'''
#include <cstdint>
#include <cstdio>
#include <string>
#define afx_msg
using LRESULT=intptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using UINT_PTR=uintptr_t;using HWND=void*;
int deleted{},icons{};
bool IsWindow(HWND h){return h!=nullptr;}
struct CCommon {
    static void WriteLog(const wchar_t*, const wchar_t*) {}     //日志不是本用例的断言对象
};
struct Bar {
    UINT_PTR token{};bool valid{};
    Bar(UINT_PTR t,bool v):token(t),valid(v){}
    ~Bar(){++deleted;}
    HWND GetSafeHwnd(){return valid?this:nullptr;}
    UINT_PTR GetInstanceToken(){return token;}
};
struct App {
    struct{bool m_show_task_bar_wnd=true;}m_cfg_data;
    struct{bool show_notify_icon=true;}m_general_data;
    struct{unsigned int update_layered_window_error_code=1;}m_taskbar_data;
    std::wstring m_log_path;
    bool IsForceShowNotifyIcon(){return true;}
}theApp;
struct CPulseBarDlg {
    Bar* m_tBarDlg{};
    void AddNotifyIcon(){++icons;}
    LRESULT OnTaskbarWndClosed(WPARAM,LPARAM);
};
'''+method+r'''
int main(){
    CPulseBarDlg host;int failures{};
    const auto check=[&](bool ok,const char* name){std::printf("%s %s\n",ok?"PASS":"FAIL",name);failures+=!ok;};
    host.m_tBarDlg=new Bar(2,true);
    host.OnTaskbarWndClosed(1,0);
    check(host.m_tBarDlg && host.m_tBarDlg->token==2 && theApp.m_cfg_data.m_show_task_bar_wnd && deleted==0,
          "queued-close-for-A-cannot-hide-or-delete-new-B");
    host.m_tBarDlg->valid=false;
    host.OnTaskbarWndClosed(1,0);
    check(host.m_tBarDlg && deleted==0,"queued-close-for-A-cannot-delete-invalid-but-unrelated-B");
    host.OnTaskbarWndClosed(2,0);
    // 面板意外关闭只清运行时对象，用户「要显示」的意图必须保留：
    // 旧实现把它改成 false，导致面板在显示而菜单未勾选，退出后还写进配置。
    check(host.m_tBarDlg==nullptr && deleted==1 && theApp.m_cfg_data.m_show_task_bar_wnd && theApp.m_taskbar_data.update_layered_window_error_code==0,
          "matching-close-cleans-object-and-error-state-but-keeps-display-intent");
    host.OnTaskbarWndClosed(2,0);
    check(deleted==1,"duplicate-close-is-idempotent");

    // 意外关闭后必须留一个入口：托盘图标关闭时自动补上
    CPulseBarDlg second;
    theApp.m_general_data.show_notify_icon=false;
    second.m_tBarDlg=new Bar(3,true);
    second.OnTaskbarWndClosed(3,0);
    check(icons==1 && theApp.m_general_data.show_notify_icon,
          "unexpected-close-restores-tray-icon-entry");
    return failures?1:0;
}
'''
    run,env=native.build(code,'queued-window-close')
    print('ARTIFACTS',run.as_posix())
    return int(not native.execute(run,env,'close'))


if __name__=='__main__':raise SystemExit(main())
