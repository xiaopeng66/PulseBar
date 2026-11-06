#include "stdafx.h"
#include "SpeedGraphTip.h"
#include "Common.h"
#include "PulseBar.h"
#include "CommonData.h"
#include <gdiplus.h>

//S3 超级悬停面板布局(逻辑像素):
//  头行18(标题+图钉) 数值行14;其下空间由两张曲线图均分(网速图略高),
//  图表内顶部留边距给图例,曲线绘制区之下不留空白
namespace
{
    const int PANEL_W = 380;
    const int HEADER_H = 18;
    const int VALUE_H = 14;
    const int VALUE_H2 = 14;      //第二行数值(CPU/GPU/内存/硬盘)
    //S6: 两张图表等高(50px),配合各 3 条均分网格线,线数与间距都一致
    const int NET_CHART_H = 50;
    const int CPU_CHART_H = 50;
    const int CHART_GAP = 4;
    const int CHART_TOP_MARGIN = 12;   //图表内顶部边距,曲线在此之下绘制,不触碰图例
    const int PAD = 4;
    const int BOTTOM_PAD = 0;
    const int PANEL_GAP = 4;

    //逻辑像素 -> 物理像素
    inline int S(int value, int dpi) { return value * dpi / 96; }

    //纵向布局:只在这里算一次,CalcWindowHeight 与 Draw 共用同一份结果。
    //历史上两处各写一套算式:窗口高度按 dpi 缩放,Draw 的纵轴没缩放,
    //96dpi 下两者恰好都等于 155 所以看不出来;120dpi 时窗口 193 而内容只到 155,
    //面板底部空出 38px 纯背景;144dpi 空 77px。
    struct PanelLayout
    {
        int header_h;       //头行高
        int y_value;        //第一行数值的顶边
        int value_h;        //第一行数值高
        int value_h2;       //第二行数值高
        int y_net;          //网速图顶边
        int net_chart_h;    //网速图高
        int chart_gap;      //两图间距
        int y_cpu;          //CPU 图顶边
        int cpu_chart_h;    //CPU 图高
        int bottom;         //内容底边 = 窗口客户区高度
    };

    PanelLayout CalcLayout(int dpi)
    {
        PanelLayout l{};
        l.header_h = S(HEADER_H, dpi);
        l.y_value = l.header_h + S(2, dpi);
        l.value_h = S(VALUE_H, dpi);
        l.value_h2 = S(VALUE_H2, dpi);
        l.y_net = l.y_value + l.value_h + S(1, dpi) + l.value_h2 + S(2, dpi);
        l.net_chart_h = S(NET_CHART_H, dpi);
        l.chart_gap = S(CHART_GAP, dpi);
        l.y_cpu = l.y_net + l.net_chart_h + l.chart_gap;
        l.cpu_chart_h = S(CPU_CHART_H, dpi);
        l.bottom = l.y_cpu + l.cpu_chart_h + S(BOTTOM_PAD, dpi);
        return l;
    }

    COLORREF PanelBackgroundColor(const TaskbarThemeInfo* theme)
    {
        return theme != nullptr ? RGB(theme->bg_r, theme->bg_g, theme->bg_b) : RGB(32, 32, 36);
    }

    COLORREF PanelLabelColor(const TaskbarThemeInfo* theme)
    {
        return theme != nullptr ? theme->label : RGB(160, 160, 165);
    }
    COLORREF PanelValueColor(const TaskbarThemeInfo* theme)
    {
        return theme != nullptr ? theme->value : RGB(235, 235, 240);
    }
    COLORREF PanelAccentColor(const TaskbarThemeInfo* theme)
    {
        return theme != nullptr ? theme->accent : RGB(80, 160, 255);
    }

    //图钉点击热区(客户区坐标;右缘留 6px,避免被窗口圆角区域裁掉)
    CRect PinRect(int client_w, int dpi)
    {
        return CRect(client_w - S(34, dpi), 0, client_w - S(6, dpi), S(HEADER_H, dpi));
    }

