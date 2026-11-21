#include "stdafx.h"
#include "PluginManager.h"
#include "SettingsHelper.h"
#include "PulseBar.h"

namespace
{
    using StableColorMap = std::map<std::wstring, TaskbarItemColor>;

    std::wstring StableColorIdentity(const CommonDisplayItem& item)
    {
        const wchar_t* item_id = item.GetItemIniKeyName();
        if (item_id == nullptr || *item_id == L'\0')
            return {};
        return std::wstring(item.IsPlugin() ? L"plugin:" : L"builtin:") + item_id;
    }

    std::wstring EncodeColorIdentity(const std::wstring& identity)
    {
        constexpr wchar_t digits[] = L"0123456789abcdef";
        std::wstring encoded;
        encoded.reserve(identity.size() * 4);
        for (wchar_t ch : identity)
        {
            const auto unit = static_cast<unsigned short>(ch);
            for (int shift = 12; shift >= 0; shift -= 4)
                encoded.push_back(digits[(unit >> shift) & 0x0f]);
        }
        return encoded;
    }

    bool DecodeColorIdentity(const std::wstring& encoded, std::wstring& identity)
    {
        if (encoded.empty() || encoded.size() % 4 != 0)
            return false;
        identity.clear();
        for (size_t i = 0; i < encoded.size(); i += 4)
        {
            unsigned short unit{};
            for (size_t j = 0; j < 4; ++j)
            {
                const wchar_t ch = encoded[i + j];
                const unsigned int digit = ch >= L'0' && ch <= L'9' ? ch - L'0'
                    : ch >= L'a' && ch <= L'f' ? ch - L'a' + 10
                    : ch >= L'A' && ch <= L'F' ? ch - L'A' + 10 : 16;
                if (digit == 16)
                    return false;
                unit = static_cast<unsigned short>((unit << 4) | digit);
            }
            identity.push_back(static_cast<wchar_t>(unit));
        }
        return true;
    }

    bool ParseColorValue(const std::wstring& text, COLORREF& value)
    {
        if (text.empty())
            return false;
        wchar_t* end{};
        errno = 0;
        const unsigned long parsed = std::wcstoul(text.c_str(), &end, 10);
        if (errno == ERANGE || end != text.c_str() + text.size() || parsed > 0xffffffffUL)
            return false;
        value = static_cast<COLORREF>(parsed);
        return true;
    }

    bool ParseStableColorMap(const std::wstring& text, StableColorMap& colors)
    {
        if (text.compare(0, 3, L"v1|") != 0)
            return false;
        StableColorMap parsed_colors;
        size_t start = 3;
        while (start < text.size())
        {
            const size_t end = text.find(L'|', start);
            const auto record = text.substr(start, end == std::wstring::npos ? end : end - start);
            const size_t equals = record.find(L'=');
            const size_t comma = equals == std::wstring::npos ? std::wstring::npos : record.find(L',', equals + 1);
            std::wstring identity;
            COLORREF label{}, value{};
            if (equals == std::wstring::npos || comma == std::wstring::npos
                || !DecodeColorIdentity(record.substr(0, equals), identity)
                || !ParseColorValue(record.substr(equals + 1, comma - equals - 1), label)
                || !ParseColorValue(record.substr(comma + 1), value))
                return false;
            parsed_colors[std::move(identity)] = { label, value };
            if (end == std::wstring::npos)
                break;
            start = end + 1;
        }
        colors = std::move(parsed_colors);
        return true;
    }

    std::wstring SerializeStableColorMap(const StableColorMap& colors)
    {
        std::wstring serialized = L"v1|";
        bool first = true;
        for (const auto& [identity, color] : colors)
        {
            if (!first)
                serialized.push_back(L'|');
            first = false;
            serialized += EncodeColorIdentity(identity);
            serialized += L'=' + std::to_wstring(color.label) + L',' + std::to_wstring(color.value);
        }
        return serialized;
    }
}

CSettingsHelper::CSettingsHelper()
    : CIniHelper(theApp.m_config_path)
{
}

CSettingsHelper::CSettingsHelper(const std::wstring& file_path)
    : CIniHelper(file_path)
{
}

