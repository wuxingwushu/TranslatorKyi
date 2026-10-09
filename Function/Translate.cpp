#include "Translate.h"
#include "LlamaTranslate.h"
#include <chrono>

// ---------------------------------------------------------------------------
// 这里原来用的是 OpenSSL，现在改成 Windows 自带的 CNG(BCrypt) / Crypt32 实现，
// 工程因此不再依赖 OpenSSL。用到的算法与原来一一对应：
//   MD5               -> BCrypt(BCRYPT_MD5_ALGORITHM)
//   base64 编解码      -> CryptBinaryToStringA / CryptStringToBinaryA
//   AES-128-ECB 加解密 -> BCrypt(BCRYPT_AES_ALGORITHM, ECB 链模式)
// 需要链接 bcrypt.lib 与 crypt32.lib（已在根 CMakeLists.txt 里加好）。
// ---------------------------------------------------------------------------
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

// 用 CNG 计算 MD5 摘要（16 字节）
static bool tk_md5(const unsigned char* data, size_t length, unsigned char digest[16]) {
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_HASH_HANDLE hHash = nullptr;
	DWORD cbObject = 0, cbHash = 0, cbDummy = 0;
	bool ok = false;

	if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, nullptr, 0) != 0)
		return false;

	if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&cbObject), sizeof(cbObject), &cbDummy, 0) == 0 &&
		BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&cbHash), sizeof(cbHash), &cbDummy, 0) == 0 &&
		cbHash == 16) {
		std::vector<unsigned char> object(cbObject);
		if (BCryptCreateHash(hAlg, &hHash, object.data(), cbObject, nullptr, 0, 0) == 0) {
			if (BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(length), 0) == 0 &&
				BCryptFinishHash(hHash, digest, 16, 0) == 0)
				ok = true;
			BCryptDestroyHash(hHash);
		}
	}

	BCryptCloseAlgorithmProvider(hAlg, 0);
	return ok;
}

// AES-128-ECB 逐块加解密（与原来 OpenSSL 低层 AES_encrypt/AES_decrypt 的行为一致：
// 不做任何填充，由调用方自己处理 PKCS#7；数据长度必须是 16 的整数倍）
static bool tk_aes_ecb(bool encrypt, const unsigned char* data, size_t length,
	const std::string& key, std::string& output) {
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_KEY_HANDLE hKey = nullptr;
	DWORD cbObject = 0, cbDummy = 0, cbDone = 0;
	bool ok = false;

	if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0)
		return false;

	if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB,
		sizeof(BCRYPT_CHAIN_MODE_ECB), 0) == 0 &&
		BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&cbObject), sizeof(cbObject), &cbDummy, 0) == 0) {

		// 与 AES_set_encrypt_key(key, 128, ...) 一致：只取 key 的前 16 字节
		std::string raw_key = key.substr(0, 16);
		raw_key.append(16 - raw_key.size(), '\0');

		std::vector<unsigned char> object(cbObject);
		if (BCryptGenerateSymmetricKey(hAlg, &hKey, object.data(), cbObject,
			reinterpret_cast<PUCHAR>(const_cast<char*>(raw_key.data())), static_cast<ULONG>(raw_key.size()), 0) == 0) {

			output.assign(length, '\0');
			NTSTATUS status = encrypt
				? BCryptEncrypt(hKey, const_cast<PUCHAR>(data), static_cast<ULONG>(length), nullptr, nullptr, 0,
					reinterpret_cast<PUCHAR>(&output[0]), static_cast<ULONG>(length), &cbDone, 0)
				: BCryptDecrypt(hKey, const_cast<PUCHAR>(data), static_cast<ULONG>(length), nullptr, nullptr, 0,
					reinterpret_cast<PUCHAR>(&output[0]), static_cast<ULONG>(length), &cbDone, 0);
			if (status == 0) {
				output.resize(cbDone);
				ok = true;
			}
			BCryptDestroyKey(hKey);
		}
	}

	BCryptCloseAlgorithmProvider(hAlg, 0);
	return ok;
}