    //任务栏窗口所属显示器的矩形。MONITOR_DEFAULTTONEAREST:任务栏窗口被
    //window_offset_left 推到屏幕之外时(该设置允许 ±800 逻辑像素),
    //仍然取最近的那块显示器作为夹紧边界,而不是让面板跟着跑出屏幕。
    //取不到显示器信息时退化为整个虚拟桌面,行为与旧代码一致(不夹紧)。
    void GetTaskbarMonitorRect(const CRect& taskbar_rect, CRect& monitor_rect)
    {
        HMONITOR monitor = MonitorFromRect(taskbar_rect, MONITOR_DEFAULTTONEAREST);
        if (monitor != nullptr)
        {
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfo(monitor, &info))
            {
                monitor_rect = info.rcMonitor;
                return;
            }
        }
        monitor_rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        monitor_rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        monitor_rect.right = monitor_rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        monitor_rect.bottom = monitor_rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }

    //S6: 图例(色段+文字,整体靠图表右上,不占独立行)
    void DrawChartLegend(CDC& dc, const CRect& chart, const std::vector<std::pair<COLORREF, const wchar_t*>>& items,
        COLORREF text_color, int dpi)
    {
        const int sw = 10 * dpi / 96;
        const int gap = 5 * dpi / 96;
        int total = 0;
        std::vector<CSize> sizes;
        for (const auto& it : items)
        {
            CSize sz = dc.GetTextExtent(it.second);
            sizes.push_back(sz);
            total += sw + gap + sz.cx + gap;
        }
        int x = chart.right - total;
        //两项图例文案是写死的字面量(各 3 字),当前面板宽度下 x 恒为正
        //(96dpi 实测 282/304,需每项 ≥16 个 CJK 字才会变负)。
        //夹到图表左缘是零成本的兜底:未来改文案或缩窄面板时图例不会画出面板外
        if (x < chart.left) x = chart.left;
        int cy = chart.top + 7 * dpi / 96;
        dc.SetTextColor(text_color);
        for (size_t i = 0; i < items.size(); i++)
        {
            CPen pen(PS_SOLID, 2, items[i].first);
            CPen* old_pen = dc.SelectObject(&pen);
            dc.MoveTo(x, cy);
            dc.LineTo(x + sw, cy);
            dc.SelectObject(old_pen);
            CRect rc_txt(x + sw + gap, cy - sizes[i].cy / 2 - 1, x + sw + gap + sizes[i].cx + 2, cy + sizes[i].cy / 2 + 1);
            dc.DrawText(items[i].second, rc_txt, DT_LEFT | DT_SINGLELINE);
            x += sw + gap + sizes[i].cx + gap;
        }
    }
}

CSpeedGraphTip::CSpeedGraphTip()
{
}

CSpeedGraphTip& CSpeedGraphTip::Instance()
{
    static CSpeedGraphTip instance;
    return instance;
}

void CSpeedGraphTip::AddSample(unsigned __int64 up_speed, unsigned __int64 down_speed, int cpu_usage, int gpu_usage)
{
    //环形覆盖:移动数组,新样本在末尾(绘制时从左旧到右新)
    for (int i = 0; i < SAMPLE_COUNT - 1; i++)
    {
        m_up[i] = m_up[i + 1];
        m_down[i] = m_down[i + 1];
        m_cpu[i] = m_cpu[i + 1];
        m_gpu[i] = m_gpu[i + 1];
    }
    m_up[SAMPLE_COUNT - 1] = up_speed;
    m_down[SAMPLE_COUNT - 1] = down_speed;
    m_cpu[SAMPLE_COUNT - 1] = max(min(cpu_usage, 100), 0);
    m_gpu[SAMPLE_COUNT - 1] = max(min(gpu_usage, 100), 0);

    //Y 轴最大值:取 60 个样本的峰值(上传/下载两条曲线取大)放大 1.25 倍留顶部空间,
    //再向上取整到 1KB 的倍数。下限 1024 经下面的 +1 步进后为 2048,保证非零避免除零
    unsigned __int64 max_v = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        if (m_up[i] > max_v) max_v = m_up[i];
        if (m_down[i] > max_v) max_v = m_down[i];
    }
    m_max_speed = max((unsigned long long)(max_v * 1.25), (unsigned long long)1024);
    m_max_speed = ((m_max_speed / 1024) + 1) * 1024;

    //面板可见时每秒重绘(钉住后鼠标已移开,靠这里保持内容实时)
    if (m_hWnd != nullptr && IsWindowVisible(m_hWnd))
        InvalidateRect(m_hWnd, nullptr, FALSE);
}

