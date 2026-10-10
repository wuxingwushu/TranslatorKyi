#pragma once
//URL 编解码的单一实现。
//原先「URL 解码」在 Translate.cpp(Translate::UrlDecode) 与 WebDav.cpp(urlDecode) 各有一份，
//「URL 编码」只存在于 Translate.cpp，这里统一收敛到 TOOL。
#include <string>

namespace TOOL {

	//URL 编码：字母数字与 - _ . ~ 原样保留，空格转 '+'，其余按字节转 %XX（UTF-8 字节逐个处理）
	std::string UrlEncode(const std::string& str);

	//URL 解码：%XX 还原为字节；'+' 原样保留（WebDAV 路径里的 '+' 是字面量，不是空格）
	std::string UrlDecode(const std::string& str);

}