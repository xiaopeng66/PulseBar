// TaskbarColorDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "PulseBar.h"
#include "TaskbarColorDlg.h"
#include "afxdialogex.h"
#include "CMFCColorDialogEx.h"


// CTaskbarColorDlg 对话框

IMPLEMENT_DYNAMIC(CTaskbarColorDlg, CBaseDialog)

CTaskbarColorDlg::CTaskbarColorDlg(const std::map<CommonDisplayItem, TaskbarItemColor>& colors, CWnd* pParent /*=NULL*/)
	: CBaseDialog(IDD_TASKBAR_COLOR_DIALOG, pParent), m_colors(colors)
{
}

CTaskbarColorDlg::~CTaskbarColorDlg()
{
}

CString CTaskbarColorDlg::GetDialogName() const
{
    return _T("TaskbarColorDlg");
}

void CTaskbarColorDlg::DoDataExchange(CDataExchange* pDX)
{
    CBaseDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list_ctrl);
}


BEGIN_MESSAGE_MAP(CTaskbarColorDlg, CBaseDialog)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST1, &CTaskbarColorDlg::OnNMDblclkList1)
    ON_WM_SIZE()
END_MESSAGE_MAP()


// CTaskbarColorDlg 消息处理程序

void CTaskbarColorDlg::ResizeColumns()
{
    if (!::IsWindow(m_list_ctrl.GetSafeHwnd()))
        return;
    CHeaderCtrl* header = m_list_ctrl.GetHeaderCtrl();
    if (header == nullptr || header->GetItemCount() < 3)
        return;
    CRect rect;
    m_list_ctrl.GetClientRect(&rect);
    if (rect.Width() <= 0)
        return;
    const int usable = max(3, rect.Width() - theApp.DPI(20) - 1);
    const int width0 = max(1, usable / 2);
    const int width1 = max(1, (usable - width0) / 2);
    const int width2 = max(1, usable - width0 - width1);
    m_list_ctrl.SetColumnWidth(0, width0);
    m_list_ctrl.SetColumnWidth(1, width1);
    m_list_ctrl.SetColumnWidth(2, width2);
}


BOOL CTaskbarColorDlg::OnInitDialog()
{
	CBaseDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化

    SetIcon(theApp.GetMenuIcon(IDI_TASKBAR_WINDOW), FALSE);		// 设置小图标

    //初始化列表控件
    m_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
    m_list_ctrl.InsertColumn(0, CCommon::LoadText(IDS_ITEM), LVCFMT_LEFT, 1);
    m_list_ctrl.InsertColumn(1, CCommon::LoadText(IDS_COLOR_LABEL), LVCFMT_LEFT, 1);
    m_list_ctrl.InsertColumn(2, CCommon::LoadText(IDS_COLOR_VALUE), LVCFMT_LEFT, 1);
    ResizeColumns();
    m_list_ctrl.SetDrawItemRangMargin(theApp.DPI(2));

    //向列表中插入行
    for (auto iter = theApp.m_plugins.AllDisplayItemsWithPlugins().begin(); iter != theApp.m_plugins.AllDisplayItemsWithPlugins().end(); ++iter)
    {
        CString item_name = iter->GetItemName();
        if (!item_name.IsEmpty())
        {
            int index = m_list_ctrl.GetItemCount();
            m_list_ctrl.InsertItem(index, item_name);
            m_list_ctrl.SetItemColor(index, 1, m_colors[*iter].label);
            m_list_ctrl.SetItemColor(index, 2, m_colors[*iter].value);
            m_list_ctrl.SetItemData(index, (DWORD_PTR)&(*iter));
        }
    }

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}



void CTaskbarColorDlg::OnNMDblclkList1(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码
    int index = pNMItemActivate->iItem;
    int col = pNMItemActivate->iSubItem;
    //双击列表空白区域时 iItem 为 -1，此时 GetItemData 返回 0，后面解引用会崩溃
    if (index < 0 || index >= m_list_ctrl.GetItemCount())
        return;
    if (col == 1 || col == 2)
    {
        COLORREF color = m_list_ctrl.GetItemColor(index, col);
        CMFCColorDialogEx colorDlg(color, 0, this);
        if (colorDlg.DoModal() == IDOK)
        {
            color = colorDlg.GetColor();
            m_list_ctrl.SetItemColor(index, col, color);
            CommonDisplayItem* item = (CommonDisplayItem*)(m_list_ctrl.GetItemData(index));
            //列表项的数据是初始化时写入的指针；防御性判空，避免任何未设置数据的行崩溃
            if (item != nullptr)
            {
                if (col == 1)
                    m_colors[*item].label = color;
                else
                    m_colors[*item].value = color;
            }
        }
    }

    *pResult = 0;
}

void CTaskbarColorDlg::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);
    if (nType != SIZE_MINIMIZED)
        ResizeColumns();
}
