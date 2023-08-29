#pragma once
#include "../base.h"
#include <json.h>
#include <curl/curl.h>
#include <openssl/md5.h>
#include <assert.h>

class Translate
{
public:
	Translate();
	~Translate();

	void SetBaiduAppID(const char* appid) { mBaiduAppid = appid; }
	void SetBaiduSecretkey(const char* secret_key) { mBaiduSecret_key = secret_key;}

	void SetYoudaoAppID(const char* appid) { mYoudaoAppid = appid; }
	void SetYoudaoSecretkey(const char* secret_key) { mYoudaoSecret_key = secret_key; }

	void SetFrom(int from) { mFrom = from; }
	void SetTo(int to) { mTo = to; }
	void SetTranslate(int translate) { mTranslate = translate; }

	std::string TranslateAPI(std::string English) {
		switch (mTranslate)
		{
		case 0:
			return Translate_Baidu(English);
			break;
		case 1:
			return Translate_ReptilesYoudao(English);
			break;
		case 2:
			return Translate_Youdao(English);
			break;
		default:
			break;
		}
	}
	const char* TranslateName[3] = {"百度","爬虫","有道" };

	const char** Baidu_items;
	const char** Youdao_items;

	int mFrom;
	int mTo;

	int mTranslate = 0;
private:
	const char* mBaiduAppid;
	const char* mBaiduSecret_key;

	const char* mYoudaoAppid;
	const char* mYoudaoSecret_key;

	


	unsigned char ToHex(unsigned char x);

	unsigned char FromHex(unsigned char x);

	std::string UrlEncode(const std::string& str);

	std::string UrlDecode(const std::string& str);

	std::string Translate_Baidu(std::string English);

	std::string Translate_Youdao(std::string English);

	std::string Translate_ReptilesYoudao(std::string English);
};