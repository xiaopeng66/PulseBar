#include "stdafx.h"
#include <bcrypt.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <thread>
#include <urlmon.h>
#include <wininet.h>
#include "PluginUpdateHelper.h"
#include "Common.h"
#include "TinyXml2Helper.h"
#include "language.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "bcrypt.lib")

static bool VersionFromString(const std::string& version_str, std::vector<int>& versions)
{
    versions.clear();
    if (version_str.empty())
        return false;
    int component{};
    bool have_digit{};
    for (const char ch : version_str)
    {
        if (ch == '.')
        {
            if (!have_digit)
                return false;
            versions.push_back(component);
            component = 0;
            have_digit = false;
            continue;
        }
        if (ch < '0' || ch > '9')
            return false;
        const int digit = ch - '0';
        if (component > (INT_MAX - digit) / 10)
            return false;
        component = component * 10 + digit;
        have_digit = true;
    }
    if (!have_digit)
        return false;
    versions.push_back(component);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
PluginVersion::PluginVersion()
{
}

PluginVersion::PluginVersion(const std::wstring& version)
{
    m_version_str = CCommon::AsciiToStr(version);
    if (!VersionFromString(m_version_str, m_version))
        m_version_str.clear();
}

PluginVersion::PluginVersion(const std::string& version)
{
    m_version_str = version;
    if (!VersionFromString(m_version_str, m_version))
        m_version_str.clear();
}

bool PluginVersion::operator<(const PluginVersion& another) const
{
    for (size_t i{}; i < m_version.size() || i < another.m_version.size(); i++)
    {
        const int lhs = GetSubVersion(i);
        const int rhs = another.GetSubVersion(i);

        if (lhs < rhs)
            return true;

        if (lhs > rhs)
            return false;
    }
    return false;
}

bool PluginVersion::operator==(const PluginVersion& another) const
{
    for (size_t i{}; i < m_version.size() || i < another.m_version.size(); i++)
    {
        if (GetSubVersion(i) != another.GetSubVersion(i))
            return false;
    }
    return true;
}

std::string PluginVersion::GetVersionString() const
{
    return m_version_str;
}

std::wstring PluginVersion::GetVersionWString() const
{
    return CCommon::AsciiToUnicode(m_version_str);
}

int PluginVersion::GetSubVersion(size_t index) const
{
    if (index < m_version.size())
        return m_version[index];
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//用 WinINet 保留系统代理设置。清单很小，限制等待时长和响应大小。
static bool ReadPluginCatalog(const std::wstring& url, std::string& body)
{
    using InternetHandle = std::unique_ptr<void, decltype(&::InternetCloseHandle)>;
    InternetHandle session(::InternetOpenW(L"PulseBar", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0),
        ::InternetCloseHandle);
    if (!session)
        return false;
    DWORD timeout_ms = 15000;
    ::InternetSetOptionW(session.get(), INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    ::InternetSetOptionW(session.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetHandle response(::InternetOpenUrlW(session.get(), url.c_str(), nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, 0), ::InternetCloseHandle);
    if (!response)
        return false;

    DWORD status{};
    DWORD status_size = sizeof(status);
    if (!::HttpQueryInfoW(response.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
        &status, &status_size, nullptr) || status != HTTP_STATUS_OK)
        return false;

    char chunk[4096];
    constexpr size_t max_catalog_size = 1024 * 1024;
    DWORD received{};
    while (::InternetReadFile(response.get(), chunk, sizeof(chunk), &received))
    {
        if (received == 0)
            return true;
        if (body.size() + received > max_catalog_size)
            return false;
        body.append(chunk, received);
    }
    return false;
}

static std::optional<std::vector<PluginStoreItem>> FetchPluginCatalog()
{
    std::string version_info;
    if (!ReadPluginCatalog(CPluginUpdateHelper::GetStoreBaseUrl() + L"docs/plugins/plugins_version.xml", version_info))
        return std::nullopt;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(version_info.c_str(), version_info.size()) != tinyxml2::XML_SUCCESS
        || doc.RootElement() == nullptr || std::strcmp(doc.RootElement()->Name(), "plugins") != 0)
        return std::nullopt;

    std::vector<PluginStoreItem> store_items;
    CTinyXml2Helper::IterateChildNode(doc.RootElement(), [&](tinyxml2::XMLElement* ele) {
        if (std::strcmp(ele->Name(), "plugin") != 0)
            return;
        PluginStoreItem item;
        //清单是 UTF-8（XML 声明与文件实体都是），按 CP_ACP 转会把中文描述变成乱码；
        //这里所有文本字段都显式按 UTF-8 解码。
        item.file_name = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "file_name"), true);
        item.version = PluginVersion(std::string(CTinyXml2Helper::ElementAttribute(ele, "version")));
        item.display_name = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "display_name"), true);
        item.author = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "author"), true);
        item.description = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "description"), true);
        item.download_path = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "download"), true);
        item.sha256 = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "sha256"), true);
        item.arch = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "arch"), true);
        const std::string official_flag(CTinyXml2Helper::ElementAttribute(ele, "official"));
        item.official = (official_flag == "true" || official_flag == "1");
        //缺文件名无法与本地插件匹配；缺失或无效版本无法安全比较，
        //两者都不入表，避免商店显示空版本或误报「有更新」
        if (item.file_name.empty() || item.version.GetVersionString().empty())
            return;
        store_items.push_back(std::move(item));
    });
    if (store_items.empty() && doc.RootElement()->FirstChildElement() != nullptr)
        return std::nullopt;
    return store_items;
}

