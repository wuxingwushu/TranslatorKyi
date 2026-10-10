#pragma once
#include "../GUI.h"
#include <string>
#include <vector>

// =====================================================================================
// ImGuiInterFace 各拆分文件之间共享的内部辅助（不对外暴露）
// -------------------------------------------------------------------------------------
// 这些函数原本是 Interface.cpp 里的文件内 static；拆成多个 .cpp 后需要跨 TU 共享，
// 因此集中声明在这里，定义分别放在 Font.cpp / ModelList.cpp。
// 它们依赖的是 ImGuiInterFace 之外的自由函数，不涉及类私有成员。
// =====================================================================================
namespace GAME {

	// 字体：文件可读性、./TTF 默认字体、加载（加载失败自动回退）
	bool FontFileReadable(const std::string& FilePath);
	std::string FirstFontInTTFFolder();
	std::string DefaultTypefacePath();
	ImFont* LoadTypeface(ImGuiIO& io, const std::string& WantedPath, float Size, const ImFontConfig* FontCfg, const ImWchar* Ranges);

	// AI 模型列表：路径→文件名、按文件名定位下标
	int FindAiModelIndex(const std::vector<std::string>& List, const std::string& CurrentPath);
	std::string CurrentAiModelName(const std::vector<std::string>& List, int Index, const std::string& CurrentPath);

}