#pragma once
//Tool 层的汇总入口：include 各功能子模块 + 提供 CopyToBuffer。
//具体实现按职责拆在 Log/Clipboard/Screen/Profile/*.cpp，字符集与文件工具在 Charset/FileUtil。
#include <time.h>
#include <string>
#include <cstring>//memcpy：下面 CopyToBuffer 用
#include <windows.h>
#include "../Variable.h"
#include <filesystem>
#include "Convert.h"//Converter<T>/BoolConverter/toString<T>
#include "Charset.h"//ws2s/s2ws/UnicodeToUtf8/Utf8ToUnicode 等
#include "FileUtil.h"//BaseName/FileStem/EqualsNoCase/FilePath
#include "Log.h"//TOOL::logger / SpdLogInit
#include "Clipboard.h"//剪贴板 / CtrlAndC/CtrlAndV
#include "Screen.h"//全屏截图
#include "Profile.h"//FPS / 耗时检测

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace TOOL {

	bool SetModifyRegedit(const char* Name, bool Bool);

	//把 src 安全地拷进定长缓冲区：最多写 destBytes-1 字节，并且一定会补 '\0'。
	//项目里原本到处是 memcpy(dest, str.c_str(), str.size()) 这种写法，没有任何长度上限，
	//剪贴板、翻译结果、配置文件里的长文本一进来就会把栈/堆上的定长数组写爆。
	inline void CopyToBuffer(char* dest, size_t destBytes, const std::string& src) {
		if (dest == nullptr || destBytes == 0) { return; }
		const size_t CopyBytes = (src.size() < destBytes - 1) ? src.size() : (destBytes - 1);
		if (CopyBytes > 0) { memcpy(dest, src.data(), CopyBytes); }
		dest[CopyBytes] = '\0';
	}
}