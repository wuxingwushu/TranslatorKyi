#pragma once//∑¿÷π∂˛¥Œ±‡“Î
#include "../base.h"
#include <json.h>
#include <curl/curl.h>


bool getUrl(char* filename);
bool postUrl(char* filename);

//http POST«Î«Û  
CURLcode curl_post_req(const std::string& url, const std::string& postParams, std::string& response);
size_t req_reply(void* ptr, size_t size, size_t nmemb, void* stream);