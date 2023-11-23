#include <napi.h>
#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <Ras.h>
#include <Raserror.h>

enum RET_ERRORS
{
	RET_NO_ERROR = 0,
	INVALID_FORMAT = 1,
	NO_PERMISSION = 2,
	SYSCALL_FAILED = 3,
	NO_MEMORY = 4,
	INVALID_OPTION_COUNT = 5,
};

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

RET_ERRORS initialize(INTERNET_PER_CONN_OPTION_LIST* options, int option_count)
{
    if (option_count < 1)
    {
        return INVALID_OPTION_COUNT;
    }

    options->dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
    options->dwOptionCount = option_count;
    options->dwOptionError = 0;

    options->pOptions = new(std::nothrow) INTERNET_PER_CONN_OPTION[option_count];
    if (options->pOptions == nullptr)
    {
        return NO_MEMORY;
    }

    return RET_NO_ERROR;
}

// 返回代理配置，返回值格式：{
//     flags: 0,
//     autoConfigUrl: 'http://xxx',
//     proxyServer: 'host:port',
//     bypassList: 'bypass1;bypass2'
// }
Napi::Value QueryProxy(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    INTERNET_PER_CONN_OPTION_LIST* options = new INTERNET_PER_CONN_OPTION_LIST;
    memset(options, 0, sizeof(INTERNET_PER_CONN_OPTION_LIST));
    initialize(options, 4);
    
    DWORD dwLen = sizeof(INTERNET_PER_CONN_OPTION_LIST);

    // On Windows 7 or above (IE8+), query with INTERNET_PER_CONN_FLAGS_UI is recommended.
	// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa385145(v=vs.85).aspx
	options->pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS_UI;

	options->pOptions[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
	options->pOptions[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
	options->pOptions[3].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;

	if (!InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, options, &dwLen))
	{
		// Set option to INTERNET_PER_CONN_FLAGS and try again to compatible with older versions of Windows.
		options->pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;

		if (!InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, options, &dwLen))
		{
			DWORD error = GetLastError();
            Napi::Error::New(env, "Failed to query proxy configuration. Error code: " + std::to_string(error)).ThrowAsJavaScriptException();
            // Free memory
            delete[] options->pOptions;
            options->pOptions = NULL;
            delete options;
            return env.Null();
		}
	}

    Napi::Object result = Napi::Object::New(env);
    result.Set(Napi::String::New(env, "flags"), Napi::Number::New(env, options->pOptions[0].Value.dwValue));
    // 将宽字符字符串转换为UTF-8字符串
    std::string proxyServer = LPWSTRToString(options->pOptions[1].Value.pszValue);
    std::string bypassList = LPWSTRToString(options->pOptions[2].Value.pszValue);
    std::string autoConfigUrl = LPWSTRToString(options->pOptions[3].Value.pszValue);

    result.Set(Napi::String::New(env, "proxyServer"), Napi::String::New(env, proxyServer));
    result.Set(Napi::String::New(env, "bypassList"), Napi::String::New(env, bypassList));
    result.Set(Napi::String::New(env, "autoConfigUrl"), Napi::String::New(env, autoConfigUrl));

    // Free memory
    for (DWORD i = 1; i < options->dwOptionCount; ++i)
	{
		if (options->pOptions[i].Value.pszValue == NULL)
		{
			continue;
		}
		GlobalFree(options->pOptions[i].Value.pszValue);
		options->pOptions[i].Value.pszValue = NULL;
	}

    delete[] options->pOptions;
    options->pOptions = NULL;
    delete options;

    return result;
}

RET_ERRORS apply_connect(INTERNET_PER_CONN_OPTION_LIST* options, LPTSTR conn)
{
	options->pszConnection = conn;

	BOOL result = InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, options, sizeof(INTERNET_PER_CONN_OPTION_LIST));
	if (!result)
	{
		return SYSCALL_FAILED;
	}

	result = InternetSetOption(NULL, INTERNET_OPTION_PROXY_SETTINGS_CHANGED, NULL, 0);
	if (!result)
	{
		return SYSCALL_FAILED;
	}

	result = InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, 0);
	if (!result)
	{
		return SYSCALL_FAILED;
	}

	return RET_NO_ERROR;
}


