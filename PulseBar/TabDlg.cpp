#include "stdafx.h"
#include "TabDlg.h"
#include "PulseBar.h"


IMPLEMENT_DYNAMIC(CTabDlg, CBaseDialog)

CTabDlg::CTabDlg(UINT nIDTemplate, CWnd * pParent) : CBaseDialog(nIDTemplate, pParent)
{
    m_pParent = pParent;
}


CTabDlg::~CTabDlg()
{
}


BOOL CTabDlg::PreTranslateMessage(MSG* pMsg)
{
    // TODO: 在此添加专用代码和/或调用基类
    //由于这是tab标签中的子对话框，因此用户按回车或ESC后不应该响应当前对话框的IDOK或IDCANCEL，
    //而应该响应tab标签的父窗口的IDOK或IDCANCEL命令
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
    {
        if (m_pParent != nullptr)
            m_pParent->SendMessage(WM_COMMAND, IDCANCEL);
        return TRUE;
    }
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        if (m_pParent != nullptr)
            m_pParent->SendMessage(WM_COMMAND, IDOK);
        return TRUE;
    }

    return CBaseDialog::PreTranslateMessage(pMsg);
}


BOOL CTabDlg::OnInitDialog()
{
    CBaseDialog::OnInitDialog();

    //分组框不填充内部，不能让 WS_CLIPCHILDREN 将整块背景排除在擦除区域之外。
    for (CWnd* ctrl = GetWindow(GW_CHILD); ctrl != nullptr; ctrl = ctrl->GetWindow(GW_HWNDNEXT))
    {
        wchar_t name[32]{};
        ::GetClassName(ctrl->GetSafeHwnd(), name, 32);
        if (_wcsicmp(name, L"Button") == 0 && (ctrl->GetStyle() & BS_TYPEMASK) == BS_GROUPBOX)
            ctrl->ModifyStyleEx(0, WS_EX_TRANSPARENT, SWP_FRAMECHANGED);
    }

    // TODO:  在此添加额外的初始化
    //将窗口背景设置成白色
    if (theApp.m_win_version.IsWindows11OrLater())
        SetBackgroundColor(RGB(249, 249, 249));
    else
        SetBackgroundColor(RGB(255, 255, 255));

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}

CWnd* CTabDlg::GetParentWindow()
{
    CWnd* pParent = GetParent();
    if (pParent != nullptr)
    {
        return pParent->GetParent();
    }
    return nullptr;
}

void CTabDlg::SetScrollbarInfo(int nPage, int nMax)
{
    nPage = max(1, nPage);
    nMax = max(nPage, nMax);
    const int old_pos = m_last_pos;
    m_last_pos = max(0, min(m_last_pos, nMax - nPage));
    m_scroll_enable = true;

    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMax = nMax - 1;
    info.nPage = nPage;
    info.nPos = m_last_pos;
    SetScrollInfo(SB_VERT, &info, TRUE);

    //按“旧位置 -> 新位置”的净位移一次性移动子控件。
    //拆成两次相反方向的移动时，若新位置被夹紧，两次位移量不相等，
    //控件会永久偏移并在每次尺寸变化时累积，造成控件互相重叠。
    CBaseDialog::ResizeDynamicLayout();
    MoveChildControls(old_pos - m_last_pos);
    SetControlMouseWheelEnable(!IsScrollBarVisible());
}

void CTabDlg::RepaintAfterMove()
{
    //子窗口移动后擦除旧位置并重绘。
    //注意：不能在这里隐藏/重新显示整页——滚动时每一格都触发一次，
    //会造成明显闪烁和抖动（旧版用 ScrollWindow 位块搬运，所以没有这个问题）。
    if (!IsWindowVisible())
    {
        UpdateWindow();
        return;
    }
    CRect client;
    GetClientRect(client);
    InvalidateRect(client, TRUE);     //连同背景一起擦除
    UpdateWindow();
}

int CTabDlg::EditControlHeight()
{
    //单行 EDIT 的文字总是画在顶部：框比文字越高，下方空隙越大，看起来就不居中。
    //按当前字体高度计算，只留很小的上下余量。
    //同一 DPI 下结果不变：缓存住，避免每次重排都为每个编辑框做一遍字体测量。
    if (m_edit_height > 0)
        return m_edit_height;
    CFont* font = GetFont();
    if (font == nullptr || font->GetSafeHandle() == nullptr)
        return DPI(22);
    LOGFONT lf{};
    font->GetLogFont(&lf);
    CClientDC dc(this);
    CFont* old = dc.SelectObject(font);
    TEXTMETRIC tm{};
    dc.GetTextMetrics(&tm);
    dc.SelectObject(old);
    (void)lf;
    m_edit_height = tm.tmHeight + DPI(6);
    return m_edit_height;
}

void CTabDlg::RepaintPageSwitch()
{
    //切换页面时控件集合变化很大：先隐藏本页让父窗口擦干净，再显示重绘，
    //这样上一页控件的像素不会残留。只在切页时做一次，不影响滚动流畅度。
    if (!IsWindowVisible())
    {
        UpdateWindow();
        return;
    }
    CWnd* parent = GetParent();
    ShowWindow(SW_HIDE);
    if (parent != nullptr && parent->GetSafeHwnd() != nullptr)
    {
        parent->Invalidate();
        parent->UpdateWindow();
    }
    ShowWindow(SW_SHOW);
    UpdateWindow();
}

