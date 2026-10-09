#pragma once
#include <string>
#include <vector>
#include <mutex>

// ============================================================================
//  LlamaTranslate —— 用 llama.cpp 加载本地 GGUF 大模型做翻译
//  默认模型：Modes/Hy-MT2-1.8B-Q4_K_M.gguf（腾讯混元 Hy-MT2 1.8B 翻译模型）
//  模型都放在程序目录下的 Modes 文件夹里，设置界面的「选择模型」会扫描它列出来
//
//  典型用法：
//      LlamaTranslate t;
//      if (t.Load(""))                                  // 空字符串 = 用默认模型路径
//      {
//          std::string zh = t.Translate("Hello world", "Chinese");
//          std::string en = t.Translate("你好，世界", "en");
//      }
//  或者直接用全局函数（内部单例，第一次调用时自动惰性加载模型）：
//      std::string zh = LlamaTranslateText("Hello world", "Chinese");
//
//  说明：
//   * 模型只在第一次 Load 时读入内存并常驻，同一个对象反复翻译不会重复加载；
//     不需要时调用 Unload() 释放。
//   * 每次翻译都会清空 KV 缓存，按「单轮对话」重新喂 prompt，互不影响。
//   * llama.cpp 的上下文不是线程安全的，本类内部用互斥锁保证同一时刻只有一次生成。
// ============================================================================

struct llama_model;
struct llama_context;
struct llama_vocab;
struct llama_sampler;
struct llama_batch_ext;

class LlamaTranslate
{
public:
	// 模型加载参数与采样参数
	struct Params
	{
		std::string ModelPath = "";			// GGUF 模型路径；空 = 用全局默认路径（DefaultModelPath）
		int   NGpuLayers = 0;				// 卸载到 GPU 的层数；本工程当前只编译了 CPU 后端，保持 0
		int   NCtx = 4096;					// 上下文长度（模型训练长度 262144，这里按内存占用取一个实用值）
		int   NThreads = 0;					// CPU 线程数；0 = 自动（逻辑核数）
		int   MaxTokens = 2048;				// 单次翻译最多生成多少 token
		float Temperature = 0.7f;			// 采样温度
		int   TopK = 20;					// top-k
		float TopP = 0.6f;					// top-p
		float RepeatPenalty = 1.05f;		// 重复惩罚（1.0 = 关闭）
		int   RepeatLastN = 256;			// 重复惩罚回看的 token 数
	};

	LlamaTranslate();
	~LlamaTranslate();

	// 内部持有 llama.cpp 的裸指针，禁止拷贝
	LlamaTranslate(const LlamaTranslate&) = delete;
	LlamaTranslate& operator=(const LlamaTranslate&) = delete;

	// 加载模型（重复调用会先卸载旧模型）；ModelPath 为空时用全局默认路径
	bool Load(const Params& params);
	bool Load(const std::string& modelPath);

	// 卸载模型、释放上下文与采样链
	void Unload();

	bool IsLoaded() const { return mModel != nullptr; }
	const std::string& ModelDesc() const { return mModelDesc; }	//模型描述（名字/参数量/量化方式）
	const std::string& LastError() const { return mLastError; }	//最近一次失败原因
	const Params& GetParams() const { return mParams; }

	// 翻译：TargetLang / SourceLang 支持 "zh"、"Chinese"、"中文" 之类；SourceLang 可留空或 "auto"
	std::string Translate(const std::string& text, const std::string& targetLang, const std::string& sourceLang = "");

	// 用完全自定义的指令翻译：prompt 结构为「指令 + 原文」，指令后面自己补好换行/空格
	std::string TranslateWithInstruction(const std::string& instruction, const std::string& text);

	// 语言代码/别名 → 模型认识的英文语言名（查不到就原样返回，认为调用方给的就是语言名）
	static std::string LanguageName(const std::string& lang);

	// 全局默认模型路径（相对路径会按可执行文件所在目录逐级向上查找）
	static const std::string& DefaultModelPath();
	static void SetDefaultModelPath(const std::string& path);

	// 模型文件夹名（相对程序位置）：AI 模型都放这里
	static const char* ModelsFolder();
	// 扫描模型文件夹（程序目录含逐级向上）里找到的 .gguf，返回可直接使用的相对路径，
	// 例如 "./Modes/Hy-MT2-1.8B-Q4_K_M.gguf"；同一个文件名只保留离程序最近的那份
	static std::vector<std::string> ListModelFiles();

private:
	std::string Generate(const std::string& prompt);			//跑一次完整的生成
	std::string MakePrompt(const std::string& userContent) const;	//按 Hy 模板拼 prompt
	void ApplySamplerParams();									//按当前参数重建采样链
	void ReleaseContext();										//释放 batch / sampler / context
	static std::string ResolveModelPath(const std::string& path);	//把相对路径解析成真实存在的路径

	Params				mParams;
	llama_model*		mModel = nullptr;
	llama_context*		mCtx = nullptr;
	const llama_vocab*	mVocab = nullptr;
	llama_sampler*		mSampler = nullptr;
	llama_batch_ext*	mBatch = nullptr;
	std::string			mModelDesc;
	std::string			mLastError;
	std::mutex			mMutex;		//翻译期间独占
};

// 便捷全局函数：内部单例 + 默认模型路径，第一次调用时自动加载模型
std::string LlamaTranslateText(const std::string& text, const std::string& targetLang = "Chinese");
