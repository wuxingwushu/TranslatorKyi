#pragma once//防止二次编译
#include "base.h"



//string 转 wstring
std::string ws2s(const std::wstring& ws);

//wstring 转 string
std::wstring s2ws(const std::string& s);

//Unicode 转到 utf8
std::string  UnicodeToUtf8(const std::string& Unicode);

//utf8 转到 Unicode
std::string Utf8ToUnicode(const char* szU8);
