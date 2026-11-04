//封装的绘图类
#pragma once
#include "IDrawCommon.h"
#include "D2D1Support.h"
#include "CommonData.h"
#include "Nullable.hpp"
#include "DrawCommonEx.h"
#include <memory>

class CDrawCommon final : public IDrawCommon
{
public:

    CDrawCommon();
    ~CDrawCommon();

    void Create(CDC* pDC, CWnd* pMainWnd);
    void SetFont(CFont* pfont) override;		//设置绘制文本的字体
    void SetDC(CDC* pDC);		//设置绘图的DC
    virtual CDC* GetDC() override { return m_pDC; }
    void SetBackColor(COLORREF back_color, BYTE alpha = 255) override { m_back_color = back_color; }
    void SetTextColor(const COLORREF text_color, BYTE alpha = 255) override
    {
        m_pDC->SetTextColor(text_color);
    }

    void DrawWindowText(CRect rect, LPCTSTR lpszString, COLORREF color, Alignment align = Alignment::LEFT, bool draw_back_ground = false, bool multi_line = false, BYTE alpha = 255) override; //在指定的矩形区域内绘制文本

    void SetDrawRect(CRect rect) override;		//设置绘图剪辑区域
    static void SetDrawRect(CDC* pDC, CRect rect);

    //绘制一个位图
    void DrawBitmap(CBitmap& bitmap, CPoint start_point, CSize size, StretchMode stretch_mode = StretchMode::STRETCH);
    void DrawBitmap(UINT bitmap_id, CPoint start_point, CSize size, StretchMode stretch_mode = StretchMode::STRETCH);
    void DrawBitmap(HBITMAP hbitmap, CPoint start_point, CSize size, StretchMode stretch_mode = StretchMode::STRETCH, BYTE alpha = 255) override;

    void DrawIcon(HICON hIcon, CPoint start_point, CSize size) override;

    void FillRect(CRect rect, COLORREF color, BYTE alpha = 255) override; //用纯色填充矩形
    void FillRectWithBackColor(CRect rect);			//使用背景色填充矩形
    void DrawRectOutLine(CRect rect, COLORREF color, int width = 1, bool dot_line = false, BYTE alpha = 255, int radius = 0) override; //绘制矩形边框。如果dot_line为true，则为虚线

    //任务栏窗口主题背景(B:圆角面板/渐变色条;A:自定义背景图)
    void DrawThemeBackground(const CRect& rect, int corner_radius);

    void DrawLine(CPoint start_point, CPoint end_point, COLORREF color, BYTE alpha = 255) override; //使用当前画笔画线

    virtual int GetTextWidth(LPCTSTR lpszString) override;

    virtual void GetTextExtent(const wchar_t* lpszString, int& w, int& h) override;

private:
    CDC* m_pDC{};		//用于绘图的CDC类的指针
    CWnd* m_pMainWnd{};	//绘图窗口的句柄
    CFont* m_pfont{};
    COLORREF m_back_color{};
    CDrawCommonEx m_gdi_plus_drawer;

    //优化:画笔缓存。网速曲线每帧对每条线调用 DrawLine(最多每像素一条),
    //原版每次 CreatePen/DeleteObject;这里按 (颜色,样式,宽度) 复用。
    struct PenKey
    {
        COLORREF color;
        int style;
        int width;
        bool operator<(const PenKey& o) const
        {
            if (color != o.color) return color < o.color;
            if (style != o.style) return style < o.style;
            return width < o.width;
        }
    };
    std::map<PenKey, std::unique_ptr<CPen>> m_pen_cache;
    CPen* GetCachedPen(COLORREF color, int style, int width);
};

