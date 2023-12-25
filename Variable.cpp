#include "Variable.h"

namespace Variable {
	unsigned int WrapSize = 12;

	extern void ReadFile(char* FilePath) {
		IniPath = FilePath;
		inih::INIReader* iniData = new inih::INIReader(IniPath);

		PopUpNotificationBool = iniData->Get<bool>("Hitokoto", "PopUpNotificationBool");
		HitokotoTimeInterval = iniData->Get<int>("Hitokoto", "HitokotoTimeInterval");
		HitokotoDisplayDuration = iniData->Get<int>("Hitokoto", "HitokotoDisplayDuration");
		HitokotoPosX = iniData->Get<float>("Hitokoto", "HitokotoPosX");
		HitokotoPosY = iniData->Get<float>("Hitokoto", "HitokotoPosY");
		HitokotoFontSize = iniData->Get<float>("Hitokoto", "HitokotoFontSize");
		HitokotoTTFBool = iniData->Get<bool>("Hitokoto", "HitokotoTTFBool");
		HitokotoFontBool = iniData->Get<bool>("Hitokoto", "HitokotoFontBool");
		HitokotoFont = iniData->Get<std::string>("Hitokoto", "HitokotoFont");

		WebDav_url = iniData->Get<std::string>("WebDav", "url");
		WebDav_username = iniData->Get<std::string>("WebDav", "username");
		WebDav_password = iniData->Get<std::string>("WebDav", "password");
		WebDav_WebFile = iniData->Get<std::string>("WebDav", "WebFile");
		OpcodeBool = iniData->Get<bool>("WebDav", "OpcodeBool");
		LanguageBool = iniData->Get<bool>("WebDav", "LanguageBool");
		TessDataBool = iniData->Get<bool>("WebDav", "TessDataBool");
		TTFBool = iniData->Get<bool>("WebDav", "TTFBool");

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
		std::vector<unsigned int> LScreenshotColor = iniData->GetVector<unsigned int>("Set", "ScreenshotColor");
		for (size_t i = 0; i < LScreenshotColor.size(); i++)
		{
			ScreenshotColor[i] = LScreenshotColor[i];
		}
		Script = iniData->Get<std::string>("Set", "Script");
		ScriptBool = iniData->Get<bool>("Set", "ScriptBool");

		delete iniData;
	}

	extern void SaveFile() {
		inih::INIReader* iniData = new inih::INIReader(IniPath);
		//保存 一言
		iniData->UpdateEntry("Hitokoto", "PopUpNotificationBool", PopUpNotificationBool);
		iniData->UpdateEntry("Hitokoto", "HitokotoTimeInterval", HitokotoTimeInterval);
		iniData->UpdateEntry("Hitokoto", "HitokotoDisplayDuration", HitokotoDisplayDuration);
		iniData->UpdateEntry("Hitokoto", "HitokotoPosX", HitokotoPosX);
		iniData->UpdateEntry("Hitokoto", "HitokotoPosY", HitokotoPosY);
		iniData->UpdateEntry("Hitokoto", "HitokotoFontSize", HitokotoFontSize);
		iniData->UpdateEntry("Hitokoto", "HitokotoTTFBool", HitokotoTTFBool);
		iniData->UpdateEntry("Hitokoto", "HitokotoFontBool", HitokotoFontBool);
		iniData->UpdateEntry("Hitokoto", "HitokotoFont", HitokotoFont);
		//保存 WebDav
		iniData->UpdateEntry("WebDav", "url", WebDav_url);
		iniData->UpdateEntry("WebDav", "username", WebDav_username);
		iniData->UpdateEntry("WebDav", "password", WebDav_password);
		iniData->UpdateEntry("WebDav", "WebFile", WebDav_WebFile);
		iniData->UpdateEntry("WebDav", "OpcodeBool", OpcodeBool);
		iniData->UpdateEntry("WebDav", "LanguageBool", LanguageBool);
		iniData->UpdateEntry("WebDav", "TessDataBool", TessDataBool);
		iniData->UpdateEntry("WebDav", "TTFBool", TTFBool);
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
		//保存 截图颜色
		iniData->UpdateEntry("Set", "ScreenshotColor", VectorToString<unsigned char>(ScreenshotColor, 4));
		//保存 脚本设置
		iniData->UpdateEntry("Set", "Script", Script);
		iniData->UpdateEntry("Set", "ScriptBool", ScriptBool);

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

	//一言
	bool PopUpNotificationBool;
	int HitokotoTimeInterval;
	int HitokotoDisplayDuration;
	float HitokotoPosX;
	float HitokotoPosY;
	float HitokotoFontSize;
	bool HitokotoFontBool;
	bool HitokotoTTFBool;
	std::string HitokotoFont;

	//WebDav
	std::string WebDav_url;
	std::string WebDav_username;
	std::string WebDav_password;
	std::string WebDav_WebFile;
	bool OpcodeBool;
	bool LanguageBool;
	bool TessDataBool;
	bool TTFBool;

	//快捷键
	int MakeUp;//组合
	std::string Screenshotkey;		//截图
	std::string Choicekey;			//选择
	std::string Replacekey;			//替换
	
	std::string Model;					//模型
	int DisplayTime;					//显示时间
	float FontSize;						//字体大小
	int ReplaceLanguage;				//替换为什么语言
	bool FontBool;						//是否开启自定义字体
	std::string FontFilePath;			//字体文件路径
	bool Startup;						//开机启动
	std::string Language;				//语言
	unsigned char ScreenshotColor[4];	//截图颜色
	std::string Script;					//脚本
	bool ScriptBool;					//是否开启脚本
}


namespace Language {

