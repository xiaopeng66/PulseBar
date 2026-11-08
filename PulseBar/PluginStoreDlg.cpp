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

    //后台拉取线程完成后通知 UI 线程；wParam=1 表示拉取成功
    constexpr UINT WM_STORE_FETCH_DONE = WM_APP + 520;

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
        const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(buffer);
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;
        const DWORD nt_offset = static_cast<DWORD>(dos_header->e_lfanew);
        if (nt_offset + sizeof(IMAGE_NT_HEADERS32) > read_size)
            return 0;
        const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS32*>(buffer + nt_offset);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
            return 0;
        return nt_headers->FileHeader.Machine;
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

    //ARM64EC 进程可加载 ARM64EC 与 ARM64 插件（也可经 CHPE 加载 AMD64）；
    //取得 Machine 为 0（非 PE 或读头失败）时交由加载期判定，不在这里拦截。
    bool IsPluginArchCompatible(WORD machine, WORD host)
    {
        if (machine == 0 || machine == host)
            return true;
#if defined(_M_ARM64EC)
        if (machine == IMAGE_FILE_MACHINE_ARM64 || machine == IMAGE_FILE_MACHINE_AMD64)
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
        if (safe_name == L"." || safe_name == L"..")
            return false;
        if (safe_name.FindOneOf(L"\\/:*?\"<>|") >= 0)
            return false;
        //Windows 保留设备名不能作为普通文件名：CON / PRN / AUX / NUL / COM1-9 / LPT1-9
        CString stem = safe_name;
        const int dot = stem.ReverseFind(L'.');
        if (dot > 0)
            stem = stem.Left(dot);
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
        const HRESULT hr = ::URLDownloadToFileW(nullptr, url.c_str(), save_path.c_str(), 0, nullptr);
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
        while (ok && ::ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read_size, nullptr) && read_size > 0)
            ok = BCRYPT_SUCCESS(::BCryptHashData(hash, buffer.data(), read_size, 0));

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

    //正在被本进程加载的 DLL 不能删除也不能覆盖，但可以改名让出文件名：
    //Windows 加载器以 FILE_SHARE_DELETE 打开映像，重命名成功、写入失败。
    //旧文件留在 plugins 目录里，下次启动时由 LoadPlugins 清理。
    bool MoveLoadedPluginAside(const CString& path)
    {
        for (int index = 0; index < 10; ++index)
        {
            CString backup;
            if (index == 0)
                backup.Format(L"%s.pb-old", path.GetString());
            else
                backup.Format(L"%s.pb-old%d", path.GetString(), index);

            //上一次留下的同名残留可能同样被占用：换下一个编号
            if (::DeleteFileW(backup) == FALSE && ::GetFileAttributesW(backup) != INVALID_FILE_ATTRIBUTES)
                continue;
            if (::MoveFileW(path, backup))
                return true;
        }
        return false;
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

void CPluginStoreDlg::DoDataExchange(CDataExchange* pDX)
{
    CBaseDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CPluginStoreDlg, CBaseDialog)
    ON_BN_CLICKED(IDC_STORE_REFRESH, &CPluginStoreDlg::OnBnClickedStoreRefresh)
    ON_BN_CLICKED(IDC_STORE_INSTALL, &CPluginStoreDlg::OnBnClickedStoreInstall)
    ON_NOTIFY(NM_CLICK, IDC_STORE_LIST, &CPluginStoreDlg::OnNMClickStoreList)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_STORE_LIST, &CPluginStoreDlg::OnLvnItemchangedStoreList)
    ON_NOTIFY(NM_DBLCLK, IDC_STORE_LIST, &CPluginStoreDlg::OnNMDblclkStoreList)
    ON_MESSAGE(WM_STORE_FETCH_DONE, &CPluginStoreDlg::OnStoreFetchDone)
    ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CPluginStoreDlg::OnInitDialog()
{
    CBaseDialog::OnInitDialog();

    m_list.SubclassDlgItem(IDC_STORE_LIST, this);
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_list.InsertColumn(0, CCommon::LoadText(TXT_STORE_COL_NAME), LVCFMT_LEFT, theApp.DPI(120));
    m_list.InsertColumn(1, CCommon::LoadText(TXT_STORE_COL_VERSION), LVCFMT_LEFT, theApp.DPI(60));
    m_list.InsertColumn(2, CCommon::LoadText(TXT_STORE_COL_AUTHOR), LVCFMT_LEFT, theApp.DPI(90));
    m_list.InsertColumn(3, CCommon::LoadText(TXT_STORE_COL_STATUS), LVCFMT_LEFT, theApp.DPI(70));

    //先用启动线程已经拉到的快照填充，对话框立刻可见；
    //快照为空时在后台补拉，绝不在 UI 线程上等 DNS/TLS/重定向。
    RefreshList();
    if (m_items.empty())
        StartFetch();
    ResizeColumns();
    return TRUE;
}

