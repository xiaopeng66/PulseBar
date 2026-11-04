#pragma once
#include "BaseDialog.h"
#include "CommonData.h"
#include "ColorStatic.h"

//自定义主题只编辑草稿，由外层设置统一应用。
class CCustomThemeDlg : public CBaseDialog
{
public:
    CCustomThemeDlg(TaskBarSettingData& data, CWnd* pParent = nullptr);
    enum { IDD = IDD_CUSTOM_THEME_DIALOG };

    bool ExportThemeToFile(const std::wstring& path);
    bool ImportThemeFromFile(const std::wstring& path);

protected:
    virtual CString GetDialogName() const override;
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;

    afx_msg LRESULT OnStaticClicked(WPARAM wParam, LPARAM lParam);
    afx_msg void OnCbnSelchangeStyleCombo();
    afx_msg void OnBnClickedBoldCheck();
    afx_msg void OnEnChangeRadiusEdit();
    afx_msg void OnEnChangeOpacityEdit();
    afx_msg void OnBnClickedFromCurrentBtn();
    afx_msg void OnBnClickedImportBtn();
    afx_msg void OnBnClickedExportBtn();

    void LoadDraft();
    void UpdateColorStatics();
    void CollectSettings();

    TaskBarSettingData& m_target;
    TaskBarSettingData m_data;
    CColorStatic m_bg_color_static;
    CColorStatic m_label_color_static;
    CColorStatic m_value_color_static;
    CColorStatic m_accent_color_static;
    CComboBox m_style_combo;
    bool m_loading{ true };

    DECLARE_MESSAGE_MAP()
};
