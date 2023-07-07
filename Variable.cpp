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
		FontBool = iniData->Get<bool>("Set", "FontBool");
		FontFilePath = iniData->Get<std::string>("Set", "FontFilePath");
		Startup = iniData->Get<bool>("Set", "Startup");
		Language = iniData->Get<std::string>("Set", "Language");

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
		iniData->UpdateEntry("Set", "FontBool", FontBool);
		//保存 字体文件路径
		iniData->UpdateEntry("Set", "FontFilePath", FontFilePath);
		//保存 开机启动
		iniData->UpdateEntry("Set", "Startup", Startup);
		//保存 语言
		iniData->UpdateEntry("Set", "Language", Language);

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
	std::string Language;//语言
}


namespace Language {

	void ReadFile(std::string FilePath) {
		inih::INIReader iniData = inih::INIReader("./Language/" + FilePath + ".ini");

		TranslationKey = iniData.Get<std::string>("Translate","TranslationKey");
		From = iniData.Get<std::string>("Translate", "From");
		To = iniData.Get<std::string>("Translate", "To");

		AccountKey = iniData.Get<std::string>("Set", "AccountKey");
		BaiduID = iniData.Get<std::string>("Set", "BaiduID");
		BaiduKey = iniData.Get<std::string>("Set", "BaiduKey");
		YoudaoID = iniData.Get<std::string>("Set", "YoudaoID");
		YoudaoKey = iniData.Get<std::string>("Set", "YoudaoKey");
		ShortcutKeys = iniData.Get<std::string>("Set", "ShortcutKeys");
		KeyCombination = iniData.Get<std::string>("Set", "KeyCombination");
		ScreenshotTranslation = iniData.Get<std::string>("Set", "ScreenshotTranslation");
		SelectTranslation = iniData.Get<std::string>("Set", "SelectTranslation");
		ReplaceTranslation = iniData.Get<std::string>("Set", "ReplaceTranslation");
		Startup = iniData.Get<std::string>("Set", "Startup");
		ResidenceTime = iniData.Get<std::string>("Set", "ResidenceTime");
		FontSize = iniData.Get<std::string>("Set", "FontSize");
		TesseractModel = iniData.Get<std::string>("Set", "TesseractModel");
		NotTesseractModelText = iniData.Get<std::string>("Set", "NotTesseractModelText");
		UseTTF_Typeface = iniData.Get<std::string>("Set", "UseTTF_Typeface");
		TTF_Folder = iniData.Get<std::string>("Set", "TTF_Folder");
		TessDataFolder = iniData.Get<std::string>("Set", "TessDataFolder");
		TTF_Typeface = iniData.Get<std::string>("Set", "TTF_Typeface");
		NotTTF_TypefaceText = iniData.Get<std::string>("Set", "NotTTF_TypefaceText");
		ReplaceLanguage = iniData.Get<std::string>("Set", "ReplaceLanguage");
		Save = iniData.Get<std::string>("Set", "Save");
		Close = iniData.Get<std::string>("Set", "Close");
		Language = iniData.Get<std::string>("Set", "Language");

		Set = iniData.Get<std::string>("tray", "Set");
		Exit = iniData.Get<std::string>("tray", "Exit");
		strncpy = iniData.Get<std::string>("tray", "strncpy");

		FindWin = iniData.Get<std::string>("error", "FindWin");
	}

	//翻译界面
	std::string TranslationKey;			//翻译键
	std::string From;					//From
	std::string To;						//To

	//设置界面
	std::string AccountKey;				//翻译密钥
	std::string BaiduID;				//百度ID
	std::string BaiduKey;				//百度Key
	std::string YoudaoID;				//有道ID
	std::string YoudaoKey;				//有道Key
	std::string ShortcutKeys;			//快捷键
	std::string KeyCombination;			//组合键
	std::string ScreenshotTranslation;	//截图翻译
	std::string SelectTranslation;		//选择翻译
	std::string ReplaceTranslation;		//替换翻译
	std::string Startup;				//开机启动
	std::string ResidenceTime;			//滞留时间（ms）
	std::string FontSize;				//字体大小
	std::string TesseractModel;			//Tesseract模型
	std::string NotTesseractModelText;	//你没有Tesseract模型，模型放在当前程序位置的TessData
	std::string UseTTF_Typeface;		//使用TTF字体
	std::string TTF_Folder;				//TTF文件夹
	std::string TessDataFolder;			//TessData文件夹
	std::string TTF_Typeface;			//TTF字体
	std::string NotTTF_TypefaceText;	//你没有TTF字体，字体放在当前程序位置的TTF
	std::string ReplaceLanguage;		//替换语言
	std::string Save;					//保存
	std::string Close;					//关闭
	std::string Language;				//语言

	//系统托盘
	std::string Set;					//设置
	std::string Exit;					//退出
	std::string strncpy;					//人家叫翻译姬！

	//Error
	std::string FindWin;				//已经启动啦！
}