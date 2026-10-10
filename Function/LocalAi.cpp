#include "Translate.h"
#include "LlamaTranslate.h"
#include <chrono>

// =====================================================================================
// 本地 AI 模型翻译（llama.cpp 加载 GGUF 模型）
// -------------------------------------------------------------------------------------
// 模型推理很慢（本机 i5-4590：首次加载十几秒，之后每句 1~2 秒，长文更久），
// 所以界面走的是「后台线程 + 主循环取结果」，同步接口只留给脚本之类的调用。
// =====================================================================================
namespace
{
	//llama.cpp 的上下文只允许一个生成在跑，模型加载也不能并发，这里统一串行化
	std::mutex gAiEngineMutex;

	LlamaTranslate& AiEngine()
	{
		static LlamaTranslate sEngine;
		return sEngine;
	}

	//steady_clock 毫秒：只用来算「模型闲置了多久」，不受系统时间被调整的影响
	long long AiNowMs()
	{
		return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	//Data.ini/语言下拉框里存的是 Baidu_items 那套代码（zh、cht、jp、kor…），
	//模型只认识语言名，而且 LlamaTranslate::LanguageName() 里没有 cht/kor/yue 这几个。
	std::string AiLanguageName(const std::string& Code)
	{
		if (Code.empty() || Code == "auto") { return std::string(); }

		std::string Lower;
		Lower.reserve(Code.size());
		for (size_t i = 0; i < Code.size(); i++)
		{
			Lower += (char)tolower((unsigned char)Code[i]);
		}

		if (Lower == "cht" || Lower == "zh-cht" || Lower == "zh_cht" || Lower == "zh-tw" || Lower == "zh_tw") { return "Traditional Chinese"; }
		if (Lower == "zh-chs" || Lower == "zh_chs") { return "Chinese"; }
		if (Lower == "kor") { return "Korean"; }
		if (Lower == "yue") { return "Cantonese"; }
		if (Lower == "vie") { return "Vietnamese"; }
		if (Lower == "fra") { return "French"; }

		return LlamaTranslate::LanguageName(Code);
	}

	//按需加载模型（调用方要自己拿 gAiEngineMutex）
	bool AiLoadEngineLocked(std::string& Error)
	{
		LlamaTranslate& Engine = AiEngine();
		if (Engine.IsLoaded()) { return true; }

		LlamaTranslate::Params Params;
		Params.ModelPath = Variable::AiModelPath;
		Params.NThreads = Variable::AiThreads;
		Params.NCtx = Variable::AiNCtx;
		Params.MaxTokens = Variable::AiMaxTokens;
		Params.Temperature = Variable::AiTemperature;

		if (!Engine.Load(Params))
		{
			Error = Engine.LastError();
			return false;
		}
		return true;
	}
}

std::string Translate::Translate_LlamaByCode(const std::string& English, const std::string& TargetLangCode, const std::string& SourceLangCode)
{
	if (English.empty()) { return std::string(); }
	mAiLastUseMs = AiNowMs();	//记下使用时刻，闲置卸载靠它判断

	std::string Error;
	{
		std::lock_guard<std::mutex> Lock(gAiEngineMutex);
		if (!AiLoadEngineLocked(Error))
		{
			mAiLastError = Error;
			TOOL::logger->error("AI model load failed: {}", Error);
			return std::string();
		}
	}

	const std::string Target = AiLanguageName(TargetLangCode);
	const std::string Source = AiLanguageName(SourceLangCode);

	//Translate() 内部自己加锁，这里不要重复持有 gAiEngineMutex（Generate 可能跑很久）
	const std::string Result = AiEngine().Translate(English, Target.empty() ? "Chinese" : Target, Source);
	mAiLastError = AiEngine().LastError();
	if (Result.empty())
	{
		TOOL::logger->error("AI model translate failed: {}", mAiLastError);
	}
	return Result;
}

std::string Translate::Translate_Llama(const std::string& English)
{
	//目标语言取当前选择，源语言 0 是「自动检测」（不告诉模型源语言）
	std::string TargetCode;
	if (mTo >= 0 && mTo < (int)Variable::Baiduitems.size()) { TargetCode = Variable::Baiduitems[mTo]; }

	std::string SourceCode;
	if (mFrom > 0 && mFrom < (int)Variable::Baiduitems.size()) { SourceCode = Variable::Baiduitems[mFrom]; }

	return Translate_LlamaByCode(English, TargetCode, SourceCode);
}

bool Translate::AiBeginTranslation(const std::string& English, const std::string& TargetLangCode)
{
	if (mAiRunning.load()) { return false; }//上一个任务还没结束，这次不等它
	AiJoin();//回收已经结束的线程
	mAiLastUseMs = AiNowMs();	//「加载模型」也算一次使用，闲置倒计时从这一刻开始

	//源语言在开始时就固定下来：异步期间界面上可能已经把 From/To 改了
	std::string SourceCode;
	if (mFrom > 0 && mFrom < (int)Variable::Baiduitems.size()) { SourceCode = Variable::Baiduitems[mFrom]; }

	const std::string Text = English;
	const std::string Target = TargetLangCode;

	mAiResult.clear();
	mAiLastError.clear();
	mAiStage = AiStageLoading;
	mAiDone = false;
	mAiRunning = true;

	mAiThread = std::thread([this, Text, Target, SourceCode]() {
		std::string Error;
		bool Loaded = false;
		{
			std::lock_guard<std::mutex> Lock(gAiEngineMutex);
			Loaded = AiLoadEngineLocked(Error);
		}

		if (!Loaded)
		{
			mAiLastError = Error;
			TOOL::logger->warn("AI model load failed (async): {}", Error);
		}
		else if (!Text.empty())
		{
			mAiStage = AiStageGenerating;
			mAiResult = Translate_LlamaByCode(Text, Target, SourceCode);
			if (mAiResult.empty()) { TOOL::logger->warn("AI translate empty (async, err={})", mAiLastError); }
		}

		mAiStage = AiStageIdle;
		mAiDone = true;
		mAiRunning = false;
	});
	return true;
}

bool Translate::AiBeginLoad()
{
	return AiBeginTranslation(std::string(), std::string());
}

bool Translate::AiTakeResult(std::string& Result)
{
	if (!mAiDone.load()) { return false; }
	AiJoin();
	Result = mAiResult;
	mAiDone = false;
	return true;
}

bool Translate::AiModelLoaded() const
{
	std::lock_guard<std::mutex> Lock(gAiEngineMutex);
	return AiEngine().IsLoaded();
}

std::string Translate::AiModelDesc() const
{
	std::lock_guard<std::mutex> Lock(gAiEngineMutex);
	return AiEngine().ModelDesc();
}

void Translate::AiUnloadModel()
{
	//正在翻译时先不卸载：卸载要和生成抢同一份上下文，等任务结束再点一次即可
	if (mAiRunning.load()) { return; }
	if (mWebRunning.load() || mSyncBusy.load() > 0) { return; }	//后台线程/脚本里的同步翻译可能正在用同一个引擎，等它跑完
	AiJoin();

	std::lock_guard<std::mutex> Lock(gAiEngineMutex);
	AiEngine().Unload();
}

void Translate::AiJoin()
{
	//只有线程已经跑完（mAiRunning 置回 false）才 join，否则会把界面卡住
	if (mAiThread.joinable() && !mAiRunning.load())
	{
		mAiThread.join();
	}
}

void Translate::AiPollIdle()
{
	const int IdleSeconds = Variable::AiIdleUnload;
	if (IdleSeconds <= 0) { return; }	//没开启这个功能
	if (mAiRunning.load()) { return; }	//正在加载/翻译，不能卸
	if (mWebRunning.load() || mSyncBusy.load() > 0) { return; }	//后台线程/脚本里的同步翻译可能正在用同一个引擎

	if (!AiModelLoaded())
	{
		//没加载就顺手把计时归零，免得刚加载完就被上一段闲置时间算超时
		mAiLastUseMs = AiNowMs();
		return;
	}

	const long long ElapsedMs = AiNowMs() - mAiLastUseMs;
	if (ElapsedMs >= (long long)IdleSeconds * 1000)
	{
		//这里在主线程：llama_model_free 映射的模型释放很快，最多卡一下
		TOOL::logger->info("AI model unloaded after {} seconds idle", IdleSeconds);
		AiUnloadModel();
		mAiLastUseMs = AiNowMs();
	}
}

int Translate::AiIdleRemaining() const
{
	const int IdleSeconds = Variable::AiIdleUnload;
	if (IdleSeconds <= 0 || mAiRunning.load() || !AiModelLoaded()) { return -1; }

	const int Left = IdleSeconds - (int)((AiNowMs() - mAiLastUseMs) / 1000);
	return Left > 0 ? Left : 0;
}

std::string Translate::DefaultAiModelPath()
{
	return LlamaTranslate::DefaultModelPath();
}

const char* Translate::AiModelsFolder()
{
	return LlamaTranslate::ModelsFolder();
}

std::vector<std::string> Translate::AiModelFiles()
{
	return LlamaTranslate::ListModelFiles();
}