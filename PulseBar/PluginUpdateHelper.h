#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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
    enum class Status { Idle, Checking, Ready, Failed };
    struct Catalog
    {
        Status status{ Status::Idle };
        bool has_snapshot{ false };
        std::vector<PluginStoreItem> items;
    };

    void RefreshAsync();        //启动、管理、商店共用一个请求
    Status GetStatus() const;
    Catalog GetCatalog() const;
    static std::wstring GetStoreBaseUrl();

    //仅在 UI 线程访问。新文件写入磁盘后，运行中的插件仍要等重启才更换。
    void MarkPendingRestart(const std::wstring& file_name, const PluginVersion& version);
    PluginVersion GetPendingVersion(const std::wstring& file_name) const;
    bool HasPendingRestart() const { return !m_pending_versions.empty(); }

private:
    struct SharedCatalog
    {
        std::mutex mutex;
        Catalog catalog;
    };
    std::shared_ptr<SharedCatalog> m_catalog{ std::make_shared<SharedCatalog>() };
    std::map<std::wstring, PluginVersion> m_pending_versions;
};

const PluginStoreItem* FindPluginStoreItem(const std::vector<PluginStoreItem>& items, const std::wstring& file_name);
bool IsStorePackageCompatible(const std::wstring& arch);

enum class PluginStoreState { Available, Installed, UpdateAvailable, NewerLocally, PendingRestart, Incompatible };
PluginStoreState GetPluginStoreState(const PluginStoreItem& item, bool installed,
    const PluginVersion& local_version, const PluginVersion& pending_version);

enum class PluginInstallResult { Installed, InUse, Failed };
PluginInstallResult CommitPluginUpdate(const std::wstring& staged_path, const std::wstring& target_path);

