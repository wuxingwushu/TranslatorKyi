#pragma once
//日志模块：spdlog 全局 logger 与初始化。原先在 Tool.cpp 里，和转换/文件/剪贴板/截图/计时混在一起。
#include "spdlog/spdlog.h"

namespace TOOL {

	extern spdlog::logger* logger;

	void SpdLogInit();

	//退出路径专用的「路标」日志：直接写文件，不走 spdlog（实现见 Log.cpp）。
	//排查「退出时崩溃」时，日志里最后一条 [STEP] 就是程序崩之前走到的位置。
	void LogStep(const char* Tag);
}