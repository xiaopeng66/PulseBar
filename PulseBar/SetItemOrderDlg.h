#pragma once
#include "BaseDialog.h"
#include "ListCtrlEx.h"

class CSetItemOrderDlg : public CBaseDialog
{
    DECLARE_DYNAMIC(CSetItemOrderDlg)

public:
    CSetItemOrderDlg(CWnd* pParent = nullptr);
    virtual ~CSetItemOrderDlg();

    void SetItemOrder(const std::vector<int>& item_order);
    const std::vector<int>& GetItemOrder() const;
    void SetDisplayItem(const DisplayItemSet& display_item);
    DisplayItemSet GetDisplayItem() const;
    void SetPluginDisplayItem(const StringSet& plugin_item);
    const StringSet& GetPluginDisplayItem() const;
    void SetAppearance(const DispStrings& display_texts, const std::map<CommonDisplayItem, TaskbarItemColor>& colors);
    const DispStrings& GetDisplayTexts() const;
    const std::map<CommonDisplayItem, TaskbarItemColor>& GetColors() const;
    bool ColorsModified() const { return m_colors_modified; }   //用户在表里改过颜色

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_SELECT_ORDER_DIALOG };
#endif

private:
    class CItemList : public CListCtrlEx
    {
    public:
        void FinishEdit() { EndEdit(); }
    };
    CTaskbarItemOrderHelper m_item_order;
    CItemList m_list_ctrl;
    DisplayItemSet m_display_item;
    StringSet m_plugin_item;
    DispStrings m_display_texts;
    std::map<CommonDisplayItem, TaskbarItemColor> m_colors;
    std::vector<CommonDisplayItem> m_all_displayed_item;
    std::vector<bool> m_row_checked;
    bool m_loading{};
    bool m_colors_modified{};

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual CString GetDialogName() const override;
    void ShowItem(int selected = -1);
    void SaveRows();
    void LayoutColumns();
    void EnableCtrl(int selected);
    bool GetItemChecked(CommonDisplayItem item);
    void SaveItemChecked(CommonDisplayItem item, bool checked);
    void MoveItem(int direction);

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;
    afx_msg void OnBnClickedMoveUpButton();
    afx_msg void OnBnClickedMoveDownButton();
    afx_msg void OnBnClickedRestoreDefaultButton();
    afx_msg void OnItemChanged(NMHDR* header, LRESULT* result);
    afx_msg void OnItemClicked(NMHDR* header, LRESULT* result);
    afx_msg void OnItemDoubleClicked(NMHDR* header, LRESULT* result);
    afx_msg void OnCustomDraw(NMHDR* header, LRESULT* result);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    virtual BOOL PreTranslateMessage(MSG* msg) override;
};
