#include "Translate.h"
#include "../Tool/UrlCodec.h"//TOOL::UrlEncode / TOOL::UrlDecode
#include "../Tool/Http.h"    //TOOL::HttpGet / 共享写回调
#include "Crypto.h"          //md5
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>

// =====================================================================================
// 普通翻译源（百度 / 爬虫 / 有道）的 HTTP 实现
// -------------------------------------------------------------------------------------
// 线程编排在 Translate.cpp；这里只负责拼签名、发请求、解析 JSON。
// =====================================================================================

//（ 详细详细查看百度翻译API文档：https ://fanyi-api.baidu.com/product/113 ）
std::string Translate::Translate_Baidu(std::string English, int FromIndex, int ToIndex) {
    //语言索引夹一下：后台线程拿到的索引是从主线程复制过来的，越界会读坏 Baidu_items
    const int From = (FromIndex >= 0 && FromIndex < (int)Variable::Baiduitems.size()) ? FromIndex : 0;
    const int To = (ToIndex >= 0 && ToIndex < (int)Variable::Baiduitems.size()) ? ToIndex : 0;

    //appid / secret_key 由调用方（Variable 里的 Data.ini 配置）在构造 Translate 时填进来

    //不存在单词取消翻译
    if (strlen(English.c_str()) <= 1) {
        return std::string(u8"No Words Present");
    }

    //签名串 = appid + 原文 + salt + 密钥（这里的原文不要 Url_Encode）
    const std::string Salt = std::to_string(rand());
    const std::string Sign = md5(std::string(mBaiduAppid) + English + Salt + mBaiduSecret_key);

    std::string Url = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
    Url += "appid=" + std::string(mBaiduAppid);
    Url += "&q=" + TOOL::UrlEncode(English);//生成网页链接时 English 才要 Url_Encode（见百度翻译API文档）
    Url += "&from=" + std::string(Baidu_items[From]);
    Url += "&to=" + std::string(Baidu_items[To]);
    Url += "&salt=" + Salt;
    Url += "&sign=" + Sign;

    //超时（连接 5 秒 / 总共 30 秒）：这几个请求跑在后台线程里，界面不会被卡住，
    //但网络不通时不能让界面一直显示「翻译中…」，退出时也不能让 join 无限等下去。
    const TOOL::HttpResult Http = TOOL::HttpGet(Url, {}, 5, 30);
    if (Http.Curl != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(Http.Curl));
        return "错误";
    }

    if (Http.Body.empty()) {
        return "错误";
    }
    Json::Value value;
    Json::Reader reader;
    if (!reader.parse(Http.Body, value)) {
        printf("parse json error!");
        return "错误";
    }
    return value["trans_result"][0]["dst"].asString();
}

std::string Translate::Translate_Youdao(std::string English, int FromIndex, int ToIndex)
{
    //语言索引夹一下：后台线程拿到的索引是从主线程复制过来的，越界会读坏 Youdao_items
    const int From = (FromIndex >= 0 && FromIndex < (int)Variable::Youdaoitems.size()) ? FromIndex : 0;
    const int To = (ToIndex >= 0 && ToIndex < (int)Variable::Youdaoitems.size()) ? ToIndex : 0;

    //appid / secret_key 由调用方（Variable 里的 Data.ini 配置）在构造 Translate 时填进来

    //不存在单词取消翻译
    if (strlen(English.c_str()) <= 1) {
        return std::string(u8"No Words Present");
    }

    //签名串 = appid + 原文 + salt + 密钥（这里的原文不要 Url_Encode）
    const std::string Salt = std::to_string(rand());
    const std::string Sign = md5(std::string(mYoudaoAppid) + English + Salt + mYoudaoSecret_key);

    std::string Url = "http://openapi.youdao.com/api?";
    Url += "appKey=" + std::string(mYoudaoAppid);
    Url += "&q=" + TOOL::UrlEncode(English);//生成网页链接时 English 才要 Url_Encode（见有道翻译API文档）
    Url += "&from=" + std::string(Youdao_items[From]);
    Url += "&to=" + std::string(Youdao_items[To]);
    Url += "&sign=" + Sign;
    Url += "&salt=" + Salt;

    //超时同百度那条：连接 5 秒 / 总共 30 秒
    const TOOL::HttpResult Http = TOOL::HttpGet(Url, {}, 5, 30);
    if (Http.Curl != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(Http.Curl));
        return "错误";
    }

    if (Http.Body.empty()) {
        return "错误";
    }
    Json::Value value;
    Json::Reader reader;
    if (!reader.parse(Http.Body, value)) {
        printf("parse json error!");
        return "错误";
    }
    return value["web"][0]["value"][0].asString();
}

std::string Translate::Translate_ReptilesYoudao(std::string English) {
    std::string url = "https://dict.youdao.com/webtranslate?&doctype=json&type=AUTO&to=AUTO&i=" + TOOL::UrlEncode(English);/*&to=ja*/

    // 设置User-Agent头字段
    const std::string userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36 Edg/115.0.1901.203";
    //超时同上面两条：连接 5 秒 / 总共 30 秒
    const TOOL::HttpResult Http = TOOL::HttpGet(url, { "User-Agent: " + userAgent }, 5, 30);
    if (Http.Curl != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(Http.Curl) << std::endl;
    }
    const std::string& result = Http.Body;

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