size_t TranslateWrite_data(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* str = (std::string*)userdata;
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

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
	AiJoin();
	WebJoin();
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

void Translate::WebJoin()
{
	//还在跑的时候不能 join（会把主循环卡住），留给下一次 WebTakeResult()
	if (mWebThread.joinable() && !mWebRunning.load()) { mWebThread.join(); }
}

// =====================================================================================
// 脚本里的 TranslateAPI()：后台翻译 + 挂起脚本
// -------------------------------------------------------------------------------------
// =====================================================================================
// 脚本里的 TranslateAPI() 直接走上面的同步实现：AngelScript 的 context->Suspend() 不会
// 重新调用被挂起的系统函数（原因见 Translate.h 里的说明），所以「脚本 + 后台线程」这条路
// 改成把整个脚本丢到后台线程里跑（AngelScriptCode::BeginRun），主循环不再被脚本占住。
// =====================================================================================

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
	if (Result.empty() && !mAiLastError.empty())
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


unsigned char Translate::ToHex(unsigned char x)
{
    return  x > 9 ? x + 55 : x + 48;
}

unsigned char Translate::FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}



std::string Translate::UrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (isalnum((unsigned char)str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~'))
            strTemp += str[i];
        else if (str[i] == ' ')
            strTemp += "+";
        else
        {
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] % 16);
        }
    }
    return strTemp;
}


std::string Translate::UrlDecode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (str[i] == '+') strTemp += ' ';
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high * 16 + low;
        }
        else strTemp += str[i];
    }
    return strTemp;
}



//（ 详细详细查看百度翻译API文档：https ://fanyi-api.baidu.com/product/113 ）
std::string Translate::Translate_Baidu(std::string English, int FromIndex, int ToIndex) {
    //语言索引夹一下：后台线程拿到的索引是从主线程复制过来的，越界会读坏 Baidu_items
    const int From = (FromIndex >= 0 && FromIndex < (int)Variable::Baiduitems.size()) ? FromIndex : 0;
    const int To = (ToIndex >= 0 && ToIndex < (int)Variable::Baiduitems.size()) ? ToIndex : 0;

    //appid             //将myAppid替换为您自己的appid
    //secret_key        //将mySecretKey替换为您自己的mySecretKey
    //English           //将apple替换为您自己要翻译的文本，确保输入文本使用UTF-8编码！
    //from;             //用您自己的语言类型的输入文本替换en
    //to;               //用您自己语言类型的输出文本替换zh

    //不存在单词取消翻译
    if (strlen(English.c_str()) <= 1) {
        return std::string(u8"No Words Present");
    }


    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        char myurl[100000] = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
        
        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[100000] = "";
        strcat(sign, mBaiduAppid);
        strcat(sign, English.c_str());//获取加密MD5时 English 不要进行 Url_Encode 处理  （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(sign, salt);
        strcat(sign, mBaiduSecret_key);
        unsigned char md[16];
        int i;
        char tmp[3] = { '\0' }, buf[33] = { '\0' };
        tk_md5((const unsigned char*)sign, strlen(sign), md);
        for (i = 0; i < 16; i++) {
            sprintf(tmp, "%2.2x", md[i]);
            strcat(buf, tmp);
        }
        //printf("%s\n", buf);
        strcat(myurl, "appid=");
        strcat(myurl, mBaiduAppid);
        strcat(myurl, "&q=");
        strcat(myurl, UrlEncode(English).c_str());//生成网页链接时 English 才要进行 Url_Encode 处理   （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(myurl, "&from=");
        strcat(myurl, Baidu_items[From]);
        strcat(myurl, "&to=");
        strcat(myurl, Baidu_items[To]);
        strcat(myurl, "&salt=");
        strcat(myurl, salt);
        strcat(myurl, "&sign=");
        strcat(myurl, buf);
        //printf("%s\n", myurl);
        //设置访问的地址
        curl_easy_setopt(curl, CURLOPT_URL, &myurl);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TranslateWrite_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return "错误";
        }

        if (readBuffer.size() == 0) {
            return "错误";
        }
        Json::Value value;
        Json::Reader reader;
        if (!reader.parse(readBuffer, value)) {
            printf("parse json error!");
            return "错误";
        }
        std::string Chinese = value["trans_result"][0]["dst"].asString();
        return Chinese;
    }
    return "错误";
}

