#pragma once
#include <string>
#include "ini.h"//软件数据
using namespace inih;//启用 ini 读取

namespace Variable {

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
}