void CSettingsHelper::SaveFontData(const wchar_t* AppName, const FontInfo& font)
{
    WriteString(AppName, L"font_name", wstring(font.name));
    WriteInt(AppName, L"font_size", font.size);
    bool style[4];
    style[0] = font.bold;
    style[1] = font.italic;
    style[2] = font.underline;
    style[3] = font.strike_out;
    WriteBoolArray(AppName, L"font_style", style, 4);
}

void CSettingsHelper::LoadFontData(const wchar_t* AppName, FontInfo& font, const FontInfo& default_font) const
{
    //历史版本曾把 UTF-8 语言串按 ANSI 读入并把乱码存回配置，这里无损还原
    font.name = CCommon::RepairMisdecodedText(GetString(AppName, L"font_name", default_font.name)).c_str();
    font.size = GetInt(AppName, L"font_size", default_font.size);
    bool style[4];
    GetBoolArray(AppName, L"font_style", style, 4);
    font.bold = style[0];
    font.italic = style[1];
    font.underline = style[2];
    font.strike_out = style[3];
}

void CSettingsHelper::LoadTaskbarWndColors(const wchar_t* AppName, const wchar_t* KeyName, std::map<CommonDisplayItem, TaskbarItemColor>& text_colors, const wchar_t* default_str)
{
    wstring str{ default_str };
    _GetString(AppName, KeyName, str);
    const std::wstring stable_key = std::wstring(KeyName) + L"_by_id";
    std::wstring stable_text;
    StableColorMap stable_colors;
    if (_GetString(AppName, stable_key.c_str(), stable_text) && ParseStableColorMap(stable_text, stable_colors))
    {
        const COLORREF default_color = _wtoi(default_str);
        for (const auto& item : theApp.m_plugins.AllDisplayItemsWithPlugins())
        {
            const auto identity = StableColorIdentity(item);
            const auto saved = stable_colors.find(identity);
            text_colors[item] = identity.empty() || saved == stable_colors.end()
                ? TaskbarItemColor{ default_color, default_color } : saved->second;
        }
        return;
    }
    std::vector<wstring> split_result;
    CCommon::StringSplit(str, L',', split_result);
    size_t index = 0;
    COLORREF default_color = _wtoi(default_str);
    for (auto iter = theApp.m_plugins.AllDisplayItemsWithPlugins().begin(); iter != theApp.m_plugins.AllDisplayItemsWithPlugins().end(); ++iter)
    {
        if (iter->IsPlugin())
        {
            text_colors[*iter] = { default_color, default_color };
        }
        else if (index < split_result.size())
            text_colors[*iter].label = _wtoi(split_result[index].c_str());
        //else if (!split_result.empty())
        //    text_colors[*iter].label = _wtoi(split_result[0].c_str());
        else
            text_colors[*iter].label = default_color;

        if (!iter->IsPlugin() && index + 1 < split_result.size())
            text_colors[*iter].value = _wtoi(split_result[index + 1].c_str());
        //else if (split_result.size() > 1)
        //    text_colors[*iter].value = _wtoi(split_result[1].c_str());
        else
            text_colors[*iter].value = default_color;
        index += 2;
    }
}

void CSettingsHelper::LoadTaskbarWndColors(const wchar_t* AppName, const wchar_t* KeyName, std::map<CommonDisplayItem, TaskbarItemColor>& text_colors, COLORREF default_color)
{
    CString default_str;
    default_str.Format(_T("%d"), default_color);
    LoadTaskbarWndColors(AppName, KeyName, text_colors, default_str.GetString());
}

void CSettingsHelper::SaveTaskbarWndColors(const wchar_t* AppName, const wchar_t* KeyName, const std::map<CommonDisplayItem, TaskbarItemColor>& text_colors)
{
    CString str;
    const std::wstring stable_key = std::wstring(KeyName) + L"_by_id";
    StableColorMap stable_colors;
    std::wstring previous_stable_text;
    if (_GetString(AppName, stable_key.c_str(), previous_stable_text))
        ParseStableColorMap(previous_stable_text, stable_colors);
    for (auto iter = text_colors.begin(); iter != text_colors.end(); ++iter)
    {
        CString tmp;
        tmp.Format(_T("%d,%d,"), iter->second.label, iter->second.value);
        str += tmp;
        const auto identity = StableColorIdentity(iter->first);
        if (!identity.empty())
            stable_colors[identity] = iter->second;
    }
    _WriteString(AppName, KeyName, wstring(str));
    _WriteString(AppName, stable_key.c_str(), SerializeStableColorMap(stable_colors));
}

