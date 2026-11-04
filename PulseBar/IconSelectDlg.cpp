// IconSelectDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "PulseBar.h"
#include "IconSelectDlg.h"
#include "afxdialogex.h"


// CIconSelectDlg 对话框

IMPLEMENT_DYNAMIC(CIconSelectDlg, CBaseDialog)

CIconSelectDlg::CIconSelectDlg(int icon_selected, CWnd* pParent /*=NULL*/)
    : CBaseDialog(IDD_ICON_SELECT_DIALOG, pParent), m_icon_selected{ icon_selected }
{

}

CIconSelectDlg::~CIconSelectDlg()
{
}

int CIconSelectDlg::GetIconSelected() const
{
    return theApp.NormalizeNotifyIconSelection(m_icon_selected);
}

void CIconSelectDlg::DoDataExchange(CDataExchange* pDX)
{
    CBaseDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_ICON_PREVIEW, m_preview_pic);
    DDX_Control(pDX, IDC_COMBO1, m_icon_select_combo);
}

BEGIN_MESSAGE_MAP(CIconSelectDlg, CBaseDialog)
    ON_CBN_SELCHANGE(IDC_COMBO1, &CIconSelectDlg::OnCbnSelchangeCombo1)
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()


// CIconSelectDlg 消息处理程序


BOOL CIconSelectDlg::OnInitDialog()
{
    CBaseDialog::OnInitDialog();

    SetIcon(theApp.GetMenuIcon(IDR_MAINFRAME), FALSE);
    m_preview_pic.SetWindowPos(nullptr, 0, 0, theApp.DPI(200), theApp.DPI(40), SWP_NOZORDER | SWP_NOMOVE);

    m_icon_selected = GetIconSelected();
    m_icon_select_combo.AddString(CCommon::LoadText(IDS_PULSEBAR_ICON_BLUE));
    m_icon_select_combo.AddString(CCommon::LoadText(IDS_PULSEBAR_ICON_LIGHT));
    m_icon_select_combo.AddString(CCommon::LoadText(IDS_PULSEBAR_ICON_DARK));
    m_icon_select_combo.AddString(CCommon::LoadText(IDS_PULSEBAR_ICON_AUTO));
    m_icon_select_combo.SetCurSel(m_icon_selected);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void CIconSelectDlg::OnCbnSelchangeCombo1()
{
    m_icon_selected = theApp.NormalizeNotifyIconSelection(m_icon_select_combo.GetCurSel());
    m_preview_pic.Invalidate(FALSE);
}


void CIconSelectDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT draw_item)
{
    if (nIDCtl != IDC_ICON_PREVIEW || draw_item == nullptr)
    {
        CBaseDialog::OnDrawItem(nIDCtl, draw_item);
        return;
    }
    CDC* dc = CDC::FromHandle(draw_item->hDC);
    CRect rect(draw_item->rcItem);
    const int size = theApp.DPI(16);
    //同一真实图标同时放在浅、深背景：静态样式两边画同一个图标，
    //“自动”样式各画任务栏深浅下实际生效的变体（浅背景→深色图标，深背景→浅色图标）。
    for (int side = 0; side < 2; ++side)
    {
        CRect sample = rect;
        sample.left += side * rect.Width() / 2;
        sample.right = rect.left + (side + 1) * rect.Width() / 2;
        dc->FillSolidRect(sample, side == 0 ? RGB(243, 243, 243) : RGB(32, 32, 32));
        int selected = GetIconSelected();
        if (selected == NOTIFY_ICON_AUTO)
            selected = theApp.m_win_version.GetMajorVersion() >= 10
                ? theApp.SelectNotifyIconForTheme(NOTIFY_ICON_LIGHT, side == 0)
                : NOTIFY_ICON_DARK;
        ::DrawIconEx(draw_item->hDC, sample.CenterPoint().x - size / 2,
            sample.CenterPoint().y - size / 2, theApp.GetNotifyIcon(selected), size, size, 0, nullptr, DI_NORMAL);
    }
}


CString CIconSelectDlg::GetDialogName() const
{
    return _T("IconSelectDlg");
}

bool CIconSelectDlg::InitializeControls()
{
    RepositionTextBasedControls({
        { CtrlTextInfo::L1, IDC_SELECT_ICON_STATIC },
        { CtrlTextInfo::C0, IDC_COMBO1 }
    });
    return true;
}
