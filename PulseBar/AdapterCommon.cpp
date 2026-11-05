#include "stdafx.h"
#include "AdapterCommon.h"


CAdapterCommon::CAdapterCommon()
{
}


CAdapterCommon::~CAdapterCommon()
{
}

bool CAdapterCommon::GetInterfaceCounters(unsigned int interface_index, unsigned long long interface_luid,
    NetworkCounterSample& sample)
{
    sample = {};
#ifndef COMPILE_FOR_WINXP
    if (interface_index == 0 && interface_luid == 0)
        return false;
    //iphlpapi is an existing application dependency. Resolve the Vista+ export
    //without adding an unconditional import to the optional XP build.
    using GetIfEntry2Function = NETIO_STATUS (NETIOAPI_API_*)(PMIB_IF_ROW2);
    static const auto get_entry = []() -> GetIfEntry2Function
    {
        const HMODULE module = ::GetModuleHandleW(L"iphlpapi.dll");
        return module == nullptr ? nullptr
            : reinterpret_cast<GetIfEntry2Function>(::GetProcAddress(module, "GetIfEntry2"));
    }();
    if (get_entry == nullptr)
        return false;
    MIB_IF_ROW2 row{};
    row.InterfaceIndex = interface_index;
    row.InterfaceLuid.Value = interface_luid;
    if (get_entry(&row) != NO_ERROR || row.OperStatus != IfOperStatusUp
        || row.InterfaceLuid.Value == 0
        || (interface_index != 0 && row.InterfaceIndex != interface_index)
        || (interface_luid != 0 && row.InterfaceLuid.Value != interface_luid))
        return false;
    sample.interface_index = row.InterfaceIndex;
    sample.interface_luid = row.InterfaceLuid.Value;
    sample.in_bytes = row.InOctets;
    sample.out_bytes = row.OutOctets;
    return true;
#else
    //The XP-only build has no native 64-bit interface counters.
    //Unavailable is safer than counting wrapped DWORD values as full-width data.
    return false;
#endif
}

void CAdapterCommon::GetAdapterInfo(vector<NetWorkConection>& adapters)
{
	adapters.clear();
	PIP_ADAPTER_INFO pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[sizeof(IP_ADAPTER_INFO)];		//PIP_ADAPTER_INFO结构体指针存储本机网卡信息
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);		//得到结构体大小,用于GetAdaptersInfo参数
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);	//调用GetAdaptersInfo函数,填充pIpAdapterInfo指针变量;其中stSize参数既是一个输入量也是一个输出量

	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		//如果函数返回的是ERROR_BUFFER_OVERFLOW
		//则说明GetAdaptersInfo参数传递的内存空间不够,同时其传出stSize,表示需要的空间大小
		//这也是说明为什么stSize既是一个输入量也是一个输出量
		delete[] (BYTE*)pIpAdapterInfo;	//释放原来的内存空间
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];	//重新申请内存空间用来存储所有网卡信息
		nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);		//再次调用GetAdaptersInfo函数,填充pIpAdapterInfo指针变量
	}

	PIP_ADAPTER_INFO pIpAdapterInfoHead = pIpAdapterInfo;	//保存pIpAdapterInfo链表中第一个元素的地址
	if (ERROR_SUCCESS == nRel)
	{
		while (pIpAdapterInfo)
		{
			NetWorkConection connection;
			connection.description = pIpAdapterInfo->Description;
            connection.interface_index = pIpAdapterInfo->Index;
			connection.ip_address = CCommon::StrToUnicode(pIpAdapterInfo->IpAddressList.IpAddress.String);
			connection.subnet_mask = CCommon::StrToUnicode(pIpAdapterInfo->IpAddressList.IpMask.String);
			connection.default_gateway = CCommon::StrToUnicode(pIpAdapterInfo->GatewayList.IpAddress.String);

			adapters.push_back(connection);
			pIpAdapterInfo = pIpAdapterInfo->Next;
		}
	}
	//释放内存空间
	if (pIpAdapterInfoHead)
	{
		delete[] (BYTE*)pIpAdapterInfoHead;
	}
	if (adapters.empty())
	{
		NetWorkConection connection{};
		connection.description = CCommon::UnicodeToStr(CCommon::LoadText(L"<", IDS_NO_CONNECTION, L">"));
		adapters.push_back(connection);
	}
}

void CAdapterCommon::RefreshIpAddress(vector<NetWorkConection>& adapters)
{
	vector<NetWorkConection> adapters_tmp;
	GetAdapterInfo(adapters_tmp);
	for (const auto& adapter_tmp : adapters_tmp)
	{
		for (auto& adapter : adapters)
		{
			if (adapter_tmp.description == adapter.description)
			{
				adapter.ip_address = adapter_tmp.ip_address;
				adapter.subnet_mask = adapter_tmp.subnet_mask;
				adapter.default_gateway = adapter_tmp.default_gateway;
			}
		}
	}
}

