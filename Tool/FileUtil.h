#pragma once
//路径 / 文件名 的通用工具，以及目录扫描。
//原先「取文件名」在 Tool.cpp(StrName)、WebDav.cpp(GetStrName)、Interface.cpp(PathFileName)、
//LlamaTranslate.cpp(FileNameOf) 各实现了一遍，「大小写不敏感比较」也重复了三份，这里收敛到一处。
#include <string>
#include <vector>

namespace TOOL {

	//取路径中的文件名（含扩展名），同时兼容 '/' 与 '\'。"./Modes/foo.gguf" → "foo.gguf"
	std::string BaseName(const std::string& Path);

	//取文件名并去掉最后一个扩展名，同时兼容 '/' 与 '\'。"./Modes/foo.gguf" → "foo"
	std::string FileStem(const std::string& Path);

	//大小写不敏感(ASCII)的字符串比较：Windows 的文件名不区分大小写
	bool EqualsNoCase(const std::string& A, const std::string& B);

	//扫描 path 目录，把扩展名等于 Suffix 的文件（去扩展名后）依次加入 strS；
	//其中名字等于 Name 的那一项下标写入 Index[0]。
	void FilePath(const char* path, std::vector<std::string>* strS, const char* Suffix, const char* Name, int* Index);

}