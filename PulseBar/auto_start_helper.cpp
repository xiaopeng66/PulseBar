#include "stdafx.h"
#include "auto_start_helper.h"

#include <Lmcons.h>

#include <atlbase.h>
#include <atlcomcli.h>
#include <comdef.h>
#include <memory>
#include <sddl.h>
#include <taskschd.h>
#include "SimpleXML.h"
#include "PulseBar.h"     //theApp.m_log_path（记录开机自动运行的失败信息）
#include "Common.h"             //CCommon::WriteLog
# pragma comment(lib, "taskschd.lib")
# pragma comment(lib, "advapi32.lib")

// Helper macros from wix.
// TODO: use "s" and "..." parameters to report errors from these functions.
#define ExitOnFailure(x, s, ...) \
    if (FAILED(x))               \
    {                            \
        goto LExit;              \
    }
#define ExitWithLastError(x, s, ...)       \
    {                                      \
        DWORD Dutil_er = ::GetLastError(); \
        x = HRESULT_FROM_WIN32(Dutil_er);  \
        if (!FAILED(x))                    \
        {                                  \
            x = E_FAIL;                    \
        }                                  \
        goto LExit;                        \
    }
#define ExitFunction() \
    {                  \
        goto LExit;    \
    }

const DWORD USERNAME_DOMAIN_LEN = DNLEN + UNLEN + 2; // Domain Name + '\' + User Name + '\0'
const DWORD USERNAME_LEN = UNLEN + 1; // User Name + '\0'

class ScopedComApartment
{
public:
    ScopedComApartment() : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), m_uninitialize(SUCCEEDED(m_result)) {}
    ~ScopedComApartment()
    {
        if (m_uninitialize)
            CoUninitialize();
    }

    bool CanUseCom() const
    {
        return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT m_result;
    bool m_uninitialize;
};

static bool IsNotFoundHRESULT(HRESULT hr)
{
    return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
}

class ScopedTokenHandle
{
public:
    ~ScopedTokenHandle()
    {
        if (m_handle)
            CloseHandle(m_handle);
    }
    HANDLE* Address() { return &m_handle; }
    HANDLE Get() const { return m_handle; }

private:
    HANDLE m_handle = nullptr;
};

static HRESULT BuildTaskSecurityDescriptor(std::wstring& sddl)
{
    ScopedTokenHandle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.Address()))
    {
        const DWORD error = GetLastError();
        return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
    }

    DWORD required_size = 0;
    SetLastError(ERROR_SUCCESS);
    GetTokenInformation(token.Get(), TokenUser, nullptr, 0, &required_size);
    DWORD error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER || required_size == 0)
        return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);

    std::vector<BYTE> token_info(required_size);
    if (!GetTokenInformation(token.Get(), TokenUser, token_info.data(), required_size, &required_size))
    {
        error = GetLastError();
        return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
    }

    LPWSTR user_sid_string = nullptr;
    TOKEN_USER* token_user = reinterpret_cast<TOKEN_USER*>(token_info.data());
    if (!ConvertSidToStringSidW(token_user->User.Sid, &user_sid_string))
    {
        error = GetLastError();
        return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
    }
    std::unique_ptr<wchar_t, decltype(&LocalFree)> sid_owner(user_sid_string, &LocalFree);

    sddl = L"D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;";
    sddl += user_sid_string;
    sddl += L")";
    return S_OK;
}

static HRESULT DeleteTaskFromFolder(ITaskService* service, const std::wstring& folder_path, const std::wstring& task_name)
{
    CComPtr<ITaskFolder> folder;
    HRESULT hr = service->GetFolder(_bstr_t(folder_path.c_str()), &folder);
    if (FAILED(hr))
        return IsNotFoundHRESULT(hr) ? S_OK : hr;

    hr = folder->DeleteTask(_bstr_t(task_name.c_str()), 0);
    return IsNotFoundHRESULT(hr) ? S_OK : hr;
}