void CTabDlg::MoveChildControls(int offset)
{
    if (offset == 0)
        return;
    //用系统滚动：一次位块搬运同时移动子窗口，并擦除/重绘暴露出来的区域。
    //逐控件移动 + 整页重绘会闪烁；而只搬像素不擦除会留下旧边框和旧文字。
    //SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE 同时解决这两个问题。
    ::ScrollWindowEx(m_hWnd, 0, offset, nullptr, nullptr, nullptr, nullptr,
        SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    UpdateWindow();
}

void CTabDlg::ScrollWindowSimple(int step, bool absolute)
{
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    GetScrollInfo(SB_VERT, &info, SIF_ALL);
    const int max_pos = max(0, info.nMax - static_cast<int>(info.nPage) + 1);
    int next_pos;
    if (absolute)
        next_pos = step < 0 ? max_pos : step;
    else
    {
        if (step == INT_MAX)
            step = static_cast<int>(info.nPage);
        else if (step == INT_MIN)
            step = -static_cast<int>(info.nPage);
        next_pos = m_last_pos - step;
    }
    next_pos = max(0, min(next_pos, max_pos));
    const int offset = m_last_pos - next_pos;
    m_last_pos = next_pos;
    SetScrollPos(SB_VERT, next_pos, TRUE);
    MoveChildControls(offset);
}

void CTabDlg::EnableDlgCtrl(UINT id, bool enable)
{
    CWnd* pCtrl = GetDlgItem(id);
    if (pCtrl != nullptr)
        pCtrl->EnableWindow(enable);
}

void CTabDlg::ShowDlgCtrl(UINT id, bool show)
{
    CWnd* pCtrl = GetDlgItem(id);
    if (pCtrl == nullptr || !::IsWindow(pCtrl->GetSafeHwnd()))
        return;

    CRect rect;
    pCtrl->GetWindowRect(&rect);
    ScreenToClient(&rect);
    pCtrl->ShowWindow(show ? SW_SHOW : SW_HIDE);
    //显隐动态控件后，父窗口必须擦除原矩形并立即重绘；仅 ShowWindow
    //会把分组框/编辑框的旧像素留在 DWM 表面，切换分区时尤其明显。
    RedrawWindow(&rect, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

BEGIN_MESSAGE_MAP(CTabDlg, CBaseDialog)
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
    ON_WM_SIZE()
END_MESSAGE_MAP()


void CTabDlg::ResizeDynamicLayout()
{
    // m_pDynamicLayout->Adjust()会无视窗口滚动更新控件位置
    // 之前在父窗口OnSize后重置不可靠（进入退出最大化和贴靠时没能正常工作），也没找到检出这两种特殊OnSize的方法
    // 框架内部调用ResizeDynamicLayout太早，来不及先复位滚动，故覆写此虚方法介入

    // 如果已启用滚动那么此处不调用基类使框架的处理无效，改为在父窗口的OnSize后自行调用
    if (!m_scroll_enable)
        CBaseDialog::ResizeDynamicLayout();
}

void CTabDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    //参考资料：https://www.cnblogs.com/ranjiewen/p/6013922.html

    if (m_scroll_enable)
    {
        int step = theApp.DPI(16);
        switch (nSBCode)
        {
        case SB_LINEUP:                         //Scroll one line up
            ScrollWindowSimple(step);
            break;
        case SB_LINEDOWN:                       //Scroll one line down
            ScrollWindowSimple(-step);
            break;
        case SB_PAGEUP:                         //Scroll one page up.
            ScrollWindowSimple(INT_MAX);
            break;
        case SB_PAGEDOWN:                       //Scroll one page down
            ScrollWindowSimple(INT_MIN);
            break;
        case SB_THUMBPOSITION:                  //Scroll to the absolute position. The current position is provided in nPos
            break;
        case SB_THUMBTRACK:                     //Drag scroll box to specified position. The current position is provided in nPos
            ScrollWindowSimple(nPos, true);
            break;
        case SB_TOP:                            // 顶部
            ScrollWindowSimple(0, true);
            break;
        case SB_BOTTOM:                         // 底部
            ScrollWindowSimple(-1, true);
            break;
        case SB_ENDSCROLL:                      //End scroll
            break;
        default:
            break;
        }
    }
    CBaseDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}


BOOL CTabDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if (m_scroll_enable)
    {
        int step = theApp.DPI(64) * zDelta / 120;
        ScrollWindowSimple(step);
        SCROLLINFO scrollinfo;
        GetScrollInfo(SB_VERT, &scrollinfo, SIF_ALL);
        m_last_pos = scrollinfo.nPos;
    }

    return CBaseDialog::OnMouseWheel(nFlags, zDelta, pt);
}


void CTabDlg::OnOK()
{
    // TODO: 在此添加专用代码和/或调用基类

    CBaseDialog::OnOK();
}


bool CTabDlg::IsScrollBarVisible()
{
    SCROLLINFO scrollinfo{};
    scrollinfo.cbSize = sizeof(scrollinfo);
    GetScrollInfo(SB_VERT, &scrollinfo, SIF_ALL);
    bool visible = scrollinfo.nPage < scrollinfo.nMax;
    return visible;
}

void CTabDlg::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);

    // TODO: 在此处添加消息处理程序代码
    SetControlMouseWheelEnable(!IsScrollBarVisible());      //如果显示了滚动条，则禁止控件响应鼠标滚轮，此时鼠标滚轮用于滚动窗口
}
