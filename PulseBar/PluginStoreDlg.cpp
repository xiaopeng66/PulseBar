// PluginStoreDlg.cpp: 插件商店对话框
//

#include "stdafx.h"
#include "PulseBar.h"
#include "PluginStoreDlg.h"
#include "FilePathHelper.h"
#include <urlmon.h>
#include <bcrypt.h>
#include <thread>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "bcrypt.lib")

namespace
{
#ifndef IMAGE_FILE_MACHINE_ARM64EC
#define IMAGE_FILE_MACHINE_ARM64EC 0xA641
#endif

    constexpr UINT_PTR WorkTimer = 1;

    //读取 PE 的 Machine 字段：提前拒绝架构不匹配的插件，避免白重启一次
    WORD GetPeMachineFromStore(const std::wstring& path)
    {
        HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return 0;
        BYTE buffer[4096]{};
        DWORD read_size{};
        const BOOL read_ok = ::ReadFile(handle, buffer, static_cast<DWORD>(sizeof(buffer)), &read_size, nullptr);
        ::CloseHandle(handle);
        if (!read_ok || read_size < sizeof(IMAGE_DOS_HEADER))
            return 0;
        IMAGE_DOS_HEADER dos_header{};
        memcpy(&dos_header, buffer, sizeof(dos_header));
        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE || dos_header.e_lfanew < static_cast<LONG>(sizeof(dos_header)))
            return 0;
        const DWORD nt_offset = static_cast<DWORD>(dos_header.e_lfanew);
        if (nt_offset >= read_size || read_size - nt_offset < sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER))
            return 0;
        DWORD signature{};
        memcpy(&signature, buffer + nt_offset, sizeof(signature));
        if (signature != IMAGE_NT_SIGNATURE)
            return 0;
        IMAGE_FILE_HEADER header{};
        memcpy(&header, buffer + nt_offset + sizeof(signature), sizeof(header));
        return (header.Characteristics & IMAGE_FILE_DLL) ? header.Machine : 0;
    }

    WORD GetHostMachine()
    {
#if defined(_M_ARM64EC)
        return IMAGE_FILE_MACHINE_ARM64EC;
#elif defined(_M_ARM64)
        return IMAGE_FILE_MACHINE_ARM64;
#elif defined(_M_X64)
        return IMAGE_FILE_MACHINE_AMD64;
#else
        return IMAGE_FILE_MACHINE_I386;
#endif
    }

    //兼容性仍以下载文件的实际架构为准。
    bool IsPluginArchCompatible(WORD machine, WORD host)
    {
        if (machine == host)
            return true;
#if defined(_M_ARM64EC)
        if (machine == IMAGE_FILE_MACHINE_AMD64)
            return true;
#endif
        return false;
    }

    //清单来自网络，file_name 必须是纯文件名：防止 "..\\x.dll" 之类穿越到插件目录之外
    bool IsSafePluginFileName(const CString& declared_name, CString& safe_name)
    {
        safe_name = CFilePathHelper(declared_name.GetString()).GetFileName().c_str();
        if (safe_name.IsEmpty() || safe_name.CompareNoCase(declared_name) != 0)
            return false;   // 含路径部分（GetFileName 提取后与原值不同）
        if (safe_name.GetLength() <= 4 || safe_name.Right(4).CompareNoCase(L".dll") != 0)
            return false;
        if (safe_name.FindOneOf(L"\\/:*?\"<>|") >= 0)
            return false;
        //Windows 保留设备名不能作为普通文件名：CON / PRN / AUX / NUL / COM1-9 / LPT1-9
        CString stem = safe_name;
        const int dot = stem.Find(L'.');
        if (dot > 0)
            stem = stem.Left(dot);
        stem.TrimRight();
        stem.MakeUpper();
        if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL")
            return false;
        if (stem.GetLength() == 4
            && (stem.Left(3) == L"COM" || stem.Left(3) == L"LPT")
            && stem[3] >= L'1' && stem[3] <= L'9')
            return false;
        return true;
    }

    //URLDownloadToFile 按二进制原样落盘，避免 ReadString 破坏 DLL
    bool DownloadBinaryFile(const std::wstring& url, const std::wstring& save_path)
    {
        if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
            return false;
        const HRESULT hr = ::URLDownloadToFileW(nullptr, url.c_str(), save_path.c_str(), 0, nullptr);
        ::CoUninitialize();
        return SUCCEEDED(hr);
    }

    std::wstring ComputeSha256Hex(const std::wstring& file_path)
    {
        HANDLE file = ::CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return std::wstring();

        BCRYPT_ALG_HANDLE algorithm{};
        if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        {
            ::CloseHandle(file);
            return std::wstring();
        }

        DWORD hash_length{};
        DWORD result_length{};
        if (!BCRYPT_SUCCESS(::BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hash_length), sizeof(hash_length), &result_length, 0))
            || hash_length == 0)
        {
            ::BCryptCloseAlgorithmProvider(algorithm, 0);
            ::CloseHandle(file);
            return std::wstring();
        }

        BCRYPT_HASH_HANDLE hash{};
        bool ok = BCRYPT_SUCCESS(::BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
        std::vector<BYTE> buffer(64 * 1024);
        DWORD read_size{};
        while (ok)
        {
            ok = ::ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read_size, nullptr) != FALSE;
            if (!ok || read_size == 0)
                break;
            ok = BCRYPT_SUCCESS(::BCryptHashData(hash, buffer.data(), read_size, 0));
        }

        std::vector<UCHAR> digest(hash_length);
        if (ok)
            ok = BCRYPT_SUCCESS(::BCryptFinishHash(hash, digest.data(), hash_length, 0));
        if (hash != nullptr)
            ::BCryptDestroyHash(hash);
        ::BCryptCloseAlgorithmProvider(algorithm, 0);
        ::CloseHandle(file);

        if (!ok)
            return std::wstring();
        std::wstring hex;
        wchar_t pair[4]{};
        for (UCHAR byte : digest)
        {
            swprintf_s(pair, L"%02x", byte);
            hex += pair;
        }
        return hex;
    }

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