//将开机自动运行相关的失败信息写入 error.log。
//这个流程的错误过去被静默吞掉（只能看到“设置失败”却不知道哪一步失败），
//因此所有失败分支都要留下可追查的记录。
static void WriteAutoRunLog(const std::wstring& text)
{
    CCommon::WriteLog(text.c_str(), theApp.m_log_path.c_str());
}

//带 HRESULT 的失败记录，避免各处重复拼字符串
static void WriteAutoRunHrLog(const wchar_t* format, HRESULT hr)
{
    CString text;
    text.Format(format, hr);
    CCommon::WriteLog(text.GetString(), theApp.m_log_path.c_str());
}

bool create_auto_start_task_for_this_user(bool runElevated)
{
    ScopedComApartment com_apartment;
    if (!com_apartment.CanUseCom())
        return false;

    HRESULT hr = S_OK;

    WCHAR username_domain[USERNAME_DOMAIN_LEN];
    WCHAR username[USERNAME_LEN];

    std::wstring wstrTaskName;
    std::wstring task_sddl;

    CComPtr<ITaskService> pService;
    CComPtr<ITaskFolder> pTaskFolder;
    CComPtr<ITaskDefinition> pTask;
    CComPtr<IRegistrationInfo> pRegInfo;
    CComPtr<ITaskSettings> pSettings;
    CComPtr<ITriggerCollection> pTriggerCollection;
    CComPtr<IRegisteredTask> pRegisteredTask;

    // ------------------------------------------------------
    // Get the Domain/Username for the trigger.
    if (!GetEnvironmentVariable(L"USERNAME", username, USERNAME_LEN))
    {
        ExitWithLastError(hr, "Getting username failed: %x", hr);
    }
    if (!GetEnvironmentVariable(L"USERDOMAIN", username_domain, USERNAME_DOMAIN_LEN))
    {
        ExitWithLastError(hr, "Getting the user's domain failed: %x", hr);
    }
    wcscat_s(username_domain, L"\\");
    wcscat_s(username_domain, username);

    // Task Name.
    wstrTaskName = L"Autorun for ";
    wstrTaskName += username;

    // Get the executable path passed to the custom action.
    WCHAR wszExecutablePath[MAX_PATH];
    GetModuleFileName(NULL, wszExecutablePath, MAX_PATH);

    hr = BuildTaskSecurityDescriptor(task_sddl);
    ExitOnFailure(hr, "Cannot build the current account's task security descriptor: %x", hr);

    // ------------------------------------------------------
    // Create an instance of the Task Service.
    hr = CoCreateInstance(CLSID_TaskScheduler,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_ITaskService,
                          (void**)&pService);
    ExitOnFailure(hr, "Failed to create an instance of ITaskService: %x", hr);

    // Connect to the task service.
    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    ExitOnFailure(hr, "ITaskService::Connect failed: %x", hr);

    // ------------------------------------------------------
    // Get the PulseBar task folder. Creates it if it doesn't exist.
    hr = pService->GetFolder(_bstr_t(L"\\PulseBar"), &pTaskFolder);
    if (IsNotFoundHRESULT(hr))
    {
        // Folder doesn't exist. Get the Root folder and create the PulseBar subfolder.
        CComPtr<ITaskFolder> pRootFolder;
        hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
        ExitOnFailure(hr, "Cannot get Root Folder pointer: %x", hr);
        hr = pRootFolder->CreateFolder(_bstr_t(L"\\PulseBar"), _variant_t(L""), &pTaskFolder);
    }
    ExitOnFailure(hr, "Cannot open or create PulseBar task folder: %x", hr);

    // 如果任务已存在且其命令行就是当前程序的路径，则修复权限并启用它；
    // 否则继续向下执行重新注册任务（例如程序被移动到其他目录后，原任务的路径已失效），
    // 避免“函数返回成功但任务仍指向错误路径”而导致开机自动运行失效
    {
        CComPtr<IRegisteredTask> pExistingRegisteredTask;
        hr = pTaskFolder->GetTask(_bstr_t(wstrTaskName.c_str()), &pExistingRegisteredTask);
        if (SUCCEEDED(hr))
        {
            bool command_path_match = false;
            CComBSTR xml_buff;
            HRESULT hr_xml = pExistingRegisteredTask->get_Xml(&xml_buff);
            if (SUCCEEDED(hr_xml))
            {
                CSimpleXML xml;
                xml.LoadXMLContentDirect(xml_buff.m_str);
                std::wstring existing_command = xml.GetNode(L"Command", L"Exec");
                command_path_match = (existing_command == wszExecutablePath);
                if (!command_path_match)
                {
                    //路径不一致：必须重新注册，否则任务会一直指向失效的旧路径
                    std::wstring log;
                    log = L"开机自动运行：已存在的任务指向的路径与当前程序不一致，将重新注册。\r\n旧路径: ";
                    log += existing_command;
                    log += L"\r\n当前路径: ";
                    log += wszExecutablePath;
                    WriteAutoRunLog(log);
                }
            }
            else
            {
                //读不到任务XML就无法判断路径，此时必须重建，不能当作“路径一致”提前返回
                WriteAutoRunHrLog(L"开机自动运行：读取已存在任务的XML失败（hr=0x%08X），将重新注册任务。", hr_xml);
            }

            HRESULT hr_security = pExistingRegisteredTask->SetSecurityDescriptor(
                _bstr_t(task_sddl.c_str()), TASK_DONT_ADD_PRINCIPAL_ACE);
            if (FAILED(hr_security))
                WriteAutoRunHrLog(L"开机自动运行：修复已存在任务权限失败（hr=0x%08X），将重新注册任务。", hr_security);

            HRESULT hr_enable = pExistingRegisteredTask->put_Enabled(VARIANT_TRUE);
            if (command_path_match)
            {
                if (SUCCEEDED(hr_enable) && SUCCEEDED(hr_security))
                {
                    hr = S_OK;
                    ExitFunction();
                }
                if (FAILED(hr_enable))
                    WriteAutoRunHrLog(L"开机自动运行：启用已存在的任务失败（hr=0x%08X），将重新注册任务。", hr_enable);
            }
            hr = S_OK;
        }
    }

    // Create the task builder object to create the task.
    hr = pService->NewTask(0, &pTask);
    ExitOnFailure(hr, "Failed to create a task definition: %x", hr);

    // ------------------------------------------------------
    // Get the registration info for setting the identification.
    hr = pTask->get_RegistrationInfo(&pRegInfo);
    ExitOnFailure(hr, "Cannot get identification pointer: %x", hr);
    hr = pRegInfo->put_Author(_bstr_t(username_domain));
    ExitOnFailure(hr, "Cannot put identification info: %x", hr);

    // ------------------------------------------------------
    // Create the settings for the task
    hr = pTask->get_Settings(&pSettings);
    ExitOnFailure(hr, "Cannot get settings pointer: %x", hr);

    hr = pSettings->put_StartWhenAvailable(VARIANT_FALSE);
    ExitOnFailure(hr, "Cannot put_StartWhenAvailable setting info: %x", hr);
    hr = pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
    ExitOnFailure(hr, "Cannot put_StopIfGoingOnBatteries setting info: %x", hr);
    hr = pSettings->put_ExecutionTimeLimit(_bstr_t(L"PT0S")); //Unlimited
    ExitOnFailure(hr, "Cannot put_ExecutionTimeLimit setting info: %x", hr);
    hr = pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    ExitOnFailure(hr, "Cannot put_DisallowStartIfOnBatteries setting info: %x", hr);

    // ------------------------------------------------------
    // Get the trigger collection to insert the logon trigger.
    hr = pTask->get_Triggers(&pTriggerCollection);
    ExitOnFailure(hr, "Cannot get trigger collection: %x", hr);

    // Add the logon trigger to the task.
    {
        CComPtr<ITrigger> pTrigger;
        CComPtr<ILogonTrigger> pLogonTrigger;
        hr = pTriggerCollection->Create(TASK_TRIGGER_LOGON, &pTrigger);
        ExitOnFailure(hr, "Cannot create the trigger: %x", hr);

        hr = pTrigger->QueryInterface(
            IID_ILogonTrigger, (void**)&pLogonTrigger);
        ExitOnFailure(hr, "QueryInterface call failed for ILogonTrigger: %x", hr);

        hr = pLogonTrigger->put_Id(_bstr_t(L"Trigger1"));

        // Timing issues may make explorer not be started when the task runs.
        // Add a little delay to mitigate this.
        hr = pLogonTrigger->put_Delay(_bstr_t(L"PT03S"));

        // Define the user. The task will execute when the user logs on.
        // The specified user must be a user on this computer.
        hr = pLogonTrigger->put_UserId(_bstr_t(username_domain));
        ExitOnFailure(hr, "Cannot add user ID to logon trigger: %x", hr);
    }

    // ------------------------------------------------------
    // Add an Action to the task. This task will execute the path passed to this custom action.
    {
        CComPtr<IActionCollection> pActionCollection;
        CComPtr<IAction> pAction;
        CComPtr<IExecAction> pExecAction;

        // Get the task action collection pointer.
        hr = pTask->get_Actions(&pActionCollection);
        ExitOnFailure(hr, "Cannot get Action collection pointer: %x", hr);

        // Create the action, specifying that it is an executable action.
        hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
        ExitOnFailure(hr, "Cannot create the action: %x", hr);

        // QI for the executable task pointer.
        hr = pAction->QueryInterface(
            IID_IExecAction, (void**)&pExecAction);
        ExitOnFailure(hr, "QueryInterface call failed for IExecAction: %x", hr);

        // Set the path of the executable (passed as CustomActionData).
        hr = pExecAction->put_Path(_bstr_t(wszExecutablePath));
        ExitOnFailure(hr, "Cannot set path of executable: %x", hr);
    }

    // ------------------------------------------------------
    // Create the principal for the task
    {
        CComPtr<IPrincipal> pPrincipal;
        hr = pTask->get_Principal(&pPrincipal);
        ExitOnFailure(hr, "Cannot get principal pointer: %x", hr);

        // Set up principal information:
        hr = pPrincipal->put_Id(_bstr_t(L"Principal1"));

        hr = pPrincipal->put_UserId(_bstr_t(username_domain));

        hr = pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);

        if (runElevated)
        {
            hr = pPrincipal->put_RunLevel(_TASK_RUNLEVEL::TASK_RUNLEVEL_HIGHEST);
        }
        else
        {
            hr = pPrincipal->put_RunLevel(_TASK_RUNLEVEL::TASK_RUNLEVEL_LUA);
        }
        ExitOnFailure(hr, "Cannot put principal run level: %x", hr);
    }
    // ------------------------------------------------------
    // Save the task with access restricted to SYSTEM, administrators and this account.
    {
        hr = pTaskFolder->RegisterTaskDefinition(
            _bstr_t(wstrTaskName.c_str()),
            pTask,
            TASK_CREATE_OR_UPDATE | TASK_DONT_ADD_PRINCIPAL_ACE,
            _variant_t(username_domain),
            _variant_t(),
            TASK_LOGON_INTERACTIVE_TOKEN,
            _variant_t(task_sddl.c_str()),
            &pRegisteredTask);
        ExitOnFailure(hr, "Error saving the Task : %x", hr);
    }

