#include "Hitokoto.h"

size_t write_data(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* str = (std::string*)userdata;
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string GetHitokoto()
{
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    std::string hitokoto;


    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://v1.hitokoto.cn");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            Json::Reader reader;
            Json::Value value;
            reader.parse(readBuffer, value);
            hitokoto = value["hitokoto"].asString() + " ---- " + value["from"].asString();
        }
    }

    return hitokoto;
}