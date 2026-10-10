#pragma once
//全屏截图（BGRA 缓冲）。原先在 Tool.cpp 里。

namespace TOOL {

	char* screen(char* buf);//截图&保存（全屏）。返回的缓冲区由本函数自己持有，调用方不要 delete[]
}