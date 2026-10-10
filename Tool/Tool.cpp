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
		TCHAR pFileName[MAX_PATH] = { 0 };
		DWORD dwRet = GetModuleFileName(NULL, pFileName, MAX_PATH);

		HKEY hKey;
		LPCTSTR lpRun = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
		long lRet = RegOpenKeyEx(HKEY_CURRENT_USER, lpRun, 0, KEY_WRITE, &hKey);
		if (lRet != ERROR_SUCCESS)
			return false;

		if (Bool) {
			lRet = RegSetValueEx(hKey, Name, 0, REG_SZ, (const BYTE*)pFileName, (_tcslen(pFileName) + 1) * sizeof(TCHAR));
			if (lRet != ERROR_SUCCESS) {
				RegCloseKey(hKey);
				return false;
			}
		}
		else {
			lRet = RegDeleteValue(hKey, Name);
			if (lRet != ERROR_SUCCESS && lRet != ERROR_FILE_NOT_FOUND) {
				RegCloseKey(hKey);
				return false;
			}
		}
		RegCloseKey(hKey);
		return true;
	}
}