namespace
{
    //误把 UTF-8 当 ANSI 解码时，无法映射的字节会变成 '?'，这类文本已无法无损还原；
    //它能确定是坏值（默认文本里没有 '?'），退回默认文案比显示乱码好。
    bool HasLostByteMojibake(const std::wstring& value, const std::wstring& default_text)
    {
        bool has_non_ascii{ false };
        bool has_question{ false };
        for (wchar_t ch : value)
        {
            if (ch > 0x7F)
                has_non_ascii = true;
            if (ch == L'?')
                has_question = true;
        }
        return has_non_ascii && has_question && default_text.find(L'?') == std::wstring::npos;
    }
}

void CSettingsHelper::LoadDisplayStr(const wchar_t* AppName, DispStrings& disp_str, bool is_main_window) const
{
    for (auto display_item : AllDisplayItems)
    {
        const std::wstring default_text{ CommonDisplayItem(display_item).DefaultString(is_main_window) };
        std::wstring str{ default_text };
        bool exist = GetString(AppName, CommonDisplayItem(display_item).GetItemIniKeyName(), str);
        if (exist)
        {
            const std::wstring repaired = CCommon::RepairMisdecodedText(str);
            if (repaired != str)
                str = repaired;                             //能无损还原：用还原后的文本
            else if (HasLostByteMojibake(str, default_text))
                str = default_text;                         //有字节丢失：退回默认文本
        }
        //主窗口只读取配置文件中存在的项，任务栏窗口读取所有项
        if (!is_main_window || exist)
            disp_str.Get(display_item) = str;
    }
}

void CSettingsHelper::SaveDisplayStr(const wchar_t* AppName, const DispStrings& disp_str)
{
    for (const auto& item : disp_str.GetAllItems())
    {
        if (!item.first.IsPlugin())
        {
            WriteString(AppName, item.first.GetItemIniKeyName(), item.second);
        }
    }
}

void CSettingsHelper::LoadPluginDisplayStr(const wchar_t* AppName, DispStrings& disp_str, bool is_main_window)
{
    for (const auto& plugin : theApp.m_plugins.GetPluginItems())
    {
        //GetItemLableText()/GetItemId() 都可能返回空指针，而 std::wstring 的构造
        //会直接把空指针当成字符串起点去读，导致启动阶段崩溃。
        //加载插件时已经拒收了这类项目，这里是第二道防线。
        if (plugin == nullptr)
            continue;
        //插件 getter 可能崩溃（/EHa 下可捕获）：跳过该项，不能中断配置加载
        try
        {
            if (plugin->GetItemLableText() == nullptr || plugin->GetItemId() == nullptr)
                continue;
            std::wstring str{ plugin->GetItemLableText() };
            bool exist = GetString(AppName, plugin->GetItemId(), str);
            //主窗口只读取配置文件中存在的项，任务栏窗口读取所有项
            if (!is_main_window || exist)
                disp_str.Load(plugin->GetItemId(), str);
        }
        catch (...)
        {
            LogPluginCallbackFailure(L"IPluginItem::GetItemLableText/GetItemId");
        }
    }
}

void CSettingsHelper::SavePluginDisplayStr(const wchar_t* AppName, const DispStrings& disp_str)
{
    for (const auto& plugin : theApp.m_plugins.GetPluginItems())
    {
        if (plugin == nullptr)
            continue;
        try
        {
            if (plugin->GetItemId() == nullptr)
                continue;
            if (disp_str.GetAllItems().find(plugin) != disp_str.GetAllItems().end())
                WriteString(AppName, plugin->GetItemId(), disp_str.GetConst(plugin));
        }
        catch (...)
        {
            LogPluginCallbackFailure(L"IPluginItem::GetItemId");
        }
    }
}