	void ReadFile(std::string FilePath) {
		inih::INIReader iniData = inih::INIReader("./Language/" + FilePath + ".ini");

		TranslationKey = iniData.Get<std::string>("Translate","TranslationKey_");
		From = iniData.Get<std::string>("Translate", "From_");
		To = iniData.Get<std::string>("Translate", "To_");

		PopUpNotification = iniData.Get<std::string>("Set", "HitokotoPopUpNotification_");
		HitokotoTimeInterval = iniData.Get<std::string>("Set", "HitokotoTimeInterval_");
		HitokotoDisplayDuration = iniData.Get<std::string>("Set", "HitokotoDisplayDuration_");
		IndependentTypeface = iniData.Get<std::string>("Set", "IndependentTypeface_");
		InternalFontPattern = iniData.Get<std::string>("Set", "InternalFontPattern_");
		PositionX = iniData.Get<std::string>("Set", "PositionX_");
		PositionY = iniData.Get<std::string>("Set", "PositionY_");
		HitokotoFontSize = iniData.Get<std::string>("Set", "HitokotoFontSize_");
		jianguoyunWebDav = iniData.Get<std::string>("Set", "jianguoyunWebDav_");
		ServerAddress = iniData.Get<std::string>("Set", "ServerAddress_");
		Account = iniData.Get<std::string>("Set", "Account_");
		SecretKey = iniData.Get<std::string>("Set", "SecretKey_");
		ApplyName = iniData.Get<std::string>("Set", "ApplyName_");
		BackupsFolder = iniData.Get<std::string>("Set", "BackupsFolder_");
		Backups = iniData.Get<std::string>("Set", "Backups_");
		Recovery = iniData.Get<std::string>("Set", "Recovery_");
		Return = iniData.Get<std::string>("Set", "Return_");
		RecoveryList = iniData.Get<std::string>("Set", "RecoveryList_");
		Restoration = iniData.Get<std::string>("Set", "Restoration_");
		Delete = iniData.Get<std::string>("Set", "Delete_");
		Cancel = iniData.Get<std::string>("Set", "Cancel_");
		Confirm = iniData.Get<std::string>("Set", "Confirm_");
		AccountKey = iniData.Get<std::string>("Set", "AccountKey_");
		BaiduID = iniData.Get<std::string>("Set", "BaiduID_");
		BaiduKey = iniData.Get<std::string>("Set", "BaiduKey_");
		YoudaoID = iniData.Get<std::string>("Set", "YoudaoID_");
		YoudaoKey = iniData.Get<std::string>("Set", "YoudaoKey_");
		ShortcutKeys = iniData.Get<std::string>("Set", "ShortcutKeys_");
		KeyCombination = iniData.Get<std::string>("Set", "KeyCombination_");
		ScreenshotTranslation = iniData.Get<std::string>("Set", "ScreenshotTranslation_");
		SelectTranslation = iniData.Get<std::string>("Set", "SelectTranslation_");
		ReplaceTranslation = iniData.Get<std::string>("Set", "ReplaceTranslation_");
		Startup = iniData.Get<std::string>("Set", "Startup_");
		ResidenceTime = iniData.Get<std::string>("Set", "ResidenceTime_");
		FontSize = iniData.Get<std::string>("Set", "FontSize_");
		TesseractModel = iniData.Get<std::string>("Set", "TesseractModel_");
		NotTesseractModelText = iniData.Get<std::string>("Set", "NotTesseractModelText_");
		UseTTF_Typeface = iniData.Get<std::string>("Set", "UseTTF_Typeface_");
		TTF_Folder = iniData.Get<std::string>("Set", "TTF_Folder_");
		TessDataFolder = iniData.Get<std::string>("Set", "TessDataFolder_");
		TTF_Typeface = iniData.Get<std::string>("Set", "TTF_Typeface_");
		NotTTF_TypefaceText = iniData.Get<std::string>("Set", "NotTTF_TypefaceText_");
		ReplaceLanguage = iniData.Get<std::string>("Set", "ReplaceLanguage_");
		Save = iniData.Get<std::string>("Set", "Save_");
		Close = iniData.Get<std::string>("Set", "Close_");
		Language = iniData.Get<std::string>("Set", "Language_");
		ScreenshotColor = iniData.Get<std::string>("Set", "ScreenshotColor_");
		Script = iniData.Get<std::string>("Set", "Script_");
		NotScript = iniData.Get<std::string>("Set", "NotScript_");

		Set = iniData.Get<std::string>("tray", "Set_");
		ShutUp = iniData.Get<std::string>("tray", "ShutUp_");
		Speak = iniData.Get<std::string>("tray", "Speak_");
		Exit = iniData.Get<std::string>("tray", "Exit_");
		strncpy = iniData.Get<std::string>("tray", "strncpy_");
	}

