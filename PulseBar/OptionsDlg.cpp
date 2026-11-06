// OptionsDlg.cpp : 设置导航与编辑草稿
#include "stdafx.h"
#include "PulseBar.h"
#include "OptionsDlg.h"


IMPLEMENT_DYNAMIC(COptionsDlg, CBaseDialog)

COptionsDlg::COptionsDlg(int tab, CWnd* pParent)
    : CBaseDialog(IDD_OPTIONS_DIALOG, pParent), m_tab_selected(tab)
{
    m_notify_icon_selected = theApp.m_cfg_data.m_notify_icon_selected;
}

COptionsDlg::~COptionsDlg()
{
}

CString COptionsDlg::GetDialogName() const
{
    return OPTION_DLG_NAME;
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CBaseDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SETTINGS_NAVIGATION, m_navigation);
    DDX_Control(pDX, IDC_SETTINGS_PAGE_TITLE, m_page_title);
}

BEGIN_MESSAGE_MAP(COptionsDlg, CBaseDialog)
    ON_WM_SIZE()
    ON_WM_DRAWITEM()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_LBN_SELCHANGE(IDC_SETTINGS_NAVIGATION, &COptionsDlg::OnNavigationChanged)
    ON_BN_CLICKED(IDC_APPLY_BUTTON, &COptionsDlg::OnBnClickedApplyButton)
END_MESSAGE_MAP()


BOOL COptionsDlg::OnInitDialog()
{
    //设置窗口是固定尺寸：不恢复配置里保存的历史尺寸，
    //否则以前拉宽过的窗口会让内容区留出大片空白。
    //窗口创建时用的是模板坐标(0,0)；初始化期间先保持隐藏，排布完成后才显示，
    //否则会先闪出一个没有内容的空边框再跳到最终位置。
    ShowWindow(SW_HIDE);
    GetWindowRect(m_min_window);
    SetRememberDlgSize(false);
    CBaseDialog::OnInitDialog();
    SetWindowPos(nullptr, 0, 0, m_min_window.Width(), m_min_window.Height(),
        SWP_NOMOVE | SWP_NOZORDER);
    CenterInWorkArea();
    SetIcon(theApp.GetMenuIcon(IDI_SETTINGS), FALSE);
    //与子页保持同一底色：子页隐藏重绘时父窗口会用该底色擦除，避免露出白色区块
    SetBackgroundColor(theApp.m_win_version.IsWindows11OrLater() ? RGB(249, 249, 249) : RGB(255, 255, 255));

    m_navigation.AddString(CCommon::LoadText(L"TXT_SETTINGS_CONTENT"));
    m_navigation.AddString(CCommon::LoadText(L"TXT_SETTINGS_APPEARANCE"));
    m_navigation.AddString(CCommon::LoadText(L"TXT_SETTINGS_INTERACTION"));
    m_navigation.AddString(CCommon::LoadText(L"TXT_SETTINGS_NETWORK_PAGE"));
    m_navigation.AddString(CCommon::LoadText(L"TXT_SETTINGS_STARTUP_PAGE"));
    m_navigation.AddString(CCommon::LoadText(L"TXT_SETTINGS_ALERTS_PAGE"));
    m_navigation.SetItemHeight(0, DPI(34));
    FontInfo nav_font;
    nav_font.name = theApp.m_str_table.GetLanguageInfo().default_font_name.c_str();
    nav_font.size = 9;
    nav_font.Create(m_nav_font, theApp.GetDpi());
    nav_font.bold = true;
    nav_font.Create(m_nav_font_bold, theApp.GetDpi());
    nav_font.size = 11;
    nav_font.Create(m_title_font, theApp.GetDpi());
    if (m_title_font.GetSafeHandle() != nullptr)
        m_page_title.SetFont(&m_title_font);

    //首屏在外壳隐藏时完成创建和布局，避免先显示空白窗口再等定时器填充。
    if (m_tab2_dlg.Create(IDD_TASKBAR_SETTINGS_DIALOG, this) == FALSE)
    {
        EndDialog(IDCANCEL);
        return TRUE;
    }
    //子页的 SetRedraw(TRUE) 可能恢复自身可见位，先隐藏再按最终尺寸排布。
    m_tab2_dlg.ShowWindow(SW_HIDE);
    m_ready = true;
    SelectPage(m_tab_selected);
    if (m_tab_selected >= 3 && !m_tab3_created)
    {
        EndDialog(IDCANCEL);
        return TRUE;
    }
    //首屏已排布完成，此时一次显示到位，不再让窗口先以空内容闪现。
    ShowWindow(SW_SHOWNORMAL);
    return TRUE;
}


