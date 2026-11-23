// PluginStoreDlg.cpp: 插件商店对话框
//

#include "stdafx.h"
#include "PulseBar.h"
#include "PluginStoreDlg.h"
#include "FilePathHelper.h"

namespace
{
    constexpr UINT_PTR WorkTimer = 1;

    const wchar_t* StoreStateText(PluginStoreState state)
    {
        switch (state)
        {
        case PluginStoreState::Available: return TXT_STORE_STATUS_NOT_INSTALLED;
        case PluginStoreState::UpdateAvailable: return TXT_PLUGIN_AVAILABLE_UPDATE;
        case PluginStoreState::NewerLocally: return TXT_STORE_LOCAL_NEWER;
        case PluginStoreState::PendingRestart: return TXT_PLUGIN_PENDING_RESTART;
        case PluginStoreState::Incompatible: return TXT_STORE_INCOMPATIBLE;
        default: return TXT_STORE_STATUS_INSTALLED;
        }
    }
}

IMPLEMENT_DYNAMIC(CPluginStoreDlg, CBaseDialog)

CPluginStoreDlg::CPluginStoreDlg(CWnd* pParent)
    : CBaseDialog(IDD_PLUGIN_STORE_DIALOG, pParent)
{
}

CPluginStoreDlg::~CPluginStoreDlg()
{
}

BEGIN_MESSAGE_MAP(CPluginStoreDlg, CBaseDialog)
    ON_BN_CLICKED(IDC_STORE_REFRESH, &CPluginStoreDlg::OnBnClickedStoreRefresh)
    ON_BN_CLICKED(IDC_STORE_INSTALL, &CPluginStoreDlg::OnBnClickedStoreInstall)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_STORE_LIST, &CPluginStoreDlg::OnLvnItemchangedStoreList)
    ON_NOTIFY(NM_DBLCLK, IDC_STORE_LIST, &CPluginStoreDlg::OnNMDblclkStoreList)
    ON_WM_SIZE()
    ON_WM_TIMER()
END_MESSAGE_MAP()

BOOL CPluginStoreDlg::OnInitDialog()
{
    CBaseDialog::OnInitDialog();
    SetIcon(theApp.GetMenuIcon(IDI_PLUGINS), FALSE);
    SetBackgroundColor(GetSysColor(COLOR_WINDOW));
    m_list.SubclassDlgItem(IDC_STORE_LIST, this);
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_DOUBLEBUFFER);
    m_list.InsertColumn(0, CCommon::LoadText(TXT_STORE_COL_NAME), LVCFMT_LEFT, 1);
    m_list.InsertColumn(1, CCommon::LoadText(TXT_STORE_COL_LOCAL_VERSION), LVCFMT_LEFT, 1);
    m_list.InsertColumn(2, CCommon::LoadText(TXT_STORE_COL_CATALOG_VERSION), LVCFMT_LEFT, 1);
    m_list.InsertColumn(3, CCommon::LoadText(TXT_STORE_COL_STATUS), LVCFMT_LEFT, 1);

    StartFetch();
    ResizeColumns();
    return TRUE;
}

void CPluginStoreDlg::StartFetch()
{
    if (m_install.IsActive())
        return;
    theApp.m_plugin_update.RefreshAsync();
    RefreshList();
}

