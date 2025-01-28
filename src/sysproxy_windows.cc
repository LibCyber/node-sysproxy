#include <napi.h>
#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <Ras.h>
#include <Raserror.h>
// #include <iostream>

enum RET_ERRORS
{
	RET_NO_ERROR = 0,
	INVALID_FORMAT = 1,
	NO_PERMISSION = 2,
	SYSCALL_FAILED = 3,
	NO_MEMORY = 4,
	INVALID_OPTION_COUNT = 5,
};

// 支持格式化输出调试信息
#define DEBUG_LOG(msg) printf("[Debug] %s\n", msg);

// 将标准字符串（UTF-8）转换为宽字符串（UTF-16）
LPWSTR StringToLPWSTR(const std::string& inputStr) {
    int bufLen = MultiByteToWideChar(CP_UTF8, 0, inputStr.c_str(), -1, NULL, 0);
    if (bufLen == 0) {
        return nullptr;
    }

    LPWSTR wideStr = new wchar_t[bufLen];
    if (wideStr == nullptr) {
        return nullptr;
    }

    int convResult = MultiByteToWideChar(CP_UTF8, 0, inputStr.c_str(), -1, wideStr, bufLen);
    if (convResult == 0) {
        delete[] wideStr;
        return nullptr;
    }

    return wideStr;
}

// 将宽字符串（UTF-16）转换为标准字符串（UTF-8）
std::string LPWSTRToString(const wchar_t* wideString) {
    if (!wideString) return "";

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, NULL, 0, NULL, NULL);
    std::string utf8String(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideString, -1, &utf8String[0], sizeNeeded, NULL, NULL);
    
    // Remove the null terminator
    utf8String.resize(sizeNeeded - 1);
    return utf8String;
}

RET_ERRORS initialize(INTERNET_PER_CONN_OPTION_LIST* optionList, int optionCount)
{
    // DEBUG_LOG("initialize called");
    if (optionCount < 1)
    {
        return INVALID_OPTION_COUNT;
    }

    optionList->dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
    optionList->dwOptionCount = optionCount;
    optionList->dwOptionError = 0;

    optionList->pOptions = new(std::nothrow) INTERNET_PER_CONN_OPTION[optionCount];
    if (optionList->pOptions == nullptr)
    {
        return NO_MEMORY;
    }

    // DEBUG_LOG("initialize success");
    return RET_NO_ERROR;
}

// 检查并释放 optionList
void free_optionList(INTERNET_PER_CONN_OPTION_LIST* optionList) {
    // DEBUG_LOG("free_optionList called");
    if (!optionList) return;

    if (optionList->pOptions) {
        for (DWORD i = 0; i < optionList->dwOptionCount; i++) {
            // 只释放自己 new 出来的那几项
            if ((optionList->pOptions[i].dwOption == INTERNET_PER_CONN_PROXY_SERVER ||
                 optionList->pOptions[i].dwOption == INTERNET_PER_CONN_PROXY_BYPASS ||
                 optionList->pOptions[i].dwOption == INTERNET_PER_CONN_AUTOCONFIG_URL) &&
                optionList->pOptions[i].Value.pszValue != NULL)
            {
                // 在 SetProxy 中才会使用 new wchar_t[] 
                // 所以这里仅在它不为 NULL 时 delete[]，没问题。
                delete[] optionList->pOptions[i].Value.pszValue;
                optionList->pOptions[i].Value.pszValue = 0;
            } else {
                if (optionList->pOptions[i].dwOption == INTERNET_PER_CONN_FLAGS_UI || optionList->pOptions[i].dwOption == INTERNET_PER_CONN_FLAGS) {
                    // LocalFree(optionList->pOptions[i].Value.pszValue);
                    // optionList->pOptions[i].Value.pszValue = NULL;
                }
            }
        }
        
        delete[] optionList->pOptions;
        optionList->pOptions = NULL;
    }
    delete optionList;

    // DEBUG_LOG("free_optionList success");
}

