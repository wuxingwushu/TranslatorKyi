#pragma once//防止二次编译
#include <time.h>
#include <string.h>
#include <iostream>
#include <tchar.h>
#include <sstream>
#include <windows.h>//WIN API
//#include "vld.h"//内存泄露检测库 （Debug 时才会启动）


static clock_t shijian;//测试耗时
/*shijian = clock();
printf("%d\n", shijian - clock());
*/

/*
std::cout <<   << std::endl;
 

GetKeyState按键表
https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
*/


/*
#include <iostream>
#include <string>
#include <curl/curl.h>

// 这是有道翻译 API 的地址
const std::string API_URL = "http://fanyi.youdao.com/translate?&doctype=json&type=ZH_CN2EN";

// 这是有道翻译 API 的私人 key，需要在有道翻译官网申请
const std::string PRIVATE_KEY = "your_private_key";

// 这是有道翻译 API 的私人 key，需要在有道翻译官网申请
const std::string APP_KEY = "your_app_key";

// 这是有道翻译 API 的输入语言，可以是 "ZH_CN" 或 "EN"
const std::string INPUT_LANG = "ZH_CN";

// 这是有道翻译 API 的输出语言，可以是 "ZH_CN" 或 "EN"
const std::string OUTPUT_LANG = "EN";

// 这是一个函数，用于将字符串转换为 URL 编码
std::string url_encode(const std::string& str) {
  // 初始化 curl 库
  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Error: failed to initialize curl" << std::endl;
    return "";
  }

  // 设置 curl 库的选项
  curl_easy_setopt(curl, CURLOPT_URL, API_URL.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ("q=" + str).c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

  // 设置 HTTP 头部
  struct curl_s

*/