void CSpeedGraphTip::CreateWindowIfNeeded()
{
    if (m_hWnd != nullptr)
        return;
    WNDCLASSEX wc{ sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"TMLiteSpeedGraphTip";
    wc.hbrBackground = nullptr;
    RegisterClassEx(&wc);

    m_hWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP,
        0, 0, 10, 10, nullptr, nullptr, wc.hInstance, this);
}

int CSpeedGraphTip::CalcWindowHeight(int dpi) const
{
    //S6: 布局只含 头行+数值行+两张曲线图(曲线下方不再有文字区)
    //与 Draw 共用 CalcLayout,窗口高度必然等于内容底边
    return CalcLayout(dpi).bottom;
}

void CSpeedGraphTip::Show(const CPoint& cursor_pos, const CRect& taskbar_wnd_rect, UINT dpi)
{
    if (m_pinned && IsVisible() && dpi == m_dpi)
        return;     //钉住期间不移动；DPI 变化时仍需重设大小并刷新首帧

    CreateWindowIfNeeded();
    if (m_hWnd == nullptr || !IsWindow(m_hWnd))
        return;

    const bool was_visible = IsVisible();
    const bool dpi_changed = m_dpi != dpi;
    m_dpi = dpi;
    int w = S(PANEL_W, static_cast<int>(m_dpi));
    int h = CalcWindowHeight(m_dpi);
    const int gap = S(PANEL_GAP, static_cast<int>(m_dpi));

    //放置:优先贴在任务栏窗口上方,放不下才翻到下方;水平/垂直都夹紧在
    //任务栏所属显示器内。历史上只判断 y<0(虚拟桌面原点)且不夹紧水平方向:
    //window_offset_left 允许 ±800 逻辑像素,任务栏窗口被推到屏幕右缘时
    //面板会整块跑出显示器(实测右侧溢出 518px,见 docs 15.11)。
    //取不到显示器信息时 helper 退化为整个虚拟桌面(不夹紧),行为与旧版一致
    CRect monitor_rect;
    GetTaskbarMonitorRect(taskbar_wnd_rect, monitor_rect);

    int x = cursor_pos.x - w / 2;
    if (x + w > monitor_rect.right)
        x = monitor_rect.right - w;
    if (x < monitor_rect.left)
        x = monitor_rect.left;      //显示器比面板还窄时贴左边,不越界

    int y = taskbar_wnd_rect.top - h - gap;
    if (y < monitor_rect.top)
        y = taskbar_wnd_rect.bottom + gap;
    if (y + h > monitor_rect.bottom)
        y = monitor_rect.bottom - h;
    if (y < monitor_rect.top)
        y = monitor_rect.top;       //显示器比面板还矮时贴顶边,不越界

    CRect current;
    const bool have_rect = ::GetWindowRect(m_hWnd, &current) != FALSE;
    const CRect target(x, y, x + w, y + h);
    const bool size_changed = !have_rect || current.Width() != w || current.Height() != h;
    if (!was_visible || !have_rect || current != target)
        SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (!was_visible || size_changed || dpi_changed)
    {
        //只有尺寸/DPI 改变时重建 region；鼠标移动不应每次分配 GDI 区域并整帧重画。
        int rgn_r = max(2, 6 * static_cast<int>(dpi) / 96);
        CRgn rgn;
        rgn.CreateRoundRectRgn(0, 0, w + 1, h + 1, rgn_r * 2, rgn_r * 2);
        SetWindowRgn(m_hWnd, (HRGN)rgn.Detach(), FALSE);
        InvalidateRect(m_hWnd, nullptr, FALSE);
        if (!was_visible)
            ::UpdateWindow(m_hWnd);
    }
}

