#include "stdafx.h"
#include "TraySpeedIcon.h"
#include "Common.h"

CTraySpeedIcon::CTraySpeedIcon()
{
}

CTraySpeedIcon::~CTraySpeedIcon()
{
    Destroy();
}

void CTraySpeedIcon::Destroy()
{
    if (m_hIcon != nullptr)
    {
        DestroyIcon(m_hIcon);
        m_hIcon = nullptr;
    }
}

//把速度格式化为图标内的短文本:自适应单位,尽量短
static void FormatShortSpeed(unsigned __int64 speed_b, CString& str)
{
    double kb = speed_b / 1024.0;
    if (kb < 1.0)
        str = _T("0");
    else if (kb < 100.0)
        str.Format(_T("%.0fK"), kb);
    else if (kb < 1024.0)
        str.Format(_T("%dK"), static_cast<int>(kb));
    else
        str.Format(_T("%.1fM"), kb / 1024.0);
}

HICON CTraySpeedIcon::GetSpeedIcon(unsigned __int64 up_speed, unsigned __int64 down_speed, int size)
{
    if (size < 8 || size > 256)
        return nullptr;
    //缓存命中仍返回有效句柄，托盘重建不能把“未变化”当作“无图标”。
    if (m_hIcon != nullptr && up_speed == m_last_up && down_speed == m_last_down && size == m_last_size)
        return m_hIcon;

    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = size;
    bi.bV5Height = -size;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HBITMAP color_bitmap = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    if (color_bitmap == nullptr || bits == nullptr)
    {
        if (color_bitmap != nullptr)
            DeleteObject(color_bitmap);
        return nullptr;
    }
    memset(bits, 0, size * size * 4);
    HDC dc = CreateCompatibleDC(nullptr);
    if (dc == nullptr)
    {
        DeleteObject(color_bitmap);
        return nullptr;
    }
    HGDIOBJ old_bitmap = SelectObject(dc, color_bitmap);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));

    const int text_height = max(6, size / 2 - 1);
    if (m_font.GetSafeHandle() == nullptr || m_font_size != text_height)
    {
        m_font.DeleteObject();
        LOGFONT font{};
        font.lfHeight = -text_height;
        font.lfWeight = FW_BOLD;
        font.lfCharSet = DEFAULT_CHARSET;
        font.lfQuality = ANTIALIASED_QUALITY;
        _tcscpy_s(font.lfFaceName, _T("Segoe UI"));
        m_font.CreateFontIndirect(&font);
        m_font_size = text_height;
    }
    HGDIOBJ old_font = SelectObject(dc, m_font.GetSafeHandle());
    auto draw_fitted = [&](RECT rect, const CString& text) {
        HFONT scaled_font{};
        int height = text_height;
        SIZE extent{};
        GetTextExtentPoint32(dc, text, text.GetLength(), &extent);
        while (extent.cx > rect.right - rect.left && height > 5)
        {
            SelectObject(dc, m_font.GetSafeHandle());
            if (scaled_font != nullptr)
                DeleteObject(scaled_font);
            LOGFONT font{};
            m_font.GetLogFont(&font);
            height = max(5, height - 1);
            font.lfHeight = -height;
            scaled_font = CreateFontIndirect(&font);
            if (scaled_font == nullptr)
                break;
            SelectObject(dc, scaled_font);
            GetTextExtentPoint32(dc, text, text.GetLength(), &extent);
        }
        const bool drawn = DrawText(dc, text, text.GetLength(), &rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) != 0;
        SelectObject(dc, m_font.GetSafeHandle());
        if (scaled_font != nullptr)
            DeleteObject(scaled_font);
        return drawn;
    };
    CString up_text, down_text;
    FormatShortSpeed(up_speed, up_text);
    FormatShortSpeed(down_speed, down_text);
    const bool drawn = m_font.GetSafeHandle() != nullptr
        && draw_fitted({ 0, 0, size, size / 2 }, up_text)
        && draw_fitted({ 0, size / 2, size, size }, down_text);
    SelectObject(dc, old_font);
    SelectObject(dc, old_bitmap);
    DeleteDC(dc);
    GdiFlush();

    //GDI 字体只写 RGB，不写 alpha；将白字覆盖率转为预乘 ARGB，保留抗锯齿边缘。
    auto* pixels = static_cast<DWORD*>(bits);
    for (int y = 0; y < size; ++y)
    {
        const COLORREF color = y < size / 2 ? RGB(80, 200, 120) : RGB(240, 100, 100);
        for (int x = 0; x < size; ++x)
        {
            DWORD& pixel = pixels[y * size + x];
            const DWORD alpha = max(GetRValue(pixel), max(GetGValue(pixel), GetBValue(pixel)));
            pixel = (alpha << 24) | ((GetRValue(color) * alpha / 255) << 16)
                | ((GetGValue(color) * alpha / 255) << 8) | (GetBValue(color) * alpha / 255);
        }
    }
    std::vector<BYTE> mask_bits(size * ((size + 31) / 32) * 4, 0);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, mask_bits.data());
    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmColor = color_bitmap;
    info.hbmMask = mask;
    HICON icon = drawn && mask != nullptr ? CreateIconIndirect(&info) : nullptr;
    DeleteObject(color_bitmap);
    if (mask != nullptr)
        DeleteObject(mask);
    if (icon == nullptr)
        return nullptr;

    Destroy();
    m_hIcon = icon;
    m_last_up = up_speed;
    m_last_down = down_speed;
    m_last_size = size;
    return m_hIcon;
}