	//翻译界面
	std::string TranslationKey;			//翻译键
	std::string From;					//From
	std::string To;						//To

	//设置界面
	std::string PopUpNotification;		//一言弹窗
	std::string HitokotoTimeInterval;	//弹窗时间间隔
	std::string HitokotoDisplayDuration;//弹窗显示时长
	std::string IndependentTypeface;	//独立字模
	std::string InternalFontPattern;	//内部字模
	std::string PositionX;				//位置X
	std::string PositionY;				//位置Y
	std::string HitokotoFontSize;		//一言字体大小
	std::string jianguoyunWebDav;		//坚果云WebDav
	std::string ServerAddress;			//服务器地址
	std::string Account;				//账户
	std::string SecretKey;				//密钥
	std::string ApplyName;				//应用名称
	std::string BackupsFolder;			//选择需要备份的文件夹
	std::string Backups;				//备份
	std::string Recovery;				//恢复
	std::string Return;					//返回
	std::string RecoveryList;			//恢复列表
	std::string Restoration;			//复原
	std::string Delete;					//删除
	std::string Cancel;					//取消
	std::string Confirm;				//确定
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
	std::string ScreenshotColor;		//截图颜色
	std::string Script;					//脚本
	std::string NotScript;				//没有脚本

	//系统托盘
	std::string Set;					//设置
	std::string ShutUp;					//言闭
	std::string Speak;					//言开
	std::string Exit;					//退出
	std::string strncpy;				//人家叫翻译姬！
}