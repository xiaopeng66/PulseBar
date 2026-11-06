#include "stdafx.h"
#include "DrawCommon.h"
#include "DrawCommonHelper.h"
#include "PulseBar.h"
#include "WindowsSettingHelper.h"
#include "TaskbarThemeGeometry.h"

CDrawCommon::CDrawCommon()
{
}

CDrawCommon::~CDrawCommon()
{
}

void CDrawCommon::Create(CDC* pDC, CWnd* pMainWnd)
{
    m_pDC = pDC;
    m_pMainWnd = pMainWnd;
    if (pMainWnd != nullptr)
        m_pfont = m_pMainWnd->GetFont();
    m_gdi_plus_drawer.Create(pDC);
}

void CDrawCommon::SetFont(CFont* pfont)
{
    m_pfont = pfont;
    m_pDC->SelectObject(m_pfont);
}

void CDrawCommon::SetDC(CDC* pDC)
{
    m_pDC = pDC;
    m_gdi_plus_drawer.Create(pDC);
}

void CDrawCommon::DrawWindowText(CRect rect, LPCTSTR lpszString, COLORREF color, Alignment align, bool draw_back_ground, bool multi_line, BYTE alpha)
{
    m_pDC->SetTextColor(color);
    if (!draw_back_ground)
        m_pDC->SetBkMode(TRANSPARENT);
    m_pDC->SelectObject(m_pfont);
    CSize text_size = m_pDC->GetTextExtent(lpszString);

    auto format = DrawCommonHelper::ProccessTextFormat(rect, text_size, align, multi_line);

    if (draw_back_ground)
        m_pDC->FillSolidRect(rect, m_back_color);
    m_pDC->DrawText(lpszString, rect, format);
}

void CDrawCommon::SetDrawRect(CRect rect)
{
    CRgn rgn;
    rgn.CreateRectRgnIndirect(rect);
    m_pDC->SelectClipRgn(&rgn);
}

void CDrawCommon::SetDrawRect(CDC* pDC, CRect rect)
{
    CRgn rgn;
    rgn.CreateRectRgnIndirect(rect);
    pDC->SelectClipRgn(&rgn);
}

