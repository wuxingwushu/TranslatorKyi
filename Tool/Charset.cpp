#include "Charset.h"
#include <windows.h>
#include <cstdio>

namespace TOOL {

	//宽字符 → 多字节（按系统 ANSI 代码页，中文系统上就是 GBK/936）。
	//不再用 setlocale + wcstombs_s：setlocale 改的是进程全局状态，AI 翻译在后台线程、
	//界面在主线程，两个线程同时进来会互相把 locale 改回去，偶发转出错乱的文本。
	std::string ws2s(const std::wstring& ws)
	{
		if (ws.empty())
		{
			return std::string();
		}
		const int Len = WideCharToMultiByte(CP_ACP, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
		if (Len <= 0)
		{
			return std::string();
		}
		std::string Result((size_t)Len, '\0');
		WideCharToMultiByte(CP_ACP, 0, ws.c_str(), (int)ws.size(), &Result[0], Len, nullptr, nullptr);
		return Result;
	}

	//多字节（系统 ANSI/GBK）→ 宽字符，与 ws2s 互为反向
	std::wstring s2ws(const std::string& s)
	{
		if (s.empty())
		{
			return std::wstring();
		}
		const int Len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
		if (Len <= 0)
		{
			return std::wstring();
		}
		std::wstring Result((size_t)Len, L'\0');
		MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &Result[0], Len);
		return Result;
	}

	//GBK（系统 ANSI）字节 → UTF-8。输入是剪贴板/老接口给的 ANSI 字节，
	//输出是可以直接交给 ImGui、curl、jsoncpp 的 UTF-8。
	std::string UnicodeToUtf8(const std::string& str) {
		if (str.empty())
		{
			return std::string();
		}
		const std::wstring wstr = s2ws(str);
		if (wstr.empty())
		{
			return std::string();
		}
		const int Len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
		if (Len <= 0)
		{
			return std::string();
		}
		std::string Result((size_t)Len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &Result[0], Len, nullptr, nullptr);
		return Result;
	}

	//UTF-8 → GBK（系统 ANSI）字节，给只认 ANSI 的地方用（例如 CF_TEXT 剪贴板）。
	//老实现是手写 UTF-8 解码：它把四字节序列硬塞进单个 wchar_t，超出 BMP 的字符
	//（emoji 等）会解错，这里换成 Win32 的转换，代理对也交给系统处理。
	std::string Utf8ToUnicode(const std::string& utf8_str) {
		if (utf8_str.empty())
		{
			return std::string();
		}
		const std::wstring Wide = Utf8ToWide(utf8_str);
		if (Wide.empty())
		{
			return std::string();
		}
		return ws2s(Wide);
	}

	//UTF-8 字节 → 宽字符(CP_UTF8)
	std::wstring Utf8ToWide(const std::string& s)
	{
		if (s.empty())
		{
			return std::wstring();
		}
		const int Len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
		if (Len <= 0)
		{
			return std::wstring();
		}
		std::wstring Result((size_t)Len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &Result[0], Len);
		return Result;
	}

	//宽字符 → UTF-8 字节(CP_UTF8)
	std::string WideToUtf8(const std::wstring& s)
	{
		if (s.empty())
		{
			return std::string();
		}
		const int Len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
		if (Len <= 0)
		{
			return std::string();
		}
		std::string Result((size_t)Len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &Result[0], Len, nullptr, nullptr);
		return Result;
	}

	//以 UTF-8 路径打开文件：Windows 上非 ASCII 路径必须走宽字符 API，
	//而项目内部（以及 llama.cpp 自己的 ggml_fopen）都按 CP_UTF8 解释路径，
	//所以这里也用 CP_UTF8 → 宽字符 → _wfopen，中文名的模型才能被正确判定/加载。
	FILE* OpenUtf8File(const std::string& path, const wchar_t* mode)
	{
		if (path.empty())
		{
			return nullptr;
		}
		const std::wstring Wide = Utf8ToWide(path);
		if (Wide.empty())
		{
			return nullptr;
		}
		return _wfopen(Wide.c_str(), mode);
	}

}