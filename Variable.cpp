#include "Variable.h"
#include "Tool/Convert.h"//TOOL::BoolConverter：解析缺省值文本里的 bool
#include <cstdio>//printf：写盘失败的兜底提示（DebugLog.h 是空实现，这里要一条一定能编译的输出）
#include <cstdlib>//atoi/atof：解析缺省值文本
#include <memory>//std::make_unique：SaveFile 里 RAII 持有 INIReader
#include <type_traits>//if constexpr 里判断类型

namespace {

	//把配置表里的缺省值文本解析成具体类型。
	//只有"原来就带缺省值"的项才会用到它（其余项 Default 为 nullptr，键缺失时照旧抛异常）。
	template <typename T>
	T ParseDefaultText(const char* Text)
	{
		if constexpr (std::is_same_v<T, std::string>) { return std::string(Text); }
		else if constexpr (std::is_same_v<T, bool>) { return TOOL::BoolConverter(Text); }
		else if constexpr (std::is_floating_point_v<T>) { return (T)std::atof(Text); }
		else { return (T)std::atoi(Text); }
	}

	//按配置表的一项从 ini 读一个值：
	//  Default == nullptr —— 原逻辑：键缺失/值非法就抛异常；
	//  Default != nullptr —— 原逻辑：读不到就用缺省值（等价于 INIReader::Get 的三参数版本）。
	template <typename T>
	T ReadCfg(const inih::INIReader& Reader, const char* Section, const char* Key, const char* Default)
	{
		if constexpr (std::is_same_v<T, std::vector<std::string>>)
		{
			if (Default == nullptr) { return Reader.GetVector<std::string>(Section, Key); }
			try { return Reader.GetVector<std::string>(Section, Key); }
			catch (const std::runtime_error&) { return T{}; }
		}
		else
		{
			if (Default == nullptr) { return Reader.Get<T>(Section, Key); }
			try { return Reader.Get<T>(Section, Key); }
			catch (const std::runtime_error&) { return ParseDefaultText<T>(Default); }
		}
	}
}

namespace Variable {
	unsigned int WrapSize = 12;

	// ---------------------------------------------------------------------------
	//  全局变量定义：全部由 Variable.h 里的两张表生成（不再手写定义墙）
	// ---------------------------------------------------------------------------
#define TK_CONFIG_DEF(Section, Key, Type, Name, Default) Type Name{};
	TK_CONFIG_ITEMS(TK_CONFIG_DEF)
	TK_CONFIG_READONLY_ITEMS(TK_CONFIG_DEF)
#undef TK_CONFIG_DEF

	char* IniPath;//储存文件路径

	int windows_Width;//屏幕宽度
	int windows_Heigth;//屏幕高度
	int ScreenShot_Width;//上一次截图用到的宽度
	int ScreenShot_Heigth;//上一次截图用到的高度

	std::string eng = "";//原文
	std::string zhong = "";//翻译

	unsigned char ScreenshotColor[4];	//截图颜色

	//渲染设备选择
	VulkanDeviceModeEnum VulkanDeviceMode = VulkanDeviceModeEnum::AutoBest;//设备选择模式
	std::vector<VulkanDeviceInfo> VulkanDetectedDevices;//这次识别到的设备列表
	bool RunningOnSoftwareRenderer = false;		//这次是否跑在 CPU 软件渲染上
	std::string RunningDeviceName = "";			//这次实际用的设备名
	std::string CpuSoftwareRenderReason = "";	//自动降级到 CPU 的原因

