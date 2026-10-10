#include "Hitokoto.h"
#include "../Tool/Http.h"//TOOL::HttpGet / 共享写回调

std::string GetHitokoto()
{
    std::string hitokoto;

    const TOOL::HttpResult Http = TOOL::HttpGet("https://v1.hitokoto.cn");
    if ((Http.Curl == CURLE_OK) && (Http.Body.size() > 0)) {
        Json::Reader reader;
        Json::Value value;
        reader.parse(Http.Body, value);
        hitokoto = value["hitokoto"].asString() + "  --  " + value["from"].asString();
    }

    return hitokoto;
}