#include "Translate.h"
#include <openssl/aes.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>

size_t TranslateWrite_data(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* str = (std::string*)userdata;
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

Translate::Translate()
{   
    Baidu_items = new const char* [Variable::Baiduitems.size()];
    for (int i = 0; i < Variable::Baiduitems.size(); i++)
    {
        Baidu_items[i] = Variable::Baiduitems[i].c_str();
    }
    Youdao_items = new const char* [Variable::Youdaoitems.size()];
    for (int i = 0; i < Variable::Youdaoitems.size(); i++)
    {
        Youdao_items[i] = Variable::Youdaoitems[i].c_str();
    }
}

Translate::~Translate()
{

}


unsigned char Translate::ToHex(unsigned char x)
{
    return  x > 9 ? x + 55 : x + 48;
}

unsigned char Translate::FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}



std::string Translate::UrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (isalnum((unsigned char)str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~'))
            strTemp += str[i];
        else if (str[i] == ' ')
            strTemp += "+";
        else
        {
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] % 16);
        }
    }
    return strTemp;
}


std::string Translate::UrlDecode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (str[i] == '+') strTemp += ' ';
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high * 16 + low;
        }
        else strTemp += str[i];
    }
    return strTemp;
}



//（ 详细详细查看百度翻译API文档：https ://fanyi-api.baidu.com/product/113 ）
std::string Translate::Translate_Baidu(std::string English) {
    //appid             //将myAppid替换为您自己的appid
    //secret_key        //将mySecretKey替换为您自己的mySecretKey
    //English           //将apple替换为您自己要翻译的文本，确保输入文本使用UTF-8编码！
    //from;             //用您自己的语言类型的输入文本替换en
    //to;               //用您自己语言类型的输出文本替换zh

    //不存在单词取消翻译
    if (strlen(English.c_str()) <= 1) {
        return std::string(u8"No Words Present");
    }


    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        char myurl[100000] = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
        
        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[100000] = "";
        strcat(sign, mBaiduAppid);
        strcat(sign, English.c_str());//获取加密MD5时 English 不要进行 Url_Encode 处理  （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(sign, salt);
        strcat(sign, mBaiduSecret_key);
        unsigned char md[16];
        int i;
        char tmp[3] = { '\0' }, buf[33] = { '\0' };
        MD5((unsigned char*)sign, strlen(sign), md);
        for (i = 0; i < 16; i++) {
            sprintf(tmp, "%2.2x", md[i]);
            strcat(buf, tmp);
        }
        //printf("%s\n", buf);
        strcat(myurl, "appid=");
        strcat(myurl, mBaiduAppid);
        strcat(myurl, "&q=");
        strcat(myurl, UrlEncode(English).c_str());//生成网页链接时 English 才要进行 Url_Encode 处理   （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(myurl, "&from=");
        strcat(myurl, Baidu_items[mFrom]);
        strcat(myurl, "&to=");
        strcat(myurl, Baidu_items[mTo]);
        strcat(myurl, "&salt=");
        strcat(myurl, salt);
        strcat(myurl, "&sign=");
        strcat(myurl, buf);
        //printf("%s\n", myurl);
        //设置访问的地址
        curl_easy_setopt(curl, CURLOPT_URL, &myurl);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TranslateWrite_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return "错误";
        }

        if (readBuffer.size() == 0) {
            return "错误";
        }
        Json::Value value;
        Json::Reader reader;
        if (!reader.parse(readBuffer, value)) {
            printf("parse json error!");
            return "错误";
        }
        std::string Chinese = value["trans_result"][0]["dst"].asString();
        return Chinese;
    }
    return "错误";
}

std::string Translate::Translate_Youdao(std::string English)
{
    //appid             //将myAppid替换为您自己的appid
    //secret_key        //将mySecretKey替换为您自己的mySecretKey
    //English           //将apple替换为您自己要翻译的文本，确保输入文本使用UTF-8编码！
    //from;             //用您自己的语言类型的输入文本替换en
    //to;               //用您自己语言类型的输出文本替换zh


    //不存在单词取消翻译
    if (strlen(English.c_str()) <= 1) {
        return std::string(u8"No Words Present");
    }

    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        char myurl[100000] = "http://openapi.youdao.com/api?";

        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[100000] = "";
        strcat(sign, mYoudaoAppid);
        strcat(sign, English.c_str());//获取加密MD5时 English 不要进行 Url_Encode 处理  （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(sign, salt);
        strcat(sign, mYoudaoSecret_key);
        unsigned char md[16];
        int i;
        char tmp[3] = { '\0' }, buf[33] = { '\0' };
        MD5((unsigned char*)sign, strlen(sign), md);
        for (i = 0; i < 16; i++) {
            sprintf(tmp, "%2.2x", md[i]);
            strcat(buf, tmp);
        }
        //printf("%s\n", buf);
        strcat(myurl, "appKey=");
        strcat(myurl, mYoudaoAppid);
        strcat(myurl, "&q=");
        strcat(myurl, UrlEncode(English).c_str());//生成网页链接时 English 才要进行 Url_Encode 处理   （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(myurl, "&from=");
        strcat(myurl, Youdao_items[mFrom]);
        strcat(myurl, "&to=");
        strcat(myurl, Youdao_items[mTo]);
        strcat(myurl, "&sign=");
        strcat(myurl, buf);
        strcat(myurl, "&salt=");
        strcat(myurl, salt);
        //printf("%s\n", myurl);
        //设置访问的地址
        curl_easy_setopt(curl, CURLOPT_URL, &myurl);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TranslateWrite_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return "错误";
        }

        if (readBuffer.size() == 0) {
            return "错误";
        }
        Json::Value value;
        Json::Reader reader;
        if (!reader.parse(readBuffer, value)) {
            printf("parse json error!");
            return "错误";
        }
        std::string Chinese = value["web"][0]["value"][0].asString();
        return Chinese;
    }
    return "错误";
}

