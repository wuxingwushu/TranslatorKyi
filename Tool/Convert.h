#pragma once
//类型转换的通用工具：把配置/脚本里的字符串转成具体类型。
//原先这套 Converter<T>/BoolConverter 在 Tool.cpp 与 opcode.cpp 各有一份完全相同的实现，
//其中 opcode.cpp 那份还把模板定义放在 .cpp 里（其它翻译单元无法实例化，属可用性缺陷）。
//这里收敛到唯一一份：模板必须定义在头文件里，调用方直接 include 本文件即可。
#include <string>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <unordered_map>

namespace TOOL {

	//把字符串解析成 T（char/int/float/double 等）。失败时抛 std::runtime_error。
	template <typename T>
	T Converter(const std::string& s) {
		try {
			T v{};
			std::istringstream _(s);
			_.exceptions(std::ios::failbit);
			_ >> v;
			return v;
		}
		catch (std::exception&) {
			throw std::runtime_error("cannot parse value '" + s + "' to type<T>.");
		}
	}

	//把任意可 << 到流里的值转成字符串（原先定义在 Tool.cpp 里，模板放 .cpp 其它 TU 用不了）。
	template <typename T>
	std::string toString(const T& t)
	{
		std::ostringstream oss;  //创建一个格式化输出流
		oss << t;             //把值传递如流中
		return oss.str();
	}

	//把 "1/true/yes/on"（以及对应的假值）解析成 bool，大小写不敏感。失败时抛 std::runtime_error。
	inline bool BoolConverter(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		static const std::unordered_map<std::string, bool> s2b{
			{"1", true},  {"true", true},   {"yes", true}, {"on", true},
			{"0", false}, {"false", false}, {"no", false}, {"off", false},
		};
		auto const value = s2b.find(s);
		if (value == s2b.end()) {
			throw std::runtime_error("'" + s + "' is not a valid boolean value.");
		}
		return value->second;
	}

}