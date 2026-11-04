#include "stdafx.h"
#include "CustomThemeDlg.h"
#include "Common.h"
#include "PulseBar.h"
#include "IniHelper.h"
#include "CMFCColorDialogEx.h"

CCustomThemeDlg::CCustomThemeDlg(TaskBarSettingData& data, CWnd* pParent)
    : CBaseDialog(IDD_CUSTOM_THEME_DIALOG, pParent), m_target(data), m_data(data)
{
}

CString CCustomThemeDlg::GetDialogName() const
{
    return _T("CustomThemeDlg");
}

void CCustomThemeDlg::DoDataExchange(CDataExchange* pDX)
{
    CBaseDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CUSTOM_BG_COLOR, m_bg_color_static);
    DDX_Control(pDX, IDC_CUSTOM_LABEL_COLOR, m_label_color_static);
    DDX_Control(pDX, IDC_CUSTOM_VALUE_COLOR, m_value_color_static);
    DDX_Control(pDX, IDC_CUSTOM_ACCENT_COLOR, m_accent_color_static);
    DDX_Control(pDX, IDC_CUSTOM_STYLE_COMBO, m_style_combo);
}

BEGIN_MESSAGE_MAP(CCustomThemeDlg, CBaseDialog)
    ON_MESSAGE(WM_STATIC_CLICKED, &CCustomThemeDlg::OnStaticClicked)
    ON_CBN_SELCHANGE(IDC_CUSTOM_STYLE_COMBO, &CCustomThemeDlg::OnCbnSelchangeStyleCombo)
    ON_BN_CLICKED(IDC_CUSTOM_BOLD_CHECK, &CCustomThemeDlg::OnBnClickedBoldCheck)
    ON_EN_CHANGE(IDC_CUSTOM_RADIUS_EDIT, &CCustomThemeDlg::OnEnChangeRadiusEdit)
    ON_EN_CHANGE(IDC_CUSTOM_OPACITY_EDIT, &CCustomThemeDlg::OnEnChangeOpacityEdit)
    ON_BN_CLICKED(IDC_CUSTOM_FROM_CURRENT_BTN, &CCustomThemeDlg::OnBnClickedFromCurrentBtn)
    ON_BN_CLICKED(IDC_CUSTOM_IMPORT_BTN, &CCustomThemeDlg::OnBnClickedImportBtn)
    ON_BN_CLICKED(IDC_CUSTOM_EXPORT_BTN, &CCustomThemeDlg::OnBnClickedExportBtn)
END_MESSAGE_MAP()

BOOL CCustomThemeDlg::OnInitDialog()
{
    CBaseDialog::OnInitDialog();
    m_style_combo.AddString(CCommon::LoadText(L"TXT_CUSTOM_STYLE0"));
    m_style_combo.AddString(CCommon::LoadText(L"TXT_CUSTOM_STYLE1"));
    m_style_combo.AddString(CCommon::LoadText(L"TXT_CUSTOM_STYLE2"));
    m_style_combo.AddString(CCommon::LoadText(L"TXT_CUSTOM_STYLE3"));
    m_style_combo.AddString(CCommon::LoadText(L"TXT_CUSTOM_STYLE4"));
    LoadDraft();
    return TRUE;
}

void CCustomThemeDlg::LoadDraft()
{
    m_loading = true;
    UpdateColorStatics();
    m_style_combo.SetCurSel(max(0, min(m_data.custom_style, 4)));
    CheckDlgButton(IDC_CUSTOM_BOLD_CHECK, m_data.custom_value_bold);
    SetDlgItemInt(IDC_CUSTOM_RADIUS_EDIT, m_data.tb_corner_radius);
    SetDlgItemInt(IDC_CUSTOM_OPACITY_EDIT, m_data.tb_panel_opacity);
    m_loading = false;
}

void CCustomThemeDlg::UpdateColorStatics()
{
    m_bg_color_static.SetFillColor(m_data.custom_bg);
    m_label_color_static.SetFillColor(m_data.custom_label);
    m_value_color_static.SetFillColor(m_data.custom_value);
    m_accent_color_static.SetFillColor(m_data.custom_accent);
}

void CCustomThemeDlg::CollectSettings()
{
    CString text;
    GetDlgItemText(IDC_CUSTOM_RADIUS_EDIT, text);
    m_data.tb_corner_radius = max(0, min(_ttoi(text), 32));
    GetDlgItemText(IDC_CUSTOM_OPACITY_EDIT, text);
    m_data.tb_panel_opacity = max(1, min(_ttoi(text), 100));
    m_data.custom_style = max(0, min(m_style_combo.GetCurSel(), 4));
    m_data.custom_value_bold = IsDlgButtonChecked(IDC_CUSTOM_BOLD_CHECK) != 0;
}

void CCustomThemeDlg::OnOK()
{
    CollectSettings();
    m_data.tb_theme = -1;
    m_target = m_data;
    CBaseDialog::OnOK();
}

afx_msg LRESULT CCustomThemeDlg::OnStaticClicked(WPARAM wParam, LPARAM lParam)
{
    COLORREF* color{};
    switch (::GetDlgCtrlID(reinterpret_cast<CWnd*>(wParam)->GetSafeHwnd()))
    {
    case IDC_CUSTOM_BG_COLOR: color = &m_data.custom_bg; break;
    case IDC_CUSTOM_LABEL_COLOR: color = &m_data.custom_label; break;
    case IDC_CUSTOM_VALUE_COLOR: color = &m_data.custom_value; break;
    case IDC_CUSTOM_ACCENT_COLOR: color = &m_data.custom_accent; break;
    }
    if (color != nullptr)
    {
        CMFCColorDialogEx dlg(*color, 0, this);
        if (dlg.DoModal() == IDOK)
        {
            *color = dlg.GetColor();
            UpdateColorStatics();
        }
    }
    return 0;
}

