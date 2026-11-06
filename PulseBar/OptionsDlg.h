#pragma once
#include "TaskBarSettingsDlg.h"
#include "GeneralSettingsDlg.h"
#include "BaseDialog.h"

#define OPTION_DLG_NAME _T("OptionsDlg")

class COptionsDlg : public CBaseDialog
{
    DECLARE_DYNAMIC(COptionsDlg)

public:
    COptionsDlg(int tab = 0, CWnd* pParent = nullptr);
    virtual ~COptionsDlg();

    CTaskBarSettingsDlg m_tab2_dlg{ this };
    CGeneralSettingsDlg m_tab3_dlg{ this };
    int m_notify_icon_selected{};

protected:
    CListBox m_navigation;
    CStatic m_page_title;
    CFont m_nav_font;
    CFont m_nav_font_bold;
    CFont m_title_font;
    int m_tab_selected{};
    int m_hover_item{ -1 };
    CRect m_min_window;
    bool m_ready{};
    bool m_tab3_created{};
    //上一次真正排布过的分区与客户区尺寸：尺寸/分区都没变时不重复 ShowSection
    int m_laid_section{ -1 };
    CSize m_laid_size{ -1, -1 };
    //当前实际显示的是哪一类子页（0=任务栏页，1=通用页）：用于判断是否真的换页
    int m_shown_section{ -1 };

    void SelectPage(int page);
    void EnsureGeneralPage();
    void LayoutPages();
    void CenterInWorkArea();
    bool CollectSettings();
    virtual CString GetDialogName() const override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;
    virtual void OnCancel() override;
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnNavigationChanged();
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnBnClickedApplyButton();
};