// 返回代理配置，返回值格式：{
//     flags: 0,
//     autoConfigUrl: 'http://xxx',
//     proxyServer: 'host:port',
//     bypassList: 'bypass1;bypass2'
// }
Napi::Value QueryProxy(const Napi::CallbackInfo& info) {
    // DEBUG_LOG("QueryProxy called");
    Napi::Env env = info.Env();

    INTERNET_PER_CONN_OPTION_LIST* optionList = new INTERNET_PER_CONN_OPTION_LIST;
    memset(optionList, 0, sizeof(INTERNET_PER_CONN_OPTION_LIST));
    initialize(optionList, 4);
    
    DWORD dwLen = sizeof(INTERNET_PER_CONN_OPTION_LIST);

    // On Windows 7 or above (IE8+), query with INTERNET_PER_CONN_FLAGS_UI is recommended.
	// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa385145(v=vs.85).aspx
    optionList->pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS_UI;

    optionList->pOptions[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    optionList->pOptions[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
    optionList->pOptions[3].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;

	if (!InternetQueryOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, optionList, &dwLen))
	{
		// DEBUG_LOG("InternetQueryOptionW failed");
		// Set option to INTERNET_PER_CONN_FLAGS and try again to compatible with older versions of Windows.
		optionList->pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;

		if (!InternetQueryOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, optionList, &dwLen))
		{
			DWORD error = GetLastError();
            Napi::Error::New(env, "Failed to query proxy configuration. Error code: " + std::to_string(error)).ThrowAsJavaScriptException();
            // Free memory
            free_optionList(optionList);
            return env.Null();
		}
	}

    // DEBUG_LOG("InternetQueryOptionW success");

    Napi::Object result = Napi::Object::New(env);
    result.Set(Napi::String::New(env, "flags"), Napi::Number::New(env, optionList->pOptions[0].Value.dwValue));
    // 将宽字符字符串转换为UTF-8字符串
    std::string proxyServer = LPWSTRToString(optionList->pOptions[1].Value.pszValue);
    std::string bypassList = LPWSTRToString(optionList->pOptions[2].Value.pszValue);
    std::string autoConfigUrl = LPWSTRToString(optionList->pOptions[3].Value.pszValue);

    // **马上释放** WinINet 分配的字符串
    if (optionList->pOptions[1].Value.pszValue) {
        LocalFree(optionList->pOptions[1].Value.pszValue);
        optionList->pOptions[1].Value.pszValue = NULL;
    }
    if (optionList->pOptions[2].Value.pszValue) {
        LocalFree(optionList->pOptions[2].Value.pszValue);
        optionList->pOptions[2].Value.pszValue = NULL;
    }
    if (optionList->pOptions[3].Value.pszValue) {
        LocalFree(optionList->pOptions[3].Value.pszValue);
        optionList->pOptions[3].Value.pszValue = NULL;
    }

    result.Set(Napi::String::New(env, "proxyServer"), Napi::String::New(env, proxyServer));
    result.Set(Napi::String::New(env, "bypassList"), Napi::String::New(env, bypassList));
    result.Set(Napi::String::New(env, "autoConfigUrl"), Napi::String::New(env, autoConfigUrl));

    // Free memory
    free_optionList(optionList);

    // DEBUG_LOG("QueryProxy success");
    return result;
}

RET_ERRORS apply_connect(INTERNET_PER_CONN_OPTION_LIST* optionList, LPWSTR conn)
{
    // DEBUG_LOG("apply_connect called");
	optionList->pszConnection = conn;

	BOOL result = InternetSetOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, optionList, sizeof(INTERNET_PER_CONN_OPTION_LIST));
	if (!result)
	{
		return SYSCALL_FAILED;
	}

	result = InternetSetOptionW(NULL, INTERNET_OPTION_PROXY_SETTINGS_CHANGED, NULL, 0);
	if (!result)
	{
		return SYSCALL_FAILED;
	}

	result = InternetSetOptionW(NULL, INTERNET_OPTION_REFRESH, NULL, 0);
	if (!result)
	{
		return SYSCALL_FAILED;
	}

    // DEBUG_LOG("apply_connect success");
	return RET_NO_ERROR;
}


RET_ERRORS apply(INTERNET_PER_CONN_OPTION_LIST* optionList)
{
    // DEBUG_LOG("apply called");
	RET_ERRORS ret;
	DWORD dwCb = 0;
	DWORD dwRet;
	DWORD dwEntries = 0;
	LPRASENTRYNAME lpRasEntryName = NULL;

	// Set LAN
	if ((ret = apply_connect(optionList, NULL)) > RET_NO_ERROR)
	{
		return ret;
	}

	// Find connections and apply proxy settings
	dwRet = RasEnumEntriesW(NULL, NULL, lpRasEntryName, &dwCb, &dwEntries);

	if (dwRet == ERROR_BUFFER_TOO_SMALL)
	{
		lpRasEntryName = (LPRASENTRYNAME)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwCb);

		if (lpRasEntryName == NULL)
		{
			return NO_MEMORY;
		}

		for (DWORD i = 0; i < dwEntries; i++)
		{
			lpRasEntryName[i].dwSize = sizeof(RASENTRYNAME);
		}

		dwRet = RasEnumEntriesW(NULL, NULL, lpRasEntryName, &dwCb, &dwEntries);
	}

	if (dwRet != ERROR_SUCCESS)
	{
    	HeapFree(GetProcessHeap(), 0, lpRasEntryName);
	    lpRasEntryName = NULL;
		return SYSCALL_FAILED;
	}
	else
	{
		if (dwEntries > 0)
		{
			for (DWORD i = 0; i < dwEntries; i++)
			{
				if ((ret = apply_connect(optionList, lpRasEntryName[i].szEntryName)) > RET_NO_ERROR)
				{
					HeapFree(GetProcessHeap(), 0, lpRasEntryName);
					lpRasEntryName = NULL;
					return ret;
				}
			}
		}
	}

    // DEBUG_LOG("apply success");
	return RET_NO_ERROR;
}

