#pragma once
//字符集转换的单一入口。
//项目内部统一以 UTF-8 承载文本，但 Windows 上还有两类接口只认别的编码：
//  1) 宽字符 API（_wfopen、WriteConsoleW 等）——用 Utf8ToWide / WideToUtf8；
//  2) 老式 ANSI 接口与 CF_TEXT 剪贴板——用 UnicodeToUtf8 / Utf8ToUnicode（走系统代码页，中文系统是 GBK）。
//原先这些「探测长度 → 分配 → 再转换」的样板散落在 main.cpp / Tool.cpp / LlamaTranslate.cpp /
//instance.cpp 四处，这里收敛成唯一一份实现。
#include <string>
#include <cstdio>

namespace TOOL {

	//宽字符 → 多字节：按系统 ANSI 代码页(中文系统是 GBK)，给 CF_TEXT 剪贴板/老式 ANSI 接口用
	std::string ws2s(const std::wstring& ws);

	//多字节(系统 ANSI/GBK) → 宽字符，与 ws2s 互为反向
	std::wstring s2ws(const std::string& s);

	//GBK(系统 ANSI) 字节 → UTF-8：剪贴板/老接口进来的文本用它转成内部统一的 UTF-8
	std::string UnicodeToUtf8(const std::string& str);

	//UTF-8 → GBK(系统 ANSI) 字节：给只认 ANSI 的接口用（例如 CF_TEXT）
	std::string Utf8ToUnicode(const std::string& utf8_str);

	//UTF-8 字节 → 宽字符(CP_UTF8)：交给 Windows 宽字符 API 用
	std::wstring Utf8ToWide(const std::string& s);

	//宽字符 → UTF-8 字节(CP_UTF8)，与 Utf8ToWide 互为反向
	std::string WideToUtf8(const std::wstring& s);

	//以 UTF-8 路径打开文件：Windows 上非 ASCII 路径必须走宽字符 API 才能正确打开
	FILE* OpenUtf8File(const std::string& path, const wchar_t* mode);

}