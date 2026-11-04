#pragma once
#include "ColorStatic.h"
#include "afxwin.h"
#include "SpinEdit.h"
#include "TabDlg.h"
#include "TaskbarColorDlg.h"
#include "ComboBox2.h"
#include "CustomThemeDlg.h"

// CTaskBarSettingsDlg 对话框

class CTaskBarSettingsDlg : public CTabDlg
{
    DECLARE_DYNAMIC(CTaskBarSettingsDlg)

public:
    CTaskBarSettingsDlg(CWnd* pParent = NULL);   // 标准构造函数
    virtual ~CTaskBarSettingsDlg();

    bool IsStyleModified();

    //如果开启了自动适应Windows10深色/浅色模式功能时，将当前配置保存到对应预设
    void SaveColorSettingToDefaultStyle();
    bool CollectSettings();
    void ShowSection(int section, int height);
    //外观/位置与交互分区延迟初始化：首次显示某分区前由外层调用，幂等。
    void EnsureSectionReady(int section);
    virtual void OnSettingsApplied() override;


    //选项设置数据
    TaskBarSettingData m_data;

	CWinVersionHelper m_win_version;        //当前Windows的版本

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TASKBAR_SETTINGS_DIALOG };
#endif

protected:
    //控件变量
    CColorStatic m_text_color_static;
    CColorStatic m_back_color_static;
    CColorStatic m_status_bar_color_static;
    CToolTipCtrl m_toolTip;
    CComboBox2 m_unit_combo;
    CButton m_hide_unit_chk;
    CSpinEdit m_font_size_edit;
    CComboBox2 m_double_click_combo;
    CComboBox2 m_digit_number_combo;
    CMenu m_default_style_menu;
    CMenu m_modify_default_style_menu;
    CButton m_background_transparent_chk;
    CButton m_atuo_adapt_light_theme_chk;
    CButton m_auto_set_back_color_chk;
    CComboBox2 m_memory_display_combo;
    CSpinEdit m_item_space_edit;
    CSpinEdit m_vertical_margin_edit;
    CSpinEdit m_net_speed_figure_max_val_edit;
    CComboBox2 m_net_speed_figure_max_val_unit_combo;
    CComboBox2 m_displays_combo;
    CComboBox2 m_theme_combo;
    CComboBox2 m_hover_combo;
    CSpinEdit m_opacity_edit;
    CSpinEdit m_corner_edit;
    CStatic m_theme_preview;
    struct SectionControl
    {
        HWND hwnd{};
        CRect rect;
        int section{};
        bool visible{};
        int id{};           //控件 ID（InitializeSections 一次性记录）
        bool is_edit{};     //是否单行 EDIT（重排高度需要单独计算）
    };
    std::vector<SectionControl> m_section_controls;
    std::map<int, TaskBarSettingData> m_pending_presets;
    int m_section{-1};
    int m_section_height{};
    bool m_loading{ true };
    bool m_rendering_visible{};
    bool m_section_ready[3]{ true, false, false };  //分区0立即初始化，1/2延迟

    bool m_style_modified{};

protected:
    void DrawStaticColor();
    void IniUnitCombo();

    void ApplyDefaultStyle(int index);      //应用一个预设方案

public:
    void ModifyDefaultStyle(int index);     //将当前颜色设置保存到一个预设方案

protected:
    void EnableControl();
    void EnsureAppearanceSection();     //外观分区延迟初始化
    void EnsureInteractionSection();    //位置与交互分区延迟初始化
    void EnsureTooltip();               //工具提示延迟到第一次鼠标移动
    virtual void SetControlMouseWheelEnable(bool enable) override;
    virtual bool InitializeControls() override;

    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedSetFontButton1();
    afx_msg void OnBnClickedTaskbarWndOnLeftCheck();
    afx_msg void OnBnClickedSpeedShortModeCheck();
    afx_msg void OnBnClickedCustomThemeBtn();   //S5: 打开自定义主题编辑器
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    afx_msg void OnCbnSelchangeUnitCombo();
    afx_msg void OnBnClickedHideUnitCheck();
    virtual void OnOK();
    afx_msg void OnBnClickedValueRightAlignCheck();
    afx_msg void OnBnClickedHidePercentageCheck();
protected:
    afx_msg LRESULT OnStaticClicked(WPARAM wParam, LPARAM lParam);
public:
    afx_msg void OnBnClickedSpecifyEachItemColorCheck();
    afx_msg void OnCbnSelchangeDoubleClickCombo();
    afx_msg void OnBnClickedHorizontalArrangeCheck();
    afx_msg void OnBnClickedShowStatusBarCheck();
    afx_msg void OnBnClickedSeparateValueUnitCheck();
    afx_msg void OnBnClickedUnitByteRadio();
    afx_msg void OnBnClickedUnitBitRadio();
    afx_msg void OnBnClickedDefaultStyleButton();
    afx_msg void OnDestroy();
    afx_msg void OnBnClickedBrowseButton();
    afx_msg void OnBnClickedCMGraphBarRadio();
    afx_msg void OnBnClickedCMGraphPLOTRadio();
    afx_msg void OnBnClickedBackgroundTransparentCheck();
    afx_msg void OnBnClickedAutoAdaptSettingsButton();
    afx_msg void OnBnClickedAutoAdaptLightThemeCheck();
    afx_msg void OnBnClickedAutoSetBackColorCheck();
    afx_msg void OnCbnSelchangeMemoryDisplayCombo();
    afx_msg void OnBnClickedShowDashedBox();
    afx_msg void OnBnClickedSetOrderButton();
    afx_msg void OnEnChangeItemSpaceEdit();
    afx_msg void OnEnChangeVerticalMarginEdit();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    afx_msg void OnBnClickedShowNetSpeedFigureCheck();
    afx_msg void OnCbnSelchangeNetSpeedFigureMaxValueUnitCombo();
    afx_msg void OnEnChangeNetSpeedFigureMaxValueEdit();
    afx_msg void OnBnClickedGdiRadio();
    afx_msg void OnBnClickedD2dRadio();
    afx_msg void OnBnClickedEnableColorEmojiCheck();
    afx_msg void OnCbnSelchangeDigitNumberCombo();
    afx_msg void OnBnClickedWin11SettingsButton();
    afx_msg void OnCbnSelchangeDisplayToShowTaskbarWndCombo();
    afx_msg void OnBnClickedUsageGraphFollowSystemCheck();
    afx_msg void OnEnChangeFontSizeEdit1();
    afx_msg void OnThemeChanged();
    afx_msg void OnRenderingSettings();
    afx_msg void OnHoverChanged();
    afx_msg void OnBackgroundImageChanged();
    afx_msg void OnSelectBackgroundImage();
    afx_msg void OnThemeDimensionsChanged();
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    void InitializeSections();
    void RefreshThemeControls();
    void RefreshSectionVisibility();
};
