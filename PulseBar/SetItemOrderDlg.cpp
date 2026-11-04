// SetItemOrderDlg.cpp: 监控项目、顺序、文字与颜色编辑
#include "stdafx.h"
#include "PulseBar.h"
#include "SetItemOrderDlg.h"
#include "CMFCColorDialogEx.h"

IMPLEMENT_DYNAMIC(CSetItemOrderDlg, CBaseDialog)

CSetItemOrderDlg::CSetItemOrderDlg(CWnd* pParent)
    : CBaseDialog(IDD_SELECT_ORDER_DIALOG, pParent), m_item_order(false)
{
    m_item_order.Init();
    SetAppearance(theApp.m_taskbar_data.disp_str, theApp.m_taskbar_data.text_colors);
}

CSetItemOrderDlg::~CSetItemOrderDlg()
{
}

void CSetItemOrderDlg::SetItemOrder(const std::vector<int>& item_order)
{
    m_item_order.SetOrder(item_order);
}

const std::vector<int>& CSetItemOrderDlg::GetItemOrder() const
{
    return m_item_order.GetItemOrderConst();
}

void CSetItemOrderDlg::SetDisplayItem(const DisplayItemSet& display_item)
{
    m_display_item = display_item;
}

DisplayItemSet CSetItemOrderDlg::GetDisplayItem() const
{
    return m_display_item;
}

void CSetItemOrderDlg::SetPluginDisplayItem(const StringSet& plugin_item)
{
    m_plugin_item = plugin_item;
}

const StringSet& CSetItemOrderDlg::GetPluginDisplayItem() const
{
    return m_plugin_item;
}

void CSetItemOrderDlg::SetAppearance(const DispStrings& display_texts, const std::map<CommonDisplayItem, TaskbarItemColor>& colors)
{
    m_display_texts = display_texts;
    m_colors = colors;
}

const DispStrings& CSetItemOrderDlg::GetDisplayTexts() const
{
    return m_display_texts;
}

const std::map<CommonDisplayItem, TaskbarItemColor>& CSetItemOrderDlg::GetColors() const
{
    return m_colors;
}

CString CSetItemOrderDlg::GetDialogName() const
{
    return _T("MonitorItemsDlg");
}

void CSetItemOrderDlg::DoDataExchange(CDataExchange* pDX)
{
    CBaseDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list_ctrl);
}

BEGIN_MESSAGE_MAP(CSetItemOrderDlg, CBaseDialog)
    ON_BN_CLICKED(IDC_MOVE_UP_BUTTON, &CSetItemOrderDlg::OnBnClickedMoveUpButton)
    ON_BN_CLICKED(IDC_MOVE_DOWN_BUTTON, &CSetItemOrderDlg::OnBnClickedMoveDownButton)
    ON_BN_CLICKED(IDC_RESTORE_DEFAULT_BUTTON, &CSetItemOrderDlg::OnBnClickedRestoreDefaultButton)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, &CSetItemOrderDlg::OnItemChanged)
    ON_NOTIFY(NM_CLICK, IDC_LIST1, &CSetItemOrderDlg::OnItemClicked)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST1, &CSetItemOrderDlg::OnItemDoubleClicked)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST1, &CSetItemOrderDlg::OnCustomDraw)
    ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CSetItemOrderDlg::OnInitDialog()
{
    //固定尺寸：不恢复配置里保存的历史大小，避免列表被拉伸、按钮被挤出窗口
    SetRememberDlgSize(false);
    CBaseDialog::OnInitDialog();
    CRect rc;
    GetWindowRect(rc);
    SetWindowPos(nullptr, 0, 0, rc.Width(), rc.Height(), SWP_NOMOVE | SWP_NOZORDER);
    SetIcon(theApp.GetMenuIcon(IDI_ITEM), FALSE);
    m_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_DOUBLEBUFFER);
    m_list_ctrl.InsertColumn(0, CCommon::LoadText(L"TXT_SETTINGS_ITEM_SHOW"), LVCFMT_CENTER, 60);
    m_list_ctrl.InsertColumn(1, CCommon::LoadText(L"TXT_SETTINGS_ITEM_NAME"), LVCFMT_LEFT, 180);
    m_list_ctrl.InsertColumn(2, CCommon::LoadText(L"TXT_SETTINGS_ITEM_TEXT"), LVCFMT_LEFT, 160);
    m_list_ctrl.InsertColumn(3, CCommon::LoadText(L"TXT_SETTINGS_ITEM_LABEL_COLOR"), LVCFMT_CENTER, 110);
    m_list_ctrl.InsertColumn(4, CCommon::LoadText(L"TXT_SETTINGS_ITEM_VALUE_COLOR"), LVCFMT_CENTER, 110);
    LayoutColumns();
    m_list_ctrl.SetEditColMethod(CListCtrlEx::EC_NONE);
    ShowItem();
    return TRUE;
}

