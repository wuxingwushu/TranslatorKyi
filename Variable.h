#pragma once
#include <string>
#include <sstream>//toString / VectorToString 用
#include <vector>//识别到的显卡列表 std::vector<VulkanDeviceInfo>
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
	//上一次截图（TOOL::screen）实际用到的尺寸。截图缓冲区按这个尺寸分配，
	//和 windows_* 不一定相等（截图之后用户可能换了分辨率/显示器/DPI），
	//OCR、纹理上传、显示都必须用这一对，不能再读 windows_*，否则会越界读那块缓冲区。
	extern int ScreenShot_Width;//上一次截图的宽度
	extern int ScreenShot_Heigth;//上一次截图的高度

	extern std::string eng;//原文
	extern std::string zhong;//翻译

	// ============================================================================
	//  配置项表（表驱动）
	// ----------------------------------------------------------------------------
	//  每行 = (ini 节名, ini 键名, 类型, 变量名, 缺省值文本)。
	//  声明、定义、ReadFile、SaveFile 四处全部由这一张表生成 —— 原先这四处各手写一遍
	//  （Variable.h 声明墙 + Variable.cpp 定义墙 + ReadFile 平铺 + SaveFile 平铺），
	//  新增一个配置项要改四处，漏一处就是"存了不读/读了不存"。
	//  缺省值写字符串文本；nullptr 表示"原逻辑没有缺省值"（键缺失或值非法时照样抛异常）。
	//
	//  ⚠ 这张表在 Variable.cpp 里被重复展开，不要给它 #undef。
	// ============================================================================
#define TK_CONFIG_ITEMS(X) \
	/* 一言 */ \
	X(Hitokoto, PopUpNotificationBool,    bool,        PopUpNotificationBool, nullptr) \
	X(Hitokoto, HitokotoTimeInterval,     int,         HitokotoTimeInterval,  nullptr) \
	X(Hitokoto, HitokotoDisplayDuration,  int,         HitokotoDisplayDuration, nullptr) \
	X(Hitokoto, HitokotoPosX,             float,       HitokotoPosX,          nullptr) \
	X(Hitokoto, HitokotoPosY,             float,       HitokotoPosY,          nullptr) \
	X(Hitokoto, HitokotoFontSize,         float,       HitokotoFontSize,      nullptr) \
	X(Hitokoto, HitokotoTTFBool,          bool,        HitokotoTTFBool,       nullptr) \
	X(Hitokoto, HitokotoFontBool,         bool,        HitokotoFontBool,      nullptr) \
	X(Hitokoto, HitokotoFont,             std::string, HitokotoFont,          nullptr) \
	/* WebDav */ \
	X(WebDav,   url,                      std::string, WebDav_url,             nullptr) \
	X(WebDav,   username,                 std::string, WebDav_username,        nullptr) \
	X(WebDav,   password,                 std::string, WebDav_password,        nullptr) \
	X(WebDav,   WebFile,                  std::string, WebDav_WebFile,         nullptr) \
	X(WebDav,   OpcodeBool,               bool,        OpcodeBool,            nullptr) \
	X(WebDav,   LanguageBool,             bool,        LanguageBool,          nullptr) \
	X(WebDav,   TessDataBool,             bool,        TessDataBool,          nullptr) \
	X(WebDav,   TTFBool,                  bool,        TTFBool,               nullptr) \
	/* 百度翻译 */ \
	X(BaiduAPI, Baidu_ID,                 std::string, BaiduAppid,            nullptr) \
	X(BaiduAPI, Baidu_Key,                std::string, BaiduSecret_key,       nullptr) \
	/* 有道翻译 */ \
	X(YoudaoAPI, Youdao_ID,               std::string, YoudaoAppid,           nullptr) \
	X(YoudaoAPI, Youdao_Key,              std::string, YoudaoSecret_key,      nullptr) \
	/* 翻译配置 */ \
	X(FT,       Translate,                int,         Translate,             nullptr) \
	X(FT,       From,                     int,         From,                  nullptr) \
	X(FT,       To,                       int,         To,                    nullptr) \
	/* AI 模型翻译（老配置文件没有这些键，用缺省值补齐） */ \
	X(FT,       AIModelPath,              std::string, AiModelPath,           "") \
	X(FT,       AIThreads,                int,         AiThreads,             "0") \
	X(FT,       AINCtx,                   int,         AiNCtx,                "4096") \
	X(FT,       AIMaxTokens,              int,         AiMaxTokens,           "2048") \
	X(FT,       AITemperature,            float,       AiTemperature,         "0.7") \
	X(FT,       AIIdleUnload,             int,         AiIdleUnload,          "0") \
	X(FT,       AIDeviceMode,             int,         AiDeviceMode,          "0") \
	X(FT,       AIDeviceName,             std::string, AiDeviceName,          "") \
	/* 快捷键 */ \
	X(Key,      MakeUp,                   int,         MakeUp,                nullptr) \
	X(Key,      Screenshotkey,            std::string, Screenshotkey,         nullptr) \
	X(Key,      Choicekey,                std::string, Choicekey,             nullptr) \
	X(Key,      Replacekey,               std::string, Replacekey,            nullptr) \
	/* 设置 */ \
	X(Set,      TesseractModel,           std::string, Model,                 nullptr) \
	X(Set,      DisplayTime,              int,         DisplayTime,           nullptr) \
	X(Set,      FontSize,                 float,       FontSize,              nullptr) \
	X(Set,      ReplaceLanguage,          int,         ReplaceLanguage,       nullptr) \
	X(Set,      FontBool,                 bool,        FontBool,              nullptr) \
	X(Set,      FontFilePath,             std::string, FontFilePath,          nullptr) \
	X(Set,      Startup,                  bool,        Startup,               nullptr) \
	X(Set,      Language,                 std::string, Language,              nullptr) \
	X(Set,      Script,                   std::string, Script,                nullptr) \
	X(Set,      ScriptBool,               bool,        ScriptBool,            nullptr) \
	X(Set,      VulkanDeviceName,         std::string, VulkanDeviceName,      "")

	// 只读项（会被读进来，但 SaveFile 不回写 —— 保持原逻辑）。
