#include "Variable.h"

namespace Variable {
	extern void ReadFile(char* FilePath) {
		IniPath = FilePath;
		inih::INIReader* iniData = new inih::INIReader(IniPath);

		BaiduAppid = iniData->Get<std::string>("BaiduAPI", "Baidu_ID");
		BaiduSecret_key = iniData->Get<std::string>("BaiduAPI", "Baidu_Key");
		Baiduitems = iniData->GetVector<std::string>("BaiduAPI", "Baidu_items");
		BaiduitemsName = iniData->GetVector<std::string>("BaiduAPI", "Baidu_itemsName");

		YoudaoAppid = iniData->Get<std::string>("YoudaoAPI", "Youdao_ID");
		YoudaoSecret_key = iniData->Get<std::string>("YoudaoAPI", "Youdao_Key");
		Youdaoitems = iniData->GetVector<std::string>("YoudaoAPI", "Youdao_items");
		YoudaoitemsName = iniData->GetVector<std::string>("YoudaoAPI", "Youdao_itemsName");

		Translate = iniData->Get<int>("FT", "Translate");
		From = iniData->Get<int>("FT", "From");
		To = iniData->Get<int>("FT", "To");

		MakeUp = iniData->Get<int>("Key", "MakeUp");
		Screenshotkey = iniData->Get<std::string>("Key", "Screenshotkey");
		Choicekey = iniData->Get<std::string>("Key", "Choicekey");
		Replacekey = iniData->Get<std::string>("Key", "Replacekey");

		Model = iniData->Get<std::string>("Set", "TesseractModel");

		DisplayTime = iniData->Get<float>("Set", "DisplayTime") * 1000;
	}

	extern void SaveFile() {
		inih::INIReader* iniData = new inih::INIReader(IniPath);
		iniData->UpdateEntry("Window", "Width", BaiduAppid);//修改
		inih::INIWriter::write_Gai(IniPath, *iniData);//保存
	}

	char* IniPath;//储存文件路径

	int windows_Width;//屏幕宽度
	int windows_Heigth;//屏幕高度

	std::string eng = "";//原文
	std::string zhong = "";//翻译

	std::string BaiduAppid;//ID
	std::string BaiduSecret_key;//Key
	std::vector<std::string> Baiduitems;
	std::vector<std::string> BaiduitemsName;

	std::string YoudaoAppid;//ID
	std::string YoudaoSecret_key;//Key
	std::vector<std::string> Youdaoitems;
	std::vector<std::string> YoudaoitemsName;

	int Translate;//翻译引擎
	int From;//被翻译的语言
	int To;//翻译成什么语言

	//快捷键
	int MakeUp;//组合
	std::string Screenshotkey;//截图
	std::string Choicekey;//选择
	std::string Replacekey;//替换
	
	std::string Model;//模型
	float DisplayTime;//显示时间
}