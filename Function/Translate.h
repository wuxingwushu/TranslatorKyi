#pragma once//·ÀÖ¹¶ş´Î±àÒë
#include "../base.h"
#include <json.h>
#include <curl/curl.h>
#include <openssl/md5.h>
#include <assert.h>


std::string Translate_Baidu(const char* appid, const char* secret_key, char* English);