#pragma once
//文本占位符替换：语言文件里的模板都是 "%s"/"%d" 形式，这里做统一替换。
//原先 TranslatePanel.cpp 的 AiTextWithNumber / AiTextWithString 与
//SettingsPanel.cpp 的 FillDeviceText 是同一套逻辑的三份实现，现收敛到这里。
#include <string>
#include <vector>

namespace TOOL {

	//把模板里的占位符按出现顺序依次替换成 Values 里的值。
	//识别 %s 与 %d 两种占位符（%d 的数值由调用方自己转成字符串）；
	//Values 用完之后剩下的占位符原样保留（与原来 FillDeviceText 的行为一致）。
	std::string ReplaceTokens(const std::string& Tpl, const std::vector<std::string>& Values);

	//单值版本：模板里只有一个占位符时用（等价于传一个元素的 Values）。
	std::string ReplaceToken(const std::string& Tpl, const std::string& Value);

}
