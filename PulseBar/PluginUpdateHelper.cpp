#include "stdafx.h"
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

        versions.push_back(atoi(str_ver.c_str()));
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
    std::wstring url;
    //插件商店版本表随主仓库发布，避免依赖额外的空仓库
    //更新源为Gitee
    if (theApp.m_general_data.update_source == 1)
        url = L"https://gitee.com/xiaopeng66/PulseBar/raw/main/docs/plugins/plugins_version.xml";
    //更新源为Github
    else
        url = L"https://raw.githubusercontent.com/xiaopeng66/PulseBar/main/docs/plugins/plugins_version.xml";

    //获取url内容
    std::string version_info;
    if (!CCommon::GetURL(url, version_info))
        return false;       //下载失败时保留上一次成功的版本表，不清空

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
        item.file_name = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "file_name"));
        item.version = PluginVersion(std::string(CTinyXml2Helper::ElementAttribute(ele, "version")));
        item.display_name = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "display_name"));
        item.author = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "author"));
        item.description = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "description"));
        item.download_path = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "download"));
        item.sha256 = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "sha256"));
        item.arch = CCommon::StrToUnicode(CTinyXml2Helper::ElementAttribute(ele, "arch"));
        if (item.file_name.empty())
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
    //清单与插件文件都随主仓库发布
    if (theApp.m_general_data.update_source == 1)
        return L"https://gitee.com/xiaopeng66/PulseBar/raw/main/";
    return L"https://raw.githubusercontent.com/xiaopeng66/PulseBar/main/";
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
