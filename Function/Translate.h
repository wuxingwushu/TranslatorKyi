#pragma once
#include "../base.h"
#include <json.h>
#include <curl/curl.h>
#include <assert.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

class Translate
{
public:
	Translate();
	~Translate();

	void SetBaiduAppID(const char* appid) { mBaiduAppid = appid; }
	void SetBaiduSecretkey(const char* secret_key) { mBaiduSecret_key = secret_key;}

	void SetYoudaoAppID(const char* appid) { mYoudaoAppid = appid; }
	void SetYoudaoSecretkey(const char* secret_key) { mYoudaoSecret_key = secret_key; }

	void SetFrom(int from) { mFrom = from; }
	void SetTo(int to) { mTo = to; }
	//配置里存了范围外的值（人手改 Data.ini）时退回百度，免得下面用 TranslateName[mTranslate] 越界
	void SetTranslate(int translate) { mTranslate = (translate >= 0 && translate <= AiTranslate) ? translate : 0; }

	std::string TranslateAPI(std::string English) {
		//同步翻译期间置位：AI 引擎的闲置卸载要避开这个窗口，别把正在用的模型卸掉
		mSyncBusy.fetch_add(1);
		std::string Result;
		if (mTranslate == AiTranslate) { Result = Translate_Llama(English); }
		else { Result = TranslateAPIIndexed(English, mTranslate, mFrom, mTo); }
		mSyncBusy.fetch_sub(1);
		return Result;
	}

	//后台线程用的翻译入口：引擎和语言在调用那一刻就定死（值拷贝进 lambda），
	//免得主线程随后又改 mTranslate/mFrom/mTo（比如 Ctrl+Alt+R 会临时换目标语言）影响这次结果。
	std::string TranslateAPIIndexed(const std::string& English, int EngineIndex, int FromIndex, int ToIndex) {
		switch (EngineIndex)
		{
		case 0:
			return Translate_Baidu(English, FromIndex, ToIndex);
		case 1:
			return Translate_ReptilesYoudao(English);
		case 2:
			return Translate_Youdao(English, FromIndex, ToIndex);
		default:
			break;
		}
		return std::string();
	}

	//翻译方式：0 百度、1 爬虫（有道网页）、2 有道、3 本地 AI 模型（llama.cpp）
	static const int AiTranslate = 3;

	bool IsAiTranslate() const { return mTranslate == AiTranslate; }

	const char* TranslateName[4] = {"百度","爬虫","有道","AI模型" };

	const char** Baidu_items;
	const char** Youdao_items;

	int mFrom;
	int mTo;

	int mTranslate = 0;

	// ================= 普通翻译源（百度/爬虫/有道）的后台线程 =================
	//以前这三个源是在主循环里同步发 HTTP 的：请求没回来这一帧就画不出来，界面看着像卡住。
	//现在统一丢到后台线程，窗口一按就出来（先显示「翻译中…」），结果由主循环取回来再更新界面。
	bool WebBeginTranslation(const std::string& English, int EngineIndex, int FromIndex, int ToIndex);
	//取回后台结果：返回 true 表示任务已结束（Result 可能是空串，说明失败）
	bool WebTakeResult(std::string& Result);
	bool WebRunning() const { return mWebRunning.load(); }
	//等后台线程结束（析构/退出时调用，避免线程还在用 this）
	void WebJoin();

	// ================= 本地 AI 模型翻译（llama.cpp + GGUF 模型） =================
	//模型不是翻译接口，一秒钟只能算十几个 token，所以有两套入口：
	//  · Translate_Llama()：同步，界面会卡住（只给脚本/其它同步调用用）
	//  · AiBeginTranslation() + AiTakeResult()：后台线程，主循环每帧来取结果
	//模型按需加载，参数取自设置界面（Variable::AiModelPath 等）。