std::string Translate::Translate_Youdao(std::string English, int FromIndex, int ToIndex)
{
    //语言索引夹一下：后台线程拿到的索引是从主线程复制过来的，越界会读坏 Youdao_items
    const int From = (FromIndex >= 0 && FromIndex < (int)Variable::Youdaoitems.size()) ? FromIndex : 0;
    const int To = (ToIndex >= 0 && ToIndex < (int)Variable::Youdaoitems.size()) ? ToIndex : 0;

    //appid             //将myAppid替换为您自己的appid
    //secret_key        //将mySecretKey替换为您自己的mySecretKey
    //English           //将apple替换为您自己要翻译的文本，确保输入文本使用UTF-8编码！
    //from;             //用您自己的语言类型的输入文本替换en
    //to;               //用您自己语言类型的输出文本替换zh


    //不存在单词取消翻译
    if (strlen(English.c_str()) <= 1) {
        return std::string(u8"No Words Present");
    }

    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        char myurl[100000] = "http://openapi.youdao.com/api?";

        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[100000] = "";
        strcat(sign, mYoudaoAppid);
        strcat(sign, English.c_str());//获取加密MD5时 English 不要进行 Url_Encode 处理  （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(sign, salt);
        strcat(sign, mYoudaoSecret_key);
        unsigned char md[16];
        int i;
        char tmp[3] = { '\0' }, buf[33] = { '\0' };
        tk_md5((const unsigned char*)sign, strlen(sign), md);
        for (i = 0; i < 16; i++) {
            sprintf(tmp, "%2.2x", md[i]);
            strcat(buf, tmp);
        }
        //printf("%s\n", buf);
        strcat(myurl, "appKey=");
        strcat(myurl, mYoudaoAppid);
        strcat(myurl, "&q=");
        strcat(myurl, UrlEncode(English).c_str());//生成网页链接时 English 才要进行 Url_Encode 处理   （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(myurl, "&from=");
        strcat(myurl, Youdao_items[From]);
        strcat(myurl, "&to=");
        strcat(myurl, Youdao_items[To]);
        strcat(myurl, "&sign=");
        strcat(myurl, buf);
        strcat(myurl, "&salt=");
        strcat(myurl, salt);
        //printf("%s\n", myurl);
        //设置访问的地址
        curl_easy_setopt(curl, CURLOPT_URL, &myurl);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TranslateWrite_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return "错误";
        }

        if (readBuffer.size() == 0) {
            return "错误";
        }
        Json::Value value;
        Json::Reader reader;
        if (!reader.parse(readBuffer, value)) {
            printf("parse json error!");
            return "错误";
        }
        std::string Chinese = value["web"][0]["value"][0].asString();
        return Chinese;
    }
    return "错误";
}

// 回调函数处理服务器响应
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

std::string Translate::Translate_ReptilesYoudao(std::string English) {
    std::string url = "https://dict.youdao.com/webtranslate?&doctype=json&type=AUTO&to=AUTO&i=" + UrlEncode(English);/*&to=ja*/
    std::string result;

    // 初始化 libcurl
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();

    if (curl) {
        // 设置 URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // 设置回调函数和缓冲区
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

        // 设置User-Agent头字段
        std::string userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36 Edg/115.0.1901.203";
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

        // 发起请求
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        // 清理资源
        curl_easy_cleanup(curl);
    }

    // 清理 libcurl
    curl_global_cleanup();

    if (result.size() == 0) {
        return "错误";
    }
    Json::Value value;
    Json::Reader reader;
    if (!reader.parse(result, value)) {
        printf("parse json error!");
        return "错误";
    }
    std::string Chinese = value["translateResult"][0][0]["tgt"].asString();
    return Chinese;
}