#define TK_CONFIG_READONLY_ITEMS(X) \
	X(BaiduAPI,  Baidu_items,     std::vector<std::string>, Baiduitems,      nullptr) \
	X(BaiduAPI,  Baidu_itemsName, std::vector<std::string>, BaiduitemsName,  nullptr) \
	X(YoudaoAPI, Youdao_items,    std::vector<std::string>, Youdaoitems,     nullptr) \
	X(YoudaoAPI, Youdao_itemsName, std::vector<std::string>, YoudaoitemsName, nullptr)

#define TK_CONFIG_DECL(Section, Key, Type, Name, Default) extern Type Name;
	TK_CONFIG_ITEMS(TK_CONFIG_DECL)
	TK_CONFIG_READONLY_ITEMS(TK_CONFIG_DECL)
#undef TK_CONFIG_DECL

	extern unsigned char ScreenshotColor[4];//截图颜色（4 个分量，特殊读写：见 Variable.cpp）

	//渲染设备选择（Vulkan）
	//AutoBest 自动选最高性能、AutoWorst 自动选最低性能、CPU 走 SwiftShader 软件渲染、
	//Specific 用 VulkanDeviceName 指定某一台。索引值和 ini 里存的值一一对应，不要随意调换。
	enum class VulkanDeviceModeEnum {
		AutoBest = 0,
		AutoWorst = 1,
		CPU = 2,
		Specific = 3
	};

	//识别到的 Vulkan 设备。探测时由 Vulkan/instance.cpp 填充，设置界面的下拉框直接读它。
	struct VulkanDeviceInfo {
		std::string name;		//和 VkPhysicalDeviceProperties::deviceName 完全一致
		int deviceType = 0;		//1 集成显卡 2 独立显卡 3 虚拟显卡 4 CPU 软件设备 0 其它
		bool usable = true;		//是否满足最低要求（缺各向异性采样或 VK_KHR_swapchain 就不满足）
	};

	extern VulkanDeviceModeEnum VulkanDeviceMode;				//设备选择模式
	extern std::vector<VulkanDeviceInfo> VulkanDetectedDevices;	//这次识别到的设备列表
	extern bool RunningOnSoftwareRenderer;						//这次是否跑在 CPU 软件渲染上
	extern std::string RunningDeviceName;						//这次实际用的设备名
	extern std::string CpuSoftwareRenderReason;					//自动降级到 CPU 的原因

	inline bool IsCpuRenderingMode() noexcept { return VulkanDeviceMode == VulkanDeviceModeEnum::CPU; }
	inline bool IsSpecificDeviceMode() noexcept { return VulkanDeviceMode == VulkanDeviceModeEnum::Specific; }
}

