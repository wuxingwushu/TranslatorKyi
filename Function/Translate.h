#pragma once//��ֹ���α���
#include "../base.h"
#include <json.h>
#include <curl/curl.h>
#include <openssl/md5.h>
#include <assert.h>


std::string Translate_Baidu(const char* appid, const char* secret_key, std::string English, const char* from, const char* to);
std::string Translate_Youdao(const char* appid, const char* secret_key, std::string English, const char* from, const char* to);