	void ReadFile(char* FilePath) {
		IniPath = FilePath;
		//用栈对象持有：中途任何一项读取失败抛异常时，不会再漏掉 delete（旧写法是 new + delete）。
		inih::INIReader iniData(IniPath);

		//配置项：按表读取（键名/节名/类型/目标变量全在 Variable.h 的表里）
#define TK_CONFIG_READ(Section, Key, Type, Name, Default) Name = ReadCfg<Type>(iniData, #Section, #Key, Default);
		TK_CONFIG_ITEMS(TK_CONFIG_READ)
		TK_CONFIG_READONLY_ITEMS(TK_CONFIG_READ)
#undef TK_CONFIG_READ

		//配置被改坏时退回能用的值，免得 llama.cpp 直接报错
		if (AiThreads < 0) { AiThreads = 0; }
		if (AiNCtx < 256) { AiNCtx = 4096; }
		if (AiMaxTokens < 16) { AiMaxTokens = 2048; }
		if (AiTemperature <= 0.0f || AiTemperature > 2.0f) { AiTemperature = 0.7f; }
		if (AiIdleUnload < 0) { AiIdleUnload = 0; }			//0 = 不自动卸载
		if (AiIdleUnload > 86400) { AiIdleUnload = 86400; }	//最多一天
		//运行设备模式：0 自动最高 1 自动最低 2 CPU 3 指定（见 LlamaTranslate::DeviceMode）
		if (AiDeviceMode < 0 || AiDeviceMode > 3) { AiDeviceMode = 0; }

		//截图颜色：单独读写（4 个分量，不能直接进配置表）
		std::vector<unsigned int> LScreenshotColor = iniData.GetVector<unsigned int>("Set", "ScreenshotColor");
		for (size_t i = 0; i < LScreenshotColor.size() && i < 4; i++)
		{
			ScreenshotColor[i] = (unsigned char)LScreenshotColor[i];
		}

		//渲染设备模式：int 读入 → 越界退回自动 → 转枚举（老配置文件里没有这个键，用缺省值）
		int LVulkanDeviceMode = iniData.Get<int>("Set", "VulkanDeviceMode", (int)VulkanDeviceModeEnum::AutoBest);
		if (LVulkanDeviceMode < (int)VulkanDeviceModeEnum::AutoBest || LVulkanDeviceMode > (int)VulkanDeviceModeEnum::Specific)
		{
			LVulkanDeviceMode = (int)VulkanDeviceModeEnum::AutoBest;//配置被人改坏时退回自动
		}
		VulkanDeviceMode = (VulkanDeviceModeEnum)LVulkanDeviceMode;
	}

	void SaveFile() {
		//读不出来（Data.ini 被删/被占用）时宁可这次什么都不做，也不能让异常把进程带走
		std::unique_ptr<inih::INIReader> iniData;
		try
		{
			iniData = std::make_unique<inih::INIReader>(IniPath);
		}
		catch (const std::exception& e)
		{
			printf("读取 %s 失败，本次设置未保存：%s\n", IniPath, e.what());
			return;
		}

		//配置项：按同一张表回写（有则覆盖、无则新增）
#define TK_CONFIG_WRITE(Section, Key, Type, Name, Default) iniData->UpdateEntry(#Section, #Key, Name);
		TK_CONFIG_ITEMS(TK_CONFIG_WRITE)
#undef TK_CONFIG_WRITE

		//单独写的两项（与 ReadFile 对应）
		iniData->UpdateEntry("Set", "ScreenshotColor", VectorToString<unsigned char>(ScreenshotColor, 4));
		iniData->UpdateEntry("Set", "VulkanDeviceMode", toString((int)VulkanDeviceMode));

		//写盘失败（文件被占用/只读/路径不存在）不能让异常逃出去：
		//这里是 ImGui 帧回调 → 主循环的调用链，中间没有任何 try/catch，
		//异常会一路 std::terminate 掉整个进程，表现就是"点一下保存，程序直接没了"。
		//降级成打印，设置仍留在内存里，至少程序还活着。
		try
		{
			inih::INIWriter::write_Gai(IniPath, *iniData);//保存
		}
		catch (const std::exception& e)
		{
			printf("保存 %s 失败：%s\n", IniPath, e.what());
		}
	}
}


namespace Language {

	//变量定义：由 Variable.h 里的语言表生成
#define TK_LANG_DEF(Section, Name) std::string Name;
	TK_LANGUAGE_STRINGS(TK_LANG_DEF)
#undef TK_LANG_DEF
#define TK_LANG_ALIAS_DEF(Section, Key, Name) std::string Name;
	TK_LANGUAGE_STRINGS_ALIAS(TK_LANG_ALIAS_DEF)
#undef TK_LANG_ALIAS_DEF

	void ReadFile(std::string FilePath) {
		inih::INIReader iniData("./Language/" + FilePath + ".ini");

		//ini 里的键名一般是「变量名 + 下划线」，由表里的变量名拼出来
#define TK_LANG_READ(Section, Name) Name = iniData.Get<std::string>(#Section, #Name "_");
		TK_LANGUAGE_STRINGS(TK_LANG_READ)
#undef TK_LANG_READ
		//键名与变量名对不上的少数项走特例表（键名直接写出来）
#define TK_LANG_ALIAS_READ(Section, Key, Name) Name = iniData.Get<std::string>(#Section, #Key "_");
		TK_LANGUAGE_STRINGS_ALIAS(TK_LANG_ALIAS_READ)
#undef TK_LANG_ALIAS_READ
	}
}