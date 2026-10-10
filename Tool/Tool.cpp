#include "Tool.h"
#include <tchar.h>

//Tool.cpp 原先混了 6 类职责（日志/转换/文件/剪贴板/截图/计时），已按功能拆出：
//   Log.cpp       —— SpdLogInit / TOOL::logger
//   Clipboard.cpp —— ClipboardTochar / CopyToClipboard / CtrlAndC /CtrlAndV
//   Screen.cpp    —— screen（全屏截图）
//   Profile.cpp   —— FPS / 耗时检测
//   Convert.h     —— Converter<T> / BoolConverter / toString<T>（模板必须放头文件）
//   Charset.h、FileUtil.h —— 字符集转换、文件名/目录扫描
//这里只剩「写注册表」这一件与上面都无关的系统操作。

namespace TOOL {

	bool SetModifyRegedit(const char* Name, bool Bool) {
		//写入的是「开机自启动」项，里面保存的是本程序的完整路径。
		//原来走的是 ANSI 版 API（工程没有定义 UNICODE，TCHAR 就是 char）：
		//程序放在含中文的路径下时，写进注册表的就是按 936 编过的字节，系统启动时找不到这个文件。
		//这里统一改成宽字符版 API（路径用 GetModuleFileNameW 拿，注册表项/值名把 UTF-8 转宽字符）。
		wchar_t pFileName[MAX_PATH] = { 0 };
		const DWORD dwRet = GetModuleFileNameW(NULL, pFileName, MAX_PATH);
		if (dwRet == 0 || dwRet >= MAX_PATH) {
			if (logger) { logger->error("SetModifyRegedit(): 取程序路径失败，GetLastError={}", GetLastError()); }
			return false;
		}

		const wchar_t* lpRun = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
		HKEY hKey = NULL;
		LSTATUS lRet = RegOpenKeyExW(HKEY_CURRENT_USER, lpRun, 0, KEY_WRITE, &hKey);
		if (lRet != ERROR_SUCCESS) {
			if (logger) { logger->error("SetModifyRegedit(): 打开注册表项失败，错误码={}", lRet); }
			return false;
		}

		//Name 在工程内部是 UTF-8（调用方传的是字面量 "TranslatorKyi"，转换后一样）
		const std::wstring NameW = TOOL::Utf8ToWide(Name == nullptr ? "" : Name);

		if (Bool) {
			lRet = RegSetValueExW(hKey, NameW.c_str(), 0, REG_SZ, (const BYTE*)pFileName,
				(DWORD)((std::wcslen(pFileName) + 1) * sizeof(wchar_t)));
			if (lRet != ERROR_SUCCESS) {
				if (logger) { logger->error("SetModifyRegedit(): 写入开机启动项失败，错误码={}", lRet); }
				RegCloseKey(hKey);
				return false;
			}
		}
		else {
			lRet = RegDeleteValueW(hKey, NameW.c_str());
			//项本来就不存在（用户没开过开机启动）也算成功
			if (lRet != ERROR_SUCCESS && lRet != ERROR_FILE_NOT_FOUND) {
				if (logger) { logger->error("SetModifyRegedit(): 删除开机启动项失败，错误码={}", lRet); }
				RegCloseKey(hKey);
				return false;
			}
		}
		RegCloseKey(hKey);
		return true;
	}
}