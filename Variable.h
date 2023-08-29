#pragma once
#include <string>
#include "ini.h"//软件数据
using namespace inih;//启用 ini 读取

template<typename T>std::string toString(const T t)
{
	std::ostringstream oss;  //创建一个格式化输出流
	oss << t;             //把值传递如流中
	return oss.str();
}

template <typename T>
std::string VectorToString(T* v, unsigned int size) {
	std::string str = "";
	for (size_t i = 0; i < size-1; i++)
	{
		str = str + toString(int(v[i])) + " ";
	}
	str = str + toString(int(v[size - 1]));
	return str;
}

namespace Variable {

	extern unsigned int WrapSize;


	// ini 配置信息处理
	extern char* IniPath;//储存文件路径
	//extern inih::INIReader* iniData;//读取文件工具
	extern void ReadFile(char* FilePath);//读取
	extern void SaveFile();//保存

	extern int windows_Width;//屏幕宽度
	extern int windows_Heigth;//屏幕高度

	extern std::string eng;//原文
	extern std::string zhong;//翻译

	//快捷键
	extern int MakeUp;//组合
	extern std::string Screenshotkey;//截图
	extern std::string Choicekey;//选择
	extern std::string Replacekey;//替换


	//百度翻译
	extern std::string BaiduAppid;//ID
	extern std::string BaiduSecret_key;//Key
	extern std::vector<std::string> Baiduitems;
	extern std::vector<std::string> BaiduitemsName;

	//有道翻译
	extern std::string YoudaoAppid;//ID
	extern std::string YoudaoSecret_key;//Key
	extern std::vector<std::string> Youdaoitems;
	extern std::vector<std::string> YoudaoitemsName;

	extern int Translate;//翻译引擎
	extern int From;//被翻译的语言
	extern int To;//翻译成什么语言

	//OCR识别模型
	extern std::string Model;//模型

	//设置
	extern int DisplayTime;//显示时间
	extern float FontSize;//字体大小
	extern int ReplaceLanguage;//替换为什么语言
	extern bool FontBool;//是否引用字体
	extern std::string FontFilePath;//字体文件路径
	extern bool Startup;//开机启动
	extern std::string Language;//语言
	extern unsigned char ScreenshotColor[4];//截图颜色
	extern std::string Script;					//脚本
	extern bool ScriptBool;//是否开启脚本
}

namespace Language {
	extern void ReadFile(std::string FilePath);//读取

	//翻译界面
	extern std::string TranslationKey;			//翻译键
	extern std::string From;					//From
	extern std::string To;						//To

	//设置界面
	extern std::string AccountKey;				//翻译密钥
	extern std::string BaiduID;					//百度ID
	extern std::string BaiduKey;				//百度Key
	extern std::string YoudaoID;				//有道ID
	extern std::string YoudaoKey;				//有道Key
	extern std::string ShortcutKeys;			//快捷键
	extern std::string KeyCombination;			//组合键
	extern std::string ScreenshotTranslation;	//截图翻译
	extern std::string SelectTranslation;		//选择翻译
	extern std::string ReplaceTranslation;		//替换翻译
	extern std::string Startup;					//开机启动
	extern std::string ResidenceTime;			//滞留时间（ms）
	extern std::string FontSize;				//字体大小
	extern std::string TesseractModel;			//Tesseract模型
	extern std::string NotTesseractModelText;	//你没有Tesseract模型，模型放在当前程序位置的TessData
	extern std::string UseTTF_Typeface;			//使用TTF字体
	extern std::string TTF_Folder;				//TTF文件夹
	extern std::string TessDataFolder;			//TessData文件夹
	extern std::string TTF_Typeface;			//TTF字体
	extern std::string NotTTF_TypefaceText;		//你没有TTF字体，字体放在当前程序位置的TTF
	extern std::string ReplaceLanguage;			//替换语言
	extern std::string Save;					//保存
	extern std::string Close;					//关闭
	extern std::string Language;				//语言
	extern std::string ScreenshotColor;			//截图颜色
	extern std::string Script;					//脚本
	extern std::string NotScript;				//没有脚本

	//系统托盘
	extern std::string Set;						//设置
	extern std::string Exit;					//退出
	extern std::string strncpy;					//人家叫翻译姬！

}