#pragma once
#include "afxwin.h"
#include "BaseDialog.h"

// CIconSelectDlg 对话框

class CIconSelectDlg : public CBaseDialog
{
    DECLARE_DYNAMIC(CIconSelectDlg)

public:
    CIconSelectDlg(int icon_selected, CWnd* pParent = NULL);   // 标准构造函数
    virtual ~CIconSelectDlg();

    int GetIconSelected() const;

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ICON_SELECT_DIALOG };
#endif

protected:
    CStatic m_preview_pic;
    CComboBox m_icon_select_combo;

    int m_icon_selected{};

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
    virtual CString GetDialogName() const override;
    virtual bool InitializeControls() override;

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnCbnSelchangeCombo1();
protected:
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT draw_item);
};
