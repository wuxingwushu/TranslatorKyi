#include "Translate.h"





unsigned char ToHex(unsigned char x)
{
    return  x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}



std::string UrlEncode(const std::string& str)
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


std::string UrlDecode(const std::string& str)
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
std::string Translate_Baidu(const char* appid, const char* secret_key, std::string English, char* from, char* to) {
    //appid             //replace myAppid with your own appid
    //secret_key        //replace mySecretKey with your own mySecretKey
    //English           //replace apple with your own text to be translate, ensure that the input text is encoded with UTF-8!
    //from;             //replace en with your own language type of input text
    //to;               //replace zh with your own language type of output text

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
        char myurl[1000] = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
        
        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char sign[120] = "";
        strcat(sign, appid);
        strcat(sign, English.c_str());//获取加密MD5时 English 不要进行 Url_Encode 处理  （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(sign, salt);
        strcat(sign, secret_key);
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
        strcat(myurl, appid);
        strcat(myurl, "&q=");
        strcat(myurl, UrlEncode(English).c_str());//生成网页链接时 English 才要进行 Url_Encode 处理   （ 详细详细查看百度翻译API文档：https://fanyi-api.baidu.com/product/113 ）
        strcat(myurl, "&from=");
        strcat(myurl, from);
        strcat(myurl, "&to=");
        strcat(myurl, to);
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
            return 0;
        }
        std::string Chinese = value["trans_result"][0]["dst"].asString();
        delete[] kaox;

        return Chinese;
    }
    return 0;
}