RET_ERRORS apply(INTERNET_PER_CONN_OPTION_LIST* options)
{
	RET_ERRORS ret;
	DWORD dwCb = 0;
	DWORD dwRet;
	DWORD dwEntries = 0;
	LPRASENTRYNAME lpRasEntryName = NULL;

	// Set LAN
	if ((ret = apply_connect(options, NULL)) > RET_NO_ERROR)
		goto free_calloc;

	// Find connections and apply proxy settings
	dwRet = RasEnumEntries(NULL, NULL, lpRasEntryName, &dwCb, &dwEntries);

	if (dwRet == ERROR_BUFFER_TOO_SMALL)
	{
		lpRasEntryName = (LPRASENTRYNAME)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwCb);

		if (lpRasEntryName == NULL)
		{
			ret = NO_MEMORY;
			goto free_calloc;
		}

		for (DWORD i = 0; i < dwEntries; i++)
		{
			lpRasEntryName[i].dwSize = sizeof(RASENTRYNAME);
		}

		dwRet = RasEnumEntries(NULL, NULL, lpRasEntryName, &dwCb, &dwEntries);
	}

	if (dwRet != ERROR_SUCCESS)
	{
		ret = SYSCALL_FAILED;
		goto free_ras;
	}
	else
	{
		if (dwEntries > 0)
		{
			for (DWORD i = 0; i < dwEntries; i++)
			{
				if ((ret = apply_connect(options, lpRasEntryName[i].szEntryName)) > RET_NO_ERROR)
					goto free_ras;
			}
		}
	}

	ret = RET_NO_ERROR;

free_ras:
	HeapFree(GetProcessHeap(), 0, lpRasEntryName);
	lpRasEntryName = NULL;
	/* fall through */
free_calloc:
	free(options->pOptions);
	options->pOptions = NULL;

	return ret;
}

// 设置代理，传参格式：{
//     flags: 3,
//     autoConfigUrl: '',
//     proxyServer: 'host:port',
//     bypassList: 'bypass1;bypass2'
// }
// 提取传参中的代理配置，通过apply和apply_connect设置代理
Napi::Boolean SetProxy(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // 校验传参，每个参数都是必传的
    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    Napi::Object options = info[0].As<Napi::Object>();

    INTERNET_PER_CONN_OPTION_LIST* proxyOptions = new INTERNET_PER_CONN_OPTION_LIST;
    memset(proxyOptions, 0, sizeof(INTERNET_PER_CONN_OPTION_LIST));
    RET_ERRORS ret = initialize(proxyOptions, 4);
    if (ret != RET_NO_ERROR) {
        Napi::Error::New(env, "Failed to initialize proxy options").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    // Initialize proxy options
    int option_count = 0;
    if (options.Has("flags")) {
        proxyOptions->pOptions[option_count].dwOption = INTERNET_PER_CONN_FLAGS;
        proxyOptions->pOptions[option_count].Value.dwValue = options.Get("flags").As<Napi::Number>().Int32Value();
        option_count++;
    } else {
        Napi::Error::New(env, "Missing flags").ThrowAsJavaScriptException();
        // Free memory
        delete[] proxyOptions->pOptions;
        proxyOptions->pOptions = NULL;
        delete proxyOptions;
        return Napi::Boolean::New(env, false);
    }

    if (options.Has("autoConfigUrl")) {
        proxyOptions->pOptions[option_count].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
        std::string autoConfigUrl = options.Get("autoConfigUrl").As<Napi::String>().Utf8Value();
        proxyOptions->pOptions[option_count].Value.pszValue = StringToLPWSTR(autoConfigUrl);
        option_count++;
    } else {
        Napi::Error::New(env, "Missing flags").ThrowAsJavaScriptException();
        // Free memory
        delete[] proxyOptions->pOptions;
        proxyOptions->pOptions = NULL;
        delete proxyOptions;
        return Napi::Boolean::New(env, false);
    }

    if (options.Has("proxyServer")) {
        proxyOptions->pOptions[option_count].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
        std::string proxyServer = options.Get("proxyServer").As<Napi::String>().Utf8Value();
        proxyOptions->pOptions[option_count].Value.pszValue = StringToLPWSTR(proxyServer);
        option_count++;
    } else {
        Napi::Error::New(env, "Missing flags").ThrowAsJavaScriptException();
        // Free memory
        delete[] proxyOptions->pOptions;
        proxyOptions->pOptions = NULL;
        delete proxyOptions;
        return Napi::Boolean::New(env, false);
    }

    if (options.Has("bypassList")) {
        proxyOptions->pOptions[option_count].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
        std::string bypassList = options.Get("bypassList").As<Napi::String>().Utf8Value();
        proxyOptions->pOptions[option_count].Value.pszValue = StringToLPWSTR(bypassList);
        option_count++;
    } else {
        Napi::Error::New(env, "Missing flags").ThrowAsJavaScriptException();
        // Free memory
        delete[] proxyOptions->pOptions;
        proxyOptions->pOptions = NULL;
        delete proxyOptions;
        return Napi::Boolean::New(env, false);
    }

    ret = apply(proxyOptions);
    if (ret != RET_NO_ERROR) {
        Napi::Error::New(env, "Failed to apply proxy options, error code: " + std::to_string(ret)).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    return Napi::Boolean::New(env, true);
}


Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "queryWindowsProxy"), Napi::Function::New(env, QueryProxy));
    exports.Set(Napi::String::New(env, "setWindowsProxy"), Napi::Function::New(env, SetProxy));
    return exports;
}

NODE_API_MODULE(sysproxy, Init)