namespace Language {
	extern void ReadFile(std::string FilePath);//读取

	// ============================================================================
	//  语言字符串表（表驱动）
	// ----------------------------------------------------------------------------
	//  每行 = (ini 节名, 变量名)。ini 里的键名恒为「变量名 + 下划线」，例如
	//  TranslationKey -> TranslationKey_。声明、定义、ReadFile 三处都由这张表生成。
	//  ⚠ 这张表在 Variable.cpp 里被重复展开，不要给它 #undef。
	// ============================================================================
#define TK_LANGUAGE_STRINGS(X) \
	/* 翻译界面 */ \
	X(Translate, TranslationKey)			/*翻译键*/ \
	X(Translate, From)						/*From*/ \
	X(Translate, To)						/*To*/ \
	/* 设置界面 */ \
	X(Set, HitokotoTimeInterval)			/*弹窗时间间隔*/ \
	X(Set, HitokotoDisplayDuration)			/*弹窗显示时长*/ \
	X(Set, IndependentTypeface)				/*独立字模*/ \
	X(Set, InternalFontPattern)				/*默认字模（勾上时用程序目录 TTF 里的默认字体）*/ \
	X(Set, DefaultTypeface)					/*默认字模（含 %s，界面里换成字体路径）*/ \
	X(Set, PositionX)						/*位置X*/ \
	X(Set, PositionY)						/*位置Y*/ \
	X(Set, HitokotoFontSize)				/*一言字体大小*/ \
	X(Set, jianguoyunWebDav)				/*坚果云WebDav*/ \
	X(Set, ServerAddress)					/*服务器地址*/ \
	X(Set, Account)							/*账户*/ \
	X(Set, SecretKey)						/*密钥*/ \
	X(Set, ApplyName)						/*应用名称*/ \
	X(Set, BackupsFolder)					/*选择需要备份的文件夹*/ \
	X(Set, Backups)							/*备份*/ \
	X(Set, Recovery)						/*恢复*/ \
	X(Set, Return)							/*返回*/ \
	X(Set, RecoveryList)					/*恢复列表*/ \
	X(Set, Restoration)						/*复原*/ \
	X(Set, Delete)							/*删除*/ \
	X(Set, Cancel)							/*取消*/ \
	X(Set, Confirm)							/*确定*/ \
	X(Set, AccountKey)						/*翻译密钥*/ \
	X(Set, BaiduID)							/*百度ID*/ \
	X(Set, BaiduKey)						/*百度Key*/ \
	X(Set, YoudaoID)						/*有道ID*/ \
	X(Set, YoudaoKey)						/*有道Key*/ \
	/* 界面重构新增：设置界面左侧分类导航 / 翻译窗口 / 关于页 */ \
	X(Set, NavTranslate)					/*导航-翻译服务*/ \
	X(Set, NavAI)							/*导航-AI模型*/ \
	X(Set, NavHotkey)						/*导航-快捷键*/ \
	X(Set, NavGeneral)						/*导航-常规*/ \
	X(Set, NavInterface)					/*导航-界面*/ \
	X(Set, NavHitokoto)						/*导航-一言*/ \
	X(Set, NavBackup)						/*导航-备份*/ \
	X(Set, NavAbout)						/*导航-关于*/ \
	X(Set, Saved)							/*提示-已保存*/ \
	X(Set, Clear)							/*翻译窗-清空*/ \
	X(Set, CopyResult)						/*翻译窗-复制译文*/ \
	X(Set, SwapLanguage)					/*翻译窗-互换源/目标语言*/ \
	X(Set, Engine)							/*翻译窗-翻译源*/ \
	X(Set, EngineHint) \
	X(Set, SourceLanguage)					/*翻译窗-源语言*/ \
	X(Set, TargetLanguage)					/*翻译窗-目标语言*/ \
	X(Set, AboutText)						/*关于页说明文字*/ \
	/* 本地 AI 模型（设置界面） */ \
	X(Set, AIModel)							/*AI模型（本地llama.cpp）*/ \
	X(Set, AIModelPath)						/*模型路径*/ \
	X(Set, AIModelDefault)					/*恢复默认路径*/ \
	X(Set, AIModelSelect)					/*选择模型（扫描 Modes 文件夹）*/ \
	X(Set, AIModelRefresh)					/*刷新模型列表*/ \
	X(Set, NotAiModelText)					/*没有找到模型（提示把 .gguf 放进 Modes 文件夹）*/ \
	X(Set, AIDevice)						/*运行设备（本地 AI 跑在 CPU 还是显卡上）*/ \
	X(Set, AIDeviceAutoBest)				/*自动选择最高性能*/ \
	X(Set, AIDeviceAutoWorst)				/*自动选择最低性能*/ \
	X(Set, AIDeviceCPU)						/*CPU（不使用显卡）*/ \
	X(Set, AIDeviceRefresh)					/*刷新设备列表*/ \
	X(Set, AIDeviceItem)					/*设备标签（描述 + 类型/显存，两个 %s）*/ \
	X(Set, AIDeviceTypeGPU)					/*显卡*/ \
	X(Set, AIDeviceTypeIGPU)				/*集成显卡*/ \
	X(Set, AIDeviceTypeACCEL)				/*加速器*/ \
	X(Set, AIDeviceTypeOther)				/*其它设备*/ \
	X(Set, AIDeviceMissing)					/*指定设备没识别到（带 %s）*/ \
	X(Set, AIDeviceHint)					/*切换设备后要重新加载模型*/ \
	X(Set, AIDeviceCurrent)					/*「当前使用设备」的标签（这行显示真正在跑的设备，可能和上面选的不同）*/ \
	X(Set, AIDeviceCurrentNone)				/*还没加载模型，没有正在用的设备*/ \
	X(Set, AIThreads)						/*推理线程数*/ \
	X(Set, AINCtx)							/*上下文长度*/ \
	X(Set, AIMaxTokens)						/*单次最多生成*/ \
	X(Set, AITemperature)					/*采样温度*/ \
	X(Set, AIIdleUnload)					/*空闲多少秒后自动卸载模型（0 = 不卸载）*/ \
	X(Set, AIIdleLeft)						/*｜空闲 %d 秒后自动卸载（带 %d，秒数）*/ \
	X(Set, AIHint)							/*使用提示*/ \
	X(Set, AIStatusLoaded)					/*状态：已加载（带 %s，模型信息）*/ \
	X(Set, AIStatusNotLoaded)				/*状态：未加载*/ \
	X(Set, AIStatusLoading)					/*状态：正在加载模型*/ \
	X(Set, AIStatusGenerating)				/*状态：正在翻译*/ \
	X(Set, AILoad)							/*加载模型按钮*/ \
	X(Set, AIUnload)						/*卸载模型按钮*/ \
	X(Set, AILoading)						/*正在加载模型…（带 %d，已用秒数）*/ \
	X(Set, AITranslating)					/*AI 翻译中…（带 %d，已用秒数）*/ \
	X(Set, Recognizing)						/*截图识别中…（截图翻译先 OCR 再翻）*/ \
	X(Set, Translating)						/*翻译中…（普通翻译源，如百度/有道）*/ \
	X(Set, AIFailed)						/*翻译失败（带 %s，错误信息）*/ \
	X(Set, AIFailedEmpty)					/*翻译失败（没有错误信息）*/ \
	X(Set, ShortcutKeys)					/*快捷键*/ \
	X(Set, KeyCombination)					/*组合键*/ \
	X(Set, ScreenshotTranslation)			/*截图翻译*/ \
	X(Set, SelectTranslation)				/*选择翻译*/ \
	X(Set, ReplaceTranslation)				/*替换翻译*/ \
	X(Set, Startup)							/*开机启动*/ \
	X(Set, ResidenceTime)					/*滞留时间（ms）*/ \
	X(Set, FontSize)						/*字体大小*/ \
	X(Set, TesseractModel)					/*Tesseract模型*/ \
	X(Set, NotTesseractModelText)			/*你没有Tesseract模型，模型放在当前程序位置的TessData*/ \
	X(Set, UseTTF_Typeface)					/*使用TTF字体*/ \
	X(Set, TTF_Folder)						/*TTF文件夹*/ \
	X(Set, TessDataFolder)					/*TessData文件夹*/ \
	X(Set, TTF_Typeface)					/*TTF字体*/ \
	X(Set, NotTTF_TypefaceText)				/*你没有TTF字体，字体放在当前程序位置的TTF*/ \
	X(Set, ReplaceLanguage)					/*替换语言*/ \
	X(Set, Save)							/*保存*/ \
	X(Set, Close)							/*关闭*/ \
	X(Set, Language)						/*语言*/ \
	X(Set, ScreenshotColor)					/*截图颜色*/ \
	X(Set, Script)							/*脚本*/ \
	X(Set, NotScript)						/*没有脚本*/ \
	/* 渲染设备选择（带 %s 的是模板，界面里会替换成设备名） */ \
	X(Set, RenderDevice)					/*渲染设备*/ \
	X(Set, RenderDeviceAutoBest)			/*自动选择最高性能*/ \
	X(Set, RenderDeviceAutoWorst)			/*自动选择最低性能*/ \
	X(Set, RenderDeviceCPU)					/*CPU 软件渲染*/ \
	X(Set, RenderDeviceUnusable)			/*不满足最低要求*/ \
	X(Set, RenderDeviceTypeIGPU)			/*集成显卡*/ \
	X(Set, RenderDeviceTypeDGPU)			/*独立显卡*/ \
	X(Set, RenderDeviceTypeVirtual)			/*虚拟显卡*/ \
	X(Set, RenderDeviceTypeCPU)				/*CPU 软件设备*/ \
	X(Set, RenderDeviceTypeOther)			/*其它*/ \
	X(Set, RenderDeviceItem)				/*设备标签（名字 + 类型，两个 %s）*/ \
	X(Set, RenderDeviceItemBad)				/*设备标签-不满足要求（三个 %s）*/ \
	X(Set, RenderDeviceRestart)				/*重启程序后生效*/ \
	X(Set, RenderDeviceMissing)				/*指定设备没识别到（带 %s）*/ \
	X(Set, RenderDeviceCurrentCPU)			/*当前：CPU 软件渲染（带 %s）*/ \
	X(Set, RenderDeviceCurrentSpecific)		/*当前：指定设备（带 %s）*/ \
	X(Set, RenderDeviceCurrentGPU)			/*当前：显卡渲染（带 %s）*/ \
	X(Set, RenderDeviceDegrade)				/*降级提示前缀（带 %s）*/ \
	X(Set, RenderDeviceHelp1)				/*帮助第 1 行*/ \
	X(Set, RenderDeviceHelp2)				/*帮助第 2 行*/ \
	X(Set, RenderDeviceHelp3)				/*帮助第 3 行*/ \
	X(Set, RenderDeviceHelp4)				/*帮助第 4 行*/ \
	X(Set, RenderDeviceHelp5)				/*帮助第 5 行*/ \
	X(Set, RenderDeviceHelp6)				/*帮助第 6 行*/ \
	X(Set, RenderDeviceHelp7)				/*帮助第 7 行*/ \
	/* 系统托盘 */ \
	X(tray, Set)							/*设置*/ \
	X(tray, ShutUp)							/*言闭*/ \
	X(tray, Speak)							/*言开*/ \
	X(tray, Exit)							/*退出*/ \
	X(tray, OpenFolder)						/*打开程序目录*/ \
	X(tray, strncpy)						/*人家叫翻译姬！*/

	// 特例表：ini 键名与变量名不一致的语言项（键名 = Key + 下划线，变量名 = Name）。
	// 绝大多数项的键名就是「变量名 + 下划线」，所以单独把少数对不上的列在这里，
	// 免得为了一个特例让上面 126 行都多写一列。
#define TK_LANGUAGE_STRINGS_ALIAS(X) \
	X(Set, HitokotoPopUpNotification, PopUpNotification)	/*一言弹窗（键名不是 PopUpNotification_）*/

#define TK_LANG_DECL(Section, Name) extern std::string Name;
	TK_LANGUAGE_STRINGS(TK_LANG_DECL)
#undef TK_LANG_DECL
#define TK_LANG_ALIAS_DECL(Section, Key, Name) extern std::string Name;
	TK_LANGUAGE_STRINGS_ALIAS(TK_LANG_ALIAS_DECL)
#undef TK_LANG_ALIAS_DECL
}