void CPluginUpdateHelper::RefreshAsync()
{
    const auto shared = m_catalog;
    {
        std::lock_guard<std::mutex> lock(shared->mutex);
        if (shared->catalog.status == Status::Checking)
            return;
        shared->catalog.status = Status::Checking;
    }

    //线程只持有共享清单，不访问对话框或应用对象。关闭窗口后结果仍可供商店使用。
    try
    {
        std::thread([shared]() {
            std::optional<std::vector<PluginStoreItem>> items;
            try
            {
                items = FetchPluginCatalog();
            }
            catch (...)
            {
                //异常与网络失败一样处理，保留上次成功的清单。
            }
            std::lock_guard<std::mutex> lock(shared->mutex);
            if (items)
            {
                shared->catalog.items.swap(*items);
                shared->catalog.has_snapshot = true;
            }
            shared->catalog.status = items ? Status::Ready : Status::Failed;
        }).detach();
    }
    catch (...)
    {
        std::lock_guard<std::mutex> lock(shared->mutex);
        shared->catalog.status = Status::Failed;
    }
}

CPluginUpdateHelper::Status CPluginUpdateHelper::GetStatus() const
{
    std::lock_guard<std::mutex> lock(m_catalog->mutex);
    return m_catalog->catalog.status;
}

CPluginUpdateHelper::Catalog CPluginUpdateHelper::GetCatalog() const
{
    std::lock_guard<std::mutex> lock(m_catalog->mutex);
    return m_catalog->catalog;
}

std::wstring CPluginUpdateHelper::GetStoreBaseUrl()
{
    return L"https://github.com/xiaopeng66/PulseBar/raw/main/";
}

static std::wstring PluginFileKey(std::wstring name)
{
    for (auto& ch : name)
    {
        if (ch >= L'A' && ch <= L'Z')
            ch += L'a' - L'A';
    }
    return name;
}

void CPluginUpdateHelper::MarkPendingRestart(const std::wstring& file_name, const PluginVersion& version)
{
    m_pending_versions[PluginFileKey(file_name)] = version;
}

PluginVersion CPluginUpdateHelper::GetPendingVersion(const std::wstring& file_name) const
{
    const auto iter = m_pending_versions.find(PluginFileKey(file_name));
    return iter != m_pending_versions.end() ? iter->second : PluginVersion();
}

const PluginStoreItem* FindPluginStoreItem(const std::vector<PluginStoreItem>& items, const std::wstring& file_name)
{
    for (const auto& item : items)
    {
        if (::_wcsicmp(item.file_name.c_str(), file_name.c_str()) == 0)
            return &item;
    }
    return nullptr;
}

bool IsStorePackageCompatible(const std::wstring& arch)
{
    if (arch.empty())
        return true;
    const auto name = PluginFileKey(arch);
#if defined(_M_ARM64EC)
    return name == L"arm64ec" || name == L"x64";
#elif defined(_M_ARM64)
    return name == L"arm64";
#elif defined(_M_X64)
    return name == L"x64";
#else
    return name == L"x86";
#endif
}

PluginStoreState GetPluginStoreState(const PluginStoreItem& item, bool installed,
    const PluginVersion& local_version, const PluginVersion& pending_version)
{
    if (!pending_version.GetVersionString().empty())
        return PluginStoreState::PendingRestart;
    if (!IsStorePackageCompatible(item.arch))
        return PluginStoreState::Incompatible;
    if (!installed)
        return PluginStoreState::Available;
    if (!local_version.GetVersionString().empty())
    {
        if (item.version < local_version)
            return PluginStoreState::NewerLocally;
        if (local_version < item.version && !item.download_path.empty())
            return PluginStoreState::UpdateAvailable;
    }
    return PluginStoreState::Installed;
}

