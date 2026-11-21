"""Plugin colors follow stable item IDs when a preceding plugin is unavailable."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    source = native.source("SettingsHelper.cpp")
    helpers = source[:source.index("CSettingsHelper::CSettingsHelper()")]
    load = native.method(source, "void CSettingsHelper::LoadTaskbarWndColors(const wchar_t* AppName, const wchar_t* KeyName, std::map<CommonDisplayItem, TaskbarItemColor>& text_colors, const wchar_t* default_str)")
    save = native.method(source, "void CSettingsHelper::SaveTaskbarWndColors(const wchar_t* AppName, const wchar_t* KeyName, const std::map<CommonDisplayItem, TaskbarItemColor>& text_colors)")
    code = r'''#include <map>
#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <cwchar>
#include <cerrno>
#include <cstdio>
using std::map;using std::set;using std::vector;using std::wstring;
using COLORREF=unsigned long;
struct TaskbarItemColor{unsigned long label{},value{};};
struct Plugin{wstring id;const wchar_t* GetItemId()const{return id.c_str();}};
struct CommonDisplayItem{Plugin* plugin{};int builtin{};CommonDisplayItem()=default;CommonDisplayItem(Plugin* p):plugin(p){}
    CommonDisplayItem(int b):builtin(b){}bool IsPlugin()const{return plugin!=nullptr;}
    const wchar_t* GetItemIniKeyName()const{return plugin?plugin->GetItemId():builtin==0?L"up_string":L"down_string";}
    bool operator<(const CommonDisplayItem& other)const{if(IsPlugin()!=other.IsPlugin())return !IsPlugin();
        return plugin?plugin->id<other.plugin->id:builtin<other.builtin;}};
struct PluginManager{vector<Plugin*> items;set<CommonDisplayItem> all_items;const set<CommonDisplayItem>& AllDisplayItemsWithPlugins(){
    all_items={CommonDisplayItem(0),CommonDisplayItem(1)};for(auto* item:items)all_items.emplace(item);return all_items;}};
struct App{PluginManager m_plugins;}theApp;
struct CCommon{static void StringSplit(const wstring& text,wchar_t delimiter,vector<wstring>& values){
    size_t start=0;while(start<text.size()){size_t end=text.find(delimiter,start);values.push_back(text.substr(start,end==wstring::npos?end:end-start));
        if(end==wstring::npos)break;start=end+1;}}};
map<wstring,wstring> ini;
class CIniHelper{protected:bool _GetString(const wchar_t*,const wchar_t* key,wstring& value)const{auto it=ini.find(key);if(it==ini.end())return false;value=it->second;return true;}
    void _WriteString(const wchar_t*,const wchar_t* key,const wstring& value){ini[key]=value;}};
struct CString:std::wstring{using std::wstring::wstring;void Format(const wchar_t*,int,int){} };
class CSettingsHelper:public CIniHelper{public:
    void LoadTaskbarWndColors(const wchar_t*,const wchar_t*,std::map<CommonDisplayItem,TaskbarItemColor>&,const wchar_t*);
    void SaveTaskbarWndColors(const wchar_t*,const wchar_t*,const std::map<CommonDisplayItem,TaskbarItemColor>&);};
#define _T(text) L##text
'''+native.no_includes(helpers)+load+save+r'''
bool check(const std::map<CommonDisplayItem,TaskbarItemColor>& values,Plugin* item,unsigned long label,unsigned long value){auto it=values.find(CommonDisplayItem(item));
    return it!=values.end()&&it->second.label==label&&it->second.value==value;}
int wmain(int argc,wchar_t** argv){if(argc!=3||std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0)return 2;
    Plugin a{L"plugin-a"},b{L"plugin-b"};theApp.m_plugins.items={&a,&b};CSettingsHelper settings;
    std::map<CommonDisplayItem,TaskbarItemColor> initial{{CommonDisplayItem(&a),{11,12}},{CommonDisplayItem(&b),{21,22}}};
    settings.SaveTaskbarWndColors(L"task_bar",L"task_bar_text_color",initial);
    theApp.m_plugins.items={&b};std::map<CommonDisplayItem,TaskbarItemColor> after_disable;
    settings.LoadTaskbarWndColors(L"task_bar",L"task_bar_text_color",after_disable,L"7");
    if(!check(after_disable,&b,21,22))return 1;
    settings.SaveTaskbarWndColors(L"task_bar",L"task_bar_text_color",after_disable);
    theApp.m_plugins.items={&a,&b};std::map<CommonDisplayItem,TaskbarItemColor> after_reenable;
    settings.LoadTaskbarWndColors(L"task_bar",L"task_bar_text_color",after_reenable,L"7");
    if(!check(after_reenable,&a,11,12)||!check(after_reenable,&b,21,22))return 1;
    ini.erase(L"task_bar_text_color_by_id");
    ini[L"task_bar_text_color"]=L"31,32,41,42,51,52,61,62,";
    theApp.m_plugins.items={&b};std::map<CommonDisplayItem,TaskbarItemColor> migrated;
    settings.LoadTaskbarWndColors(L"task_bar",L"task_bar_text_color",migrated,L"7");
    const auto up=migrated.find(CommonDisplayItem(0)),down=migrated.find(CommonDisplayItem(1));
    if(up==migrated.end()||up->second.label!=31||up->second.value!=32||
       down==migrated.end()||down->second.label!=41||down->second.value!=42||
       !check(migrated,&b,7,7)) {
        std::wcerr<<L"legacy mismatch up="<<(up==migrated.end()?L"missing":std::to_wstring(up->second.label)+L","+std::to_wstring(up->second.value))
            <<L" down="<<(down==migrated.end()?L"missing":std::to_wstring(down->second.label)+L","+std::to_wstring(down->second.value))
            <<L" plugin="<<check(migrated,&b,7,7)<<L"\\n";
        return 1;
    }
    return 0;}
'''
    run, env = native.build(code, "plugin-color-identity")
    result = native.execute(run, env, "identity")
    print(f"ARTIFACTS {run}")
    return int(not result)


if __name__ == "__main__":
    raise SystemExit(main())
