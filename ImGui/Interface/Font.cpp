#include "../Interface.h"
#include "InterfaceInternal.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

// =====================================================================================
// 字体加载
// -------------------------------------------------------------------------------------
//Font.h（内嵌的 Test.ttf 字模）已经删除，字体一律从 TTF 文件里读：
//  1. 设置里选中的字体文件（Variable::FontFilePath / Variable::HitokotoFont）
//  2. 程序目录 ./TTF 下的默认字体（约定 SmileySans-Oblique.ttf，没有就取目录里第一个 ttf）
//  3. ImGui 自带字模（只有 ASCII，纯兜底，保证界面不会一个字体都没有）
//注意：ImGui 的 AddFontFromFileTTF 读不到文件时会直接 IM_ASSERT 失败（Debug 下一运行就弹框），
//所以这里必须先自己确认文件在不在，不能把路径直接丢给 ImGui。
// =====================================================================================
namespace GAME {

	static const char* const DefaultFontFileName = "./TTF/SmileySans-Oblique.ttf";

	bool FontFileReadable(const std::string& FilePath) {
		if (FilePath.empty()) {
			return false;
		}
		std::error_code ec;
		return std::filesystem::is_regular_file(FilePath, ec) && !ec;
	}

	//./TTF 目录里按文件名排序的第一个 ttf（用户换字体文件后不用改代码）
	std::string FirstFontInTTFFolder() {
		std::error_code ec;
		std::filesystem::directory_iterator Iterator("./TTF", ec);
		if (ec) {
			return std::string();
		}
		std::vector<std::string> Files;
		for (const auto& Entry : Iterator) {
			std::error_code EntryEc;
			if (!Entry.is_regular_file(EntryEc)) {
				continue;
			}
			std::string Extension = Entry.path().extension().string();
			for (size_t i = 0; i < Extension.size(); i++) {
				Extension[i] = (char)std::tolower((unsigned char)Extension[i]);
			}
			if (Extension == ".ttf") {
				Files.push_back(Entry.path().string());
			}
		}
		if (Files.empty()) {
			return std::string();
		}
		std::sort(Files.begin(), Files.end());
		return Files.front();
	}

	//当前实际会使用的默认字模路径（设置界面里显示给用户看）
	std::string DefaultTypefacePath() {
		if (FontFileReadable(DefaultFontFileName)) {
			return DefaultFontFileName;
		}
		return FirstFontInTTFFolder();
	}

	//加载字体：文件不存在或读不出来就自动退回默认字模，永远返回一个可用字体（ImGui::PushFont 不接受空指针）
	ImFont* LoadTypeface(ImGuiIO& io, const std::string& WantedPath, float Size, const ImFontConfig* FontCfg, const ImWchar* Ranges) {
		if (FontFileReadable(WantedPath)) {
			if (ImFont* Font = io.Fonts->AddFontFromFileTTF(WantedPath.c_str(), Size, FontCfg, Ranges)) {
				return Font;
			}
		}
		else if (!WantedPath.empty()) {
			TOOL::logger->warn("Typeface file not usable, fallback to default typeface: " + WantedPath);
		}
		std::string DefaultPath = DefaultTypefacePath();
		if (FontFileReadable(DefaultPath)) {
			if (ImFont* Font = io.Fonts->AddFontFromFileTTF(DefaultPath.c_str(), Size, FontCfg, Ranges)) {
				return Font;
			}
		}
		TOOL::logger->warn("No usable TTF typeface found, using ImGui built-in typeface");
		return io.Fonts->AddFontDefault();
	}

}