//std::string iv = "ydsecret://query/iv/C@lZe2YzHtZ2CYgaXKSVfsb7Y4QWHjITPPZ0nQp87fBeJ!Iv6v^6fvi2WN@bYpJ4";
//std::string key = "ydsecret://query/key/B*RGygVywfNBwpmBaZg*WT7SIOUP2T0C9WHMZN39j^DAdaZhAnxvGcCY6VYFwnHl";
//
//std::string md5(const std::string& input) {
//    unsigned char hash[MD5_DIGEST_LENGTH];
//    MD5(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);
//    std::string result;
//    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
//        result += hash[i];
//    }
//    return result;
//}
//
//std::string aesEncrypt(const std::string& plaintext) {
//    AES_KEY aesKey;
//    AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(key.c_str()), 128, &aesKey);
//
//    unsigned char ivBytes[AES_BLOCK_SIZE];
//    memcpy(ivBytes, iv.c_str(), AES_BLOCK_SIZE);
//
//    int inputLength = plaintext.length();
//    int paddingLength = AES_BLOCK_SIZE - (inputLength % AES_BLOCK_SIZE);
//    int outputLength = inputLength + paddingLength;
//
//    unsigned char* inputBuffer = new unsigned char[outputLength];
//    memset(inputBuffer, 0, outputLength);
//    memcpy(inputBuffer, plaintext.c_str(), inputLength);
//    for (int i = inputLength; i < outputLength; ++i) {
//        inputBuffer[i] = paddingLength;
//    }
//
//    unsigned char* outputBuffer = new unsigned char[outputLength];
//    memset(outputBuffer, 0, outputLength);
//
//    for (int i = 0; i < outputLength; i += AES_BLOCK_SIZE) {
//        AES_cbc_encrypt(inputBuffer + i, outputBuffer + i, AES_BLOCK_SIZE, &aesKey, ivBytes, AES_ENCRYPT);
//    }
//
//    std::string encryptedText(reinterpret_cast<char*>(outputBuffer), outputLength);
//
//    delete[] inputBuffer;
//    delete[] outputBuffer;
//
//    return encryptedText;
//}
//
//std::string aesDecrypt(const std::string& ciphertext) {
//    AES_KEY aesKey;
//    AES_set_decrypt_key(reinterpret_cast<const unsigned char*>(key.c_str()), 128, &aesKey);
//
//    unsigned char ivBytes[AES_BLOCK_SIZE];
//    memcpy(ivBytes, iv.c_str(), AES_BLOCK_SIZE);
//
//    int inputLength = ciphertext.length();
//    int outputLength = inputLength;
//
//    unsigned char* inputBuffer = new unsigned char[inputLength];
//    memcpy(inputBuffer, ciphertext.c_str(), inputLength);
//
//    unsigned char* outputBuffer = new unsigned char[outputLength];
//    memset(outputBuffer, 0, outputLength);
//
//    for (int i = 0; i < outputLength; i += AES_BLOCK_SIZE) {
//        AES_cbc_encrypt(inputBuffer + i, outputBuffer + i, AES_BLOCK_SIZE, &aesKey, ivBytes, AES_DECRYPT);
//    }
//
//    // Remove padding
//    int paddingLength = outputBuffer[outputLength - 1];
//    std::string decryptedText(reinterpret_cast<char*>(outputBuffer), outputLength - paddingLength);
//
//    delete[] inputBuffer;
//    delete[] outputBuffer;
//
//    return decryptedText;
//}
//
//std::string getFormData(const std::string& sentence, const std::string& fromLang, const std::string& toLang) {
//    std::time_t t = std::time(nullptr);
//    std::string mysticTime = std::to_string(t);
//
//    std::string query = "client=fanyideskweb&mysticTime=" + mysticTime + "&product=webfanyi&key=fsdsogkndfokasodnaso";
//    std::string sign = md5(query);
//
//    std::string formData = "i=" + sentence + "&from=" + fromLang + "&to=" + toLang +
//        "&domain=0&dictResult=true&keyid=webfanyi&sign=" + sign +
//        "&client=fanyideskweb&product=webfanyi&appVersion=1.0.0&vendor=web" +
//        "&pointParam=client,mysticTime,product" + "&mysticTime=" + mysticTime + "&keyfrom=fanyi.web";
//
//    return formData;
//}
//
//size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
//    size_t totalSize = size * nmemb;
//    output->append(static_cast<char*>(contents), totalSize);
//    return totalSize;
//}
//
//std::string translate(const std::string& sentence, const std::string& fromLang, const std::string& toLang) {
//    std::string url = "https://dict.youdao.com/webtranslate";
//    std::string headers = "user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36\r\n"
//        "referer: https://fanyi.youdao.com/\r\n"
//        "cookie: OUTFOX_SEARCH_USER_ID=-805044645@10.112.57.88; OUTFOX_SEARCH_USER_ID_NCOO=818822109.5585971;\r\n";
//
//    std::string formData = getFormData(sentence, fromLang, toLang);
//    std::string encryptedData = aesEncrypt(formData);
//
//    CURL* curl = curl_easy_init();
//    if (curl) {
//        std::string response;
//        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, encryptedData.c_str());
//        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, encryptedData.length());
//        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
//        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
//        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.c_str());
//
//        CURLcode res = curl_easy_perform(curl);
//        curl_easy_cleanup(curl);
//
//        if (res == CURLE_OK) {
//            std::string decryptedResponse = aesDecrypt(response);
//
//            Json::Value root;
//            Json::Reader reader;
//            bool success = reader.parse(decryptedResponse, root);
//            if (success) {
//                std::string tgt = root["translateResult"][0][0]["tgt"].asString();
//                return tgt;
//            }
//        }
//    }
//
//    return "翻译失败：" + sentence;
//}