struct CPluginStoreDlg::Download
{
    std::wstring path;
    const wchar_t* error{ nullptr };

    ~Download()
    {
        if (!path.empty())
            ::DeleteFileW(path.c_str());
    }
};

IMPLEMENT_DYNAMIC(CPluginStoreDlg, CBaseDialog)

CPluginStoreDlg::CPluginStoreDlg(CWnd* pParent, const std::wstring& selected_file)
    : CBaseDialog(IDD_PLUGIN_STORE_DIALOG, pParent)
    , m_preferred_file(selected_file)
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
    if (m_download.valid())
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
    else if (!m_download.valid())
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
    CString safe_name;
    bool installed = false;
    if (IsSafePluginFileName(item.file_name.c_str(), safe_name))
    {
        const std::wstring path = theApp.m_module_dir + L"plugins\\" + safe_name.GetString();
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
    const bool downloading = m_download.valid();
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
    if (m_download.valid() || m_catalog_status == CPluginUpdateHelper::Status::Checking
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

    CString safe_name;
    if (!IsSafePluginFileName(item.file_name.c_str(), safe_name))
    {
        MessageBox(CCommon::LoadText(IDS_STORE_INVALID_NAME), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }

    const std::wstring directory = theApp.m_module_dir + L"plugins\\";
    const std::wstring target = directory + safe_name.GetString();
    if (::GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        const auto confirm = state == PluginStoreState::UpdateAvailable
            ? CCommon::LoadTextFormat(IDS_STORE_CONFIRM_UPDATE, {
                item.display_name.empty() ? item.file_name : item.display_name,
                local_version.GetVersionWString(), item.version.GetVersionWString() })
            : CCommon::LoadTextFormat(IDS_STORE_OVERWRITE_PROMPT, { safe_name.GetString() });
        if (MessageBox(confirm, nullptr, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES)
            return;
    }

    std::wstring url = item.download_path;
    if (url.rfind(L"http://", 0) != 0 && url.rfind(L"https://", 0) != 0)
        url = CPluginUpdateHelper::GetStoreBaseUrl() + url;
    try
    {
        //临时文件由结果对象持有。关闭窗口会放弃安装，线程退出时自行清理文件。
        std::packaged_task<std::shared_ptr<Download>()> task([directory, url, checksum = item.sha256]() {
            auto result = std::make_shared<Download>();
            wchar_t temp_path[MAX_PATH]{};
            ::CreateDirectoryW(directory.c_str(), nullptr);
            if (!::GetTempFileNameW(directory.c_str(), L"pbd", 0, temp_path))
            {
                result->error = IDS_STORE_REPLACE_FAILED;
                return result;
            }
            result->path = temp_path;
            if (!DownloadBinaryFile(url, result->path))
                result->error = IDS_STORE_DOWNLOAD_FAILED;
            else if (!checksum.empty() && _wcsicmp(ComputeSha256Hex(result->path).c_str(), checksum.c_str()) != 0)
                result->error = IDS_STORE_SHA_MISMATCH;
            else if (const auto machine = GetPeMachineFromStore(result->path); machine == 0)
                result->error = IDS_STORE_INVALID_DLL;
            else if (!IsPluginArchCompatible(machine, GetHostMachine()))
                result->error = IDS_STORE_ARCH_MISMATCH;
            return result;
        });
        auto download = task.get_future();
        std::thread(std::move(task)).detach();
        m_download = std::move(download);
        m_download_index = index;
        SetTimer(WorkTimer, 100, nullptr);
        UpdateControls();
    }
    catch (...)
    {
        MessageBox(CCommon::LoadText(IDS_STORE_DOWNLOAD_FAILED), nullptr, MB_ICONWARNING | MB_OK);
    }
}

void CPluginStoreDlg::FinishDownload()
{
    std::shared_ptr<Download> result;
    try
    {
        result = m_download.get();
    }
    catch (...)
    {
        //下载线程里的异常与下载失败一样处理。
    }
    const auto item = m_items[m_download_index];
    const wchar_t* error = result ? result->error : IDS_STORE_DOWNLOAD_FAILED;
    if (error == nullptr)
    {
        const auto target = theApp.m_module_dir + L"plugins\\" + item.file_name;
        switch (CommitPluginUpdate(result->path, target))
        {
        case PluginInstallResult::Installed:
            result->path.clear();
            theApp.m_plugin_update.MarkPendingRestart(item.file_name, item.version);
            break;
        case PluginInstallResult::InUse: error = IDS_STORE_IN_USE; break;
        case PluginInstallResult::Failed: error = IDS_STORE_REPLACE_FAILED; break;
        }
    }
    result.reset();
    RefreshList();
    if (error != nullptr)
    {
        MessageBox(CCommon::LoadText(error), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }

    const auto message = CCommon::LoadTextFormat(TXT_STORE_PENDING_VERSION, { item.version.GetVersionWString() })
        + CString(L"\r\n\r\n") + CCommon::LoadText(IDS_PLUGIN_INSTALL_RESTART_PROMPT);
    if (MessageBox(message, nullptr, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDYES)
        EndDialog(IDOK);
}

void CPluginStoreDlg::OnTimer(UINT_PTR timer)
{
    if (timer == WorkTimer)
    {
        if (m_download.valid())
        {
            if (m_download.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
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
