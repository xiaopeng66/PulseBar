"""Execute the current D2D DrawBitmap method with a recording bitmap surface.

No device or desktop is used. The current shared image-sizing helper and bitmap
method run unchanged; recorded destination rectangles/clip depth are asserted.
"""
from pathlib import Path
import importlib.util
spec=importlib.util.spec_from_file_location('bitmap_native',Path(__file__).with_name('native_test_support.py'))
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)


def main():
    draw=native.method(native.source('TaskBarDlgDrawCommon.cpp'),'void CTaskBarDlgDrawCommon::DrawBitmap(')
    sizing=native.method(native.source('DrawCommonHelper.cpp'),'void DrawCommonHelper::ImageDrawAreaConvert(')
    code=r'''
#include <cstdio>
#include <limits>
using BYTE=unsigned char; using HBITMAP=void*;
struct CSize {int cx{},cy{};CSize()=default;CSize(int x,int y):cx(x),cy(y){};};
struct CPoint {int x{},y{};CPoint()=default;CPoint(int a,int b):x(a),y(b){};};
struct CRect {int left{},top{},right{},bottom{};CRect(CPoint p,CSize s):left(p.x),top(p.y),right(p.x+s.cx),bottom(p.y+s.cy){};};
struct IDrawCommon {enum class StretchMode{STRETCH,FILL,FIT};};
struct D2D1_RECT_F {float left{},top{},right{},bottom{};};
constexpr int D2D1_ANTIALIAS_MODE_PER_PRIMITIVE=0;
struct PixelSize{unsigned int width{},height{};};
struct FloatSize{float width{},height{};};
struct Bitmap {PixelSize size{200,100};PixelSize GetPixelSize(){return size;} FloatSize GetSize(){return{float(size.width),float(size.height)};}} bitmap;
struct BitmapPtr {Bitmap* value{&bitmap};operator bool()const{return value!=nullptr;} Bitmap* operator->(){return value;} Bitmap* Get(){return value;}};
struct Surface {
    int draws{},clip_depth{},clip_at_draw{}; D2D1_RECT_F destination;
    void DrawBitmap(Bitmap*,D2D1_RECT_F r,float){++draws;destination=r;clip_at_draw=clip_depth;}
    void PushAxisAlignedClip(D2D1_RECT_F,int){++clip_depth;}
    void PopAxisAlignedClip(){--clip_depth;}
} surface;
struct Support {BitmapPtr GetCachedBitmap(HBITMAP){return{};}} support;
template<class T>struct ResourceBitmapCreator {static BitmapPtr Create(Surface*,T){return{};}};
namespace DrawCommonHelper {void ImageDrawAreaConvert(CSize,CPoint&,CSize&,IDrawCommon::StretchMode);}
struct CTaskBarDlgDrawCommon {
    using StretchMode=IDrawCommon::StretchMode;
    Surface* m_p_device_context{&surface}; Support* m_p_d2d1_device_context_support{&support};
    void DrawBitmap(HBITMAP,CPoint,CSize,StretchMode,BYTE);
    static D2D1_RECT_F Convert(CRect r){return{float(r.left),float(r.top),float(r.right),float(r.bottom)};}
    void SetDrawRect(CRect r){surface.PushAxisAlignedClip(Convert(r),0);}
    void ResetClippedStateIfSet(){surface.PopAxisAlignedClip();}
};
'''
    code+=sizing+'\n'+draw+'\n'
    code+=r'''
int main(){
    CTaskBarDlgDrawCommon d;int failed{};
    const auto check=[&](bool ok,const char* name){std::printf("%s %s\n",ok?"PASS":"FAIL",name);failed+=!ok;};
    surface={};bitmap.size={200,100};
    d.DrawBitmap((void*)1,{0,0},{100,100},IDrawCommon::StretchMode::FIT,255);
    auto r=surface.destination;
    check(r.left==0 && r.top==25 && r.right==100 && r.bottom==75,"FIT-preserves-aspect-ratio-and-stays-centered");
    surface={};
    d.DrawBitmap((void*)1,{0,0},{100,100},IDrawCommon::StretchMode::FILL,255);
    r=surface.destination;
    check(r.left==-50 && r.top==0 && r.right==150 && r.bottom==100,"FILL-uses-expanded-image-not-unscaled-target-size");
    check(surface.clip_at_draw==1 && surface.clip_depth==0,"FILL-clips-during-draw-and-restores-prior-clip-afterwards");
    return failed?1:0;
}
'''
    run,env=native.build(code,'bitmap-fit-geometry')
    print('ARTIFACTS',run.as_posix())
    return int(not native.execute(run,env,'bitmap'))


if __name__=='__main__':raise SystemExit(main())
