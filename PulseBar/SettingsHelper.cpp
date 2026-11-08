#include "stdafx.h"
#include "PluginManager.h"
#include "SettingsHelper.h"
#include "PulseBar.h"

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
    std::vector<wstring> split_result;
    CCommon::StringSplit(str, L',', split_result);
    size_t index = 0;
    COLORREF default_color = _wtoi(default_str);
    for (auto iter = theApp.m_plugins.AllDisplayItemsWithPlugins().begin(); iter != theApp.m_plugins.AllDisplayItemsWithPlugins().end(); ++iter)
    {
        if (index < split_result.size())
            text_colors[*iter].label = _wtoi(split_result[index].c_str());
        //else if (!split_result.empty())
        //    text_colors[*iter].label = _wtoi(split_result[0].c_str());
        else
            text_colors[*iter].label = default_color;

        if (index + 1 < split_result.size())
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
    for (auto iter = text_colors.begin(); iter != text_colors.end(); ++iter)
    {
        CString tmp;
        tmp.Format(_T("%d,%d,"), iter->second.label, iter->second.value);
        str += tmp;
    }
    _WriteString(AppName, KeyName, wstring(str));
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
