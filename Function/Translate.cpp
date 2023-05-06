#include "Translate.h"


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
        return std::string(u8"不存在单词");
    }


    CURL* curl;
    CURLcode res;
    FILE* fp;
    fp = fopen("TemporaryData", "w+");

    curl = curl_easy_init();
    if (curl) {
        char myurl[100000] = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
        
        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[120] = "";
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
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
        fseek(fp, 0, SEEK_END);//将文件内部的指针指向文件末尾
        long lsize = ftell(fp);//获取文件长度，（得到文件位置指针当前位置相对于文件首的偏移字节数）
        fseek(fp, NULL, SEEK_SET);//将文件内部的指针重新指向一个流的开头
        char* kaox = new char[lsize];//申请内存空间，lsize*sizeof(char)是为了更严谨，16位上char占一个字符，其他机器上可能变化
        fread(kaox, 1, lsize, fp);//将pfile中内容读入pread指向内存中
        fclose(fp);


        Json::Value value;
        Json::Reader reader;
        if (!reader.parse(kaox, value)) {
            printf("parse json error!");
            delete[] kaox;
            return "错误";
        }
        std::string Chinese = value["trans_result"][0]["dst"].asString();
        delete[] kaox;
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
        return std::string(u8"不存在单词");
    }

    CURL* curl;
    CURLcode res;
    FILE* fp;
    fp = fopen("TemporaryData", "w+");

    curl = curl_easy_init();
    if (curl) {
        char myurl[100000] = "http://openapi.youdao.com/api?";

        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[120] = "";
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
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
        fseek(fp, 0, SEEK_END);//将文件内部的指针指向文件末尾
        long lsize = ftell(fp);//获取文件长度，（得到文件位置指针当前位置相对于文件首的偏移字节数）
        fseek(fp, NULL, SEEK_SET);//将文件内部的指针重新指向一个流的开头
        char* kaox = new char[lsize];//申请内存空间，lsize*sizeof(char)是为了更严谨，16位上char占一个字符，其他机器上可能变化
        fread(kaox, 1, lsize, fp);//将pfile中内容读入pread指向内存中
        fclose(fp);


        Json::Value value;
        Json::Reader reader;
        if (!reader.parse(kaox, value)) {
            printf("parse json error!");
            delete[] kaox;
            return "错误";
        }
        std::string Chinese = value["web"][0]["value"][0].asString();
        delete[] kaox;
        return Chinese;
    }
    return "错误";
}
