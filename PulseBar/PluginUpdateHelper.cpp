#include "stdafx.h"
#include <climits>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <thread>
#include <wininet.h>
#include "PluginUpdateHelper.h"
#include "Common.h"
#include "TinyXml2Helper.h"

#pragma comment(lib, "wininet.lib")

static void VersionFromString(const std::string& version_str, std::vector<int>& versions)
{
    versions.clear();
    //拆分字符串
    std::vector<std::string> vec_version;
    CCommon::StringSplit(version_str, '.', vec_version);
    //转换为整数保存
    for (const auto& str : vec_version)
    {
        //去掉除数字以外的字符
        std::string str_ver;
        for (auto ch : str)
        {
            if (ch >= '0' && ch <= '9')
                str_ver.push_back(ch);
        }

        //atoi 对超长数字是未定义行为（清单来自网络），用 strtol 并钳制到 int
        errno = 0;
        const long parsed = std::strtol(str_ver.c_str(), nullptr, 10);
        versions.push_back(parsed > INT_MAX ? INT_MAX : static_cast<int>(parsed));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
PluginVersion::PluginVersion()
{
}

PluginVersion::PluginVersion(const std::wstring& version)
{
    m_version_str = CCommon::AsciiToStr(version);
    VersionFromString(m_version_str, m_version);
}

PluginVersion::PluginVersion(const std::string& version)
{
    m_version_str = version;
    VersionFromString(m_version_str, m_version);
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
        //缺文件名无法与本地插件匹配；缺版本的条目会让版本比较退化成 0，
        //两者都不入表，避免商店显示空版本或误报「有更新」
        if (item.file_name.empty() || item.version.GetVersionString().find_first_of("0123456789") == std::string::npos)
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
