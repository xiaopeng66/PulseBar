"""The production up-down style must leave buddy text parseable on focus changes."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    source = native.source("SpinEdit.cpp")
    methods = native.method(source, "void CSpinEdit::PreSubclassWindow()") + native.method(
        source, "void CSpinEdit::OnKillFocus(CWnd* pNewWnd)")
    code = r'''#include <string>
#include <cwchar>
using DWORD=unsigned long; using BOOL=int;
constexpr DWORD WS_CHILD=1,WS_VISIBLE=2,UDS_ARROWKEYS=4,UDS_HOTTRACK=8,UDS_SETBUDDYINT=16,UDS_NOTHOUSANDS=32;
constexpr DWORD ES_CENTER=64,ES_RIGHT=128,ES_LEFT=256,WS_CLIPCHILDREN=512;constexpr int SPIN_ID=3100;
struct CRect{int left{},top{},right=100,bottom=20;int Width()const{return right-left;}};
struct CWnd{};struct CString{std::wstring value;const wchar_t* GetString()const{return value.c_str();}};
int _ttoi(const wchar_t* text){return static_cast<int>(std::wcstol(text,nullptr,10));}
class CEdit:public CWnd{public:std::wstring text;int base_focus_calls{};void ModifyStyle(DWORD,DWORD){}
    void GetClientRect(CRect* r){*r={0,0,100,20};}void GetWindowTextW(CString& out){out.value=text;}
    void OnKillFocus(CWnd*){++base_focus_calls;}void PreSubclassWindow(){};};
class CSpinButtonCtrl{public:DWORD style{};int position{};int width=16;CEdit* buddy{};
    bool Create(DWORD s,const CRect&,CWnd* parent,int){style=s;buddy=static_cast<CEdit*>(parent);return true;}
    void GetWindowRect(CRect* r){*r={0,0,width,20};}void MoveWindow(CRect*){}void SetBuddy(CEdit*){}
    void SetRange(int,int){}void SetPos(int value){position=value;buddy->text=(style&UDS_NOTHOUSANDS)?std::to_wstring(value):L"1,024";}};
class CSpinEdit:public CEdit{public:CSpinButtonCtrl m_spin;int m_spin_width{},m_step=1;void PreSubclassWindow();void OnKillFocus(CWnd*);
    int GetValue(){return m_spin.position;}void SetValue(int value){m_spin.SetPos(value);}};
'''+methods+r'''
int wmain(int argc,wchar_t** argv){if(argc!=3||std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0)return 2;
    CSpinEdit edit;edit.PreSubclassWindow();edit.SetValue(1024);CWnd next;edit.OnKillFocus(&next);
    return edit.GetValue()==1024&&edit.text==L"1024"&&edit.base_focus_calls==1?0:1;}
'''
    run, env = native.build(code, "spin-edit-focus")
    result = native.execute(run, env, "focus")
    print(f"ARTIFACTS {run}")
    return int(not result)


if __name__ == "__main__":
    raise SystemExit(main())
