#include "stdafx.h"
#include <climits>
#include <cstdlib>
#include "PluginUpdateHelper.h"
#include "Common.h"
#include "PulseBar.h"
#include "TinyXml2Helper.h"

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
bool CPluginUpdateHelper::CheckForUpdate()
{
    //商店对话框（UI 线程）与启动更新线程可能同时进入：网络请求段必须互斥，
    //否则两次请求互相覆盖、两次 swap 交错会让不同调用者拿到不同代际的表。
    std::lock_guard<std::mutex> fetch_lock(m_fetch_mutex);

    //插件商店版本表随主仓库发布。注意 GitHub 侧必须用 github.com/<owner>/<repo>/raw/…，
    //raw.githubusercontent.com 在部分网络下 TLS 会被中断（实测不可达）。
    //更新源固定为 GitHub（Gitee 仓库不存在）。
    const std::wstring github_base = L"https://github.com/xiaopeng66/PulseBar/raw/main/";
    const std::wstring manifest_path = L"docs/plugins/plugins_version.xml";

    std::string version_info;
    const bool fetched = CCommon::GetURL(github_base + manifest_path, version_info);
    if (!fetched)
        return false;       //拉取失败时保留上一次成功的版本表，不清空

    //在局部表中完成解析，成功后在锁内一次性替换共享表。
    //旧实现先 clear 共享表再边解析边 emplace，UI 线程的查询在整段
    //网络窗口内都能撞上重建中的 map（悬空引用/迭代器失效）。
    tinyxml2::XMLDocument doc;
    if (doc.Parse(version_info.c_str(), version_info.size()) != tinyxml2::XML_SUCCESS || doc.RootElement() == nullptr)
        return false;       //损坏或空响应不是成功的新表，保留最后有效快照。
    std::map<std::wstring, PluginVersion> latest_versions;
    std::vector<PluginStoreItem> store_items;
    CTinyXml2Helper::IterateChildNode(doc.RootElement(), [&](tinyxml2::XMLElement* ele) {
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
        if (item.file_name.empty() || item.version.GetVersionString().empty())
            return;
        latest_versions.emplace(item.file_name, item.version);
        store_items.push_back(std::move(item));
    });
    std::lock_guard<std::mutex> lock(m_version_mutex);
    m_latest_versions.swap(latest_versions);
    m_store_items.swap(store_items);
    return true;
}

std::vector<PluginStoreItem> CPluginUpdateHelper::GetStoreItems() const
{
    std::lock_guard<std::mutex> lock(m_version_mutex);
    return m_store_items;
}

std::wstring CPluginUpdateHelper::GetStoreBaseUrl() const
{
    //清单与插件文件都随主仓库发布，更新源固定为 GitHub
    return L"https://github.com/xiaopeng66/PulseBar/raw/main/";
}

PluginVersion CPluginUpdateHelper::GetPluginLatestVersions(const std::wstring& file_name) const
{
    std::lock_guard<std::mutex> lock(m_version_mutex);
    auto iter = m_latest_versions.find(file_name);
    if (iter != m_latest_versions.end())
    {
        return iter->second;
    }
    return PluginVersion();
}