// 设置代理，传参格式：{
//     flags: 3,
//     autoConfigUrl: '',
//     proxyServer: 'host:port',
//     bypassList: 'bypass1;bypass2'
// }
// 提取传参中的代理配置，通过apply和apply_connect设置代理
Napi::Boolean SetProxy(const Napi::CallbackInfo& info) {
    // DEBUG_LOG("SetProxy called");
    Napi::Env env = info.Env();
    try
    {
        // 校验传参，每个参数都是必传的
        if (info.Length() < 1 || !info[0].IsObject()) {
            Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
            return Napi::Boolean::New(env, false);
        }

        Napi::Object inputInfo = info[0].As<Napi::Object>();

        INTERNET_PER_CONN_OPTION_LIST* optionList = new INTERNET_PER_CONN_OPTION_LIST;
        memset(optionList, 0, sizeof(INTERNET_PER_CONN_OPTION_LIST));
        RET_ERRORS ret = initialize(optionList, 4);
        if (ret != RET_NO_ERROR) {
            Napi::Error::New(env, "Failed to initialize proxy options").ThrowAsJavaScriptException();
            free_optionList(optionList);
            return Napi::Boolean::New(env, false);
        }

        // Initialize proxy options
        int optionCount = 0;
        if (inputInfo.Has("flags")) {
            optionList->pOptions[optionCount].dwOption = INTERNET_PER_CONN_FLAGS_UI;
            optionList->pOptions[optionCount].Value.dwValue = inputInfo.Get("flags").As<Napi::Number>().Int32Value();
            optionCount++;
        } else {
            Napi::Error::New(env, "Missing flags").ThrowAsJavaScriptException();
            free_optionList(optionList);
            return Napi::Boolean::New(env, false);
        }

        if (inputInfo.Has("autoConfigUrl")) {
            optionList->pOptions[optionCount].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
            std::string autoConfigUrl = inputInfo.Get("autoConfigUrl").As<Napi::String>().Utf8Value();
            optionList->pOptions[optionCount].Value.pszValue = StringToLPWSTR(autoConfigUrl);
            optionCount++;
        } else {
            Napi::Error::New(env, "Missing autoConfigUrl").ThrowAsJavaScriptException();
            free_optionList(optionList);
            return Napi::Boolean::New(env, false);
        }

        if (inputInfo.Has("proxyServer")) {
            optionList->pOptions[optionCount].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
            std::string proxyServer = inputInfo.Get("proxyServer").As<Napi::String>().Utf8Value();
            optionList->pOptions[optionCount].Value.pszValue = StringToLPWSTR(proxyServer);
            optionCount++;
        } else {
            Napi::Error::New(env, "Missing proxyServer").ThrowAsJavaScriptException();
            free_optionList(optionList);
            return Napi::Boolean::New(env, false);
        }

        if (inputInfo.Has("bypassList")) {
            optionList->pOptions[optionCount].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
            std::string bypassList = inputInfo.Get("bypassList").As<Napi::String>().Utf8Value();
            optionList->pOptions[optionCount].Value.pszValue = StringToLPWSTR(bypassList);
            optionCount++;
        } else {
            Napi::Error::New(env, "Missing bypassList").ThrowAsJavaScriptException();
            free_optionList(optionList);
            return Napi::Boolean::New(env, false);
        }

        ret = apply(optionList);
        if (ret != RET_NO_ERROR) {
            Napi::Error::New(env, "Failed to apply proxy options, error code: " + std::to_string(ret)).ThrowAsJavaScriptException();
            free_optionList(optionList);
            return Napi::Boolean::New(env, false);
        }

        
        free_optionList(optionList);

        // DEBUG_LOG("SetProxy success");
        return Napi::Boolean::New(env, true);
    }
    catch(const std::exception& e)
    {
        Napi::Error::New(env, "Failed to set proxy, error: " + std::string(e.what())).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
}


Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "queryWindowsProxy"), Napi::Function::New(env, QueryProxy));
    exports.Set(Napi::String::New(env, "setWindowsProxy"), Napi::Function::New(env, SetProxy));
    return exports;
}

NODE_API_MODULE(sysproxy, Init)
