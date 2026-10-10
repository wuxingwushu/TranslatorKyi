#pragma once
#include <iostream>
#include <string>
#include <curl/curl.h>
#include "json.h"

//同步取一句一言：会一直等到网络返回（最长 3 秒连接 + 6 秒总时长），别在界面线程里直接调。
std::string GetHitokoto();

//后台线程取一句一言，立即返回；取到之后 HitokotoText() 就能读到。
//已经有一句在取的时候重复调用不会叠加线程（也不会阻塞调用方）。
void RequestHitokoto();

//立刻返回最近一次取到的一言（还没取到时是上一句，或空串），不阻塞。
std::string HitokotoText();
