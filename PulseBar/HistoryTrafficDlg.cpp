// HistoryTrafficDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "PulseBar.h"
#include "HistoryTrafficDlg.h"
#include "afxdialogex.h"


// CHistoryTrafficDlg 对话框

IMPLEMENT_DYNAMIC(CHistoryTrafficDlg, CBaseDialog)

CHistoryTrafficDlg::CHistoryTrafficDlg(deque<HistoryTraffic>& history_traffics, CWnd* pParent /*=NULL*/)
    : CBaseDialog(IDD_HISTORY_TRAFFIC_DIALOG, pParent), m_history_traffics(history_traffics), m_tab1_dlg(history_traffics, this), m_tab2_dlg(history_traffics, this)
{

}

CHistoryTrafficDlg::~CHistoryTrafficDlg()
{
}


CString CHistoryTrafficDlg::GetDialogName() const
{
    return _T("HistoryTrafficDlg");
}

void CHistoryTrafficDlg::DoDataExchange(CDataExchange* pDX)
{
    CBaseDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB1, m_tab);
}

void CHistoryTrafficDlg::SetTabWndSize()
{
    if (!::IsWindow(m_tab.GetSafeHwnd()) || m_tab.GetItemCount() <= 0)
        return;

    CRect rect;
    m_tab.GetClientRect(rect);
    CRect rcTabItem;
    if (!m_tab.GetItemRect(0, rcTabItem))
        return;
    rect.top += rcTabItem.Height() + 4;
    rect.left += 4;
    rect.bottom -= 4;
    rect.right -= 4;
    if (rect.right <= rect.left || rect.bottom <= rect.top)
        return;
    if (::IsWindow(m_tab1_dlg.GetSafeHwnd()))
        m_tab1_dlg.MoveWindow(&rect);
    if (::IsWindow(m_tab2_dlg.GetSafeHwnd()))
        m_tab2_dlg.MoveWindow(&rect);
}


BEGIN_MESSAGE_MAP(CHistoryTrafficDlg, CBaseDialog)
    ON_WM_GETMINMAXINFO()
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, &CHistoryTrafficDlg::OnTcnSelchangeTab1)
    ON_WM_SIZE()
END_MESSAGE_MAP()


// CHistoryTrafficDlg 消息处理程序


BOOL CHistoryTrafficDlg::OnInitDialog()
{
    CBaseDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetIcon(theApp.GetMenuIcon(IDI_STATISTICS), FALSE);		// 设置小图标

    //插入标签
    if (m_tab.InsertItem(0, CCommon::LoadText(IDS_LIST_VIEW)) < 0 ||
        m_tab.InsertItem(1, CCommon::LoadText(IDS_CALENDAR_VIEW)) < 0)
    {
        EndDialog(IDCANCEL);
        return TRUE;
    }

    //创建子对话框。任一页创建失败都不能继续使用空 HWND，否则后续 MoveWindow/
    //ShowWindow 会把错误延迟成随机崩溃或不可见的空窗口。
    const BOOL tab1_created = m_tab1_dlg.Create(IDD_HISTORY_TRAFFIC_LIST_DIALOG, &m_tab);
    const BOOL tab2_created = m_tab2_dlg.Create(IDD_HISTORY_TRAFFIC_CALENDAR_DIALOG, &m_tab);
    if (!tab1_created || !tab2_created)
    {
        if (::IsWindow(m_tab1_dlg.GetSafeHwnd()))
            m_tab1_dlg.DestroyWindow();
        if (::IsWindow(m_tab2_dlg.GetSafeHwnd()))
            m_tab2_dlg.DestroyWindow();
        EndDialog(IDCANCEL);
        return TRUE;
    }

    //调整子对话框的大小和位置
    SetTabWndSize();
    //设置默认选中的标签。SetCurFocus 只改变键盘焦点，不改变当前页。
    m_tab_selected = max(0, min(m_tab_selected, m_tab.GetItemCount() - 1));
    m_tab.SetCurSel(m_tab_selected);
    m_tab.SetCurFocus(m_tab_selected);
    m_tab1_dlg.ShowWindow(SW_HIDE);
    m_tab2_dlg.ShowWindow(SW_HIDE);
    m_tab.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    CWnd* selected = m_tab_selected == 0 ? static_cast<CWnd*>(&m_tab1_dlg)
                                        : static_cast<CWnd*>(&m_tab2_dlg);
    selected->ShowWindow(SW_SHOW);
    selected->RedrawWindow(nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    selected->SetFocus();

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}



BOOL CHistoryTrafficDlg::PreTranslateMessage(MSG* pMsg)
{
    // TODO: 在此添加专用代码和/或调用基类
    //if (GetKeyState(VK_CONTROL) & 0x80)
    //{
    //	if (pMsg->wParam == 'D')
    //	{
    //		HistoryTraffic h{};
    //		h.year = 2018;
    //		h.month = 4;
    //		h.day = 29;
    //		auto iter = std::lower_bound(m_history_traffics.begin(), m_history_traffics.end(), h, HistoryTraffic::DateGreater);
    //		int index = iter - m_history_traffics.begin();
    //	}
    //}

    return CBaseDialog::PreTranslateMessage(pMsg);
}

void CHistoryTrafficDlg::OnTcnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult)
{
    // TODO: 在此添加控件通知处理程序代码
    m_tab_selected = m_tab.GetCurSel();
    if (m_tab_selected < 0 || m_tab_selected > 1)
        m_tab_selected = 0;

    //先隐藏旧页并擦除 tab 客户区，再显示新页。只 ShowWindow(SW_SHOW/SW_HIDE)
    //会让旧页的文字留在父窗口表面，尤其在切到日历页时形成残影。
    m_tab1_dlg.ShowWindow(SW_HIDE);
    m_tab2_dlg.ShowWindow(SW_HIDE);
    CRect tab_rect;
    m_tab.GetClientRect(&tab_rect);
    m_tab.RedrawWindow(&tab_rect, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);

    CWnd* selected = m_tab_selected == 0 ? static_cast<CWnd*>(&m_tab1_dlg)
                                        : static_cast<CWnd*>(&m_tab2_dlg);
    selected->ShowWindow(SW_SHOW);
    selected->RedrawWindow(nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    selected->SetFocus();
    *pResult = 0;
}


void CHistoryTrafficDlg::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);

    // TODO: 在此处添加消息处理程序代码
    if (nType != SIZE_MINIMIZED && m_tab1_dlg.GetSafeHwnd() != NULL && m_tab2_dlg.GetSafeHwnd() != NULL)
    {
        SetTabWndSize();
    }
}