void CPluginStoreDlg::StartFetch()
{
    if (m_fetching)
        return;
    m_fetching = true;
    m_fetch_failed = false;
    CString loading_text = CCommon::LoadText(IDS_ACQUIRING);
    loading_text += L"…";
    SetDlgItemText(IDC_STORE_DESC, loading_text);
    if (CWnd* install = GetDlgItem(IDC_STORE_INSTALL))
        install->EnableWindow(FALSE);
    if (CWnd* refresh = GetDlgItem(IDC_STORE_REFRESH))
        refresh->EnableWindow(FALSE);

    //只捕获窗口句柄，不引用 this：对话框可能先被关闭，线程照常退出即可
    const HWND hwnd = GetSafeHwnd();
    std::thread([hwnd]() {
        const bool fetched = theApp.m_plugin_update.CheckForUpdate();
        if (::IsWindow(hwnd))
            ::PostMessage(hwnd, WM_STORE_FETCH_DONE, fetched ? 1 : 0, 0);
    }).detach();
}

LRESULT CPluginStoreDlg::OnStoreFetchDone(WPARAM wParam, LPARAM)
{
    m_fetching = false;
    if (CWnd* refresh = GetDlgItem(IDC_STORE_REFRESH))
        refresh->EnableWindow(TRUE);

    if (wParam == 0)
    {
        m_fetch_failed = true;
        m_items.clear();
        m_list.DeleteAllItems();
        SetDlgItemText(IDC_STORE_DESC, CCommon::LoadText(IDS_STORE_FETCH_FAILED));
        if (CWnd* install = GetDlgItem(IDC_STORE_INSTALL))
            install->EnableWindow(FALSE);
        return 0;
    }

    RefreshList();
    return 0;
}

void CPluginStoreDlg::RefreshList()
{
    m_list.DeleteAllItems();
    m_fetch_failed = false;

    m_items = theApp.m_plugin_update.GetStoreItems();

    int row = 0;
    for (const auto& item : m_items)
    {
        const CString name = item.display_name.empty() ? CString(item.file_name.c_str()) : CString(item.display_name.c_str());
        m_list.InsertItem(row, name);
        m_list.SetItemText(row, 1, item.version.GetVersionWString().c_str());
        m_list.SetItemText(row, 2, item.author.c_str());
        m_list.SetItemText(row, 3, CCommon::LoadText(
            FindLocalState(item) == 1 ? TXT_STORE_STATUS_INSTALLED : TXT_STORE_STATUS_NOT_INSTALLED));
        ++row;
    }

    if (row > 0)
    {
        m_list.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        UpdateDescription();
    }
    else
    {
        SetDlgItemText(IDC_STORE_DESC, CCommon::LoadText(TXT_STORE_EMPTY));
    }
    if (CWnd* install = GetDlgItem(IDC_STORE_INSTALL))
        install->EnableWindow(row > 0);
}

int CPluginStoreDlg::FindLocalState(const PluginStoreItem& item) const
{
    //插件加载自 exe 目录的 plugins\（见 CPluginManager::LoadPlugins），
    //安装状态必须按这个目录判断，而不是随配置目录走
    const CString path = CString(theApp.m_module_dir.c_str()) + L"plugins\\" + item.file_name.c_str();
    return (::GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
}

int CPluginStoreDlg::GetSelectedIndex() const
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (pos == nullptr)
        return -1;
    return m_list.GetNextSelectedItem(pos);
}

void CPluginStoreDlg::UpdateDescription()
{
    const int index = GetSelectedIndex();
    if (index < 0 || index >= static_cast<int>(m_items.size()))
    {
        SetDlgItemText(IDC_STORE_DESC, L"");
        return;
    }
    const auto& item = m_items[index];
    CString text;
    if (!item.description.empty())
        text = item.description.c_str();
    if (!item.file_name.empty())
        text += CString(L"\r\n") + item.file_name.c_str();
    if (!item.arch.empty())
        text += CString(L" · ") + item.arch.c_str();
    SetDlgItemText(IDC_STORE_DESC, text);
}

