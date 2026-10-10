#include "../Interface.h"
#include "InterfaceInternal.h"

// =====================================================================================
// AI 模型列表
// -------------------------------------------------------------------------------------
//设置界面「选择模型」的来源：Translate::AiModelFiles() 扫描程序目录（含逐级向上）里的
//Modes 文件夹，把 .gguf 都列出来；这里只做「路径 → 文件名」和「按文件名定位下标」两件小事。
//取文件名/大小写比较已收敛到 TOOL::BaseName / TOOL::EqualsNoCase（Tool/FileUtil.h）。
// =====================================================================================
namespace GAME {

	//当前实际会用的模型（设置里没填就是默认的那个）在列表里是第几个；找不到返回 -1
	int FindAiModelIndex(const std::vector<std::string>& List, const std::string& CurrentPath) {
		const std::string Wanted = TOOL::BaseName(CurrentPath.empty() ? Translate::DefaultAiModelPath() : CurrentPath);
		for (size_t i = 0; i < List.size(); i++) {
			if (TOOL::EqualsNoCase(TOOL::BaseName(List[i]), Wanted)) {
				return (int)i;
			}
		}
		return -1;
	}

	//当前实际会用的模型文件名（下拉框上显示的当前项）
	std::string CurrentAiModelName(const std::vector<std::string>& List, int Index, const std::string& CurrentPath) {
		if (Index >= 0 && Index < (int)List.size()) {
			return TOOL::BaseName(List[Index]);
		}
		return TOOL::BaseName(CurrentPath.empty() ? Translate::DefaultAiModelPath() : CurrentPath);
	}

}