void CDrawCommon::DrawBitmap(CBitmap& bitmap, CPoint start_point, CSize size, StretchMode stretch_mode)
{
    CDC memDC;

    //获取图像实际大小
    BITMAP bm;
    GetObject(bitmap, sizeof(BITMAP), &bm);

    memDC.CreateCompatibleDC(m_pDC);
    memDC.SelectObject(&bitmap);
    // 以下两行避免图片失真
    m_pDC->SetStretchBltMode(HALFTONE);
    m_pDC->SetBrushOrg(0, 0);

    DrawCommonHelper::ImageDrawAreaConvert(CSize(bm.bmWidth, bm.bmHeight), start_point, size, stretch_mode);

    m_pDC->StretchBlt(start_point.x, start_point.y, size.cx, size.cy, &memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    memDC.DeleteDC();
}

void CDrawCommon::DrawBitmap(UINT bitmap_id, CPoint start_point, CSize size, StretchMode stretch_mode)
{
    CBitmap bitmap;
    bitmap.LoadBitmap(bitmap_id);
    DrawBitmap(bitmap, start_point, size, stretch_mode);
}

void CDrawCommon::DrawBitmap(HBITMAP hbitmap, CPoint start_point, CSize size, StretchMode stretch_mode, BYTE)
{
    CBitmap bitmap;
    if (!bitmap.Attach(hbitmap))
        return;
    DrawBitmap(bitmap, start_point, size, stretch_mode);
    bitmap.Detach();
}

void CDrawCommon::DrawIcon(HICON hIcon, CPoint start_point, CSize size)
{
    if (m_pDC->GetSafeHdc() == NULL)
        return;
    if (size.cx == 0 || size.cy == 0)
        ::DrawIconEx(m_pDC->GetSafeHdc(), start_point.x, start_point.y, hIcon, 0, 0, 0, NULL, DI_NORMAL | DI_DEFAULTSIZE);
    else
        ::DrawIconEx(m_pDC->GetSafeHdc(), start_point.x, start_point.y, hIcon, size.cx, size.cy, 0, NULL, DI_NORMAL);
}

void CDrawCommon::FillRect(CRect rect, COLORREF color, BYTE alpha)
{
    m_pDC->FillSolidRect(rect, color);
}

void CDrawCommon::FillRectWithBackColor(CRect rect)
{
    m_pDC->FillSolidRect(rect, m_back_color);
}

void CDrawCommon::DrawRectOutLine(CRect rect, COLORREF color, int width, bool dot_line, BYTE alpha, int radius)
{
    if (radius > 0)
    {
        //半径大于0时绘制圆角矩形，使用GDI+绘制
        m_gdi_plus_drawer.DrawRectOutLine(rect, color, width, dot_line, alpha, radius);
    }
    else
    {
        CPen* pPen = GetCachedPen(color, (dot_line ? PS_DOT : PS_SOLID), width);
        CPen* pOldPen = m_pDC->SelectObject(pPen);
        CBrush* pOldBrush{ dynamic_cast<CBrush*>(m_pDC->SelectStockObject(NULL_BRUSH)) };

        rect.DeflateRect(width / 2, width / 2);
        m_pDC->Rectangle(rect);
        m_pDC->SelectObject(pOldPen);
        m_pDC->SelectObject(pOldBrush); // Restore the old brush
    }
}

//优化:按 (颜色,样式,宽度) 取缓存画笔,不存在才创建
CPen* CDrawCommon::GetCachedPen(COLORREF color, int style, int width)
{
    PenKey key{ color, style, width };
    auto it = m_pen_cache.find(key);
    if (it != m_pen_cache.end())
        return it->second.get();
    auto pPen = std::make_unique<CPen>();
    pPen->CreatePen(style, width, color);
    auto* ret = pPen.get();
    m_pen_cache[key] = std::move(pPen);
    return ret;
}

void CDrawCommon::DrawLine(CPoint start_point, CPoint end_point, COLORREF color, BYTE alpha)
{
    CPen* pPen = GetCachedPen(color, PS_SOLID, 1);
    CPen* pOldPen = m_pDC->SelectObject(pPen);
    CBrush* pOldBrush{dynamic_cast<CBrush*>(m_pDC->SelectStockObject(NULL_BRUSH))};

    m_pDC->MoveTo(start_point);
    m_pDC->LineTo(end_point);
    m_pDC->SelectObject(pOldPen);
    m_pDC->SelectObject(pOldBrush); // Restore the old brush
}

int CDrawCommon::GetTextWidth(LPCTSTR lpszString)
{
    return m_pDC->GetTextExtent(lpszString).cx;
}

void CDrawCommon::GetTextExtent(const wchar_t* lpszString, int& w, int& h)
{
    CSize size = m_pDC->GetTextExtent(lpszString);
    w = size.cx;
    h = size.cy;
}

//任务栏窗口主题背景:
//  主题0:不调用(色键纯色)
//  主题1:圆角深色/浅色面板(GDI+ 抗锯齿圆角矩形,不透明度可调)
//  主题2:横向渐变色条
//  背景图模式:tb_use_background_image=true 时拉伸绘制指定图片(或主窗口皮肤背景图)
//任务栏窗口主题背景(按 tb_theme 查主题表):
//  style 0=圆角纯色面板  1=对角渐变面板  2=面板+底部渐变点缀条  3=面板+描边  4=胶囊(全圆角)+左侧点缀条
//  tb_use_background_image=true 时优先拉伸绘制背景图;经典模式(tb_theme=0)不走此函数
void CDrawCommon::DrawThemeBackground(const CRect& rect, int corner_radius)
{
    if (m_pDC == nullptr)
        return;
    const auto& cfg = theApp.m_taskbar_data;
    const TaskbarThemeInfo* theme = GetActiveTaskbarTheme();
    if (theme == nullptr)
    {
        //回退:纯色填充(色键色)
        FillRect(rect, cfg.back_color);
        return;
    }

    const int width = rect.Width();
    const int height = rect.Height();
    if (width <= 0 || height <= 0)
        return;

    Gdiplus::Graphics graphics(m_pDC->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    const int radius = TaskbarThemeGeometry::ResolveCornerRadius(width, height, corner_radius, theme->style == 4);

    // ---- A:自定义背景图(缓存路径,尺寸变化拉伸;按圆角/胶囊裁剪) ----
    if (cfg.tb_use_background_image && !cfg.tb_background_image.empty())
    {
        static Gdiplus::Image* cached = nullptr;
        static std::wstring cached_path;
        if (cached == nullptr || cached_path != cfg.tb_background_image)
        {
            if (cached != nullptr) { delete cached; cached = nullptr; }
            Gdiplus::Image* img = Gdiplus::Image::FromFile(cfg.tb_background_image.c_str());
            if (img != nullptr && img->GetLastStatus() == Gdiplus::Ok)
            {
                cached = img;
                cached_path = cfg.tb_background_image;
            }
            else if (img != nullptr)
            {
                delete img;
            }
        }
        if (cached != nullptr)
        {
            Gdiplus::Rect rc(rect.left, rect.top, width, height);
            Gdiplus::Rect dst(rect.left, rect.top, width, height);
            if (radius > 0)
            {
                Gdiplus::GraphicsPath path;
                path.AddArc(rc.X, rc.Y, radius * 2, radius * 2, 180, 90);
                path.AddArc(rc.X + rc.Width - radius * 2, rc.Y, radius * 2, radius * 2, 270, 90);
                path.AddArc(rc.X + rc.Width - radius * 2, rc.Y + rc.Height - radius * 2, radius * 2, radius * 2, 0, 90);
                path.AddArc(rc.X, rc.Y + rc.Height - radius * 2, radius * 2, radius * 2, 90, 90);
                path.CloseFigure();
                graphics.SetClip(&path);
                graphics.DrawImage(cached, dst, 0, 0, cached->GetWidth(), cached->GetHeight(), Gdiplus::UnitPixel);
                graphics.ResetClip();
            }
            else
            {
                graphics.DrawImage(cached, dst, 0, 0, cached->GetWidth(), cached->GetHeight(), Gdiplus::UnitPixel);
            }
            return;
        }
    }

    const BYTE alpha = static_cast<BYTE>(TaskbarThemeGeometry::PanelAlphaFromPercent(cfg.tb_panel_opacity));
    const BYTE accent_alpha = alpha;
    const BYTE accent_fade_alpha = static_cast<BYTE>(TaskbarThemeGeometry::AccentGradientEndAlpha(alpha));
    Gdiplus::Color c1(alpha, theme->bg_r, theme->bg_g, theme->bg_b);
    Gdiplus::Rect rc(rect.left, rect.top, width, height);
    Gdiplus::GraphicsPath path;
    if (radius > 0)
    {
        path.AddArc(rc.X, rc.Y, radius * 2, radius * 2, 180, 90);
        path.AddArc(rc.X + rc.Width - radius * 2, rc.Y, radius * 2, radius * 2, 270, 90);
        path.AddArc(rc.X + rc.Width - radius * 2, rc.Y + rc.Height - radius * 2, radius * 2, radius * 2, 0, 90);
        path.AddArc(rc.X, rc.Y + rc.Height - radius * 2, radius * 2, radius * 2, 90, 90);
        path.CloseFigure();
    }

    // ---- 面板填充:纯色 / 对角渐变 ----
    if (theme->style == 1)
    {
        //渐变面板:底色加深与主题点缀色混合出第二色
        Gdiplus::Color c_end(alpha,
            (BYTE)((theme->bg_r + GetRValue(theme->accent)) / 2),
            (BYTE)((theme->bg_g + GetGValue(theme->accent)) / 2),
            (BYTE)((theme->bg_b + GetBValue(theme->accent)) / 2));
        Gdiplus::LinearGradientBrush lgb(rc, c1, c_end, Gdiplus::LinearGradientModeVertical);
        if (radius > 0)
            graphics.FillPath(&lgb, &path);
        else
            graphics.FillRectangle(&lgb, rc);
    }
    else
    {
        Gdiplus::SolidBrush brush(c1);
        if (radius > 0)
            graphics.FillPath(&brush, &path);
        else
            graphics.FillRectangle(&brush, rc);
    }

    // ---- 点缀 ----
    if (theme->style == 2)
    {
        //底部渐变点缀条
        TaskbarThemeGeometry::AccentBar accent_bar;
        if (TaskbarThemeGeometry::MakeBottomAccentBar(width, height, radius, 2, 2, accent_bar))
        {
            Gdiplus::Rect bar(rect.left + accent_bar.left, rect.top + accent_bar.top,
                accent_bar.width, accent_bar.height);
            Gdiplus::LinearGradientBrush lgb(bar,
                Gdiplus::Color(accent_alpha, GetRValue(theme->accent), GetGValue(theme->accent), GetBValue(theme->accent)),
                Gdiplus::Color(accent_fade_alpha, GetRValue(theme->accent), GetGValue(theme->accent), GetBValue(theme->accent)),
                Gdiplus::LinearGradientModeHorizontal);
            graphics.FillRectangle(&lgb, bar);
        }
    }
    else if (theme->style == 3)
    {
        //主题色描边
        Gdiplus::Pen pen(Gdiplus::Color(accent_alpha, GetRValue(theme->accent), GetGValue(theme->accent), GetBValue(theme->accent)), 1.2f);
        if (radius > 0)
            graphics.DrawPath(&pen, &path);
        else
            graphics.DrawRectangle(&pen, rc);
    }
    else if (theme->style == 4)
    {
        //胶囊左侧竖向点缀条
        const int dpi = GetDeviceCaps(m_pDC->GetSafeHdc(), LOGPIXELSX);
        const int inset = 4 * dpi / 96;
        const int bar_width = 3 * dpi / 96;
        TaskbarThemeGeometry::AccentBar accent_bar;
        if (TaskbarThemeGeometry::MakeLeftAccentBar(width, height, inset, inset, bar_width, accent_bar))
        {
            Gdiplus::Rect bar(rect.left + accent_bar.left, rect.top + accent_bar.top,
                accent_bar.width, accent_bar.height);
            Gdiplus::SolidBrush accent(Gdiplus::Color(accent_alpha, GetRValue(theme->accent), GetGValue(theme->accent), GetBValue(theme->accent)));
            graphics.FillRectangle(&accent, bar);
        }
    }
}
