#include "Translate.h"
#include "../Tool/Log.h"//TOOL::LogStep：退出路径路标

// =====================================================================================
// Translate 类骨架 + 后台线程编排
// -------------------------------------------------------------------------------------
// 具体职责拆分到：
//   · Function/Crypto.cpp       —— MD5 / base64 / AES（CNG 实现）
//   · Function/HttpTranslate.cpp—— 百度/爬虫/有道 的 HTTP 翻译
//   · Function/LocalAi.cpp      —— 本地 AI 模型（llama.cpp）翻译
//   · Tool/UrlCodec.cpp         —— URL 编解码
// 本文件只保留构造函数、析构函数，以及普通翻译源（百度/爬虫/有道）的后台线程编排。
// =====================================================================================

Translate::Translate()
{   
    //现在翻译都跑在后台线程里，这里先在主线程把 curl 初始化好：
    //libcurl 的隐式初始化（curl_easy_init 内部那次）不是线程安全的。
    curl_global_init(CURL_GLOBAL_ALL);

    Baidu_items = new const char* [Variable::Baiduitems.size()];
    for (int i = 0; i < Variable::Baiduitems.size(); i++)
    {
        Baidu_items[i] = Variable::Baiduitems[i].c_str();
    }
    Youdao_items = new const char* [Variable::Youdaoitems.size()];
    for (int i = 0; i < Variable::Youdaoitems.size(); i++)
    {
        Youdao_items[i] = Variable::Youdaoitems[i].c_str();
    }
}

Translate::~Translate()
{
	//后台翻译线程可能还在用 this（它每次进入 Generate 都会读成员），
	//所以析构必须等它结束，不能直接 detach。
	//这里传 true（强制等待）而不是原来的默认参数：线程还在跑时跳过 join 的话，
	//等成员 mAiThread/mWebThread 析构时它还是 joinable，std::thread 的析构函数会
	//直接 std::terminate() → abort()——这就是「退出时 ucrtbase.dll / 0xc0000409」的来源。
	TOOL::LogStep("~Translate: 开始");
	AiJoin(true);
	WebJoin(true);
	TOOL::LogStep("~Translate: 线程已等待结束");
}

// =====================================================================================
// 普通翻译源（百度/爬虫/有道）的后台线程
// -------------------------------------------------------------------------------------
//以前是在主循环里同步发 HTTP：请求没回来这一帧就画不出来，界面像卡住一样。
//现在和 AI 那条路一样「后台算 + 主循环每帧取结果」，窗口一按就出来（先显示「翻译中…」）。
// =====================================================================================
bool Translate::WebBeginTranslation(const std::string& English, int EngineIndex, int FromIndex, int ToIndex)
{
	if (mWebRunning.load()) { return false; }	//上一次还没回来，这次不等它
	WebJoin();									//回收上一次已经结束的线程

	mWebDone = false;
	mWebResult.clear();
	mWebRunning = true;
	//引擎和语言在这里就复制进线程：主线程后面再改 mTranslate/mFrom/mTo 都不影响这次结果
	mWebThread = std::thread([this, English, EngineIndex, FromIndex, ToIndex]() {
		std::string Result;
		try
		{
			Result = TranslateAPIIndexed(English, EngineIndex, FromIndex, ToIndex);
		}
		catch (...)
		{
			Result.clear();
		}
		mWebResult = Result;
		mWebRunning = false;
		mWebDone = true;
	});
	return true;
}

bool Translate::WebTakeResult(std::string& Result)
{
	if (!mWebDone.load()) { return false; }
	WebJoin();					//等线程真的退出再读 mWebResult
	Result = mWebResult;
	mWebResult.clear();
	mWebDone = false;
	return true;
}

void Translate::WebJoin(bool Force)
{
	//还在跑的时候不能 join（会把主循环卡住），留给下一次 WebTakeResult()；
	//析构/退出时（Force=true）必须等到线程真的结束。
	if (mWebThread.joinable() && (Force || !mWebRunning.load())) { mWebThread.join(); }
}

// =====================================================================================
// 脚本里的 TranslateAPI()：后台翻译 + 挂起脚本
// =====================================================================================
// 脚本里的 TranslateAPI() 直接走上面的同步实现：AngelScript 的 context->Suspend() 不会
// 重新调用被挂起的系统函数（原因见 Translate.h 里的说明），所以「脚本 + 后台线程」这条路
// 改成把整个脚本丢到后台线程里跑（AngelScriptCode::BeginRun），主循环不再被脚本占住。
// =====================================================================================