void CPluginStoreDlg::RefreshList()
{
    const int selected = GetSelectedIndex();
    if (selected >= 0 && selected < static_cast<int>(m_items.size()))
        m_preferred_file = m_items[selected].file_name;

    auto catalog = theApp.m_plugin_update.GetCatalog();
    m_catalog_status = catalog.status;
    m_has_snapshot = catalog.has_snapshot;
    m_list.SetRedraw(FALSE);
    m_list.DeleteAllItems();
    m_items = std::move(catalog.items);

    int row = 0;
    int preferred_row = -1;
    int first_update = -1;
    m_update_count = 0;
    for (const auto& item : m_items)
    {
        PluginVersion local_version;
        const auto state = FindLocalState(item, local_version);
        const auto name = item.display_name.empty() ? item.file_name.c_str() : item.display_name.c_str();
        const auto local_text = local_version.GetVersionWString();
        m_list.InsertItem(row, name);
        m_list.SetItemText(row, 1, local_text.empty() ? L"—" : local_text.c_str());
        m_list.SetItemText(row, 2, item.version.GetVersionWString().c_str());
        m_list.SetItemText(row, 3, CCommon::LoadText(StoreStateText(state)));
        if (_wcsicmp(item.file_name.c_str(), m_preferred_file.c_str()) == 0)
            preferred_row = row;
        if (state == PluginStoreState::UpdateAvailable)
        {
            if (first_update < 0)
                first_update = row;
            ++m_update_count;
        }
        ++row;
    }

    if (row > 0)
    {
        const int selection = preferred_row >= 0 ? preferred_row : max(0, first_update);
        m_list.SetItemState(selection, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_list.EnsureVisible(selection, FALSE);
    }
    m_list.SetRedraw(TRUE);
    m_list.Invalidate();
    if (m_catalog_status == CPluginUpdateHelper::Status::Checking)
        SetTimer(WorkTimer, 100, nullptr);
    else if (!m_install.IsActive())
        KillTimer(WorkTimer);
    UpdateControls();
}

PluginStoreState CPluginStoreDlg::FindLocalState(const PluginStoreItem& item, PluginVersion& local_version) const
{
    const auto pending = theApp.m_plugin_update.GetPendingVersion(item.file_name);
    for (const auto& plugin : theApp.m_plugins.GetPlugins())
    {
        const auto file_name = CFilePathHelper(plugin.file_path).GetFileName();
        if (_wcsicmp(file_name.c_str(), item.file_name.c_str()) == 0)
        {
            local_version = PluginVersion(plugin.Property(ITMPlugin::TMI_VERSION));
            return GetPluginStoreState(item, true, local_version, pending);
        }
    }

    //也识别已复制到插件目录、但尚未被当前进程加载的 DLL。
    bool installed = false;
    if (IsSafePluginFileName(item.file_name))
    {
        const std::wstring path = theApp.m_module_dir + L"plugins\\" + item.file_name;
        const DWORD attributes = ::GetFileAttributesW(path.c_str());
        installed = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }
    return GetPluginStoreState(item, installed, local_version, pending);
}

int CPluginStoreDlg::GetSelectedIndex() const
{
    return m_list.GetNextItem(-1, LVNI_SELECTED);
}

void CPluginStoreDlg::UpdateControls()
{
    using Status = CPluginUpdateHelper::Status;
    const bool downloading = m_install.IsActive();
    const bool checking = m_catalog_status == Status::Checking;
    CString status;
    if (downloading)
        status = CCommon::LoadText(TXT_STORE_DOWNLOADING);
    else if (checking)
        status = CCommon::LoadText(TXT_PLUGIN_CHECKING);
    else if (m_catalog_status == Status::Failed)
        status = CCommon::LoadText(m_has_snapshot ? TXT_PLUGIN_CHECK_FAILED_CACHED : TXT_PLUGIN_CHECK_FAILED);
    else if (m_items.empty())
        status = CCommon::LoadText(TXT_STORE_EMPTY);
    else if (m_update_count > 0)
        status = CCommon::LoadTextFormat(TXT_PLUGIN_UPDATE_COUNT, { m_update_count });
    else if (theApp.m_plugin_update.HasPendingRestart())
        status = CCommon::LoadText(TXT_PLUGIN_PENDING_RESTART);
    else
        status = CCommon::LoadText(TXT_PLUGIN_NO_UPDATES);
    SetDlgItemText(IDC_STORE_STATUS, status);
    EnableDlgCtrl(IDC_STORE_REFRESH, !checking && !downloading);

    CString description;
    const wchar_t* action = TXT_STORE_INSTALL;
    bool can_act = false;
    const int index = GetSelectedIndex();
    if (index >= 0 && index < static_cast<int>(m_items.size()))
    {
        const auto& item = m_items[index];
        PluginVersion local_version;
        const auto state = FindLocalState(item, local_version);
        can_act = !item.download_path.empty();
        switch (state)
        {
        case PluginStoreState::UpdateAvailable: action = TXT_STORE_UPDATE; break;
        case PluginStoreState::Installed: action = TXT_STORE_REINSTALL; break;
        case PluginStoreState::PendingRestart: action = TXT_PLUGIN_RESTART; can_act = true; break;
        case PluginStoreState::Incompatible:
        case PluginStoreState::NewerLocally: can_act = false; break;
        default: break;
        }
        description = item.description.c_str();
        if (!description.IsEmpty())
            description += L"\r\n";
        description += item.file_name.c_str();
        if (!item.author.empty())
            description += CString(L" · ") + item.author.c_str();
        if (!item.arch.empty())
            description += CString(L" · ") + item.arch.c_str();
        const auto pending = theApp.m_plugin_update.GetPendingVersion(item.file_name);
        if (state == PluginStoreState::PendingRestart)
            description += CString(L"\r\n") + CCommon::LoadTextFormat(TXT_STORE_PENDING_VERSION, { pending.GetVersionWString() });
    }
    SetDlgItemText(IDC_STORE_DESC, description);
    SetDlgItemText(IDC_STORE_INSTALL, CCommon::LoadText(action));
    EnableDlgCtrl(IDC_STORE_INSTALL, can_act && !checking && !downloading);
}

void CPluginStoreDlg::OnLvnItemchangedStoreList(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto* notify = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if ((notify->uChanged & LVIF_STATE) && ((notify->uOldState ^ notify->uNewState) & LVIS_SELECTED))
        UpdateControls();
    *pResult = 0;
}

void CPluginStoreDlg::OnNMDblclkStoreList(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto* activate = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);
    if (activate->iItem >= 0)
        OnBnClickedStoreInstall();
    *pResult = 0;
}