	//同步翻译（内部会按需加载模型）
	std::string Translate_Llama(const std::string& English);
	//同步翻译：显式指定目标/源语言代码（Data.ini 里 Baidu_items 那种代码，如 zh、cht、jp）
	std::string Translate_LlamaByCode(const std::string& English, const std::string& TargetLangCode, const std::string& SourceLangCode);

	//开始一次后台翻译。已有任务在跑时返回 false（不会排队）。
	bool AiBeginTranslation(const std::string& English, const std::string& TargetLangCode);
	//后台只加载模型，不翻译（设置界面里的「加载模型」）
	bool AiBeginLoad();
	//取回后台任务的结果：返回 true 表示任务已结束（Result 可能是空串，说明失败）
	bool AiTakeResult(std::string& Result);
	bool AiRunning() const { return mAiRunning.load(); }
	bool AiModelLoaded() const;
	std::string AiModelDesc() const;
	const std::string& AiLastError() const { return mAiLastError; }
	void AiUnloadModel();
	//主循环每帧调用：模型闲置超过 Variable::AiIdleUnload 秒就自动卸载（0 = 不自动卸载）
	void AiPollIdle();
	//还有多少秒自动卸载模型：未加载 / 没开启 / 正在忙时返回 -1（设置界面用它显示倒计时）
	int AiIdleRemaining() const;
	//等后台线程结束（析构/退出时调用，避免线程还在用 this）
	void AiJoin();

	// ================= 脚本里的 TranslateAPI() =================
	//AngelScript 的 context->Suspend() 不会重新调用被挂起的系统函数：as_context.cpp 的
	//asBC_CALLSYS 分支是在系统函数返回之后才检查挂起标志、那时程序指针已经越过这条指令，
	//返回槽里留下的就是挂起前写的那个值（空串）—— 脚本会拿着空串继续跑。
	//所以脚本翻译不能靠「挂起等后台结果」，现在的做法是：整个脚本丢到后台线程里跑
	//（AngelScriptCode::BeginRun()），TranslateAPI() 保持同步，主循环不再被脚本占住。

	//后台任务阶段：0 空闲、1 正在加载模型、2 正在生成
	static const int AiStageIdle = 0;
	static const int AiStageLoading = 1;
	static const int AiStageGenerating = 2;
	int AiStage() const { return mAiStage.load(); }

	//默认模型路径（相对程序位置；设置界面里的「恢复默认」用）
	static std::string DefaultAiModelPath();
	//模型文件夹名（Modes）；扫描它列出手上有的模型给设置界面选
	static const char* AiModelsFolder();
	static std::vector<std::string> AiModelFiles();

private:
	const char* mBaiduAppid;
	const char* mBaiduSecret_key;

	const char* mYoudaoAppid;
	const char* mYoudaoSecret_key;

	std::thread mWebThread;
	std::atomic<bool> mWebRunning{ false };
	std::atomic<bool> mWebDone{ false };
	std::string mWebResult;
	std::atomic<int> mSyncBusy{ 0 };//同步翻译（脚本/替换模式）正在跑的次数

	std::thread mAiThread;
	std::atomic<bool> mAiRunning{ false };
	std::atomic<bool> mAiDone{ false };
	std::atomic<int> mAiStage{ 0 };
	std::string mAiResult;
	std::string mAiLastError;
	//最近一次「使用」（开始翻译 / 加载模型）的时刻，steady_clock 毫秒，用来算闲置了多久
	long long mAiLastUseMs = 0;



	unsigned char ToHex(unsigned char x);

	unsigned char FromHex(unsigned char x);

	std::string UrlEncode(const std::string& str);

	std::string UrlDecode(const std::string& str);

	std::string Translate_Baidu(std::string English, int FromIndex, int ToIndex);

	std::string Translate_Youdao(std::string English, int FromIndex, int ToIndex);

	std::string Translate_ReptilesYoudao(std::string English);
};

std::string translate(const std::string& sentence, const std::string& fromLang = "auto", const std::string& toLang = "");
