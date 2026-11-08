#pragma once
#include <mutex>

class PluginVersion
{
public:
    PluginVersion();
    PluginVersion(const std::wstring& version);
    PluginVersion(const std::string& version);

    bool operator<(const PluginVersion& another) const;
    bool operator==(const PluginVersion& another) const;

    std::string GetVersionString() const;
    std::wstring GetVersionWString() const;

protected:
    int GetSubVersion(size_t index) const;

    private:
    std::vector<int> m_version;
    std::string m_version_str;
};

//云端清单里的一条插件记录（插件商店用）
struct PluginStoreItem
{
    std::wstring file_name;
    PluginVersion version;
    std::wstring display_name;
    std::wstring author;
    std::wstring description;
    std::wstring download_path;     //相对仓库根目录，或完整 http(s) 链接
    std::wstring sha256;            //为空时跳过校验
    std::wstring arch;
    bool official{ false };         //清单标记为官方插件
};

class CPluginUpdateHelper
{
public:
    bool CheckForUpdate();

    //获取一个插件的最新版本的副本。版本表会被后台更新线程整体替换，
    //因此按值返回快照而不是指向表内部的引用；查询持短锁，锁内不做网络或插件调用。
    PluginVersion GetPluginLatestVersions(const std::wstring& file_name) const;

    //插件商店：返回整份清单快照（持短锁拷贝）
    std::vector<PluginStoreItem> GetStoreItems() const;
    //清单所在的仓库根地址，用于拼接插件下载链接
    std::wstring GetStoreBaseUrl() const;

private:
    std::map<std::wstring, PluginVersion> m_latest_versions;    //在线获取到的每个插件的最新版本信息
    std::vector<PluginStoreItem> m_store_items;                 //同一份清单的完整字段
    std::wstring m_store_base_url;      //实际取到清单的那个源的根地址（回退后与配置项可能不同）
    mutable std::mutex m_version_mutex;     //保护 m_latest_versions / m_store_items / m_store_base_url
    std::mutex m_fetch_mutex;               //串行化 CheckForUpdate 的网络请求段
};

