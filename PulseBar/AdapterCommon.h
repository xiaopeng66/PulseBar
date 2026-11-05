#pragma once
#include "Common.h"
#ifndef COMPILE_FOR_WINXP
#include <ws2tcpip.h>
#endif
#include <iphlpapi.h>
#ifndef COMPILE_FOR_WINXP
#include <netioapi.h>
#endif

// Full-width counters are separate from the legacy rows used by the details UI.
struct NetworkCounterSample
{
    unsigned int interface_index{};
    unsigned long long interface_luid{};
    unsigned long long in_bytes{};
    unsigned long long out_bytes{};
};

//保存一个网络连接信息
struct NetWorkConection
{
    int index{};            //该连接在MIB_IFTABLE中的索引，仅供详情快照使用
    string description;     //网络描述（获取自GetAdapterInfo）
    string description_2;   //网络描述（获取自GetIfTable）
    unsigned int in_bytes;  //初始时已接收字节数（旧版详情快照）
    unsigned int out_bytes; //初始时已发送字节数（旧版详情快照）
    wstring ip_address{ L"-.-.-.-" }; //IP地址
    wstring subnet_mask{ L"-.-.-.-" }; //子网掩码
    wstring default_gateway{ L"-.-.-.-" }; //默认网关
    unsigned int interface_index{}; //OS interface identity, never a table ordinal
    unsigned long long interface_luid{}; //Pinned identity, detects reuse of interface_index
};

class CAdapterCommon
{
public:
    CAdapterCommon();
    ~CAdapterCommon();

    //获取网络连接列表，填充网络描述、IP地址、子网掩码、默认网关信息
    static void GetAdapterInfo(vector<NetWorkConection>& adapters);

    //刷新网络连接列表中的IP地址、子网掩码、默认网关信息
    static void RefreshIpAddress(vector<NetWorkConection>& adapters);

    //获取网络列表中每个网络连接的MIB_IFTABLE中的索引、初始时已接收/发送字节数的信息
    static void GetIfTableInfo(vector<NetWorkConection>& adapters, MIB_IFTABLE* pIfTable);

    //直接将MIB_IFTABLE中的所有连接添加到adapters容器中
    static void GetAllIfTableInfo(vector<NetWorkConection>& adapters, MIB_IFTABLE* pIfTable);

    //False means unavailable: do not treat legacy DWORD counters as 64-bit samples.
    static bool GetInterfaceCounters(unsigned int interface_index, unsigned long long interface_luid,
        NetworkCounterSample& sample);
private:
    //根据一个网络连接描述判断是否在IfTable列表里，返回索引，找不到则返回-1
    static int FindConnectionInIfTable(string connection, MIB_IFTABLE* pIfTable);

    //根据一个网络连接描述判断是否在IfTable接列表里，返回索引，找不到则返回-1。只需要部分匹配
    static int FindConnectionInIfTableFuzzy(string connection, MIB_IFTABLE* pIfTable);
};
