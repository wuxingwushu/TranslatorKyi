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

		DisplayTime = iniData->Get<int>("Set", "DisplayTime");
		FontSize = iniData->Get<float>("Set", "FontSize");
		ReplaceLanguage = iniData->Get<int>("Set", "ReplaceLanguage");
		FontBool = iniData->Get<int>("Set", "FontBool") ? true : false;
		FontFilePath = iniData->Get<std::string>("Set", "FontFilePath");
		Startup = iniData->Get<int>("Set", "Startup") ? true : false;

		delete iniData;
	}

	extern void SaveFile() {
		inih::INIReader* iniData = new inih::INIReader(IniPath);
		//保存 百度ID Key
		iniData->UpdateEntry("BaiduAPI", "Baidu_ID", BaiduAppid);
		iniData->UpdateEntry("BaiduAPI", "Baidu_Key", BaiduSecret_key);
		//保存 有道 ID Key
		iniData->UpdateEntry("YoudaoAPI", "Youdao_ID", BaiduAppid);
		iniData->UpdateEntry("YoudaoAPI", "Youdao_Key", BaiduSecret_key);
		//保存 翻译配置
		iniData->UpdateEntry("FT", "Translate", Translate);
		iniData->UpdateEntry("FT", "From", From);
		iniData->UpdateEntry("FT", "To", To);
		//保存 快捷键位
		iniData->UpdateEntry("Key", "MakeUp", MakeUp);
		iniData->UpdateEntry("Key", "Screenshotkey", Screenshotkey);
		iniData->UpdateEntry("Key", "Choicekey", Choicekey);
		iniData->UpdateEntry("Key", "Replacekey", Replacekey);
		//保存 选择模型
		iniData->UpdateEntry("Set", "TesseractModel", Model);
		//保存 显示时间
		iniData->UpdateEntry("Set", "DisplayTime", DisplayTime);
		//保存 字体大小
		iniData->UpdateEntry("Set", "FontSize", FontSize);
		//保存 替换语言
		iniData->UpdateEntry("Set", "ReplaceLanguage", ReplaceLanguage);
		//保存 是否开启自定义字体
		iniData->UpdateEntry("Set", "FontBool", FontBool ? "1" : "0");
		//保存 字体文件路径
		iniData->UpdateEntry("Set", "FontFilePath", FontFilePath);
		//保存 开机启动
		iniData->UpdateEntry("Set", "Startup", Startup ? "1" : "0");

		inih::INIWriter::write_Gai(IniPath, *iniData);//保存

		delete iniData;
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
	int DisplayTime;//显示时间
	float FontSize;//字体大小
	int ReplaceLanguage;//替换为什么语言
	bool FontBool;//是否开启自定义字体
	std::string FontFilePath;//字体文件路径
	bool Startup;//开机启动
}