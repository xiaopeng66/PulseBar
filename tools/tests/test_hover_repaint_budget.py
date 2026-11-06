"""Run actual hover Show() against recording window/region APIs, no desktop."""
from pathlib import Path
import importlib.util
spec=importlib.util.spec_from_file_location('hover_native',Path(__file__).with_name('native_test_support.py'))
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)


def main():
    body=native.method(native.source('SpeedGraphTip.cpp'),'void CSpeedGraphTip::Show(')
    code=r'''
#include <algorithm>
#include <cstdio>
using std::min;using std::max;using UINT=unsigned int;using BOOL=int;using HWND=void*;using HRGN=void*;
constexpr UINT SWP_NOACTIVATE=1, SWP_SHOWWINDOW=2;
constexpr BOOL FALSE=0, TRUE=1;
HWND HWND_TOPMOST=(void*)1;
struct CPoint{int x{},y{};CPoint(int a=0,int b=0):x(a),y(b){}};
struct CRect{int left{},top{},right{},bottom{};CRect()=default;CRect(int l,int t,int r,int b):left(l),top(t),right(r),bottom(b){};
int Width()const{return right-left;}int Height()const{return bottom-top;}
bool operator==(const CRect& b)const{return left==b.left&&top==b.top&&right==b.right&&bottom==b.bottom;}
bool operator!=(const CRect& b)const{return!(*this==b);}};
CRect actual;bool visible{};int positions{},regions{},invalidations{};
bool IsWindow(HWND h){return h!=nullptr;}
bool GetWindowRect(HWND,CRect* rect){*rect=actual;return true;}
void SetWindowPos(HWND,HWND,int x,int y,int w,int h,UINT flags){++positions;actual=CRect(x,y,x+w,y+h);if(flags&SWP_SHOWWINDOW)visible=true;}
void SetWindowRgn(HWND,HRGN,BOOL){++regions;}
void InvalidateRect(HWND,void*,BOOL){++invalidations;}
void UpdateWindow(HWND){} //OS synchronous paint boundary; count is owned by InvalidateRect above.
struct CRgn{void CreateRoundRectRgn(int,int,int,int,int,int){}HRGN Detach(){return(void*)1;}};
constexpr int PANEL_W=420,PANEL_GAP=4;
int S(int n,int dpi){return n*dpi/96;}
void GetTaskbarMonitorRect(const CRect&,CRect& rect){rect=CRect(0,0,1920,1080);}
struct CSpeedGraphTip{bool m_pinned{};UINT m_dpi=96;HWND m_hWnd{};
bool IsVisible()const{return visible;}
void CreateWindowIfNeeded(){if(!m_hWnd)m_hWnd=(void*)2;}
int CalcWindowHeight(UINT dpi)const{return 220*static_cast<int>(dpi)/96;}
void Show(const CPoint&,const CRect&,UINT);};
'''+body+r'''
int main(){int failures{};CSpeedGraphTip panel;
const auto check=[&](bool ok,const char* name){std::printf("%s %s pos=%d region=%d invalid=%d\n",ok?"PASS":"FAIL",name,positions,regions,invalidations);failures+=!ok;};
panel.Show({600,950},{400,1000,900,1040},96);
check(positions==1&&regions==1&&invalidations==1,"first-show-sets-bounds-region-and-paints-once");
for(int i=0;i<100;++i)panel.Show({600,950},{400,1000,900,1040},96);
check(positions==1&&regions==1&&invalidations==1,"unchanged-hover-does-not-recreate-regions-or-repaint");
panel.Show({601,950},{400,1000,900,1040},96);
check(positions==2&&regions==1&&invalidations==1,"position-only-movement-reuses-complete-content");
panel.Show({601,950},{400,1000,900,1040},144);
check(regions==2&&invalidations==2,"DPI-change-rebuilds-shape-and-invalidates-once");
return failures?1:0;}
'''
    run,env=native.build(code,'hover-repaint-budget');print('ARTIFACTS',run.as_posix())
    return int(not native.execute(run,env,'hover'))


if __name__=='__main__':raise SystemExit(main())
