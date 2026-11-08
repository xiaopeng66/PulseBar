#include "stdafx.h"
#include "UpdateHelper.h"
#include "SimpleXML.h"
#include "Common.h"


CUpdateHelper::CUpdateHelper()
{
}


CUpdateHelper::~CUpdateHelper()
{
}

void CUpdateHelper::SetUpdateSource(UpdateSource update_source)
{
    m_update_source = update_source;
}

bool CUpdateHelper::CheckForUpdate()
{
    //仓库只有 main 分支。旧代码写死 master，两个源都是 404，更新检查永远失败。
    //GitHub 侧统一用 github.com/<owner>/<repo>/raw/…：raw.githubusercontent.com
    //在部分网络下 TLS 会被中断（实测不可达）。两个源互为回退。
    const std::wstring github_url = L"https://github.com/xiaopeng66/PulseBar/raw/main/version_utf8.info";
    const std::wstring gitee_url = L"https://gitee.com/xiaopeng66/PulseBar/raw/main/version_utf8.info";
    //最后的兜底：GitHub 网页版（返回 HTML，需要把被转义的尖括号还原）
    const std::wstring github_blob = L"https://github.com/xiaopeng66/PulseBar/blob/main/version_utf8.info";

    const bool prefer_gitee = (m_update_source == UpdateSource::GiteeSource);
    std::vector<std::wstring> sources;
    sources.push_back(prefer_gitee ? gitee_url : github_url);
    sources.push_back(prefer_gitee ? github_url : gitee_url);
    sources.push_back(github_blob);     //永远放在最后：HTML 需要额外还原，能不用就不用

    wstring version_info;
    bool fetched = false;
    for (const auto& url : sources)
    {
        m_row_data = (url != github_blob);
        if (CCommon::GetURL(url, version_info, true))
        {
            fetched = true;
            break;
        }
    }
    if (!fetched)
        return false;

    if (!m_row_data)
    {
        //网页版把 <version> 转义成了 &lt;version&gt;，并带上页面其它内容，
        //从第一个被转义的 <version> 开始截断再还原
        size_t index = version_info.find(L"\uFEFF&lt;version&gt;");
        if (index == std::wstring::npos)
            index = version_info.find(L"&lt;version&gt;");
        if (index != std::wstring::npos)
            version_info = version_info.substr(index);

        CString str_version_info = version_info.c_str();
        str_version_info.Replace(L"&lt;", L"<");
        str_version_info.Replace(L"&gt;", L">");

        version_info = str_version_info;
    }

    ParseUpdateInfo(version_info);

    return true;
}

void CUpdateHelper::ParseUpdateInfo(wstring version_info)
{
    CSimpleXML version_xml;
    version_xml.LoadXMLContentDirect(version_info);

    m_version = version_xml.GetNode(L"version");

    // Validate version string: must only contain digits, dots, and alphanumeric chars.
    // An empty or malformed version string indicates a tampered/invalid manifest.
    if (m_version.empty())
        return;
    for (wchar_t ch : m_version)
    {
        if (!iswalnum(ch) && ch != L'.' && ch != L'-')
        {
            m_version.clear();
            return;
        }
    }

    wstring str_source_tag = (m_update_source == UpdateSource::GitHubSource ? L"GitHub" : L"Gitee");
    wstring str_link_tag, str_link_tag_x64, str_link_tag_arm64ec;
#ifdef WITHOUT_TEMPERATURE
    str_link_tag = L"link_without_temperature";
    str_link_tag_x64 = L"link_without_temperature_x64";
    str_link_tag_arm64ec = L"link_without_temperature_arm64ec";
#else
    str_link_tag = L"link";
    str_link_tag_x64 = L"link_x64";
    str_link_tag_arm64ec = L"link_arm64ec";
#endif
    m_link64 = version_xml.GetNode(str_link_tag_x64.c_str(), str_source_tag.c_str());
    m_link = version_xml.GetNode(str_link_tag.c_str(), str_source_tag.c_str());
    m_link_arm64ec = version_xml.GetNode(str_link_tag_arm64ec.c_str(), str_source_tag.c_str());

    // Validate all download URLs: must use HTTPS and originate from a trusted domain.
    // This prevents a network-level attacker from injecting plain-HTTP or off-domain
    // download links via a tampered update manifest (MITM / supply-chain protection).
    auto IsValidDownloadUrl = [](const std::wstring& url) -> bool {
        if (url.empty())
            return true;
        // Enforce HTTPS scheme
        if (url.compare(0, 8, L"https://") != 0)
            return false;
        // Enforce trusted hosting domains (GitHub or Gitee)
        const std::wstring host = url.substr(8);
        return (host.compare(0, 11, L"github.com/") == 0 ||
                host.compare(0, 10, L"gitee.com/") == 0);
    };

    if (!IsValidDownloadUrl(m_link) || !IsValidDownloadUrl(m_link64) || !IsValidDownloadUrl(m_link_arm64ec))
    {
        // Clear all update data to prevent MITM-injected URLs from being acted upon
        m_version.clear();
        m_link.clear();
        m_link64.clear();
        m_link_arm64ec.clear();
        return;
    }

    CString contents_zh_cn = version_xml.GetNode(L"contents_zh_cn", L"update_contents").c_str();
    CString contents_en = version_xml.GetNode(L"contents_en", L"update_contents").c_str();
    CString contents_zh_tw = version_xml.GetNode(L"contents_zh_tw", L"update_contents").c_str();
    contents_zh_cn.Replace(L"\\n", L"\r\n");
    contents_en.Replace(L"\\n", L"\r\n");
    contents_zh_tw.Replace(L"\\n", L"\r\n");
    m_contents_zh_cn = contents_zh_cn;
    m_contents_en = contents_en;
    m_contents_zh_tw = contents_zh_tw;
}

const std::wstring& CUpdateHelper::GetVersion() const
{
    return m_version;
}

const std::wstring& CUpdateHelper::GetLink() const
{
    return m_link;
}

const std::wstring& CUpdateHelper::GetLink64() const
{
    return m_link64;
}

const std::wstring& CUpdateHelper::GetLinkArm64ec() const
{
    return m_link_arm64ec;
}

const std::wstring& CUpdateHelper::GetContentsEn() const
{
    return m_contents_en;
}

const std::wstring& CUpdateHelper::GetContentsZhCn() const
{
    return m_contents_zh_cn;
}

const std::wstring& CUpdateHelper::GetContentsZhTw() const
{
    return m_contents_zh_tw;
}

bool CUpdateHelper::IsRowData()
{
    return m_row_data;
}