void CSetItemOrderDlg::ShowItem(int selected)
{
    m_loading = true;
    m_list_ctrl.SetRedraw(FALSE);
    m_list_ctrl.DeleteAllItems();
    m_all_displayed_item = m_item_order.GetAllDisplayItemsWithOrder();
    m_row_checked.assign(m_all_displayed_item.size(), false);
    const TaskbarItemColor default_color = m_colors.empty() ? TaskbarItemColor{} : m_colors.begin()->second;
    for (int row = 0; row < static_cast<int>(m_all_displayed_item.size()); ++row)
    {
        const auto& item = m_all_displayed_item[row];
        if (m_display_texts.GetAllItems().find(item) == m_display_texts.GetAllItems().end())
            m_display_texts.Get(item) = item.DefaultString(false);
        if (m_colors.find(item) == m_colors.end())
            m_colors[item] = default_color;
        m_row_checked[row] = GetItemChecked(item);
        m_list_ctrl.InsertItem(row, m_row_checked[row] ? L"✓" : L"");
        m_list_ctrl.SetItemText(row, 1, item.GetItemName());
        m_list_ctrl.SetItemText(row, 2, m_display_texts.GetConst(item).c_str());
        const auto& color = m_colors[item];
        for (int column = 3; column <= 4; ++column)
        {
            COLORREF value = column == 3 ? color.label : color.value;
            CString text;
            text.Format(_T("#%02X%02X%02X"), GetRValue(value), GetGValue(value), GetBValue(value));
            m_list_ctrl.SetItemText(row, column, text);
        }
    }
    if (selected >= 0 && selected < m_list_ctrl.GetItemCount())
    {
        m_list_ctrl.SetItemState(selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_list_ctrl.EnsureVisible(selected, FALSE);
    }
    m_list_ctrl.SetRedraw(TRUE);
    m_list_ctrl.Invalidate();
    m_loading = false;
    EnableCtrl(selected);
}

void CSetItemOrderDlg::SaveRows()
{
    //回车、按钮和窗口关闭都先提交仍在编辑的单元格。
    m_list_ctrl.FinishEdit();
    for (int row = 0; row < m_list_ctrl.GetItemCount(); ++row)
    {
        const auto& item = m_all_displayed_item[row];
        SaveItemChecked(item, m_row_checked[row]);
        m_display_texts.Get(item) = m_list_ctrl.GetItemText(row, 2).GetString();
    }
}

void CSetItemOrderDlg::EnableCtrl(int selected)
{
    GetDlgItem(IDC_MOVE_UP_BUTTON)->EnableWindow(selected > 0);
    GetDlgItem(IDC_MOVE_DOWN_BUTTON)->EnableWindow(selected >= 0 && selected + 1 < m_list_ctrl.GetItemCount());
}

bool CSetItemOrderDlg::GetItemChecked(CommonDisplayItem item)
{
    if (item.IsPlugin())
    {
        //GetItemId() 可能返回空指针，Contains 的参数是 std::wstring
        if (item.PluginItem() == nullptr || item.PluginItem()->GetItemId() == nullptr)
            return false;
        return m_plugin_item.Contains(item.PluginItem()->GetItemId());
    }
    return m_display_item.Contains(item.ItemType());
}

void CSetItemOrderDlg::SaveItemChecked(CommonDisplayItem item, bool checked)
{
    if (item.IsPlugin())
    {
        if (item.PluginItem() != nullptr && item.PluginItem()->GetItemId() != nullptr)
            m_plugin_item.SetStrContained(item.PluginItem()->GetItemId(), checked);
    }
    else if (checked)
        m_display_item.Add(item.ItemType());
    else
        m_display_item.Remove(item.ItemType());
}

void CSetItemOrderDlg::MoveItem(int direction)
{
    SaveRows();
    const int selected = m_list_ctrl.GetNextItem(-1, LVNI_SELECTED);
    const int destination = selected + direction;
    auto& order = m_item_order.GetItemOrder();
    if (selected >= 0 && destination >= 0 && destination < static_cast<int>(order.size()))
    {
        std::swap(order[selected], order[destination]);
        ShowItem(destination);
    }
}

void CSetItemOrderDlg::OnBnClickedMoveUpButton()
{
    MoveItem(-1);
}

void CSetItemOrderDlg::OnBnClickedMoveDownButton()
{
    MoveItem(1);
}

void CSetItemOrderDlg::OnBnClickedRestoreDefaultButton()
{
    SaveRows();
    auto order = m_item_order.GetItemOrderConst();
    std::sort(order.begin(), order.end());
    m_item_order.SetOrder(order);
    ShowItem();
}

void CSetItemOrderDlg::OnItemChanged(NMHDR* header, LRESULT* result)
{
    *result = 0;
    if (m_loading)
        return;
    auto* change = reinterpret_cast<NMLISTVIEW*>(header);
    if ((change->uChanged & LVIF_STATE) != 0)
        EnableCtrl(m_list_ctrl.GetNextItem(-1, LVNI_SELECTED));
}

void CSetItemOrderDlg::OnItemClicked(NMHDR* header, LRESULT* result)
{
    *result = 0;
    auto* item = reinterpret_cast<NMITEMACTIVATE*>(header);
    const int row = item->iItem;
    if (row >= 0 && row < static_cast<int>(m_row_checked.size()) && item->iSubItem == 0)
    {
        m_row_checked[row] = !m_row_checked[row];
        SaveItemChecked(m_all_displayed_item[row], m_row_checked[row]);
        m_list_ctrl.SetItemText(row, 0, m_row_checked[row] ? L"✓" : L"");
    }
}

void CSetItemOrderDlg::OnItemDoubleClicked(NMHDR* header, LRESULT* result)
{
    *result = 0;
    auto* item = reinterpret_cast<NMITEMACTIVATE*>(header);
    const int row = item->iItem;
    const int column = item->iSubItem;
    if (row < 0 || row >= static_cast<int>(m_all_displayed_item.size()))
        return;
    SaveRows();
    if (column == 2)
        m_list_ctrl.Edit(row, column);
    else if (column == 3 || column == 4)
    {
        auto& colors = m_colors[m_all_displayed_item[row]];
        CMFCColorDialogEx dlg(column == 3 ? colors.label : colors.value, 0, this);
        if (dlg.DoModal() == IDOK)
        {
            if (column == 3)
                colors.label = dlg.GetColor();
            else
                colors.value = dlg.GetColor();
            //改过颜色后必须打开“指定每个项目的颜色”，否则任务栏仍用统一文字色，设置看起来无效
            m_colors_modified = true;
            ShowItem(row);
        }
    }
}

void CSetItemOrderDlg::OnCustomDraw(NMHDR* header, LRESULT* result)
{
    auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
    *result = CDRF_DODEFAULT;
    if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
        *result = CDRF_NOTIFYITEMDRAW;
    else if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
        *result = CDRF_NOTIFYSUBITEMDRAW;
    else if (draw->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
    {
        const int row = static_cast<int>(draw->nmcd.dwItemSpec);
        const int column = draw->iSubItem;
        if (row < 0 || row >= static_cast<int>(m_all_displayed_item.size()))
            return;
        if (column == 3 || column == 4)
        {
            const auto& colors = m_colors[m_all_displayed_item[row]];
            COLORREF color = column == 3 ? colors.label : colors.value;
            draw->clrTextBk = color;
            const int brightness = GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114;
            draw->clrText = brightness > 140000 ? RGB(0, 0, 0) : RGB(255, 255, 255);
        }
    }
}

void CSetItemOrderDlg::LayoutColumns()
{
    if (m_list_ctrl.GetSafeHwnd() == nullptr || !::IsWindow(m_list_ctrl.GetSafeHwnd()))
        return;
    //对话框创建阶段 OnSize 会先于 OnInitDialog 触发，此时列还没插入。
    //对不存在的列调用 SetColumnWidth 会破坏列表头内部结构，销毁时在 COMCTL32 崩溃。
    CHeaderCtrl* header = m_list_ctrl.GetHeaderCtrl();
    if (header == nullptr || header->GetItemCount() != 5)
        return;
    CRect rect;
    m_list_ctrl.GetClientRect(rect);
    const int width = rect.Width() - GetSystemMetrics(SM_CXVSCROLL) - DPI(2);
    if (width <= 0)
        return;
    const int widths[5] = { 9, 31, 25, 17, 17 };
    int used = 0;
    for (int i = 0; i < 5; ++i)
    {
        int column_width = (i == 4) ? width - used : width * widths[i] / 100;
        m_list_ctrl.SetColumnWidth(i, column_width);
        used += column_width;
    }
}

void CSetItemOrderDlg::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);
    LayoutColumns();
}

BOOL CSetItemOrderDlg::PreTranslateMessage(MSG* msg)
{
    if (msg->message == WM_KEYDOWN && msg->wParam == VK_SPACE && GetFocus() == &m_list_ctrl)
    {
        const int row = m_list_ctrl.GetNextItem(-1, LVNI_SELECTED);
        if (row >= 0 && row < static_cast<int>(m_row_checked.size()))
        {
            m_row_checked[row] = !m_row_checked[row];
            SaveItemChecked(m_all_displayed_item[row], m_row_checked[row]);
            m_list_ctrl.SetItemText(row, 0, m_row_checked[row] ? L"✓" : L"");
            return TRUE;
        }
    }
    return CBaseDialog::PreTranslateMessage(msg);
}

void CSetItemOrderDlg::OnOK()
{
    SaveRows();
    if (m_display_item.IsEmpty() && m_plugin_item.data().empty())
        m_display_item.Add(TDI_UP);
    CBaseDialog::OnOK();
}