PluginInstallResult CommitPluginUpdate(const std::wstring& staged_path, const std::wstring& target_path)
{
    if (::MoveFileExW(staged_path.c_str(), target_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return PluginInstallResult::Installed;
    const DWORD attributes = ::GetFileAttributesW(target_path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return PluginInstallResult::Failed;

    //已加载的 DLL 可以改名。保留原文件，直到新文件成功就位。
    const auto backup_path = target_path + L".pb-old";
    if (!::MoveFileExW(target_path.c_str(), backup_path.c_str(), MOVEFILE_WRITE_THROUGH))
        return PluginInstallResult::InUse;
    if (::MoveFileExW(staged_path.c_str(), target_path.c_str(), MOVEFILE_WRITE_THROUGH))
        return PluginInstallResult::Installed;

    //如果此时无法还原，下次启动会在加载插件前尝试恢复备份。
    ::MoveFileExW(backup_path.c_str(), target_path.c_str(), MOVEFILE_WRITE_THROUGH);
    return PluginInstallResult::Failed;
}

namespace
{
#ifndef IMAGE_FILE_MACHINE_ARM64EC
#define IMAGE_FILE_MACHINE_ARM64EC 0xA641
#endif

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

}

bool IsSafePluginFileName(const std::wstring& name)
{
    if (name.size() <= 4 || ::_wcsicmp(name.c_str() + name.size() - 4, L".dll") != 0
        || name.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos)
        return false;

    //Windows 保留设备名不能作为插件文件名。
    auto stem = name.substr(0, name.find(L'.'));
    const auto last = stem.find_last_not_of(L" \t\r\n\v\f");
    stem.resize(last == std::wstring::npos ? 0 : last + 1);
    if (::_wcsicmp(stem.c_str(), L"CON") == 0 || ::_wcsicmp(stem.c_str(), L"PRN") == 0
        || ::_wcsicmp(stem.c_str(), L"AUX") == 0 || ::_wcsicmp(stem.c_str(), L"NUL") == 0)
        return false;
    return !(stem.size() == 4 && stem[3] >= L'1' && stem[3] <= L'9'
        && (::_wcsnicmp(stem.c_str(), L"COM", 3) == 0 || ::_wcsnicmp(stem.c_str(), L"LPT", 3) == 0));
}

struct CPluginPackageInstall::Download
{
    std::wstring file_name;
    PluginVersion version;
    std::wstring target;
    std::wstring path;
    const wchar_t* error{ nullptr };

    ~Download()
    {
        if (!path.empty())
            ::DeleteFileW(path.c_str());
    }
};

const wchar_t* CPluginPackageInstall::Start(const PluginStoreItem& item, const std::wstring& directory)
{
    if (IsActive())
        return IDS_STORE_DOWNLOAD_FAILED;
    if (!IsSafePluginFileName(item.file_name))
        return IDS_STORE_INVALID_NAME;
    if (item.download_path.empty())
        return IDS_STORE_NO_DOWNLOAD;

    std::wstring url = item.download_path;
    if (url.rfind(L"http://", 0) != 0 && url.rfind(L"https://", 0) != 0)
        url = CPluginUpdateHelper::GetStoreBaseUrl() + url;
    try
    {
        //关闭发起更新的窗口会放弃安装，线程只持有临时文件并自行清理。
        std::packaged_task<std::shared_ptr<Download>()> task([directory, url, item]() {
            auto result = std::make_shared<Download>();
            result->file_name = item.file_name;
            result->version = item.version;
            result->target = directory + item.file_name;
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
            else if (!item.sha256.empty() && _wcsicmp(ComputeSha256Hex(result->path).c_str(), item.sha256.c_str()) != 0)
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
        return nullptr;
    }
    catch (...)
    {
        return IDS_STORE_DOWNLOAD_FAILED;
    }
}

bool CPluginPackageInstall::IsReady() const
{
    return IsActive() && m_download.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

CPluginPackageInstall::Completion CPluginPackageInstall::Finish(CPluginUpdateHelper& updates)
{
    std::shared_ptr<Download> result;
    try
    {
        result = m_download.get();
    }
    catch (...)
    {
        return { {}, IDS_STORE_DOWNLOAD_FAILED };
    }
    if (result->error != nullptr)
        return { {}, result->error };

    switch (CommitPluginUpdate(result->path, result->target))
    {
    case PluginInstallResult::Installed:
        result->path.clear();
        updates.MarkPendingRestart(result->file_name, result->version);
        return { std::move(result->version), nullptr };
    case PluginInstallResult::InUse:
        return { {}, IDS_STORE_IN_USE };
    default:
        return { {}, IDS_STORE_REPLACE_FAILED };
    }
}
