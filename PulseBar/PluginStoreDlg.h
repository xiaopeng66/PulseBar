#pragma once
#include "BaseDialog.h"
#include "PluginUpdateHelper.h"

//插件商店：列出云端清单里的插件，选择后下载到 plugins 目录
class CPluginStoreDlg : public CBaseDialog
{
    DECLARE_DYNAMIC(CPluginStoreDlg)
public:
    CPluginStoreDlg(CWnd* pParent = nullptr);
    virtual ~CPluginStoreDlg();

    enum { IDD = IDD_PLUGIN_STORE_DIALOG };

protected:
    virtual void OnOK() override {}     //仅显式选择重启时以 IDOK 退出
    DECLARE_MESSAGE_MAP()

public:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedStoreRefresh();
    afx_msg void OnBnClickedStoreInstall();
    afx_msg void OnLvnItemchangedStoreList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMDblclkStoreList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR timer);

private:
    void RefreshList();
    void StartFetch();
    void FinishDownload();
    void UpdateControls();
    void ResizeColumns();
    int GetSelectedIndex() const;
    PluginStoreState FindLocalState(const PluginStoreItem& item, PluginVersion& local_version) const;

    CListCtrl m_list;
    std::vector<PluginStoreItem> m_items;
    std::wstring m_preferred_file;
    CPluginUpdateHelper::Status m_catalog_status{ CPluginUpdateHelper::Status::Idle };
    bool m_has_snapshot{ false };
    int m_update_count{ 0 };
    CPluginPackageInstall m_install;
};
