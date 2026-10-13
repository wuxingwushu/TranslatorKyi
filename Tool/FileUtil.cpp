#include "FileUtil.h"
#include <cctype>
#include <filesystem>

namespace TOOL {

	std::string BaseName(const std::string& Path) {
		const size_t Slash = Path.find_last_of("/\\");
		return (Slash == std::string::npos) ? Path : Path.substr(Slash + 1);
	}

	std::string FileStem(const std::string& Path) {
		const std::string Name = BaseName(Path);
		const size_t Dot = Name.find_last_of('.');
		return (Dot == std::string::npos) ? Name : Name.substr(0, Dot);
	}

	bool EqualsNoCase(const std::string& A, const std::string& B) {
		if (A.size() != B.size()) {
			return false;
		}
		for (size_t i = 0; i < A.size(); i++) {
			if (std::tolower((unsigned char)A[i]) != std::tolower((unsigned char)B[i])) {
				return false;
			}
		}
		return true;
	}

	void FilePath(const char* path, std::vector<std::string>* strS, const char* Suffix, const char* Name, int* Index) {
		//目录不存在/没权限时直接返回空列表：原来不传 ec 会抛 filesystem_error，
		//在设置界面的初始化路径上没人接这个异常，会一路弹到 main 的 catch。
		std::error_code DirEc;
		std::filesystem::directory_iterator It(path, DirEc);
		if (DirEc) {
			return;
		}
		for (const auto& entry : It) {
			//一律用 u8string() 取名字，不能用 string()：
			//Windows 下 path::string() 要把宽路径转成系统 ANSI 代码页，目录里只要有一个文件名
			//含有该代码页表示不了的字符（第三方软件装的中文字体、emoji 等），它就抛
			//"No mapping for the Unicode character exists in the target multi-byte code page."
			//（Windows 错误 1113），而 u8string() 只做 UTF-8 编码，任何字符都不会失败。
			//项目内部（ImGui 的下拉框、AddFontFromFileTTF、llama.cpp）统一按 UTF-8 认字符串。
			const std::string Extension = entry.path().extension().u8string();
			if (Extension.size() < 2 || Extension[0] != '.') {
				continue;
			}
			if (!EqualsNoCase(Extension.substr(1), Suffix)) {
				continue;
			}
			const std::string Stem = entry.path().stem().u8string();
			strS->push_back(Stem);
			if (Stem == Name) {
				Index[0] = (int)strS->size() - 1;
			}
		}
	}

}