bool CSpeedGraphTip::IsCursorInHoverRegion(const CPoint& cursor_pos, const CRect& taskbar_wnd_rect) const
{
    if (taskbar_wnd_rect.PtInRect(cursor_pos))
        return true;
    if (!IsVisible())
        return false;

    CRect panel_rect;
    if (!::GetWindowRect(m_hWnd, &panel_rect))
        return false;
    if (panel_rect.PtInRect(cursor_pos))
        return true;

    //仅连接面板与任务栏窗口相邻的边，避免用两者的巨大包围盒误判悬停。
    const int horizontal_padding = max(1, 8 * static_cast<int>(m_dpi) / 96);
    //间隙必须与 Show 中实际放置用的间距同源,否则高 DPI 下桥接区盖不住真实空隙
    const int gap = max(1, S(PANEL_GAP, static_cast<int>(m_dpi)));
    CRect bridge_rect;
    bridge_rect.left = max(taskbar_wnd_rect.left, panel_rect.left) - horizontal_padding;
    bridge_rect.right = min(taskbar_wnd_rect.right, panel_rect.right) + horizontal_padding;
    if (bridge_rect.left >= bridge_rect.right)
        return false;

    if (taskbar_wnd_rect.top - panel_rect.bottom >= 0
        && taskbar_wnd_rect.top - panel_rect.bottom <= gap)
    {
        bridge_rect.top = panel_rect.bottom;
        bridge_rect.bottom = taskbar_wnd_rect.top;
    }
    else if (panel_rect.top - taskbar_wnd_rect.bottom >= 0
        && panel_rect.top - taskbar_wnd_rect.bottom <= gap)
    {
        bridge_rect.top = taskbar_wnd_rect.bottom;
        bridge_rect.bottom = panel_rect.top;
    }
    else
    {
        return false;
    }

    return bridge_rect.PtInRect(cursor_pos);
}

void CSpeedGraphTip::Hide()
{
    if (m_pinned)
        return;     //钉住时不因鼠标移开而隐藏
    if (m_hWnd != nullptr && IsWindowVisible(m_hWnd))
        ShowWindow(m_hWnd, SW_HIDE);
}

void CSpeedGraphTip::ForceHide()
{
    m_pinned = false;
    Hide();
}

void CSpeedGraphTip::Refresh()
{
    if (m_hWnd != nullptr && IsWindow(m_hWnd))
        InvalidateRect(m_hWnd, nullptr, FALSE);
}

void CSpeedGraphTip::OnDpiChanged(UINT dpi, const CRect& taskbar_wnd_rect)
{
    if (dpi == 0)
        return;

    if (!IsVisible())
    {
        m_dpi = dpi;
        return;
    }

    CRect panel_rect;
    CPoint anchor = taskbar_wnd_rect.CenterPoint();
    if (::GetWindowRect(m_hWnd, &panel_rect))
        anchor = panel_rect.CenterPoint();
    Show(anchor, taskbar_wnd_rect, dpi);
}

