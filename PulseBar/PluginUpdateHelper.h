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

class CPluginUpdateHelper
{
public:
    bool CheckForUpdate();

    //获取一个插件的最新版本的副本。版本表会被后台更新线程整体替换，
    //因此按值返回快照而不是指向表内部的引用；查询持短锁，锁内不做网络或插件调用。
    PluginVersion GetPluginLatestVersions(const std::wstring& file_name) const;

private:
    std::map<std::wstring, PluginVersion> m_latest_versions;    //在线获取到的每个插件的最新版本信息
    mutable std::mutex m_version_mutex;     //保护 m_latest_versions；更新线程重建表与UI线程查询共用
};

