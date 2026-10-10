#include "Hitokoto.h"
#include "../Tool/Http.h"//TOOL::HttpGet / 共享写回调
#include <atomic>
#include <mutex>
#include <thread>

namespace
{
	//一言的后台取词状态：界面线程只读 HitokotoText()，取词线程只写，用互斥量保护。
	std::mutex gHitokotoMutex;
	std::string gHitokotoText;
	std::atomic<bool> gHitokotoFetching{ false };
	std::thread gHitokotoThread;

	//进程收尾时等取词线程结束：它还在用 libcurl，不能让它跑在全局对象销毁之后。
	struct HitokotoThreadGuard
	{
		~HitokotoThreadGuard()
		{
			if (gHitokotoThread.joinable())
			{
				gHitokotoThread.join();
			}
		}
	};
	HitokotoThreadGuard gHitokotoThreadGuard;
}

std::string GetHitokoto()
{
    std::string hitokoto;

    //给超时：避免网络不通时把调用方（可能是界面线程）挂死在这里
    const TOOL::HttpResult Http = TOOL::HttpGet("https://v1.hitokoto.cn", {}, 3, 6);
    if ((Http.Curl == CURLE_OK) && (Http.Body.size() > 0)) {
        Json::Reader reader;
        Json::Value value;
        reader.parse(Http.Body, value);
        hitokoto = value["hitokoto"].asString() + "  --  " + value["from"].asString();
    }

    return hitokoto;
}

void RequestHitokoto()
{
	//已经有一句在取了就别叠加：重复请求直接返回（调用方一秒都不会被挡）
	if (gHitokotoFetching.load())
	{
		return;
	}
	if (gHitokotoThread.joinable())
	{
		gHitokotoThread.join();//上一次已经跑完了，收一下线程（立刻返回）
	}

	gHitokotoFetching = true;
	gHitokotoThread = std::thread([]()
	{
		const std::string Text = GetHitokoto();
		if (!Text.empty())
		{
			std::lock_guard<std::mutex> Lock(gHitokotoMutex);
			gHitokotoText = Text;//取不到就保留上一次的句子
		}
		gHitokotoFetching = false;
	});
}

std::string HitokotoText()
{
	std::lock_guard<std::mutex> Lock(gHitokotoMutex);
	return gHitokotoText;
}
