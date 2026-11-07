// PluginStoreDlg.cpp: 插件商店对话框
//

#include "stdafx.h"
#include "PulseBar.h"
#include "PluginStoreDlg.h"
#include "FilePathHelper.h"
#include <urlmon.h>
#include <bcrypt.h>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "bcrypt.lib")

namespace
{
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
        ::BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hash_length), sizeof(hash_length), &result_length, 0);

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
    ON_NOTIFY(NM_DBLCLK, IDC_STORE_LIST, &CPluginStoreDlg::OnNMDblclkStoreList)
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

    RefreshList();
    return TRUE;
}

void CPluginStoreDlg::RefreshList()
{
    CWaitCursor wait_cursor;
    m_items.clear();
    m_list.DeleteAllItems();
    m_fetch_failed = false;

    if (!theApp.m_plugin_update.CheckForUpdate())
    {
        m_fetch_failed = true;
        SetDlgItemText(IDC_STORE_DESC, CCommon::LoadText(IDS_STORE_FETCH_FAILED));
        GetDlgItem(IDC_STORE_INSTALL)->EnableWindow(FALSE);
        return;
    }

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
    GetDlgItem(IDC_STORE_INSTALL)->EnableWindow(row > 0);
}

int CPluginStoreDlg::FindLocalState(const PluginStoreItem& item) const
{
    const CString path = CString(theApp.m_config_dir.c_str()) + L"plugins\\" + item.file_name.c_str();
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

void CPluginStoreDlg::OnNMDblclkStoreList(NMHDR* pNMHDR, LRESULT* pResult)
{
    OnBnClickedStoreInstall();
    *pResult = 0;
}

void CPluginStoreDlg::OnBnClickedStoreRefresh()
{
    RefreshList();
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

    const CString plugin_dir = CString(theApp.m_config_dir.c_str()) + L"plugins\\";
    ::CreateDirectoryW(plugin_dir, nullptr);
    const CString target_path = plugin_dir + item.file_name.c_str();
    const CString temp_path = plugin_dir + item.file_name.c_str() + L".download";

    const std::wstring url = theApp.m_plugin_update.GetStoreBaseUrl() + item.download_path;
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

    ::DeleteFileW(target_path);
    if (!::MoveFileW(temp_path, target_path))
    {
        ::DeleteFileW(temp_path);
        MessageBox(CCommon::LoadText(IDS_STORE_DOWNLOAD_FAILED), nullptr, MB_ICONWARNING | MB_OK);
        return;
    }

    m_list.SetItemText(index, 3, CCommon::LoadText(TXT_STORE_STATUS_INSTALLED));

    CString message = CCommon::LoadTextFormat(IDS_STORE_INSTALL_SUCCESS, { item.file_name.c_str() });
    message += L"\r\n\r\n";
    message += CCommon::LoadText(IDS_PLUGIN_INSTALL_RESTART_PROMPT);
    if (MessageBox(message, nullptr, MB_ICONQUESTION | MB_YESNO) == IDYES)
    {
        m_restart_required = true;
        EndDialog(IDOK);
    }
}

void CPluginStoreDlg::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);
    if (m_list.GetSafeHwnd() == nullptr)
        return;
    //列表与说明区跟随窗口宽度
    CRect rect;
    GetClientRect(&rect);
    const int margin = theApp.DPI(7);
    const int list_height = theApp.DPI(96);
    m_list.MoveWindow(margin, margin, rect.Width() - margin * 2, list_height);
    if (CWnd* desc = GetDlgItem(IDC_STORE_DESC))
        desc->MoveWindow(margin, margin + list_height + theApp.DPI(5),
            rect.Width() - margin * 2, theApp.DPI(38));
}
