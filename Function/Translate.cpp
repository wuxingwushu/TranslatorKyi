#include "Translate.h"


std::string Translate_Baidu(char* English) {
    CURL* curl;
    CURLcode res;
    FILE* fp;
    fp = fopen("kao.txt", "w+");


    //English = "English English";

    // char 的 ASCII 表上的一些特殊符号 或导致翻译错误。就是格式错误。
    char* q = new char[strlen(English)];
    for (int xi = 0; xi < strlen(English); xi++) {
        if (((English[xi] < 91) && (English[xi] > 64)) || ((English[xi] < 123) && (English[xi] > 96))) {
        //if(English[xi] > 32){
            q[xi] = English[xi];
        }
        else {
            printf("%d\n", English[xi]);
            q[xi] = '-';
        }
    }


    curl = curl_easy_init();
    if (curl) {
        char myurl[1000] = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
        char* appid = "20210925000956550";    //replace myAppid with your own appid
        //char* q = English;                  //replace apple with your own text to be translate, ensure that the input text is encoded with UTF-8!
        char* from = "en";                    //replace en with your own language type of input text
        char* to = "zh";                      //replace zh with your own language type of output text
        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char* secret_key = "os4RtAbGDCDhvgWvGSPu";   //replace mySecretKey with your own mySecretKey
        char sign[120] = "";
        strcat(sign, appid);
        strcat(sign, q);
        strcat(sign, salt);
        strcat(sign, secret_key);
        //printf("%s\n", sign);
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
        strcat(myurl, q);
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
        curl_easy_setopt(curl, CURLOPT_URL, myurl);
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

        /*
        int chang=-1;
        for (int i = strlen(q) + 56; i < lsize; i++)
        {
            chang++;
            if (kaox[i] == '"') {
                break;
            }
        }

        unsigned char* Czi = new unsigned char[chang];
        for (int i = 0; i < chang; i++)
        {
            Czi[i] = kaox[strlen(q) + 56 + i];
        }

        std::cout << Czi << std::endl;
        */


        Json::Value value;
        Json::Reader reader;
        if (!reader.parse(kaox, value)) {
            printf("parse json error!");
            delete[] kaox;
            //delete[] q;
            return 0;
        }
        std::string Chinese = value["trans_result"][0]["dst"].asString().c_str();
        delete[] kaox;
        //delete[] q;

        return Chinese;
    }
    //delete[] q;
    return 0;
}