void CAdapterCommon::GetIfTableInfo(vector<NetWorkConection>& adapters, MIB_IFTABLE* pIfTable)
{
	if (pIfTable == nullptr)
		return;
	//依次在IfTable里查找每个连接
	for (size_t i{}; i < adapters.size(); i++)
	{
		if (adapters[i].description.empty())
			continue;
        int index = -1;
        if (adapters[i].interface_index != 0)
        {
            for (DWORD row = 0; row < pIfTable->dwNumEntries; ++row)
            {
                if (pIfTable->table[row].dwIndex == adapters[i].interface_index)
                {
                    index = static_cast<int>(row);
                    break;
                }
            }
        }
        else
        {
            index = FindConnectionInIfTable(adapters[i].description, pIfTable);
            if (index == -1)
                index = FindConnectionInIfTableFuzzy(adapters[i].description, pIfTable);
        }
		adapters[i].index = index;
        adapters[i].interface_luid = 0;
		if (index < 0 || index >= static_cast<int>(pIfTable->dwNumEntries))
		{
			adapters[i].in_bytes = 0;
			adapters[i].out_bytes = 0;
			adapters[i].description_2.clear();
			continue;
		}
		adapters[i].in_bytes = pIfTable->table[index].dwInOctets;
		adapters[i].out_bytes = pIfTable->table[index].dwOutOctets;
		adapters[i].description_2 = (const char*)pIfTable->table[index].bDescr;
        adapters[i].interface_index = pIfTable->table[index].dwIndex;
        NetworkCounterSample counters;
        if (GetInterfaceCounters(adapters[i].interface_index, 0, counters))
            adapters[i].interface_luid = counters.interface_luid;
	}
}

void CAdapterCommon::GetAllIfTableInfo(vector<NetWorkConection>& adapters, MIB_IFTABLE * pIfTable)
{
	if (pIfTable == nullptr)
	{
		adapters.clear();
		return;
	}
	vector<NetWorkConection> adapters_tmp;
	GetAdapterInfo(adapters_tmp);		//获取IP地址
	adapters.clear();
	for (size_t i{}; i < pIfTable->dwNumEntries; i++)
	{
		NetWorkConection connection;
		connection.description = connection.description_2 = (const char*)pIfTable->table[i].bDescr;
		connection.index = i;
		connection.in_bytes = pIfTable->table[i].dwInOctets;
		connection.out_bytes = pIfTable->table[i].dwOutOctets;
        connection.interface_index = pIfTable->table[i].dwIndex;
        NetworkCounterSample counters;
        if (GetInterfaceCounters(connection.interface_index, 0, counters))
            connection.interface_luid = counters.interface_luid;
		for (size_t j{}; j < adapters_tmp.size(); j++)
		{
			if (connection.description.find(adapters_tmp[j].description) != string::npos)
			{
				connection.ip_address = adapters_tmp[j].ip_address;
				connection.subnet_mask = adapters_tmp[j].subnet_mask;
				connection.default_gateway = adapters_tmp[j].default_gateway;
				break;
			}
		}
		adapters.push_back(connection);
	}
}

int CAdapterCommon::FindConnectionInIfTable(string connection, MIB_IFTABLE* pIfTable)
{
	if (pIfTable == nullptr)
		return -1;
	for (size_t i{}; i < pIfTable->dwNumEntries; i++)
	{
		string descr = (const char*)pIfTable->table[i].bDescr;
		if (descr == connection)
			return i;
	}
	return -1;
}

int CAdapterCommon::FindConnectionInIfTableFuzzy(string connection, MIB_IFTABLE* pIfTable)
{
	if (pIfTable == nullptr)
		return -1;
	for (size_t i{}; i < pIfTable->dwNumEntries; i++)
	{
		string descr = (const char*)pIfTable->table[i].bDescr;
		size_t index;
		//在较长的字符串里查找较短的字符串
		if (descr.size() >= connection.size())
			index = descr.find(connection);
		else
			index = connection.find(descr);
		if (index != wstring::npos)
			return i;
	}
	//如果还是没有找到，则使用字符串匹配算法查找
	double max_degree{};
	int best_index{};
	for (size_t i{}; i < pIfTable->dwNumEntries; i++)
	{
		string descr = (const char*)pIfTable->table[i].bDescr;
		double degree = CCommon::StringSimilarDegree_LD(descr, connection);
		if (degree > max_degree)
		{
			max_degree = degree;
			best_index = i;
		}
	}
	return best_index;
}
