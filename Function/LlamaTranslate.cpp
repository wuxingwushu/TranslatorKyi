#include "LlamaTranslate.h"
#include "../Tool/Charset.h"//TOOL::OpenUtf8File
#include "../Tool/FileUtil.h"//TOOL::BaseName / TOOL::EqualsNoCase

#include <llama.h>
#include <ggml-backend.h>	//枚举推理设备（ggml_backend_dev_*）：设置界面的「运行设备」用它

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>	//MultiByteToWideChar / _wfopen：模型路径统一按 UTF-8 转成宽字符再开文件
#endif

// ============================================================================
//  Hunyuan 系（GGUF 架构 hunyuan-dense，如 Hy-MT2-1.8B）对话模板里的特殊 token 文本
//  必须与 GGUF 词表里的字节完全一致：竖线是 U+FF5C（｜），下划线是 U+2581（▁）
//  模板（模型自带）：<｜hy_begin▁of▁sentence｜> [<system 文本><｜hy_place▁holder▁no▁3｜>]
//                    <｜hy_User｜><用户内容><｜hy_Assistant｜><助手内容><｜hy_place▁holder▁no▁2｜>
// ============================================================================
static const char* HY_BOS		= "<｜hy_begin▁of▁sentence｜>";		//token 120000
static const char* HY_USER		= "<｜hy_User｜>";					//token 120006
static const char* HY_ASSISTANT	= "<｜hy_Assistant｜>";				//token 120007
static const char* HY_SYS_END	= "<｜hy_place▁holder▁no▁3｜>";		//token 120021（system 结束）
static const char* HY_EOS		= "<｜hy_place▁holder▁no▁2｜>";		//token 120020（= eos，助手回答结束）

//模型统一放在程序目录的 Modes 文件夹里（Modes/Modes.txt 里也这么写着）；
//老版本放在 Environment/ 下，ResolveModelPath() 两边都会找，不会影响老配置
static std::string gDefaultModelPath = "Modes/Hy-MT2-1.8B-Q4_K_M.gguf";

//退出/重启前的「尽快收尾」请求，见 LlamaTranslate.h 的 RequestStop()/ClearStop()
std::atomic<bool> LlamaTranslate::sStopRequested{ false };

void LlamaTranslate::RequestStop()
{
	sStopRequested = true;
}

void LlamaTranslate::ClearStop()
{
	sStopRequested = false;
}

//模型文件夹的查找阶梯：程序一般跑在 build/<preset>/<配置> 里（那里也有一份 CMake 拷过去的 Modes），
//而模型可能只放在仓库根的 Modes/，所以从当前目录开始逐级向上找
static const char* const MODEL_DIR_LADDER[] = { "./Modes/", "../Modes/", "../../Modes/",
                                                "../../../Modes/", "../../../../Modes/" };
//老版本把模型放在 Environment/ 下：解析路径时一并找，老配置不会失效
static const char* const LEGACY_DIR_LADDER[] = { "./Environment/", "../Environment/", "../../Environment/",
                                                 "../../../Environment/", "../../../../Environment/" };

// 只打印错误/警告，避免 llama.cpp 的日志刷屏
static void TkLlamaLog(enum ggml_log_level level, const char* text, void* /*user_data*/)
{
	if (level >= GGML_LOG_LEVEL_ERROR && text)
	{
		fprintf(stderr, "%s", text);
	}
}

