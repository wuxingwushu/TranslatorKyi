#include "Translate.h"
#include "../Tool/UrlCodec.h"//TOOL::UrlEncode / TOOL::UrlDecode
#include "../Tool/Http.h"    //TOOL::HttpGet / 共享写回调
#include "Crypto.h"          //md5 / aes_decrypt
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

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

    //appid             //将myAppid替换为您自己的appid
    //secret_key        //将mySecretKey替换为您自己的mySecretKey
    //English           //将apple替换为您自己要翻译的文本，确保输入文本使用UTF-8编码！
    //from;             //用您自己的语言类型的输入文本替换en
    //to;               //用您自己语言类型的输出文本替换zh

    //不存在单词取消翻译
    if (strlen(English.c_str()) <= 1) {
        return std::string(u8"No Words Present");
    }


    {
        char myurl[100000] = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
        
        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[100000] = "";
        strcat(sign, mBaiduAppid);
        strcat(sign, English.c_str());//获取加密MD5时 English 不要进行 Url_Encode 处理  （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(sign, salt);
        strcat(sign, mBaiduSecret_key);
        //MD5 的十六进制小写串（原实现是逐字节 sprintf "%2.2x"，与 md5() 等价）
        const std::string buf = md5(sign);
        strcat(myurl, "appid=");
        strcat(myurl, mBaiduAppid);
        strcat(myurl, "&q=");
        strcat(myurl, TOOL::UrlEncode(English).c_str());//生成网页链接时 English 才要进行 Url_Encode 处理   （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(myurl, "&from=");
        strcat(myurl, Baidu_items[From]);
        strcat(myurl, "&to=");
        strcat(myurl, Baidu_items[To]);
        strcat(myurl, "&salt=");
        strcat(myurl, salt);
        strcat(myurl, "&sign=");
        strcat(myurl, buf.c_str());

        //超时（连接 5 秒 / 总共 30 秒）：这几个请求跑在后台线程里，界面不会被卡住，
        //但网络不通时不能让界面一直显示「翻译中…」，退出时也不能让 join 无限等下去。
        const TOOL::HttpResult Http = TOOL::HttpGet(std::string(myurl), {}, 5, 30);
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
        std::string Chinese = value["trans_result"][0]["dst"].asString();
        return Chinese;
    }
    return "错误";
}

std::string Translate::Translate_Youdao(std::string English, int FromIndex, int ToIndex)
{
    //语言索引夹一下：后台线程拿到的索引是从主线程复制过来的，越界会读坏 Youdao_items
    const int From = (FromIndex >= 0 && FromIndex < (int)Variable::Youdaoitems.size()) ? FromIndex : 0;
    const int To = (ToIndex >= 0 && ToIndex < (int)Variable::Youdaoitems.size()) ? ToIndex : 0;

    //appid             //将myAppid替换为您自己的appid
    //secret_key        //将mySecretKey替换为您自己的mySecretKey
    //English           //将apple替换为您自己要翻译的文本，确保输入文本使用UTF-8编码！
    //from;             //用您自己的语言类型的输入文本替换en
    //to;               //用您自己语言类型的输出文本替换zh


    //不存在单词取消翻译
    if (strlen(English.c_str()) <= 1) {
        return std::string(u8"No Words Present");
    }

    {
        char myurl[100000] = "http://openapi.youdao.com/api?";

        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[100000] = "";
        strcat(sign, mYoudaoAppid);
        strcat(sign, English.c_str());//获取加密MD5时 English 不要进行 Url_Encode 处理  （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(sign, salt);
        strcat(sign, mYoudaoSecret_key);
        //MD5 的十六进制小写串（原实现是逐字节 sprintf "%2.2x"，与 md5() 等价）
        const std::string buf = md5(sign);
        strcat(myurl, "appKey=");
        strcat(myurl, mYoudaoAppid);
        strcat(myurl, "&q=");
        strcat(myurl, TOOL::UrlEncode(English).c_str());//生成网页链接时 English 才要进行 Url_Encode 处理   （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(myurl, "&from=");
        strcat(myurl, Youdao_items[From]);
        strcat(myurl, "&to=");
        strcat(myurl, Youdao_items[To]);
        strcat(myurl, "&sign=");
        strcat(myurl, buf.c_str());
        strcat(myurl, "&salt=");
        strcat(myurl, salt);

        //超时同百度那条：连接 5 秒 / 总共 30 秒
        const TOOL::HttpResult Http = TOOL::HttpGet(std::string(myurl), {}, 5, 30);
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
        std::string Chinese = value["web"][0]["value"][0].asString();
        return Chinese;
    }
    return "错误";
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

// =====================================================================================
// 有道 webtranslate v2（带 AES 加解密）辅助
// =====================================================================================
std::string get_form_data(const std::string& sentence, const std::string& from_lang, const std::string& to_lang) {
    time_t current_time = time(nullptr);
    std::ostringstream mystic_time_stream;
    mystic_time_stream << std::fixed << std::setprecision(6) << std::setfill('0') << current_time;

    std::string t = mystic_time_stream.str();
    std::string key = "fsdsogkndfokasodnaso";

    std::string sign = md5("client=fanyideskweb&mysticTime=" + t + "&product=webfanyi&key=" + key);

    std::string form_data = "";
    form_data += "i=" + sentence + "&";
    form_data += "from=" + from_lang + "&";
    form_data += "to=" + to_lang + "&";
    form_data += "domain=0&";
    form_data += "dictResult=true&";
    form_data += "keyid=webfanyi&";
    form_data += "sign=" + sign + "&";
    form_data += "client=fanyideskweb&";
    form_data += "product=webfanyi&";
    form_data += "appVersion=1.0.0&";
    form_data += "vendor=web&";
    form_data += "pointParam=client,mysticTime,product&";
    form_data += "mysticTime=" + t + "&";
    form_data += "keyfrom=fanyi.web";

    return form_data;
}

std::string translate(const std::string& sentence, const std::string& from_lang, const std::string& to_lang) {
    std::string url = "https://dict.youdao.com/webtranslate";
    std::string headers = "user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/114.0.0.0 Safari/537.36\r\n"
        "referer: https://fanyi.youdao.com/\r\n"
        "cookie: OUTFOX_SEARCH_USER_ID=-805044645@10.112.57.88; "
        "OUTFOX_SEARCH_USER_ID_NCOO=818822109.5585971;\r\n";
    std::string params = get_form_data(sentence, from_lang, to_lang);

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return "";
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, params.length());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.c_str());

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void* buffer, size_t size, size_t nmemb, std::string* response) {
        response->append(reinterpret_cast<const char*>(buffer), size * nmemb);
        return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "Failed to perform curl request: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    // Decrypt the translation result using AES
    std::string key = md5("ydsecret://query/key/B*RGygVywfNBwpmBaZg*WT7SIOUP2T0C9WHMZN39j^DAdaZhAnxvGcCY6VYFwnHl");
    std::string iv = md5("ydsecret://query/iv/C@lZe2YzHtZ2CYgaXKSVfsb7Y4QWHjITPPZ0nQp87fBeJ!Iv6v^6fvi2WN@bYpJ4");

    std::string decrypted_response = aes_decrypt(response, key, iv);

    // Parse the JSON response
    // ...

    return "";  // Return the translated text
}