void CCustomThemeDlg::OnCbnSelchangeStyleCombo()
{
    if (!m_loading)
        CollectSettings();
}

void CCustomThemeDlg::OnBnClickedBoldCheck()
{
    if (!m_loading)
        CollectSettings();
}

void CCustomThemeDlg::OnEnChangeRadiusEdit()
{
    if (!m_loading)
        CollectSettings();
}

void CCustomThemeDlg::OnEnChangeOpacityEdit()
{
    if (!m_loading)
        CollectSettings();
}

void CCustomThemeDlg::OnBnClickedFromCurrentBtn()
{
    //使用外层草稿中选中的主题，而不是尚未应用的全局配置。
    const TaskbarThemeInfo* theme = FindTaskbarTheme(m_target.tb_theme);
    if (theme != nullptr)
    {
        m_data.custom_bg = RGB(theme->bg_r, theme->bg_g, theme->bg_b);
        m_data.custom_label = theme->label;
        m_data.custom_value = theme->value;
        m_data.custom_accent = theme->accent;
        m_data.custom_value_bold = theme->value_bold;
        m_data.custom_style = theme->style;
    }
    else if (m_target.tb_theme == -1)
    {
        m_data.custom_bg = m_target.custom_bg;
        m_data.custom_label = m_target.custom_label;
        m_data.custom_value = m_target.custom_value;
        m_data.custom_accent = m_target.custom_accent;
        m_data.custom_value_bold = m_target.custom_value_bold;
        m_data.custom_style = m_target.custom_style;
    }
    else
    {
        m_data.custom_bg = m_target.back_color;
        if (!m_target.text_colors.empty())
        {
            m_data.custom_label = m_target.text_colors.begin()->second.label;
            m_data.custom_value = m_target.text_colors.begin()->second.value;
        }
        m_data.custom_accent = m_target.status_bar_color;
        m_data.custom_value_bold = m_target.font.bold;
        m_data.custom_style = 0;
    }
    m_data.tb_corner_radius = m_target.tb_corner_radius;
    m_data.tb_panel_opacity = m_target.tb_panel_opacity;
    LoadDraft();
}

bool CCustomThemeDlg::ExportThemeToFile(const std::wstring& path)
{
    CollectSettings();
    CIniHelper ini{ path };
    ini.WriteInt(L"custom_theme", L"bg", m_data.custom_bg);
    ini.WriteInt(L"custom_theme", L"label", m_data.custom_label);
    ini.WriteInt(L"custom_theme", L"value", m_data.custom_value);
    ini.WriteInt(L"custom_theme", L"accent", m_data.custom_accent);
    ini.WriteBool(L"custom_theme", L"bold", m_data.custom_value_bold);
    ini.WriteInt(L"custom_theme", L"style", m_data.custom_style);
    ini.WriteInt(L"custom_theme", L"radius", m_data.tb_corner_radius);
    ini.WriteInt(L"custom_theme", L"opacity", m_data.tb_panel_opacity);
    return ini.Save();
}

bool CCustomThemeDlg::ImportThemeFromFile(const std::wstring& path)
{
    CIniHelper ini{ path };
    std::map<std::wstring, std::wstring> values;
    ini.GetAllKeyValues(L"custom_theme", values);
    if (values.find(L"bg") == values.end() || values.find(L"label") == values.end()
        || values.find(L"value") == values.end() || values.find(L"accent") == values.end())
        return false;
    m_data.custom_bg = ini.GetInt(L"custom_theme", L"bg", m_data.custom_bg) & 0x00ffffff;
    m_data.custom_label = ini.GetInt(L"custom_theme", L"label", m_data.custom_label) & 0x00ffffff;
    m_data.custom_value = ini.GetInt(L"custom_theme", L"value", m_data.custom_value) & 0x00ffffff;
    m_data.custom_accent = ini.GetInt(L"custom_theme", L"accent", m_data.custom_accent) & 0x00ffffff;
    m_data.custom_value_bold = ini.GetBool(L"custom_theme", L"bold", m_data.custom_value_bold);
    m_data.custom_style = max(0, min(ini.GetInt(L"custom_theme", L"style", m_data.custom_style), 4));
    m_data.tb_corner_radius = max(0, min(ini.GetInt(L"custom_theme", L"radius", m_data.tb_corner_radius), 32));
    m_data.tb_panel_opacity = max(1, min(ini.GetInt(L"custom_theme", L"opacity", m_data.tb_panel_opacity), 100));
    return true;
}

void CCustomThemeDlg::OnBnClickedImportBtn()
{
    CFileDialog dlg(TRUE, _T("ini"), nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        _T("Theme files (*.ini)|*.ini|All files (*.*)|*.*||"), this);
    if (dlg.DoModal() == IDOK)
    {
        if (ImportThemeFromFile(dlg.GetPathName().GetString()))
            LoadDraft();
        else
            MessageBox(CCommon::LoadText(L"TXT_SETTINGS_THEME_IMPORT_ERROR"), nullptr, MB_OK | MB_ICONWARNING);
    }
}

void CCustomThemeDlg::OnBnClickedExportBtn()
{
    CFileDialog dlg(FALSE, _T("ini"), _T("custom_theme"), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("Theme files (*.ini)|*.ini|All files (*.*)|*.*||"), this);
    if (dlg.DoModal() == IDOK && !ExportThemeToFile(dlg.GetPathName().GetString()))
        MessageBox(CCommon::LoadText(L"TXT_SETTINGS_THEME_EXPORT_ERROR"), nullptr, MB_OK | MB_ICONWARNING);
}
