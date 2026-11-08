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

    //是否需要在关闭后重启程序以加载新装的插件
    bool m_restart_required{ false };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

public:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedStoreRefresh();
    afx_msg void OnBnClickedStoreInstall();
    afx_msg void OnNMClickStoreList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnItemchangedStoreList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMDblclkStoreList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnStoreFetchDone(WPARAM wParam, LPARAM lParam);   //后台拉取完成

private:
    void RefreshList();             //只按已有快照填充列表，不发起网络请求
    void StartFetch();              //后台线程拉取清单，完成后再回到 UI 线程刷新
    void UpdateDescription();
    void ResizeColumns();
    int GetSelectedIndex() const;
    int FindLocalState(const PluginStoreItem& item) const;   //0=未安装 1=已安装

    CListCtrl m_list;
    std::vector<PluginStoreItem> m_items;
    bool m_fetch_failed{ false };
    bool m_fetching{ false };       //后台拉取进行中：重复点击刷新不再起第二个线程
};