void CSpeedGraphTip::Draw(CDC& dc, const CRect& rect)
{
    int dpi = m_dpi;    //与 Show/命中检测使用同一 DPI,窗口 DC 在进程非 DPI-aware 时返回虚化值
    //必须用 GetActiveTaskbarTheme:内置表只有 id 1..8,tb_theme==-1(自定义主题)时
    //FindTaskbarTheme 返回 nullptr,面板会退回写死的深灰配色,和紧贴的任务栏配色不一致
    //(实测 T2 底色 30,30,34 而非自定义色,见 docs 15.12)。
    const TaskbarThemeInfo* theme = GetActiveTaskbarTheme();
    COLORREF bg = PanelBackgroundColor(theme);
    COLORREF label_c = PanelLabelColor(theme);
    COLORREF value_c = PanelValueColor(theme);
    COLORREF accent = PanelAccentColor(theme);

    dc.SetBkMode(TRANSPARENT);

    // ---- 背景:圆角面板(GDI+ 抗锯齿) ----
    {
        Gdiplus::Graphics graphics(dc.GetSafeHdc());
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Rect rc(rect.left, rect.top, rect.Width() - 1, rect.Height() - 1);
        int r = 6 * dpi / 96;
        if (r < 2) r = 2;
        Gdiplus::GraphicsPath path;
        path.AddArc(rc.X, rc.Y, r * 2, r * 2, 180, 90);
        path.AddArc(rc.X + rc.Width - r * 2, rc.Y, r * 2, r * 2, 270, 90);
        path.AddArc(rc.X + rc.Width - r * 2, rc.Y + rc.Height - r * 2, r * 2, r * 2, 0, 90);
        path.AddArc(rc.X, rc.Y + rc.Height - r * 2, r * 2, r * 2, 90, 90);
        path.CloseFigure();
        Gdiplus::SolidBrush bg_brush(Gdiplus::Color(242, GetRValue(bg), GetGValue(bg), GetBValue(bg)));
        graphics.FillPath(&bg_brush, &path);
        Gdiplus::Pen border(Gdiplus::Color(180, GetRValue(accent), GetGValue(accent), GetBValue(accent)), 1.0f);
        graphics.DrawPath(&border, &path);
    }

    // ---- 字体 ----
    CFont font;
    font.CreateFont(-9 * dpi / 96, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Microsoft YaHei UI"));
    CFont* old_font = dc.SelectObject(&font);

    int w = rect.Width();
    CString text;

    //S6: 布局纵轴取自 CalcLayout(与 CalcWindowHeight 同一份算式)
    const PanelLayout layout = CalcLayout(dpi);
    const int y_value = layout.y_value;
    const int y_net = layout.y_net;
    const int y_cpu = layout.y_cpu;

    // ---- 头行:标题 + 图钉 ----
    {
        CRect rc_head(PAD * dpi / 96, S(2, dpi), w - S(30, dpi), layout.header_h);
        dc.SetTextColor(label_c);
        text = _T("监控面板");
        dc.DrawText(text, rc_head, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        //图钉:钉住=实心圆+针脚,未钉=空心圆
        CRect pin = PinRect(w, dpi);
        CPoint center((pin.left + pin.right) / 2, layout.header_h / 2);
        int rad = 4 * dpi / 96;
        CBrush br(accent);
        CPen pen(PS_SOLID, 1, accent);
        CPen* old_pen = dc.SelectObject(&pen);
        if (m_pinned)
        {
            CBrush* old_br = dc.SelectObject(&br);
            dc.Ellipse(center.x - rad, center.y - rad, center.x + rad, center.y + rad);
            dc.SelectObject(old_br);
            //针脚
            dc.MoveTo(center.x, center.y + rad);
            dc.LineTo(center.x, center.y + rad + 3 * dpi / 96);
        }
        else
        {
            CBrush* old_br = dc.SelectObject(CBrush::FromHandle((HBRUSH)GetStockObject(NULL_BRUSH)));
            dc.Ellipse(center.x - rad, center.y - rad, center.x + rad, center.y + rad);
            dc.SelectObject(old_br);
        }
        dc.SelectObject(old_pen);
    }

    // ---- 数值行(两行:第一行网速,第二行 CPU/GPU/内存/硬盘;无效数据自动省略) ----
    {
        const int x0 = PAD * dpi / 96;
        const int x1 = w - PAD * dpi / 96;
        dc.SetTextColor(value_c);

        //第一行:上下行网速
        CRect rc_val(x0, y_value, x1, y_value + layout.value_h);
        text.Format(_T("↑%s/s  ↓%s/s"),
            CCommon::DataSizeToString(m_up[SAMPLE_COUNT - 1], false).GetString(),
            CCommon::DataSizeToString(m_down[SAMPLE_COUNT - 1], false).GetString());
        CSize sz = dc.GetTextExtent(text);
        if (sz.cx > rc_val.Width())
            text = text.Left(max(1, (int)(text.GetLength() * rc_val.Width() / sz.cx) - 1)) + _T("…");
        dc.DrawText(text, rc_val, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        //第二行:CPU/GPU/内存/硬盘 + GPU温度/功耗
        CRect rc_val2(x0, y_value + layout.value_h + S(1, dpi), x1, y_value + layout.value_h + S(1, dpi) + layout.value_h2);
        text.Format(_T("CPU %d%%  GPU %d%%"), m_cpu[SAMPLE_COUNT - 1], m_gpu[SAMPLE_COUNT - 1]);
        if (theApp.m_memory_usage >= 0)
            text.AppendFormat(_T("  内存 %d%%"), theApp.m_memory_usage);
        if (theApp.m_hdd_usage >= 0)
            text.AppendFormat(_T("  硬盘 %d%%"), theApp.m_hdd_usage);
        if (theApp.m_gpu_temperature > 0)
            text.AppendFormat(_T("  %d°C"), (int)theApp.m_gpu_temperature);
        if (theApp.m_gpu_power_w > 0)
            text.AppendFormat(_T(" · %dW"), theApp.m_gpu_power_w);
        sz = dc.GetTextExtent(text);
        if (sz.cx > rc_val2.Width())
            text = text.Left(max(1, (int)(text.GetLength() * rc_val2.Width() / sz.cx) - 1)) + _T("…");
        dc.DrawText(text, rc_val2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // ---- 网速曲线(红=下行 绿=上行,填充面积图) ----
    {
        CRect chart(PAD * dpi / 96, y_net,
            w - PAD * dpi / 96, y_net + layout.net_chart_h);
        //图例在图表顶部,曲线绘制区下移 CHART_TOP_MARGIN 避免触碰
        CRect curve(chart.left, chart.top + CHART_TOP_MARGIN * dpi / 96,
            chart.right, chart.bottom);
        {
            Gdiplus::Graphics graphics(dc.GetSafeHdc());
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            //S6: 网格线固定 3 条均分整个图表区,两张图线数一致
            Gdiplus::Pen grid(Gdiplus::Color(70, 128, 128, 132), 1.0f);
            for (int i = 1; i <= 3; i++)
            {
                int gy = chart.top + chart.Height() * i / 4;
                graphics.DrawLine(&grid, chart.left, gy, chart.right, gy);
            }
            //S6: 先画所有填充再画所有描边,确保线条始终在填充之上不被遮挡
            //S6: 曲线峰值上限 = 曲线区高度的 80%,顶部 20% 做安全缓冲不触碰图例
            int usable_h = curve.Height() * 80 / 100;
            struct Series { unsigned __int64* vals; COLORREF color; int alpha; };
            Series series[] = {
                { m_down, RGB(240, 100, 100), 60 },
                { m_up, RGB(80, 200, 120), 60 },
            };
            Gdiplus::Point pts[SAMPLE_COUNT + 2];
            auto build_pts = [&](unsigned __int64* vals)
            {
                for (int i = 0; i < SAMPLE_COUNT; i++)
                {
                    pts[i].X = curve.left + (int)((__int64)curve.Width() * i / (SAMPLE_COUNT - 1));
                    int ph = (int)min((__int64)usable_h, (__int64)((double)vals[i] / m_max_speed * usable_h));
                    pts[i].Y = curve.bottom - ph;
                }
                pts[SAMPLE_COUNT].X = curve.right; pts[SAMPLE_COUNT].Y = curve.bottom;
                pts[SAMPLE_COUNT + 1].X = curve.left; pts[SAMPLE_COUNT + 1].Y = curve.bottom;
            };
            //pass 1: 填充
            for (auto& s : series)
            {
                build_pts(s.vals);
                Gdiplus::SolidBrush fill(Gdiplus::Color(s.alpha, GetRValue(s.color), GetGValue(s.color), GetBValue(s.color)));
                graphics.FillPolygon(&fill, pts, SAMPLE_COUNT + 2);
            }
            //pass 2: 描边
            for (auto& s : series)
            {
                build_pts(s.vals);
                Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(s.color), GetGValue(s.color), GetBValue(s.color)), 1.4f);
                graphics.DrawLines(&pen, pts, SAMPLE_COUNT);
            }
        }
        //S6: 图例(右上角色段+文字,不占独立行):↓下载=红 ↑上传=绿
        DrawChartLegend(dc, chart, { { RGB(240, 100, 100), _T("↓下载") }, { RGB(80, 200, 120), _T("↑上传") } }, label_c, dpi);
    }

    // ---- CPU/GPU 曲线(0-100%,填充面积图) ----
    {
        CRect chart(PAD * dpi / 96, y_cpu,
            w - PAD * dpi / 96, y_cpu + layout.cpu_chart_h);
        CRect curve(chart.left, chart.top + CHART_TOP_MARGIN * dpi / 96,
            chart.right, chart.bottom);
        {
            Gdiplus::Graphics graphics(dc.GetSafeHdc());
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            //S6: 网格线固定 3 条均分整个图表区,两张图线数一致
            Gdiplus::Pen grid(Gdiplus::Color(70, 128, 128, 132), 1.0f);
            for (int i = 1; i <= 3; i++)
            {
                int gy = chart.top + chart.Height() * i / 4;
                graphics.DrawLine(&grid, chart.left, gy, chart.right, gy);
            }
            //S6: 先填充再描边,线条不被填充遮挡;峰值上限 80% 不触碰图例
            int usable_h = curve.Height() * 80 / 100;
            struct Series { int* vals; COLORREF color; int alpha; };
            Series series[] = {
                { m_cpu, RGB(79, 195, 247), 60 },
                { m_gpu, RGB(255, 183, 77), 60 },
            };
            Gdiplus::Point pts[SAMPLE_COUNT + 2];
            auto build_pts = [&](int* vals)
            {
                for (int i = 0; i < SAMPLE_COUNT; i++)
                {
                    pts[i].X = curve.left + (int)((__int64)curve.Width() * i / (SAMPLE_COUNT - 1));
                    const int percent = max(0, min(100, vals[i]));
                    pts[i].Y = curve.bottom - usable_h * percent / 100;
                }
                pts[SAMPLE_COUNT].X = curve.right; pts[SAMPLE_COUNT].Y = curve.bottom;
                pts[SAMPLE_COUNT + 1].X = curve.left; pts[SAMPLE_COUNT + 1].Y = curve.bottom;
            };
            for (auto& s : series)
            {
                build_pts(s.vals);
                Gdiplus::SolidBrush fill(Gdiplus::Color(s.alpha, GetRValue(s.color), GetGValue(s.color), GetBValue(s.color)));
                graphics.FillPolygon(&fill, pts, SAMPLE_COUNT + 2);
            }
            for (auto& s : series)
            {
                build_pts(s.vals);
                Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(s.color), GetGValue(s.color), GetBValue(s.color)), 1.4f);
                graphics.DrawLines(&pen, pts, SAMPLE_COUNT);
            }
        }
        //S6: 图例:CPU=青 GPU=橙
        DrawChartLegend(dc, chart, { { RGB(79, 195, 247), _T("CPU") }, { RGB(255, 183, 77), _T("GPU") } }, label_c, dpi);
    }

    //S6: 曲线下方的文字区(今日流量/GPU状态/进程列表)按用户要求全部移除;
    //GPU 温度功耗并入数值行尾部,进程列表不再展示

    dc.SelectObject(old_font);
}

LRESULT CSpeedGraphTip::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    //静态 WndProc 转发到单例(窗口由单例创建,lp 在 WM_NCCREATE 中带 this)
    static CSpeedGraphTip* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        self = (CSpeedGraphTip*)((CREATESTRUCT*)lp)->lpCreateParams;
        return TRUE;
    }
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        CRect rc;
        GetClientRect(hwnd, &rc);
        if (rc.IsRectEmpty())
        {
            EndPaint(hwnd, &ps);
            return 0;
        }
        //S3: 双缓冲绘制——先画进内存位图再一次性 BitBlt,
        //消除逐段绘制时 DWM 合成到中间帧导致的文字闪烁
        CDC dcWrap;
        dcWrap.Attach(dc);
        CDC mem_dc;
        mem_dc.CreateCompatibleDC(&dcWrap);
        CBitmap mem_bmp;
        mem_bmp.CreateCompatibleBitmap(&dcWrap, rc.Width(), rc.Height());
        CBitmap* old_bmp = mem_dc.SelectObject(&mem_bmp);
        //CreateCompatibleBitmap 的首帧内容未定义；先用当前主题稳定清屏，
        //否则 Draw 的圆角路径外会把未初始化的黑/白像素提交到 DWM。
        {
            Gdiplus::Graphics buffer_graphics(mem_dc.GetSafeHdc());
            const COLORREF buffer_bg = PanelBackgroundColor(GetActiveTaskbarTheme());
            buffer_graphics.Clear(Gdiplus::Color(255, GetRValue(buffer_bg), GetGValue(buffer_bg), GetBValue(buffer_bg)));
        }
        if (self != nullptr)
        {
            self->Draw(mem_dc, rc);
            dcWrap.BitBlt(0, 0, rc.Width(), rc.Height(), &mem_dc, 0, 0, SRCCOPY);
        }
        mem_dc.SelectObject(old_bmp);
        dcWrap.Detach();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONUP:
        //点击图钉热区:切换钉住状态;点击面板其他区域:未钉住时隐藏
        if (self != nullptr)
        {
            //WM_LBUTTONUP 的 lParam 已经是窗口客户区坐标。
            CPoint pt((short)LOWORD(lp), (short)HIWORD(lp));
            CRect pin = PinRect(PANEL_W * self->m_dpi / 96, static_cast<int>(self->m_dpi));
            if (pin.PtInRect(pt))
            {
                self->m_pinned = !self->m_pinned;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else if (!self->m_pinned)
            {
                self->Hide();
            }
        }
        return 0;
    case WM_RBUTTONUP:
        //右键:解除钉住并隐藏
        if (self != nullptr)
            self->ForceHide();
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}