void COptionsDlg::EnsureGeneralPage()
{
    if (m_tab3_created)
        return;
    //以窗口句柄为准：标记丢失但窗口已经存在时只同步标记，不能对同一个 CWnd 再 Create 一次
    if (::IsWindow(m_tab3_dlg.GetSafeHwnd()) != 0)
    {
        m_tab3_created = true;
        return;
    }
    m_tab3_dlg.m_notify_icon_selected = m_notify_icon_selected;
    //Create 失败时不能标记为已创建，否则后续 ShowSection/CollectSettings 会作用在空窗口上
    m_tab3_created = m_tab3_dlg.Create(IDD_GENERAL_SETTINGS_DIALOG, this) != FALSE;
    //子页可能恢复自身可见位；先隐藏再布局，避免按模板尺寸重绘。
    if (m_tab3_created)
        m_tab3_dlg.ShowWindow(SW_HIDE);
}

void COptionsDlg::SelectPage(int page)
{
    m_tab_selected = max(0, min(page, 5));
    if (m_tab_selected >= 3)
        EnsureGeneralPage();
    m_navigation.SetCurSel(m_tab_selected);
    CString title;
    m_navigation.GetText(m_tab_selected, title);
    m_page_title.SetWindowText(title);

    const bool want_general = m_tab_selected >= 3;
    const int want_section = want_general ? 1 : 0;
    const bool has_taskbar = ::IsWindow(m_tab2_dlg.GetSafeHwnd()) != 0;
    const bool has_general = m_tab3_created && ::IsWindow(m_tab3_dlg.GetSafeHwnd()) != 0;
    CWnd* target = want_general ? static_cast<CWnd*>(has_general ? &m_tab3_dlg : nullptr)
                                : static_cast<CWnd*>(has_taskbar ? &m_tab2_dlg : nullptr);

    //两个子页共用同一块矩形，靠显隐切换。父窗口带 WS_CLIPCHILDREN，只要有子窗口
    //盖在上面，这块区域就不会被擦除；旧页的文字因此留在父窗口表面，从新页分组框
    //内部透出来形成残影。所以切换时必须先把两页都隐藏，让父窗口同步擦掉这块区域，
    //再显示新页。只在真正换页时做，同一页重复进入（含首次打开）走廉价路径。
    const bool switching = target != nullptr && m_shown_section >= 0 && want_section != m_shown_section;
    if (switching)
    {
        if (has_taskbar)
            m_tab2_dlg.ShowWindow(SW_HIDE);
        if (has_general)
            m_tab3_dlg.ShowWindow(SW_HIDE);
        CRect area;
        target->GetWindowRect(&area);
        ScreenToClient(&area);
        RedrawWindow(area, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }

    //先排布、后显示。LayoutPages 里的 ShowSection 会走 RepaintPageSwitch，
    //而后者在窗口已可见时要「隐藏→让父窗口整片擦除→重显→重绘」整页。
    //首次打开时子页还没画过任何像素，这套擦除完全是浪费（实测约 300 ms 的空窗期）；
    //在隐藏状态下排布只会走廉价的 UpdateWindow，随后 ShowWindow 一次画完。
    LayoutPages();

    //创建失败的子页没有有效 HWND，不能对其调用 ShowWindow。
    if (has_taskbar)
        m_tab2_dlg.ShowWindow(want_general ? SW_HIDE : SW_SHOW);
    if (has_general)
        m_tab3_dlg.ShowWindow(want_general ? SW_SHOW : SW_HIDE);
    if (target != nullptr)
    {
        //换页后整页（含子控件）擦除重绘：重显只让子页自身重画，分组框标题
        //由 BUTTON 控件自己绘制，不整棵子树重绘就会缺字。
        if (switching)
            target->RedrawWindow(nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        m_shown_section = want_section;
    }
}

void COptionsDlg::LayoutPages()
{
    if (!m_ready)
        return;
    CRect rect;
    GetClientRect(rect);
    const int margin = DPI(10);
    const int nav_width = DPI(122);
    const int footer = DPI(42);
    const int header = DPI(34);
    m_navigation.MoveWindow(margin, margin, nav_width, max(DPI(100), rect.Height() - footer - margin));
    const int left = nav_width + margin * 2;
    const int width = max(DPI(340), rect.Width() - left - margin);
    //标题与内容之间留出间隔，让标题下方的分隔线可见
    const int page_top = header + DPI(8);
    const int height = max(DPI(100), rect.Height() - page_top - footer);
    m_page_title.MoveWindow(left + DPI(6), margin, width - DPI(12), DPI(22));
    m_tab2_dlg.MoveWindow(left, page_top, width, height);
    if (m_tab3_created)
        m_tab3_dlg.MoveWindow(left, page_top, width, height);
    //ShowSection 会重新锚定整页几十个控件并强制重绘；分区和尺寸都没变时跳过，
    //否则每次 OnSize（包括 Create/MoveWindow 引起的）都会把整页重排一遍。
    //延迟初始化的分区在首次显示前就绪（幂等，已就绪时直接返回）。
    if (m_tab_selected < 3)
        m_tab2_dlg.EnsureSectionReady(m_tab_selected);
    if (m_tab_selected != m_laid_section || rect.Size() != m_laid_size)
    {
        if (m_tab_selected < 3)
            m_tab2_dlg.ShowSection(m_tab_selected, height);
        else if (m_tab3_created)
            m_tab3_dlg.ShowSection(m_tab_selected - 3, height);
        m_laid_section = m_tab_selected;
        m_laid_size = rect.Size();
    }

    const int button_width = DPI(72);
    const int button_height = DPI(24);
    const int gap = DPI(6);
    const int top = rect.bottom - margin - button_height;
    GetDlgItem(IDC_APPLY_BUTTON)->MoveWindow(rect.right - margin - button_width, top, button_width, button_height);
    GetDlgItem(IDCANCEL)->MoveWindow(rect.right - margin - button_width * 2 - gap, top, button_width, button_height);
    GetDlgItem(IDOK)->MoveWindow(rect.right - margin - button_width * 3 - gap * 2, top, button_width, button_height);
}

void COptionsDlg::CenterInWorkArea()
{
    CRect rect;
    GetWindowRect(rect);
    //在光标所在显示器的工作区居中：设置面板由托盘或任务栏菜单打开，
    //光标所在显示器才是用户正在看的那块屏。
    POINT cursor{};
    ::GetCursorPos(&cursor);
    HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (::GetMonitorInfo(monitor, &info) == 0)
        return;
    const CRect work(info.rcWork);
    const int width = min(rect.Width(), work.Width());
    const int height = min(rect.Height(), work.Height());
    const int x = work.left + (work.Width() - width) / 2;
    const int y = work.top + (work.Height() - height) / 2;
    SetWindowPos(nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

bool COptionsDlg::CollectSettings()
{
    //只有初始化完成后才可读控件；失败时也不能提交不完整的草稿。
    if (!m_ready)
        return false;
    if (!m_tab2_dlg.CollectSettings())
    {
        SelectPage(1);
        return false;
    }
    if (m_tab3_created && ::IsWindow(m_tab3_dlg.GetSafeHwnd()) != 0)
    {
        m_tab3_dlg.CollectSettings();
        m_notify_icon_selected = m_tab3_dlg.m_notify_icon_selected;
    }
    return true;
}

void COptionsDlg::OnOK()
{
    if (CollectSettings())
        CBaseDialog::OnOK();
}

void COptionsDlg::OnCancel()
{
    if (m_tab3_created && ::IsWindow(m_tab3_dlg.GetSafeHwnd()) != 0)
        m_tab3_dlg.RestoreUpdateSource();
    CBaseDialog::OnCancel();
}

void COptionsDlg::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);
    if (nType != SIZE_MINIMIZED)
    {
        LayoutPages();
    }
}

void COptionsDlg::OnNavigationChanged()
{
    SelectPage(m_navigation.GetCurSel());
}

void COptionsDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT info)
{
    if (nIDCtl != IDC_SETTINGS_NAVIGATION)
    {
        CBaseDialog::OnDrawItem(nIDCtl, info);
        return;
    }
    if (info->itemID == static_cast<UINT>(-1))
        return;
    CDC dc;
    dc.Attach(info->hDC);
    CRect item(info->rcItem);
    const int index = static_cast<int>(info->itemID);
    //侧栏底色：整体浅灰，与右侧内容区区分开
    dc.FillSolidRect(item, RGB(247, 248, 250));
    CString text;
    m_navigation.GetText(info->itemID, text);
    const bool selected = (info->itemState & ODS_SELECTED) != 0;
    const bool hovered = (!selected && index == m_hover_item);
    CRect pill(item);
    pill.DeflateRect(DPI(6), DPI(3));
    if (selected)
    {
        //选中：白底卡片 + 淡蓝描边 + 左侧强调色条
        CBrush fill(RGB(255, 255, 255));
        CPen edge(PS_SOLID, 1, RGB(198, 219, 245));
        CBrush* old_brush = dc.SelectObject(&fill);
        CPen* old_pen = dc.SelectObject(&edge);
        dc.RoundRect(pill, CPoint(DPI(8), DPI(8)));
        dc.SelectObject(old_brush);
        dc.SelectObject(old_pen);
        CRect bar(pill);
        bar.right = bar.left + DPI(3);
        bar.DeflateRect(0, DPI(8));
        dc.FillSolidRect(bar, RGB(0, 103, 192));
    }
    else if (hovered)
    {
        //悬停：浅灰圆角
        CBrush fill(RGB(236, 240, 246));
        CPen edge(PS_SOLID, 1, RGB(236, 240, 246));
        CBrush* old_brush = dc.SelectObject(&fill);
        CPen* old_pen = dc.SelectObject(&edge);
        dc.RoundRect(pill, CPoint(DPI(8), DPI(8)));
        dc.SelectObject(old_brush);
        dc.SelectObject(old_pen);
    }
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(selected ? RGB(0, 84, 153) : RGB(70, 74, 82));
    CFont* font = selected && m_nav_font_bold.GetSafeHandle() != nullptr ? &m_nav_font_bold
        : (m_nav_font.GetSafeHandle() != nullptr ? &m_nav_font : nullptr);
    CFont* old_font = font != nullptr ? dc.SelectObject(font) : nullptr;
    CRect text_rect(pill);
    text_rect.left += DPI(16);
    dc.DrawText(text, text_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    if ((info->itemState & ODS_FOCUS) != 0)
        dc.DrawFocusRect(pill);
    if (old_font != nullptr)
        dc.SelectObject(old_font);
    dc.Detach();
}

void COptionsDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_navigation.GetSafeHwnd() != nullptr && ::IsWindow(m_navigation.GetSafeHwnd()))
    {
        CRect nav_rect;
        m_navigation.GetWindowRect(nav_rect);
        ScreenToClient(nav_rect);
        int hovered = -1;
        if (nav_rect.PtInRect(point))
        {
            //把鼠标位置换算到列表框客户区，再按项高确定悬停项
            CPoint list_point(point);
            list_point.Offset(-nav_rect.left, -nav_rect.top);
            CRect client;
            m_navigation.GetClientRect(client);
            if (list_point.y >= 0 && list_point.y < client.Height())
            {
                hovered = m_navigation.GetTopIndex() + list_point.y / max(1, DPI(34));
                if (hovered >= m_navigation.GetCount())
                    hovered = -1;
            }
        }
        if (hovered != m_hover_item)
        {
            m_hover_item = hovered;
            m_navigation.Invalidate(FALSE);
        }
        if (hovered >= 0)
        {
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, m_hWnd, 0 };
            ::TrackMouseEvent(&track);
        }
    }
    CBaseDialog::OnMouseMove(nFlags, point);
}

void COptionsDlg::OnMouseLeave()
{
    if (m_hover_item != -1)
    {
        m_hover_item = -1;
        if (m_navigation.GetSafeHwnd() != nullptr)
            m_navigation.Invalidate(FALSE);
    }
    CBaseDialog::OnMouseLeave();
}


void COptionsDlg::OnBnClickedApplyButton()
{
    if (!CollectSettings())
        return;
    if (theApp.m_pMainWnd != nullptr && ::IsWindow(theApp.m_pMainWnd->GetSafeHwnd()))
    {
        ::SendMessage(theApp.m_pMainWnd->GetSafeHwnd(), WM_SETTINGS_APPLIED, reinterpret_cast<WPARAM>(this), 0);
        if (m_tab3_created)
            m_tab3_dlg.OnSettingsApplied();
    }
}
