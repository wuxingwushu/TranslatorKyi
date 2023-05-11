#include <iostream>
#include <curl/curl.h>
#include "json.h"

size_t write_data(char* ptr, size_t size, size_t nmemb, void* userdata);

std::string GetHitokoto();