// 回调函数处理服务器响应
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

std::string Translate::Translate_ReptilesYoudao(std::string English) {
    std::string url = "https://dict.youdao.com/webtranslate?&doctype=json&type=AUTO&to=AUTO&i=" + UrlEncode(English);/*&to=ja*/
    std::string result;

    // 初始化 libcurl
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();

    if (curl) {
        // 设置 URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // 设置回调函数和缓冲区
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

        // 设置User-Agent头字段
        std::string userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36 Edg/115.0.1901.203";
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

        // 发起请求
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        // 清理资源
        curl_easy_cleanup(curl);
    }

    // 清理 libcurl
    curl_global_cleanup();

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





//std::string iv = "ydsecret://query/iv/C@lZe2YzHtZ2CYgaXKSVfsb7Y4QWHjITPPZ0nQp87fBeJ!Iv6v^6fvi2WN@bYpJ4";
//std::string key = "ydsecret://query/key/B*RGygVywfNBwpmBaZg*WT7SIOUP2T0C9WHMZN39j^DAdaZhAnxvGcCY6VYFwnHl";
//
//std::string md5(const std::string& input) {
//    unsigned char hash[MD5_DIGEST_LENGTH];
//    MD5(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);
//    std::string result;
//    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
//        result += hash[i];
//    }
//    return result;
//}
//
//std::string aesEncrypt(const std::string& plaintext) {
//    AES_KEY aesKey;
//    AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(key.c_str()), 128, &aesKey);
//
//    unsigned char ivBytes[AES_BLOCK_SIZE];
//    memcpy(ivBytes, iv.c_str(), AES_BLOCK_SIZE);
//
//    int inputLength = plaintext.length();
//    int paddingLength = AES_BLOCK_SIZE - (inputLength % AES_BLOCK_SIZE);
//    int outputLength = inputLength + paddingLength;
//
//    unsigned char* inputBuffer = new unsigned char[outputLength];
//    memset(inputBuffer, 0, outputLength);
//    memcpy(inputBuffer, plaintext.c_str(), inputLength);
//    for (int i = inputLength; i < outputLength; ++i) {
//        inputBuffer[i] = paddingLength;
//    }
//
//    unsigned char* outputBuffer = new unsigned char[outputLength];
//    memset(outputBuffer, 0, outputLength);
//
//    for (int i = 0; i < outputLength; i += AES_BLOCK_SIZE) {
//        AES_cbc_encrypt(inputBuffer + i, outputBuffer + i, AES_BLOCK_SIZE, &aesKey, ivBytes, AES_ENCRYPT);
//    }
//
//    std::string encryptedText(reinterpret_cast<char*>(outputBuffer), outputLength);
//
//    delete[] inputBuffer;
//    delete[] outputBuffer;
//
//    return encryptedText;
//}
//
//std::string aesDecrypt(const std::string& ciphertext) {
//    AES_KEY aesKey;
//    AES_set_decrypt_key(reinterpret_cast<const unsigned char*>(key.c_str()), 128, &aesKey);
//
//    unsigned char ivBytes[AES_BLOCK_SIZE];
//    memcpy(ivBytes, iv.c_str(), AES_BLOCK_SIZE);
//
//    int inputLength = ciphertext.length();
//    int outputLength = inputLength;
//
//    unsigned char* inputBuffer = new unsigned char[inputLength];
//    memcpy(inputBuffer, ciphertext.c_str(), inputLength);
//
//    unsigned char* outputBuffer = new unsigned char[outputLength];
//    memset(outputBuffer, 0, outputLength);
//
//    for (int i = 0; i < outputLength; i += AES_BLOCK_SIZE) {
//        AES_cbc_encrypt(inputBuffer + i, outputBuffer + i, AES_BLOCK_SIZE, &aesKey, ivBytes, AES_DECRYPT);
//    }
//
//    // Remove padding
//    int paddingLength = outputBuffer[outputLength - 1];
//    std::string decryptedText(reinterpret_cast<char*>(outputBuffer), outputLength - paddingLength);
//
//    delete[] inputBuffer;
//    delete[] outputBuffer;
//
//    return decryptedText;
//}
//
//std::string getFormData(const std::string& sentence, const std::string& fromLang, const std::string& toLang) {
//    std::time_t t = std::time(nullptr);
//    std::string mysticTime = std::to_string(t);
//
//    std::string query = "client=fanyideskweb&mysticTime=" + mysticTime + "&product=webfanyi&key=fsdsogkndfokasodnaso";
//    std::string sign = md5(query);
//
//    std::string formData = "i=" + sentence + "&from=" + fromLang + "&to=" + toLang +
//        "&domain=0&dictResult=true&keyid=webfanyi&sign=" + sign +
//        "&client=fanyideskweb&product=webfanyi&appVersion=1.0.0&vendor=web" +
//        "&pointParam=client,mysticTime,product" + "&mysticTime=" + mysticTime + "&keyfrom=fanyi.web";
//
//    return formData;
//}
//
//size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
//    size_t totalSize = size * nmemb;
//    output->append(static_cast<char*>(contents), totalSize);
//    return totalSize;
//}
//
//std::string translate(const std::string& sentence, const std::string& fromLang, const std::string& toLang) {
//    std::string url = "https://dict.youdao.com/webtranslate";
//    std::string headers = "user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36\r\n"
//        "referer: https://fanyi.youdao.com/\r\n"
//        "cookie: OUTFOX_SEARCH_USER_ID=-805044645@10.112.57.88; OUTFOX_SEARCH_USER_ID_NCOO=818822109.5585971;\r\n";
//
//    std::string formData = getFormData(sentence, fromLang, toLang);
//    std::string encryptedData = aesEncrypt(formData);
//
//    CURL* curl = curl_easy_init();
//    if (curl) {
//        std::string response;
//        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, encryptedData.c_str());
//        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, encryptedData.length());
//        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
//        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
//        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.c_str());
//
//        CURLcode res = curl_easy_perform(curl);
//        curl_easy_cleanup(curl);
//
//        if (res == CURLE_OK) {
//            std::string decryptedResponse = aesDecrypt(response);
//
//            Json::Value root;
//            Json::Reader reader;
//            bool success = reader.parse(decryptedResponse, root);
//            if (success) {
//                std::string tgt = root["translateResult"][0][0]["tgt"].asString();
//                return tgt;
//            }
//        }
//    }
//
//    return "翻译失败：" + sentence;
//}

std::string md5(const std::string& input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), digest);

    char md5string[33];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(&md5string[i * 2], "%02x", (unsigned int)digest[i]);

    return std::string(md5string);
}