static const size_t AES_BLOCK_SIZE = 16;   // 原来是 openssl/aes.h 里的宏

std::string md5(const std::string& input) {
    unsigned char digest[16] = { 0 };
    tk_md5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);

    char md5string[33] = { 0 };
    for (int i = 0; i < 16; i++)
        sprintf(&md5string[i * 2], "%02x", (unsigned int)digest[i]);

    return std::string(md5string);
}

std::string base64_encode(const unsigned char* input, size_t length) {
    DWORD cch = 0;
    if (!CryptBinaryToStringA(input, static_cast<DWORD>(length),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &cch))
        return std::string();

    std::string buffer(cch, '\0');
    DWORD written = cch;
    if (!CryptBinaryToStringA(input, static_cast<DWORD>(length),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &buffer[0], &written))
        return std::string();

    // 不同版本的 CryptBinaryToStringA 对「结尾的 '\0' 是否计入 written」不一致，
    // 这里按实际内容裁剪；否则会把 base64 结尾的 '=' 也一起裁掉，导致解不回来。
    if (written > 0 && written <= buffer.size() && buffer[written - 1] == '\0')
        --written;
    buffer.resize(written <= buffer.size() ? written : buffer.size());
    return buffer;
}

std::string base64_decode(const std::string& input) {
    DWORD cb = 0;
    if (!CryptStringToBinaryA(input.c_str(), static_cast<DWORD>(input.size()),
        CRYPT_STRING_BASE64, nullptr, &cb, nullptr, nullptr))
        return std::string();

    std::string buffer(cb, '\0');
    if (!CryptStringToBinaryA(input.c_str(), static_cast<DWORD>(input.size()),
        CRYPT_STRING_BASE64, reinterpret_cast<BYTE*>(&buffer[0]), &cb, nullptr, nullptr))
        return std::string();

    buffer.resize(cb);
    return buffer;
}