LExit:
    //所有失败分支都汇聚到这里，统一留一条可追查的记录
    if (FAILED(hr))
    {
        WriteAutoRunHrLog(L"开机自动运行：创建计划任务失败（hr=0x%08X）。", hr);
    }
    return (SUCCEEDED(hr));
}

bool delete_auto_start_task_for_this_user()
{
    ScopedComApartment com_apartment;
    if (!com_apartment.CanUseCom())
        return false;

    HRESULT hr = S_OK;
    HRESULT hr_legacy = S_OK;

    WCHAR username[USERNAME_LEN];
    std::wstring wstrTaskName;

    CComPtr<ITaskService> pService;

    // ------------------------------------------------------
    // Get the Username for the task.
    if (!GetEnvironmentVariable(L"USERNAME", username, USERNAME_LEN))
    {
        ExitWithLastError(hr, "Getting username failed: %x", hr);
    }

    // Task Name.
    wstrTaskName = L"Autorun for ";
    wstrTaskName += username;

    // ------------------------------------------------------
    // Create an instance of the Task Service.
    hr = CoCreateInstance(CLSID_TaskScheduler,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_ITaskService,
                          (void**)&pService);
    ExitOnFailure(hr, "Failed to create an instance of ITaskService: %x", hr);

    // Connect to the task service.
    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    ExitOnFailure(hr, "ITaskService::Connect failed: %x", hr);

    // Delete both the current and legacy tasks; only not-found is success.
    hr = DeleteTaskFromFolder(pService, L"\\PulseBar", wstrTaskName);
    hr_legacy = DeleteTaskFromFolder(pService, L"\\TrafficMonitor", wstrTaskName);
    if (SUCCEEDED(hr))
        hr = hr_legacy;

LExit:
    if (FAILED(hr))
    {
        WriteAutoRunHrLog(L"开机自动运行：删除计划任务失败（hr=0x%08X）。", hr);
    }
    return (SUCCEEDED(hr));
}

