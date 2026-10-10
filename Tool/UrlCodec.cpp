#include "UrlCodec.h"
#include <cctype>

namespace TOOL {

	namespace {

		char ToHex(unsigned char x) {
			return (char)(x > 9 ? x + 55 : x + 48);//10~15 → 'A'~'F'，0~9 → '0'~'9'
		}

		int FromHex(char c) {
			if (c >= '0' && c <= '9') { return c - '0'; }
			if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
			if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
			return -1;
		}

	}

	std::string UrlEncode(const std::string& str) {
		std::string strTemp;
		const size_t length = str.length();
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

	std::string UrlDecode(const std::string& str) {
		std::string strTemp;
		strTemp.reserve(str.size());
		const size_t length = str.length();
		for (size_t i = 0; i < length; i++)
		{
			if (str[i] == '%' && i + 2 < length)
			{
				const int High = FromHex(str[i + 1]);
				const int Low = FromHex(str[i + 2]);
				if (High >= 0 && Low >= 0)
				{
					strTemp += (char)(High * 16 + Low);
					i += 2;
					continue;
				}
			}
			strTemp += str[i];
		}
		return strTemp;
	}

}