std::string base64_encode(const unsigned char* input, size_t length) {
    BIO* bmem = nullptr;
    BIO* b64 = nullptr;
    BUF_MEM* bptr = nullptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    std::string result(bptr->data, bptr->length - 1);

    BIO_free_all(b64);
    return result;
}

std::string base64_decode(const std::string& input) {
    BIO* b64 = nullptr;
    BIO* bmem = nullptr;
    unsigned char* buffer = new unsigned char[input.size()];
    memset(buffer, 0, input.size());

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new_mem_buf(input.c_str(), input.size());
    bmem = BIO_push(b64, bmem);
    BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);
    size_t length = BIO_read(bmem, buffer, input.size());

    std::string result(reinterpret_cast<const char*>(buffer), length);
    delete[] buffer;

    BIO_free_all(bmem);
    return result;
}

std::string aes_encrypt(const std::string& plaintext, const std::string& key, const std::string& iv) {
    AES_KEY aes_key;
    AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(key.c_str()), 128, &aes_key);

    int padding = AES_BLOCK_SIZE - (plaintext.length() % AES_BLOCK_SIZE);
    std::string padded_plaintext = plaintext + std::string(padding, static_cast<char>(padding));

    std::string ciphertext;
    ciphertext.resize(padded_plaintext.length());

    for (size_t i = 0; i < padded_plaintext.length(); i += AES_BLOCK_SIZE) {
        AES_encrypt(reinterpret_cast<const unsigned char*>(&padded_plaintext[i]),
            reinterpret_cast<unsigned char*>(&ciphertext[i]), &aes_key);
    }

    std::string encoded_ciphertext = base64_encode(reinterpret_cast<const unsigned char*>(ciphertext.c_str()),
        ciphertext.length());
    return encoded_ciphertext;
}

std::string aes_decrypt(const std::string& ciphertext, const std::string& key, const std::string& iv) {
    AES_KEY aes_key;
    AES_set_decrypt_key(reinterpret_cast<const unsigned char*>(key.c_str()), 128, &aes_key);

    std::string decoded_ciphertext = base64_decode(ciphertext);

    std::string plaintext;
    plaintext.resize(decoded_ciphertext.length());

    for (size_t i = 0; i < decoded_ciphertext.length(); i += AES_BLOCK_SIZE) {
        AES_decrypt(reinterpret_cast<const unsigned char*>(&decoded_ciphertext[i]),
            reinterpret_cast<unsigned char*>(&plaintext[i]), &aes_key);
    }

    size_t padding = static_cast<size_t>(plaintext[plaintext.length() - 1]);
    return plaintext.substr(0, plaintext.length() - padding);
}

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