// 判断 p 处是不是全角竖线「｜」(U+FF5C = EF BD 9C)
static bool IsFullWidthBar(const char* p)
{
	return (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBD && (unsigned char)p[2] == 0x9C;
}

// 去掉输出里的特殊 token 文本（形如 <｜hy_...｜>），并去掉首尾空白
static std::string StripSpecialMarks(const std::string& text)
{
	std::string out;
	out.reserve(text.size());

	size_t i = 0;
	while (i < text.size())
	{
		if (text[i] == '<' && i + 4 < text.size() && IsFullWidthBar(text.c_str() + i + 1))
		{
			//向后找配对的「｜>」
			size_t j = i + 4;
			bool closed = false;
			while (j + 3 < text.size())
			{
				if (IsFullWidthBar(text.c_str() + j) && text[j + 3] == '>')
				{
					j += 4;
					closed = true;
					break;
				}
				++j;
			}
			if (closed)
			{
				i = j;
				continue;
			}
		}
		out.push_back(text[i]);
		++i;
	}

	const size_t b = out.find_first_not_of(" \t\r\n");
	if (b == std::string::npos)
	{
		return std::string();
	}
	const size_t e = out.find_last_not_of(" \t\r\n");
	return out.substr(b, e - b + 1);
}

//以 UTF-8 路径打开文件：已收敛到 TOOL::OpenUtf8File（Tool/Charset.h）。
//Windows 上非 ASCII 路径必须走宽字符 API，而项目内部（以及 llama.cpp 自己的 ggml_fopen）
//都按 CP_UTF8 解释路径。

static bool FileExists(const std::string& path)
{
	if (path.empty())
	{
		return false;
	}
	FILE* f = TOOL::OpenUtf8File(path, L"rb");
	if (!f)
	{
		return false;
	}
	fclose(f);
	return true;
}

//取文件名(TOOL::BaseName)与大小写不敏感比较(TOOL::EqualsNoCase)已收敛到 Tool/FileUtil.h。

// 语言代码/别名 → 英文语言名（模型按英文名理解目标语言）
namespace
{
	struct LangEntry
	{
		const char* Code;
		const char* Name;
	};

	const LangEntry LANG_TABLE[] =
	{
		{ "zh", "Chinese" }, { "zh-cn", "Chinese" }, { "zh_cn", "Chinese" }, { "cn", "Chinese" },
		{ "中文", "Chinese" }, { "汉语", "Chinese" }, { "简体", "Chinese" }, { "简体中文", "Chinese" }, { "chinese", "Chinese" },
		{ "zh-tw", "Traditional Chinese" }, { "zh_tw", "Traditional Chinese" }, { "繁体", "Traditional Chinese" },
		{ "繁体中文", "Traditional Chinese" }, { "traditional chinese", "Traditional Chinese" },
		{ "en", "English" }, { "eng", "English" }, { "英文", "English" }, { "英语", "English" }, { "english", "English" },
		{ "ja", "Japanese" }, { "jp", "Japanese" }, { "日文", "Japanese" }, { "日语", "Japanese" }, { "japanese", "Japanese" },
		{ "ko", "Korean" }, { "kr", "Korean" }, { "韩语", "Korean" }, { "韩文", "Korean" }, { "korean", "Korean" },
		{ "fr", "French" }, { "fra", "French" }, { "法语", "French" }, { "french", "French" },
		{ "de", "German" }, { "deu", "German" }, { "德语", "German" }, { "german", "German" },
		{ "es", "Spanish" }, { "spa", "Spanish" }, { "西班牙语", "Spanish" }, { "spanish", "Spanish" },
		{ "ru", "Russian" }, { "rus", "Russian" }, { "俄语", "Russian" }, { "russian", "Russian" },
		{ "pt", "Portuguese" }, { "por", "Portuguese" }, { "葡萄牙语", "Portuguese" }, { "portuguese", "Portuguese" },
		{ "it", "Italian" }, { "ita", "Italian" }, { "意大利语", "Italian" }, { "italian", "Italian" },
		{ "ar", "Arabic" }, { "ara", "Arabic" }, { "阿拉伯语", "Arabic" }, { "arabic", "Arabic" },
		{ "th", "Thai" }, { "tha", "Thai" }, { "泰语", "Thai" }, { "thai", "Thai" },
		{ "vi", "Vietnamese" }, { "vie", "Vietnamese" }, { "越南语", "Vietnamese" }, { "vietnamese", "Vietnamese" },
		{ "id", "Indonesian" }, { "ind", "Indonesian" }, { "印尼语", "Indonesian" }, { "indonesian", "Indonesian" },
		{ "ms", "Malay" }, { "may", "Malay" }, { "马来语", "Malay" }, { "malay", "Malay" },
		{ "tr", "Turkish" }, { "tur", "Turkish" }, { "土耳其语", "Turkish" }, { "turkish", "Turkish" },
		{ "pl", "Polish" }, { "pol", "Polish" }, { "波兰语", "Polish" }, { "polish", "Polish" },
		{ "nl", "Dutch" }, { "nld", "Dutch" }, { "荷兰语", "Dutch" }, { "dutch", "Dutch" },
		{ "sv", "Swedish" }, { "swe", "Swedish" }, { "瑞典语", "Swedish" }, { "swedish", "Swedish" },
		{ "da", "Danish" }, { "dan", "Danish" }, { "丹麦语", "Danish" }, { "danish", "Danish" },
		{ "fi", "Finnish" }, { "fin", "Finnish" }, { "芬兰语", "Finnish" }, { "finnish", "Finnish" },
		{ "cs", "Czech" }, { "ces", "Czech" }, { "捷克语", "Czech" }, { "czech", "Czech" },
		{ "hu", "Hungarian" }, { "hun", "Hungarian" }, { "匈牙利语", "Hungarian" }, { "hungarian", "Hungarian" },
		{ "ro", "Romanian" }, { "ron", "Romanian" }, { "罗马尼亚语", "Romanian" }, { "romanian", "Romanian" },
		{ "bg", "Bulgarian" }, { "bul", "Bulgarian" }, { "保加利亚语", "Bulgarian" }, { "bulgarian", "Bulgarian" },
		{ "el", "Greek" }, { "ell", "Greek" }, { "希腊语", "Greek" }, { "greek", "Greek" },
		{ "he", "Hebrew" }, { "heb", "Hebrew" }, { "希伯来语", "Hebrew" }, { "hebrew", "Hebrew" },
		{ "hi", "Hindi" }, { "hin", "Hindi" }, { "印地语", "Hindi" }, { "hindi", "Hindi" },
		{ "bn", "Bengali" }, { "ben", "Bengali" }, { "孟加拉语", "Bengali" }, { "bengali", "Bengali" },
		{ "ta", "Tamil" }, { "tam", "Tamil" }, { "泰米尔语", "Tamil" }, { "tamil", "Tamil" },
		{ "te", "Telugu" }, { "tel", "Telugu" }, { "泰卢固语", "Telugu" }, { "telugu", "Telugu" },
		{ "ur", "Urdu" }, { "urd", "Urdu" }, { "乌尔都语", "Urdu" }, { "urdu", "Urdu" },
		{ "fa", "Persian" }, { "fas", "Persian" }, { "波斯语", "Persian" }, { "persian", "Persian" },
		{ "uk", "Ukrainian" }, { "ukr", "Ukrainian" }, { "乌克兰语", "Ukrainian" }, { "ukrainian", "Ukrainian" },
		{ "no", "Norwegian" }, { "nor", "Norwegian" }, { "挪威语", "Norwegian" }, { "norwegian", "Norwegian" },
		{ "sk", "Slovak" }, { "slk", "Slovak" }, { "斯洛伐克语", "Slovak" }, { "slovak", "Slovak" },
		{ "sl", "Slovenian" }, { "slv", "Slovenian" }, { "斯洛文尼亚语", "Slovenian" }, { "slovenian", "Slovenian" },
		{ "hr", "Croatian" }, { "hrv", "Croatian" }, { "克罗地亚语", "Croatian" }, { "croatian", "Croatian" },
		{ "sr", "Serbian" }, { "srp", "Serbian" }, { "塞尔维亚语", "Serbian" }, { "serbian", "Serbian" },
		{ "lt", "Lithuanian" }, { "lit", "Lithuanian" }, { "立陶宛语", "Lithuanian" }, { "lithuanian", "Lithuanian" },
		{ "lv", "Latvian" }, { "lav", "Latvian" }, { "拉脱维亚语", "Latvian" }, { "latvian", "Latvian" },
		{ "et", "Estonian" }, { "est", "Estonian" }, { "爱沙尼亚语", "Estonian" }, { "estonian", "Estonian" },
		{ "ca", "Catalan" }, { "cat", "Catalan" }, { "加泰罗尼亚语", "Catalan" }, { "catalan", "Catalan" },
		{ "tl", "Filipino" }, { "fil", "Filipino" }, { "菲律宾语", "Filipino" }, { "filipino", "Filipino" },
		{ "my", "Burmese" }, { "缅甸语", "Burmese" }, { "burmese", "Burmese" },
		{ "km", "Khmer" }, { "高棉语", "Khmer" }, { "khmer", "Khmer" },
		{ "lo", "Lao" }, { "老挝语", "Lao" }, { "lao", "Lao" },
		{ "ne", "Nepali" }, { "尼泊尔语", "Nepali" }, { "nepali", "Nepali" },
		{ "si", "Sinhala" }, { "僧伽罗语", "Sinhala" }, { "sinhala", "Sinhala" },
	};
}

std::string LlamaTranslate::LanguageName(const std::string& lang)
{
	if (lang.empty())
	{
		return lang;
	}
	for (const LangEntry& e : LANG_TABLE)
	{
		if (TOOL::EqualsNoCase(lang, e.Code))
		{
			return e.Name;
		}
	}
	return lang;	//查不到就认为调用方给的就是语言名（例如 "Chinese" 本身）
}

const std::string& LlamaTranslate::DefaultModelPath()
{
	return gDefaultModelPath;
}

void LlamaTranslate::SetDefaultModelPath(const std::string& path)
{
	gDefaultModelPath = path;
}

const char* LlamaTranslate::ModelsFolder()
{
	return "Modes";
}

// 扫描模型文件夹：按查找阶梯从程序目录逐级向上找 Modes，把里面的 .gguf 都列出来。
// 返回的是能直接用的相对路径（例如 "./Modes/Hy-MT2-1.8B-Q4_K_M.gguf"）；
// 同一个文件名只在离程序最近的那层出现一次（build 目录里那份和仓库根那份不会重复列）。
std::vector<std::string> LlamaTranslate::ListModelFiles()
{
	std::vector<std::string> Files;
	for (const char* Dir : MODEL_DIR_LADDER)
	{
		std::error_code DirEc;
		std::filesystem::directory_iterator It(Dir, DirEc);
		if (DirEc)	//这层没有 Modes 文件夹（或者读不了），接着往上找
		{
			continue;
		}
		std::vector<std::string> Found;
		for (const std::filesystem::directory_entry& Entry : It)
		{
			std::error_code EntryEc;
			if (!Entry.is_regular_file(EntryEc))
			{
				continue;
			}
			//扩展名/文件名都用 u8string()：Windows 下 path::string() 返回的是 ANSI(936) 字节，
			//中文名字的模型会变成乱码字符串，而 llama.cpp 按 UTF-8 认路径
			std::string Extension = Entry.path().extension().u8string();
			for (size_t i = 0; i < Extension.size(); ++i)
			{
				Extension[i] = (char)std::tolower((unsigned char)Extension[i]);
			}
			if (Extension != ".gguf")
			{
				continue;
			}
			Found.push_back(std::string(Dir) + TOOL::BaseName(Entry.path().u8string()));
		}
		std::sort(Found.begin(), Found.end());
		for (const std::string& Candidate : Found)
		{
			const std::string Name = TOOL::BaseName(Candidate);
			bool Duplicate = false;
			for (const std::string& Existing : Files)
			{
				if (TOOL::EqualsNoCase(TOOL::BaseName(Existing), Name))
				{
					Duplicate = true;
					break;
				}
			}
			if (!Duplicate)
			{
				Files.push_back(Candidate);
			}
		}
	}
	return Files;
}

// 相对路径解析：先按给的路径找，找不到就只取文件名，去「模型文件夹（Modes）」里逐级向上找，
// 再兼容老版本放模型的 Environment/；都没有就原样返回，让上层报「找不到模型文件」
std::string LlamaTranslate::ResolveModelPath(const std::string& path)
{
	if (path.empty() || FileExists(path))
	{
		return path;
	}
	const size_t slash = path.find_last_of("/\\");
	const std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
	if (FileExists(name))	//程序当前目录下就有一份同名的
	{
		return name;
	}
	for (const char* p : MODEL_DIR_LADDER)
	{
		const std::string candidate = std::string(p) + name;
		if (FileExists(candidate))
		{
			return candidate;
		}
	}
	for (const char* p : LEGACY_DIR_LADDER)
	{
		const std::string candidate = std::string(p) + name;
		if (FileExists(candidate))
		{
			return candidate;
		}
	}
	//再试试原始相对路径本身
	return path;
}

LlamaTranslate::LlamaTranslate()
{
}

LlamaTranslate::~LlamaTranslate()
{
	Unload();
}

//设备显示名：优先用描述（一般是显卡型号），没有描述就退回设备标识
static std::string DeviceDisplayName(ggml_backend_dev_t Dev)
{
	const char* Desc = ggml_backend_dev_description(Dev);
	const char* Name = ggml_backend_dev_name(Dev);
	if (Desc != nullptr && Desc[0] != '\0') { return Desc; }
	return (Name != nullptr) ? Name : "";
}

//按设备标识（如 "Vulkan0"）找一台设备：设置里指定了 Specific 就用它；找不到返回 nullptr
static ggml_backend_dev_t FindDeviceByName(const std::string& Name)
{
	const size_t Count = ggml_backend_dev_count();
	for (size_t i = 0; i < Count; i++)
	{
		ggml_backend_dev_t Dev = ggml_backend_dev_get(i);
		if (Dev == nullptr) { continue; }
		const char* DevName = ggml_backend_dev_name(Dev);
		if (DevName != nullptr && Name == DevName) { return Dev; }
	}
	return nullptr;
}

//设备类型优先级：独显 > 集成显卡 > 加速器 > 其它；数值越小越强
static int DeviceTypeRank(int Type)
{
	switch (Type)
	{
	case GGML_BACKEND_DEVICE_TYPE_GPU:   return 0;
	case GGML_BACKEND_DEVICE_TYPE_IGPU:  return 1;
	case GGML_BACKEND_DEVICE_TYPE_ACCEL: return 2;
	default:                             return 3;
	}
}

//在加速设备（非 CPU）里挑一台：先比设备类型（独显 > 集成显卡 > 加速器 > 其它），同类型再比显存大小。
//WantMaxMem = true（自动最高性能）挑最强的类型 + 显存最大的；false（自动最低性能）反过来挑最弱的类型 + 显存最小的。
//拿不到显存大小（都是 0）时同类型里按识别顺序取第一台；一台加速设备都没有则返回 nullptr（调用方退回 CPU）。
static ggml_backend_dev_t PickAccelerator(bool WantMaxMem)
{
	ggml_backend_dev_t Picked = nullptr;
	int PickedRank = 0;
	unsigned long long PickedMem = 0;
	const size_t Count = ggml_backend_dev_count();
	for (size_t i = 0; i < Count; i++)
	{
		ggml_backend_dev_t Dev = ggml_backend_dev_get(i);
		if (Dev == nullptr) { continue; }
		const int Type = (int)ggml_backend_dev_type(Dev);
		if (Type == GGML_BACKEND_DEVICE_TYPE_CPU) { continue; }//CPU 是兜底，不参与评选

		ggml_backend_dev_props Props{};
		ggml_backend_dev_get_props(Dev, &Props);
		const int Rank = DeviceTypeRank(Type);
		const unsigned long long Mem = (unsigned long long)Props.memory_total;

		bool Better = false;
		if (Picked == nullptr) { Better = true; }
		else if (Rank != PickedRank) { Better = WantMaxMem ? (Rank < PickedRank) : (Rank > PickedRank); }
		else { Better = WantMaxMem ? (Mem > PickedMem) : (Mem < PickedMem); }

		if (Better)
		{
			Picked = Dev;
			PickedRank = Rank;
			PickedMem = Mem;
		}
	}
	return Picked;
}

//llama 后端与日志只需要初始化一次；枚举设备、加载模型之前都必须先跑
void LlamaTranslate::EnsureBackendInit()
{
	static std::once_flag sOnce;
	std::call_once(sOnce, []()
	{
		llama_log_set(TkLlamaLog, nullptr);
		llama_backend_init();
	});
}

std::vector<LlamaTranslate::DeviceInfo> LlamaTranslate::ListDevices()
{
	EnsureBackendInit();

	std::vector<DeviceInfo> Result;
	const size_t Count = ggml_backend_dev_count();
	for (size_t i = 0; i < Count; i++)
	{
		ggml_backend_dev_t Dev = ggml_backend_dev_get(i);
		if (Dev == nullptr) { continue; }
		if (ggml_backend_dev_type(Dev) == GGML_BACKEND_DEVICE_TYPE_CPU) { continue; }//CPU 在设置里是单独一项（「CPU（不使用显卡）」），这里不再重复列

		DeviceInfo Info;
		const char* Name = ggml_backend_dev_name(Dev);
		const char* Desc = ggml_backend_dev_description(Dev);
		Info.name = (Name != nullptr) ? Name : "";
		Info.desc = (Desc != nullptr) ? Desc : "";
		Info.type = (int)ggml_backend_dev_type(Dev);

		ggml_backend_dev_props Props{};
		ggml_backend_dev_get_props(Dev, &Props);
		Info.memTotal = (unsigned long long)Props.memory_total;
		Info.memFree = (unsigned long long)Props.memory_free;
		Result.push_back(Info);
	}
	//下拉框里也按同样的优先级排（独显 > 集成显卡 > 加速器 > 其它），同类型里显存大的在前
	std::sort(Result.begin(), Result.end(), [](const DeviceInfo& A, const DeviceInfo& B)
	{
		const int RankA = DeviceTypeRank(A.type);
		const int RankB = DeviceTypeRank(B.type);
		if (RankA != RankB) { return RankA < RankB; }
		return A.memTotal > B.memTotal;
	});
	return Result;
}

bool LlamaTranslate::Load(const std::string& modelPath)
{
	Params params;
	params.ModelPath = modelPath;
	return Load(params);
}

bool LlamaTranslate::Load(const Params& params)
{
	std::lock_guard<std::mutex> lock(mMutex);

	//先卸载旧模型
	ReleaseContext();
	if (mModel)
	{
		llama_model_free(mModel);
		mModel = nullptr;
		mVocab = nullptr;
	}
	mModelDesc.clear();
	mLastError.clear();
	mActiveDevice.clear();

	mParams = params;
	if (mParams.ModelPath.empty())
	{
		mParams.ModelPath = gDefaultModelPath;
	}
	const std::string path = ResolveModelPath(mParams.ModelPath);
	if (!FileExists(path))
	{
		mLastError = "找不到模型文件：" + path;
		fprintf(stderr, "[LlamaTranslate] %s\n", mLastError.c_str());
		return false;
	}
	mParams.ModelPath = path;

	//llama 后端与日志只需要初始化一次（枚举设备之前也必须先跑）
	EnsureBackendInit();

	//1) 按设置决定这次用哪些设备、卸载多少层。
	//   devices 必须以 nullptr 结尾；留空表示交给 llama.cpp 自己挑（它会跳过 CPU/加速器，用所有 GPU）
	std::vector<ggml_backend_dev_t> Devices;
	int32_t NGpuLayers = mParams.NGpuLayers;
	std::string DeviceLog;
	switch (mParams.Device)
	{
	case DeviceMode::CPU:
		NGpuLayers = 0;
		DeviceLog = "CPU（不使用加速设备）";
		break;
	case DeviceMode::Specific:
	{
		ggml_backend_dev_t Picked = FindDeviceByName(mParams.DeviceName);
		if (Picked != nullptr)
		{
			Devices.push_back(Picked);
			if (NGpuLayers == 0) { NGpuLayers = -1; }	//-1 = 层全卸载过去
			DeviceLog = DeviceDisplayName(Picked) + "（设置指定）";
		}
		else
		{
			NGpuLayers = 0;
			DeviceLog = "设置指定的设备 \"" + mParams.DeviceName + "\" 这次没识别到，改用 CPU";
		}
		break;
	}
	case DeviceMode::AutoWorst:
	{
		ggml_backend_dev_t Picked = PickAccelerator(false);
		if (Picked != nullptr)
		{
			Devices.push_back(Picked);
			if (NGpuLayers == 0) { NGpuLayers = -1; }
			DeviceLog = DeviceDisplayName(Picked) + "（自动·最低性能）";
		}
		else
		{
			NGpuLayers = 0;
			DeviceLog = "没有识别到加速设备，改用 CPU";
		}
		break;
	}
	case DeviceMode::AutoBest:
	default:
	{
		ggml_backend_dev_t Picked = PickAccelerator(true);
		if (Picked != nullptr)
		{
			Devices.push_back(Picked);
			if (NGpuLayers == 0) { NGpuLayers = -1; }
			DeviceLog = DeviceDisplayName(Picked) + "（自动·最高性能）";
		}
		else
		{
			NGpuLayers = -1;	//没有加速设备：丢给 llama.cpp，它会直接在 CPU 上跑
			DeviceLog = "没有识别到加速设备，用 CPU";
		}
		break;
	}
	}
	if (!Devices.empty()) { Devices.push_back(nullptr); }

	//2) 加载模型
	llama_model_params modelParams = llama_model_default_params();
	modelParams.devices = Devices.empty() ? nullptr : Devices.data();
	modelParams.n_gpu_layers = NGpuLayers;
	modelParams.load_mode = LLAMA_LOAD_MODE_MMAP;	//该版本没有 use_mmap 字段了
	mModel = llama_model_load_from_file(path.c_str(), modelParams);
	if (!mModel && modelParams.devices != nullptr)
	{
		//走显卡这条路失败（显存不够 / 驱动有问题）时退回纯 CPU：慢一点，但至少能用
		fprintf(stderr, "[LlamaTranslate] 用「%s」加载失败，退回 CPU 重试\n", DeviceLog.c_str());
		modelParams.devices = nullptr;
		modelParams.n_gpu_layers = 0;
		mModel = llama_model_load_from_file(path.c_str(), modelParams);
	}
	if (!mModel)
	{
		mLastError = "加载模型失败：" + path;
		fprintf(stderr, "[LlamaTranslate] %s\n", mLastError.c_str());
		return false;
	}
	mVocab = llama_model_get_vocab(mModel);

	//这次真正跑在哪：devices 非空且卸载了层才是显卡，否则（含退回来的那次）就是 CPU。
	//设置界面的「当前使用设备」显示的就是它，所以必须按加载结果如实记，不能照抄上面的设置。
	if (modelParams.devices != nullptr && modelParams.n_gpu_layers != 0)
	{
		mActiveDevice = DeviceDisplayName(modelParams.devices[0]);
	}
	else
	{
		mActiveDevice = "CPU";
	}

	char desc[256] = { 0 };
	if (llama_model_desc(mModel, desc, sizeof(desc)) > 0)
	{
		mModelDesc = desc;
	}

	//2) 创建上下文
	int nCtx = mParams.NCtx > 0 ? mParams.NCtx : 4096;
	const int nCtxTrain = (int)llama_model_n_ctx_train(mModel);
	if (nCtxTrain > 0 && nCtx > nCtxTrain)
	{
		nCtx = nCtxTrain;
	}
	int nThreads = mParams.NThreads > 0 ? mParams.NThreads : (int)std::thread::hardware_concurrency();
	if (nThreads <= 0)
	{
		nThreads = 4;
	}

	llama_context_params ctxParams = llama_context_default_params();
	ctxParams.n_ctx = (uint32_t)nCtx;
	ctxParams.n_batch = (uint32_t)nCtx;	//让整段 prompt 一次就能喂进去
	ctxParams.n_threads = (uint32_t)nThreads;
	ctxParams.n_threads_batch = (uint32_t)nThreads;
	ctxParams.no_perf = true;

	mCtx = llama_init_from_model(mModel, ctxParams);
	if (!mCtx)
	{
		mLastError = "创建 llama_context 失败（内存不足？可减小 NCtx）";
		fprintf(stderr, "[LlamaTranslate] %s\n", mLastError.c_str());
		llama_model_free(mModel);
		mModel = nullptr;
		mVocab = nullptr;
		return false;
	}

	//3) 批处理对象与采样链
	mBatch = llama_batch_ext_init(mCtx);
	if (!mBatch)
	{
		mLastError = "创建 batch 失败";
		ReleaseContext();
		llama_model_free(mModel);
		mModel = nullptr;
		mVocab = nullptr;
		return false;
	}
	ApplySamplerParams();

	//4) 自检：确认模板里的特殊 token 在词表里是单个 token（否则说明模型与模板不匹配）
	{
		const char* marks[] = { HY_BOS, HY_USER, HY_ASSISTANT, HY_SYS_END, HY_EOS };
		std::vector<llama_token> probe(8);
		for (const char* mark : marks)
		{
			const int n = llama_tokenize(mVocab, mark, (int32_t)strlen(mark), probe.data(),
			                             (int32_t)probe.size(), false, true);
			if (n != 1)
			{
				fprintf(stderr, "[LlamaTranslate] 警告：特殊 token \"%s\" 没有被解析成单个 token（n=%d），"
				                "当前模型可能不是 Hy-MT2/Hunyuan 系模型\n", mark, n);
			}
		}
	}

	fprintf(stderr, "[LlamaTranslate] 模型已加载：%s（ctx=%d, threads=%d，运行设备：%s）\n",
	        mModelDesc.c_str(), nCtx, nThreads, DeviceLog.c_str());
	return true;
}

void LlamaTranslate::Unload()
{
	std::lock_guard<std::mutex> lock(mMutex);
	ReleaseContext();
	if (mModel)
	{
		llama_model_free(mModel);
		mModel = nullptr;
		mVocab = nullptr;
	}
	mModelDesc.clear();
	mActiveDevice.clear();
}

void LlamaTranslate::ReleaseContext()
{
	if (mSampler)
	{
		llama_sampler_free(mSampler);
		mSampler = nullptr;
	}
	if (mBatch)
	{
		llama_batch_ext_free(mBatch);
		mBatch = nullptr;
	}
	if (mCtx)
	{
		llama_free(mCtx);
		mCtx = nullptr;
	}
}

void LlamaTranslate::ApplySamplerParams()
{
	if (mSampler)
	{
		llama_sampler_free(mSampler);
		mSampler = nullptr;
	}
	if (!mCtx || !mVocab)
	{
		return;
	}

	llama_sampler_chain_params chainParams = llama_sampler_chain_default_params();
	chainParams.no_perf = true;
	mSampler = llama_sampler_chain_init(chainParams);

	llama_sampler_chain_add(mSampler, llama_sampler_init_penalties(llama_vocab_n_tokens(mVocab),
	                        mParams.RepeatLastN, mParams.RepeatPenalty, 0.0f, 0.0f));
	llama_sampler_chain_add(mSampler, llama_sampler_init_top_k(mParams.TopK));
	llama_sampler_chain_add(mSampler, llama_sampler_init_top_p(mParams.TopP, 1));
	llama_sampler_chain_add(mSampler, llama_sampler_init_temp(mParams.Temperature));
	llama_sampler_chain_add(mSampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

// 按模型自带的对话模板拼 prompt：<bos><｜hy_User｜>内容<｜hy_Assistant｜>
std::string LlamaTranslate::MakePrompt(const std::string& userContent) const
{
	std::string prompt;
	prompt.reserve(userContent.size() + 64);
	prompt += HY_BOS;
	prompt += HY_USER;
	prompt += userContent;
	prompt += HY_ASSISTANT;
	return prompt;
}

std::string LlamaTranslate::Translate(const std::string& text, const std::string& targetLang, const std::string& sourceLang)
{
	if (text.empty())
	{
		return std::string();
	}
	const std::string target = LanguageName(targetLang);
	std::string instruction;
	if (sourceLang.empty() || TOOL::EqualsNoCase(sourceLang, "auto"))
	{
		instruction = "Translate the following segment into " + target + ", without additional explanation.\n\n";
	}
	else
	{
		instruction = "Translate the following " + LanguageName(sourceLang) + " segment into " + target
		            + ", without additional explanation.\n\n";
	}
	return TranslateWithInstruction(instruction, text);
}

std::string LlamaTranslate::TranslateWithInstruction(const std::string& instruction, const std::string& text)
{
	if (text.empty() && instruction.empty())
	{
		return std::string();
	}
	if (!IsLoaded())
	{
		mLastError = "模型尚未加载，请先调用 Load()";
		fprintf(stderr, "[LlamaTranslate] %s\n", mLastError.c_str());
		return std::string();
	}

	//指令里可以写 {text} 占位符；没有占位符就把原文接在指令后面
	std::string body;
	const std::string placeholder = "{text}";
	const size_t at = instruction.find(placeholder);
	if (at != std::string::npos)
	{
		body = instruction;
		body.replace(at, placeholder.size(), text);
	}
	else
	{
		body = instruction + text;
	}

	std::lock_guard<std::mutex> lock(mMutex);
	return StripSpecialMarks(Generate(MakePrompt(body)));
}

std::string LlamaTranslate::Generate(const std::string& prompt)
{
	std::string result;
	mLastError.clear();
	if (!mCtx || !mVocab || !mSampler || !mBatch)
	{
		mLastError = "上下文无效";
		return result;
	}

	//1) 分词：BOS 已经写在 prompt 里，所以 add_special=false；parse_special=true 让 <｜hy_...｜> 变成单个特殊 token
	std::vector<llama_token> tokens(prompt.size() + 8);
	int32_t n = llama_tokenize(mVocab, prompt.c_str(), (int32_t)prompt.size(), tokens.data(),
	                           (int32_t)tokens.size(), false, true);
	if (n < 0)
	{
		tokens.resize((size_t)(-n));
		n = llama_tokenize(mVocab, prompt.c_str(), (int32_t)prompt.size(), tokens.data(),
		                   (int32_t)tokens.size(), false, true);
	}
	if (n <= 0)
	{
		mLastError = "分词失败";
		return result;
	}
	tokens.resize((size_t)n);

	const int nCtx = (int)llama_n_ctx(mCtx);
	if (n + mParams.MaxTokens > nCtx)
	{
		fprintf(stderr, "[LlamaTranslate] 警告：prompt %d token + 最多生成 %d token 超过上下文 %d，输出可能被截断\n",
		        n, mParams.MaxTokens, nCtx);
	}

	//2) 每次翻译都是全新的单轮对话：清空 KV 缓存、复位采样链
	llama_memory_clear(llama_get_memory(mCtx), true);
	llama_sampler_reset(mSampler);

	//3) 喂 prompt（超过 n_batch 就分块），只要最后一个 token 的 logits
	const int nBatch = (int)llama_n_batch(mCtx);
	llama_pos pos = 0;
	for (int32_t off = 0; off < n; )
	{
		const int32_t cnt = std::min<int32_t>(n - off, std::max(nBatch, 1));
		llama_batch_ext_clear(mBatch);
		for (int32_t i = 0; i < cnt; ++i)
		{
			const int32_t idx = llama_batch_ext_add_token(mBatch, 0, tokens[(size_t)(off + i)]);
			if (idx < 0)
			{
				mLastError = "batch 已满（prompt 太长）";
				return result;
			}
			const llama_pos p = pos + i;
			llama_batch_ext_set_pos(mBatch, idx, &p);
		}
		if (off + cnt >= n)
		{
			llama_batch_ext_set_output_logits(mBatch, cnt - 1, true);
		}
		if (llama_process(mCtx, LLAMA_PROCESS_TYPE_DECODE, mBatch) != 0)
		{
			mLastError = "解码失败（上下文长度不足或内存不足）";
			return result;
		}
		pos += cnt;
		off += cnt;
	}

	//4) 逐 token 采样
	int generated = 0;

	while (generated < mParams.MaxTokens)
	{
		//退出/重启请求了停止就别再往下算：手上这个 token 已经生成完了，
		//剩下的 MaxTokens 不再一个个算（长文本一次生成可能几十秒，退出不能等它）
		if (sStopRequested.load())
		{
			mLastError = "已请求停止生成";
			break;
		}

		const llama_token id = llama_sampler_sample(mSampler, mCtx, -1);
		llama_sampler_accept(mSampler, id);
		if (llama_vocab_is_eog(mVocab, id))
		{
			break;	//遇到 <｜hy_place▁holder▁no▁2｜> 等结束标记
		}

		char buf[256];
		int32_t np = llama_token_to_piece(mVocab, id, buf, (int32_t)sizeof(buf), 0, true);
		if (np < 0)
		{
			std::vector<char> big((size_t)(-np));
			np = llama_token_to_piece(mVocab, id, big.data(), (int32_t)big.size(), 0, true);
			if (np > 0)
			{
				result.append(big.data(), (size_t)np);
			}
		}
		else if (np > 0)
		{
			result.append(buf, (size_t)np);
		}

		//把刚生成的 token 送回模型，继续下一步
		llama_batch_ext_clear(mBatch);
		const int32_t idx = llama_batch_ext_add_token(mBatch, 0, id);
		if (idx < 0)
		{
			mLastError = "batch 已满";
			break;
		}
		const llama_pos p = pos;
		llama_batch_ext_set_pos(mBatch, idx, &p);
		llama_batch_ext_set_output_logits(mBatch, idx, true);
		if (llama_process(mCtx, LLAMA_PROCESS_TYPE_DECODE, mBatch) != 0)
		{
			mLastError = "解码失败";
			break;
		}
		pos += 1;
		++generated;
	}

	return result;
}

std::string LlamaTranslateText(const std::string& text, const std::string& targetLang)
{
	static LlamaTranslate sInstance;	//模型只加载一次，之后常驻
	if (!sInstance.IsLoaded())
	{
		if (!sInstance.Load(LlamaTranslate::Params()))
		{
			fprintf(stderr, "[LlamaTranslate] 加载模型失败：%s\n", sInstance.LastError().c_str());
			return std::string();
		}
	}
	return sInstance.Translate(text, targetLang);
}
