#pragma once
//剪贴板存取与 Ctrl+C / Ctrl+V 模拟按键。原先在 Tool.cpp 里。
#include <string>

namespace TOOL {

	std::string ClipboardTochar(); //获得剪贴板的内容（ANSI/GBK 字节）

	void CopyToClipboard(std::string str); //将内容复制到剪贴板

	void CtrlAndC();
	void CtrlAndV();
}