void CPluginStoreDlg::OnBnClickedStoreRefresh()
{
    StartFetch();
}

void CPluginStoreDlg::OnBnClickedStoreInstall()
{
    const int index = GetSelectedIndex();
    if (m_install.IsActive() || m_catalog_status == CPluginUpdateHelper::Status::Checking
        || index < 0 || index >= static_cast<int>(m_items.size()))
        return;
    const PluginStoreItem item = m_items[index];
    PluginVersion local_version;
    const auto state = FindLocalState(item, local_version);
    if (state == PluginStoreState::PendingRestart)
    {
        EndDialog(IDOK);
        return;
    }
    if (state == PluginStoreState::Incompatible || state == PluginStoreState::NewerLocally || item.download_path.empty())
        return;

    if (!IsSafePluginFileName(item.file_name))
    {
        MessageBox(CCommon::LoadText(IDS_STORE_INVALID_NAME), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }

    const std::wstring directory = theApp.m_module_dir + L"plugins\\";
    const std::wstring target = directory + item.file_name;
    if (::GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        const auto confirm = state == PluginStoreState::UpdateAvailable
            ? CCommon::LoadTextFormat(IDS_STORE_CONFIRM_UPDATE, {
                item.display_name.empty() ? item.file_name : item.display_name,
                local_version.GetVersionWString(), item.version.GetVersionWString() })
            : CCommon::LoadTextFormat(IDS_STORE_OVERWRITE_PROMPT, { item.file_name });
        if (MessageBox(confirm, nullptr, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES)
            return;
    }

    if (const auto* error = m_install.Start(item, directory))
    {
        MessageBox(CCommon::LoadText(error), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }
    SetTimer(WorkTimer, 100, nullptr);
    UpdateControls();
}

void CPluginStoreDlg::FinishDownload()
{
    const auto result = m_install.Finish(theApp.m_plugin_update);
    RefreshList();
    if (result.error != nullptr)
    {
        MessageBox(CCommon::LoadText(result.error), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }

    const auto message = CCommon::LoadTextFormat(TXT_STORE_PENDING_VERSION, { result.version.GetVersionWString() })
        + CString(L"\r\n\r\n") + CCommon::LoadText(IDS_PLUGIN_INSTALL_RESTART_PROMPT);
    if (MessageBox(message, nullptr, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDYES)
        EndDialog(IDOK);
}

void CPluginStoreDlg::OnTimer(UINT_PTR timer)
{
    if (timer == WorkTimer)
    {
        if (m_install.IsActive())
        {
            if (m_install.IsReady())
                FinishDownload();
        }
        else if (theApp.m_plugin_update.GetStatus() != CPluginUpdateHelper::Status::Checking)
            RefreshList();
        return;
    }
    CBaseDialog::OnTimer(timer);
}

void CPluginStoreDlg::ResizeColumns()
{
    if (!::IsWindow(m_list.GetSafeHwnd()))
        return;
    CRect rect;
    m_list.GetClientRect(&rect);
    if (rect.Width() <= 0)
        return;
    const int usable = max(4, rect.Width() - theApp.DPI(18));
    const int widths[] = { 39, 20, 20, 21 };
    for (int column = 0; column < 4; ++column)
        m_list.SetColumnWidth(column, max(1, usable * widths[column] / 100));
}

void CPluginStoreDlg::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);
    if (nType != SIZE_MINIMIZED)
        ResizeColumns();
}