void CPluginStoreDlg::OnNMClickStoreList(NMHDR* pNMHDR, LRESULT* pResult)
{
    UpdateDescription();
    *pResult = 0;
}

void CPluginStoreDlg::OnLvnItemchangedStoreList(NMHDR* pNMHDR, LRESULT* pResult)
{
    //键盘改选也要刷新说明区，不能只依赖鼠标点击
    const auto* notify = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if ((notify->uChanged & LVIF_STATE) && (notify->uNewState & LVIS_SELECTED))
        UpdateDescription();
    *pResult = 0;
}

void CPluginStoreDlg::OnNMDblclkStoreList(NMHDR* pNMHDR, LRESULT* pResult)
{
    //空白处双击（iItem == -1）不触发安装
    const auto* activate = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);
    if (activate->iItem >= 0)
        OnBnClickedStoreInstall();
    *pResult = 0;
}

void CPluginStoreDlg::OnBnClickedStoreRefresh()
{
    StartFetch();   //手动刷新：后台联网，界面不冻结
}

void CPluginStoreDlg::OnBnClickedStoreInstall()
{
    const int index = GetSelectedIndex();
    if (index < 0 || index >= static_cast<int>(m_items.size()))
        return;
    const PluginStoreItem item = m_items[index];
    if (item.download_path.empty())
    {
        MessageBox(CCommon::LoadText(IDS_STORE_NO_DOWNLOAD), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }

    //清单来自网络：文件名先归一化校验，防止路径穿越写到插件目录之外
    CString safe_name;
    if (!IsSafePluginFileName(item.file_name.c_str(), safe_name))
    {
        MessageBox(CCommon::LoadText(IDS_STORE_INVALID_NAME), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }

    //插件必须装到 exe 目录的 plugins\（LoadPlugins 从这里加载）
    const CString plugin_dir = CString(theApp.m_module_dir.c_str()) + L"plugins\\";
    ::CreateDirectoryW(plugin_dir, nullptr);
    const CString target_path = plugin_dir + safe_name;
    const CString temp_path = plugin_dir + safe_name + L".download";

    //download 可以是完整链接（GitHub Release 资产），也可以是仓库内的相对路径
    std::wstring url = item.download_path;
    if (url.rfind(L"http://", 0) != 0 && url.rfind(L"https://", 0) != 0)
        url = theApp.m_plugin_update.GetStoreBaseUrl() + item.download_path;
    {
        CWaitCursor wait_cursor;
        if (!DownloadBinaryFile(url, temp_path.GetString()))
        {
            ::DeleteFileW(temp_path);
            MessageBox(CCommon::LoadText(IDS_STORE_DOWNLOAD_FAILED), nullptr, MB_ICONWARNING | MB_OK);
            return;
        }
    }

    //清单给了校验值就必须匹配，否则删掉下载文件
    if (!item.sha256.empty())
    {
        const std::wstring actual = ComputeSha256Hex(temp_path.GetString());
        if (_wcsicmp(actual.c_str(), item.sha256.c_str()) != 0)
        {
            ::DeleteFileW(temp_path);
            MessageBox(CCommon::LoadText(IDS_STORE_SHA_MISMATCH), nullptr, MB_ICONWARNING | MB_OK);
            return;
        }
    }

    //下载完成后校验实际架构：清单的 arch 可能标错，以 DLL 本身为准
    {
        const WORD machine = GetPeMachineFromStore(temp_path.GetString());
        if (!IsPluginArchCompatible(machine, GetHostMachine()))
        {
            ::DeleteFileW(temp_path);
            MessageBox(CCommon::LoadText(IDS_STORE_ARCH_MISMATCH), nullptr, MB_ICONWARNING | MB_OK);
            return;
        }
    }

    //已存在同名插件：覆盖前必须确认，避免误操作替换掉正在用的版本
    if (::GetFileAttributesW(target_path) != INVALID_FILE_ATTRIBUTES)
    {
        CString confirm = CCommon::LoadTextFormat(IDS_STORE_OVERWRITE_PROMPT, { safe_name.GetString() });
        if (MessageBox(confirm, nullptr, MB_ICONQUESTION | MB_YESNO) != IDYES)
        {
            ::DeleteFileW(temp_path);
            return;
        }
        //删不掉说明正被程序加载：改名让出文件名，新文件顶替，重启后即加载新版本。
        //只有连改名都失败（极少数被独占打开的 DLL）才让用户先退出程序。
        if (::DeleteFileW(target_path) == FALSE && ::GetFileAttributesW(target_path) != INVALID_FILE_ATTRIBUTES)
        {
            if (!MoveLoadedPluginAside(target_path))
            {
                ::DeleteFileW(temp_path);
                MessageBox(CCommon::LoadText(IDS_STORE_IN_USE), nullptr, MB_ICONWARNING | MB_OK);
                return;
            }
        }
    }
    if (!::MoveFileW(temp_path, target_path))
    {
        ::DeleteFileW(temp_path);
        MessageBox(CCommon::LoadText(IDS_STORE_DOWNLOAD_FAILED), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }

    m_list.SetItemText(index, 3, CCommon::LoadText(TXT_STORE_STATUS_INSTALLED));

    CString message = CCommon::LoadTextFormat(IDS_STORE_INSTALL_SUCCESS, { safe_name.GetString() });
    message += L"\r\n\r\n";
    message += CCommon::LoadText(IDS_PLUGIN_INSTALL_RESTART_PROMPT);
    if (MessageBox(message, nullptr, MB_ICONQUESTION | MB_YESNO) == IDYES)
    {
        m_restart_required = true;
        EndDialog(IDOK);
    }
}

void CPluginStoreDlg::ResizeColumns()
{
    if (!::IsWindow(m_list.GetSafeHwnd()))
        return;
    CRect rect;
    m_list.GetClientRect(&rect);
    if (rect.Width() <= 0)
        return;
    CRect unit_dlu(0, 0, 1, 1);
    MapDialogRect(&unit_dlu);
    const int unit_x = max(1, unit_dlu.Width());
    const int usable = max(1, rect.Width() - unit_x * 20 - 1);
    const int width1 = usable * 18 / 100;      //版本
    const int width2 = usable * 25 / 100;      //作者
    const int width3 = usable * 18 / 100;      //状态
    const int width0 = max(1, usable - width1 - width2 - width3);
    m_list.SetColumnWidth(0, width0);
    m_list.SetColumnWidth(1, width1);
    m_list.SetColumnWidth(2, width2);
    m_list.SetColumnWidth(3, width3);
}

void CPluginStoreDlg::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);
    if (m_list.GetSafeHwnd() == nullptr)
        return;

    CRect rect;
    GetClientRect(&rect);

    //模板尺寸是 DLU，必须经 MapDialogRect 换算成像素；
    //直接当 px 用会把按钮压到低于字体行高（文字被裁）
    auto to_px = [this](int width_dlu, int height_dlu) {
        CRect r(0, 0, width_dlu, height_dlu);
        MapDialogRect(&r);
        return CSize(max(1, r.Width()), max(1, r.Height()));
    };
    const CSize margin_size = to_px(7, 7);
    const CSize gap_size = to_px(1, 5);
    const CSize desc_size = to_px(1, 38);
    const CSize button_size = to_px(1, 14);
    const CSize refresh_size = to_px(60, 1);
    const CSize install_size = to_px(80, 1);
    const CSize close_size = to_px(50, 1);
    const CSize button_gap = to_px(8, 1);

    const int margin = margin_size.cx;
    const int gap = gap_size.cy;
    const int desc_height = desc_size.cy;
    const int button_height = button_size.cy;
    const int content_width = max(1, rect.Width() - margin * 2);

    //列表占据除说明区和按钮行之外的剩余高度
    const int list_height = max(button_height * 3,
        rect.Height() - margin - gap * 2 - desc_height - button_height - margin);
    m_list.MoveWindow(margin, margin, content_width, list_height);

    const int desc_top = margin + list_height + gap;
    if (CWnd* desc = GetDlgItem(IDC_STORE_DESC))
        desc->MoveWindow(margin, desc_top, content_width, desc_height);

    const int button_top = desc_top + desc_height + gap;
    if (CWnd* refresh = GetDlgItem(IDC_STORE_REFRESH))
        refresh->MoveWindow(margin, button_top, refresh_size.cx, button_height);
    if (CWnd* install = GetDlgItem(IDC_STORE_INSTALL))
        install->MoveWindow(margin + refresh_size.cx + button_gap.cx, button_top,
            install_size.cx, button_height);
    if (CWnd* close = GetDlgItem(IDCANCEL))
        close->MoveWindow(margin + content_width - close_size.cx, button_top,
            close_size.cx, button_height);

    ResizeColumns();
}
