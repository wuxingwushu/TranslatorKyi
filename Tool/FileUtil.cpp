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
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			const std::string Extension = entry.path().extension().string();
			if (Extension.size() < 2 || Extension[0] != '.') {
				continue;
			}
			if (!EqualsNoCase(Extension.substr(1), Suffix)) {
				continue;
			}
			const std::string Stem = entry.path().stem().string();
			strS->push_back(Stem);
			if (Stem == Name) {
				Index[0] = (int)strS->size() - 1;
			}
		}
	}

}