bool is_auto_start_task_active_for_this_user(std::wstring* path)
{
    ScopedComApartment com_apartment;
    if (!com_apartment.CanUseCom())
        return false;

    HRESULT hr = S_OK;

    WCHAR username[USERNAME_LEN];
    std::wstring wstrTaskName;

    CComPtr<ITaskService> pService;
    CComPtr<ITaskFolder> pTaskFolder;

    bool command_path_match{};

    // ------------------------------------------------------
    // Get the Username for the task.
    if (!GetEnvironmentVariable(L"USERNAME", username, USERNAME_LEN))
    {
        ExitWithLastError(hr, "Getting username failed: %x", hr);
    }

    // Task Name.
    wstrTaskName = L"Autorun for ";
    wstrTaskName += username;

    // ------------------------------------------------------
    // Create an instance of the Task Service.
    hr = CoCreateInstance(CLSID_TaskScheduler,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_ITaskService,
                          (void**)&pService);
    ExitOnFailure(hr, "Failed to create an instance of ITaskService: %x", hr);

    // Connect to the task service.
    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    ExitOnFailure(hr, "ITaskService::Connect failed: %x", hr);

    // ------------------------------------------------------
    // Get the PulseBar task folder.
    hr = pService->GetFolder(_bstr_t(L"\\PulseBar"), &pTaskFolder);
    ExitOnFailure(hr, "ITaskFolder doesn't exist: %x", hr);

    // ------------------------------------------------------
    // If the task exists, disable.
    {
        CComPtr<IRegisteredTask> pExistingRegisteredTask;
        hr = pTaskFolder->GetTask(_bstr_t(wstrTaskName.c_str()), &pExistingRegisteredTask);
        if (SUCCEEDED(hr))
        {
            // Task exists, get its value.
            VARIANT_BOOL is_enabled;
            hr = pExistingRegisteredTask->get_Enabled(&is_enabled);
            //判断已存在的任务计划命令的exe文件路径是否为当前exe的路径
            CComBSTR xml_buff;
            HRESULT hr_xml = pExistingRegisteredTask->get_Xml(&xml_buff);
            std::wstring command_path;
            if (SUCCEEDED(hr_xml))
            {
                CSimpleXML xml;
                xml.LoadXMLContentDirect(xml_buff.m_str);
                command_path = xml.GetNode(L"Command", L"Exec");
            }
            else
            {
                //读不到XML时无法判断路径，此处只记录，不影响“是否已启用”的返回值
                WriteAutoRunHrLog(L"开机自动运行：读取任务XML失败（hr=0x%08X），无法校验任务指向的路径。", hr_xml);
            }
            if (path != nullptr)
                *path = command_path;
            WCHAR wszExecutablePath[MAX_PATH];
            GetModuleFileName(NULL, wszExecutablePath, MAX_PATH);
            command_path_match = (command_path == wszExecutablePath);

            if (SUCCEEDED(hr))
            {
                // Got the value. Return it.
                hr = (is_enabled == VARIANT_TRUE) ? S_OK : E_FAIL; // Fake success or fail to return the value.
                ExitFunction();
            }
        }
    }

LExit:
    return (SUCCEEDED(hr) && command_path_match);
}
