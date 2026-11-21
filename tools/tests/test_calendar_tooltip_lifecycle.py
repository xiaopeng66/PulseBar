"""The calendar owns one tooltip tool and caches the full date per window."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    method = native.method(native.source("HistoryTrafficCalendarDlg.cpp"),
                           "void CHistoryTrafficCalendarDlg::OnMouseMove(UINT nFlags, CPoint point)")
    code = r'''#include <string>
#include <vector>
#include <cstdio>
#define _T(text) L##text
using UINT=unsigned int; using COLORREF=unsigned long;
struct CPoint {int x{},y{};};
struct CRect {int left{},top{},right{},bottom{}; void OffsetRect(int x,int y){left+=x;right+=x;top+=y;bottom+=y;}
    bool PtInRect(CPoint p)const{return p.x>=left&&p.x<right&&p.y>=top&&p.y<bottom;}};
struct CString:std::wstring {using std::wstring::wstring; void Format(const wchar_t*,int,int,int){assign(L"date");}
    operator const wchar_t*()const{return c_str();}
    CString& operator+=(const CString& s){std::wstring::operator+=(s);return *this;}};
struct DayTraffic {int day{};long long up_traffic{},down_traffic{};bool mixed{};CRect rect{};long long traffic()const{return up_traffic+down_traffic;}};
constexpr int CALENDAR_HEIGHT=6,CALENDAR_WIDTH=7,IDS_TRAFFIC_USED1=1,IDS_UPLOAD=2,IDS_DOWNLOAD=3;
struct CWnd {};
struct CToolTipCtrl {int add{},update{},pop{};CString last;
    bool AddTool(CWnd*,const wchar_t*){++add;return true;}
    void UpdateTipText(const CString& text,CWnd*){++update;last=text;}
    void Pop(){++pop;}};
struct CTabDlg:CWnd {int base_moves{};void OnMouseMove(UINT,CPoint){++base_moves;}};
struct CCommon {static CString LoadText(int){return CString(L"traffic");}
    static CString LoadText(int,const wchar_t*){return CString(L"flow");}
    static CString KBytesToString(long long){return CString(L"1 KB");}};
class CHistoryTrafficCalendarDlg:public CTabDlg {
public:
    DayTraffic m_calendar[CALENDAR_HEIGHT][CALENDAR_WIDTH]{}; int m_year=2026,m_month=1,m_start_x{},m_start_y{};
    CToolTipCtrl m_tool_tips; int m_last_tip_year{},m_last_tip_month{},m_last_tip_day{};
    void OnMouseMove(UINT nFlags,CPoint point);
};
'''+method+r'''
void set_day(CHistoryTrafficCalendarDlg& dlg,int day) {dlg.m_calendar[0][0].day=day;dlg.m_calendar[0][0].rect={0,0,10,10};}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3||std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0)return 2;
    CHistoryTrafficCalendarDlg first;set_day(first,1);
    first.OnMouseMove(0,{5,5});first.OnMouseMove(0,{5,5});
    if(first.m_tool_tips.add!=0||first.m_tool_tips.update!=1||first.m_tool_tips.pop!=1)return 1;
    first.m_month=2;first.OnMouseMove(0,{5,5});if(first.m_tool_tips.update!=2)return 1;
    CHistoryTrafficCalendarDlg second;set_day(second,1);second.m_month=2;second.OnMouseMove(0,{5,5});
    if(second.m_tool_tips.update!=1||second.m_tool_tips.add!=0)return 1;
    second.OnMouseMove(0,{50,50});
    if(second.m_tool_tips.update!=2||second.m_last_tip_day!=0)return 1;
    second.OnMouseMove(0,{5,5});return second.m_tool_tips.update==3?0:1;
}
'''
    run, env = native.build(code, "calendar-tooltip-lifecycle")
    result = native.execute(run, env, "calendar")
    print(f"ARTIFACTS {run}")
    return int(not result)


if __name__ == "__main__":
    raise SystemExit(main())