std::string aes_encrypt(const std::string& plaintext, const std::string& key, const std::string& iv) {
    (void)iv;   // 与原来一致：ECB 模式不使用 IV

    int padding = AES_BLOCK_SIZE - (plaintext.length() % AES_BLOCK_SIZE);
    std::string padded_plaintext = plaintext + std::string(padding, static_cast<char>(padding));

    std::string ciphertext;
    if (!tk_aes_ecb(true, reinterpret_cast<const unsigned char*>(padded_plaintext.data()),
        padded_plaintext.length(), key, ciphertext))
        return std::string();

    return base64_encode(reinterpret_cast<const unsigned char*>(ciphertext.data()), ciphertext.length());
}

std::string aes_decrypt(const std::string& ciphertext, const std::string& key, const std::string& iv) {
    (void)iv;

    std::string decoded_ciphertext = base64_decode(ciphertext);
    if (decoded_ciphertext.empty() || decoded_ciphertext.length() % AES_BLOCK_SIZE != 0)
        return std::string();

    std::string plaintext;
    if (!tk_aes_ecb(false, reinterpret_cast<const unsigned char*>(decoded_ciphertext.data()),
        decoded_ciphertext.length(), key, plaintext))
        return std::string();

    size_t padding = static_cast<size_t>(static_cast<unsigned char>(plaintext[plaintext.length() - 1]));
    if (padding == 0 || padding > AES_BLOCK_SIZE || padding > plaintext.length())
        return plaintext;
    return plaintext.substr(0, plaintext.length() - padding);
}

std::string get_form_data(const std::string& sentence, const std::string& from_lang, const std::string& to_lang) {
    time_t current_time = time(nullptr);
    std::ostringstream mystic_time_stream;
    mystic_time_stream << std::fixed << std::setprecision(6) << std::setfill('0') << current_time;

    std::string t = mystic_time_stream.str();
    std::string key = "fsdsogkndfokasodnaso";

    std::string sign = md5("client=fanyideskweb&mysticTime=" + t + "&product=webfanyi&key=" + key);

    std::string form_data = "";
    form_data += "i=" + sentence + "&";
    form_data += "from=" + from_lang + "&";
    form_data += "to=" + to_lang + "&";
    form_data += "domain=0&";
    form_data += "dictResult=true&";
    form_data += "keyid=webfanyi&";
    form_data += "sign=" + sign + "&";
    form_data += "client=fanyideskweb&";
    form_data += "product=webfanyi&";
    form_data += "appVersion=1.0.0&";
    form_data += "vendor=web&";
    form_data += "pointParam=client,mysticTime,product&";
    form_data += "mysticTime=" + t + "&";
    form_data += "keyfrom=fanyi.web";

    return form_data;
}

std::string translate(const std::string& sentence, const std::string& from_lang, const std::string& to_lang) {
    std::string url = "https://dict.youdao.com/webtranslate";
    std::string headers = "user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/114.0.0.0 Safari/537.36\r\n"
        "referer: https://fanyi.youdao.com/\r\n"
        "cookie: OUTFOX_SEARCH_USER_ID=-805044645@10.112.57.88; "
        "OUTFOX_SEARCH_USER_ID_NCOO=818822109.5585971;\r\n";
    std::string params = get_form_data(sentence, from_lang, to_lang);

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return "";
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, params.length());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.c_str());

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void* buffer, size_t size, size_t nmemb, std::string* response) {
        response->append(reinterpret_cast<const char*>(buffer), size * nmemb);
        return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "Failed to perform curl request: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    // Decrypt the translation result using AES
    std::string key = md5("ydsecret://query/key/B*RGygVywfNBwpmBaZg*WT7SIOUP2T0C9WHMZN39j^DAdaZhAnxvGcCY6VYFwnHl");
    std::string iv = md5("ydsecret://query/iv/C@lZe2YzHtZ2CYgaXKSVfsb7Y4QWHjITPPZ0nQp87fBeJ!Iv6v^6fvi2WN@bYpJ4");

    std::string decrypted_response = aes_decrypt(response, key, iv);

    // Parse the JSON response
    // ...

    return "";  // Return the translated text
}