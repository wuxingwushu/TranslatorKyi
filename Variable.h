#pragma once
#include <string>
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

	//一言
	extern bool PopUpNotificationBool;	//是否开启一言弹窗
	extern int HitokotoTimeInterval;	//一言弹窗时间间隔
	extern int HitokotoDisplayDuration;	//一言弹窗显示时长
	extern float HitokotoPosX;			//一言弹窗位置X
	extern float HitokotoPosY;			//一言弹窗位置Y
	extern float HitokotoFontSize;		//一言字体大小
	extern bool HitokotoFontBool;		//独立字模
	extern bool HitokotoTTFBool;		//使用内部字模
	extern std::string HitokotoFont;	//字模

	//WebDav
	extern std::string WebDav_url;		//WebDav 的服务器网址
	extern std::string WebDav_username;	//WebDav 账号
	extern std::string WebDav_password;	//WebDav 密钥
	extern std::string WebDav_WebFile;	//WebDav 应用名称
	//WebDav保存 那些文件夹
	extern bool OpcodeBool;
	extern bool LanguageBool;
	extern bool TessDataBool;
	extern bool TTFBool;

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
	extern std::string VulkanDeviceName;						//指定设备的名字（Specific 模式用）
	extern std::vector<VulkanDeviceInfo> VulkanDetectedDevices;	//这次识别到的设备列表
	extern bool RunningOnSoftwareRenderer;						//这次是否跑在 CPU 软件渲染上
	extern std::string RunningDeviceName;						//这次实际用的设备名
	extern std::string CpuSoftwareRenderReason;					//自动降级到 CPU 的原因

	inline bool IsCpuRenderingMode() noexcept { return VulkanDeviceMode == VulkanDeviceModeEnum::CPU; }
	inline bool IsSpecificDeviceMode() noexcept { return VulkanDeviceMode == VulkanDeviceModeEnum::Specific; }
}

namespace Language {
	extern void ReadFile(std::string FilePath);//读取

	//翻译界面
	extern std::string TranslationKey;			//翻译键
	extern std::string From;					//From
	extern std::string To;						//To

	//设置界面
	extern std::string PopUpNotification;		//一言弹窗
	extern std::string HitokotoTimeInterval;	//弹窗时间间隔
	extern std::string HitokotoDisplayDuration;	//弹窗显示时长
	extern std::string IndependentTypeface;		//独立字模
	extern std::string InternalFontPattern;		//内部字模
	extern std::string PositionX;				//位置X
	extern std::string PositionY;				//位置Y
	extern std::string HitokotoFontSize;		//一言字体大小
	extern std::string jianguoyunWebDav;		//坚果云WebDav
	extern std::string ServerAddress;			//服务器地址
	extern std::string Account;					//账户
	extern std::string SecretKey;				//密钥
	extern std::string ApplyName;				//应用名称
	extern std::string BackupsFolder;			//选择需要备份的文件夹
	extern std::string Backups;					//备份
	extern std::string Recovery;				//恢复
	extern std::string Return;					//返回
	extern std::string RecoveryList;			//恢复列表
	extern std::string Restoration;				//复原
	extern std::string Delete;					//删除
	extern std::string Cancel;					//取消
	extern std::string Confirm;					//确定
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

	//渲染设备选择（设置界面）
	extern std::string RenderDevice;				//渲染设备
	extern std::string RenderDeviceAutoBest;		//自动选择最高性能
	extern std::string RenderDeviceAutoWorst;		//自动选择最低性能
	extern std::string RenderDeviceCPU;				//CPU 软件渲染
	extern std::string RenderDeviceUnusable;		//不满足最低要求
	extern std::string RenderDeviceTypeIGPU;		//集成显卡
	extern std::string RenderDeviceTypeDGPU;		//独立显卡
	extern std::string RenderDeviceTypeVirtual;		//虚拟显卡
	extern std::string RenderDeviceTypeCPU;			//CPU 软件设备
	extern std::string RenderDeviceTypeOther;		//其它
	extern std::string RenderDeviceItem;			//设备标签（名字 + 类型，两个 %s）
	extern std::string RenderDeviceItemBad;			//设备标签-不满足要求（三个 %s）
	extern std::string RenderDeviceRestart;			//重启程序后生效
	extern std::string RenderDeviceMissing;			//指定设备没识别到（带 %s）
	extern std::string RenderDeviceCurrentCPU;		//当前：CPU 软件渲染（带 %s）
	extern std::string RenderDeviceCurrentSpecific;	//当前：指定设备（带 %s）
	extern std::string RenderDeviceCurrentGPU;		//当前：显卡渲染（带 %s）
	extern std::string RenderDeviceDegrade;			//降级提示前缀（带 %s）
	extern std::string RenderDeviceHelp1;			//帮助第 1 行
	extern std::string RenderDeviceHelp2;			//帮助第 2 行
	extern std::string RenderDeviceHelp3;			//帮助第 3 行
	extern std::string RenderDeviceHelp4;			//帮助第 4 行
	extern std::string RenderDeviceHelp5;			//帮助第 5 行
	extern std::string RenderDeviceHelp6;			//帮助第 6 行
	extern std::string RenderDeviceHelp7;			//帮助第 7 行

	//系统托盘
	extern std::string Set;						//设置
	extern std::string ShutUp;					//言闭
	extern std::string Speak;					//言开
	extern std::string Exit;					//退出
	extern std::string strncpy;					//人家叫翻译姬！

}