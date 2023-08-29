#include "FunctionalFunctions.h"
#include <regex>//正则表达式
#include <iostream>
#include "../Variable.h"

void RegisterError(std::string str, int r) {
	if (r < 0) {
		std::cout << "Failed to register global function: " << str << std::endl;
	}
}

void AngelScriptRegister(asIScriptEngine* Engine) {
	RegisterError("Replacement",
		Engine->RegisterGlobalFunction(
			"string Replacement(string, string, string, string, string, bool, bool)",
			asFUNCTION(Replacement), asCALL_CDECL)
	);
	RegisterError("TextReplacement",
		Engine->RegisterGlobalFunction(
			"string TextReplacement(string, string, string)",
			asFUNCTION(TextReplacement), asCALL_CDECL)
	);
	RegisterError("TextDeletion",
		Engine->RegisterGlobalFunction(
			"string TextDeletion(string, string)",
			asFUNCTION(TextDeletion), asCALL_CDECL)
	);

	RegisterError("TextExists",
		Engine->RegisterGlobalFunction(
			"bool TextExists(string, string)",
			asFUNCTION(TextExists), asCALL_CDECL)
	);
	RegisterError("TextExistsConditionFrontT",
		Engine->RegisterGlobalFunction(
			"bool TextExistsConditionFrontT(string, string, string)",
			asFUNCTION(TextExistsConditionFrontT), asCALL_CDECL)
	);
	RegisterError("TextExistsConditionFrontF",
		Engine->RegisterGlobalFunction(
			"bool TextExistsConditionFrontF(string, string, string)",
			asFUNCTION(TextExistsConditionFrontF), asCALL_CDECL)
	);
	RegisterError("TextExistsConditionRearT",
		Engine->RegisterGlobalFunction(
			"bool TextExistsConditionRearT(string, string, string)",
			asFUNCTION(TextExistsConditionRearT), asCALL_CDECL)
	);
	RegisterError("TextExistsConditionRearF",
		Engine->RegisterGlobalFunction(
			"bool TextExistsConditionRearF(string, string, string)",
			asFUNCTION(TextExistsConditionRearF), asCALL_CDECL)
	);
	RegisterError("TextExistsConditionFrontToRear",
		Engine->RegisterGlobalFunction(
			"bool TextExistsConditionFrontToRear(string, string, string, string, bool, bool)",
			asFUNCTION(TextExistsConditionFrontToRear), asCALL_CDECL)
	);

	RegisterError("LeaveOnlyLetters",
		Engine->RegisterGlobalFunction(
			"string LeaveOnlyLetters(string)",
			asFUNCTION(LeaveOnlyLetters), asCALL_CDECL)
	);
	RegisterError("WordSeparation",
		Engine->RegisterGlobalFunction(
			"string WordSeparation(string)",
			asFUNCTION(WordSeparation), asCALL_CDECL)
	);
	RegisterError("RemoveExcessiveSpaces",
		Engine->RegisterGlobalFunction(
			"string RemoveExcessiveSpaces(string)",
			asFUNCTION(RemoveExcessiveSpaces), asCALL_CDECL)
	);
	RegisterError("UppercaseStart",
		Engine->RegisterGlobalFunction(
			"string UppercaseStart(string)",
			asFUNCTION(UppercaseStart), asCALL_CDECL)
	);
	RegisterError("DeletionSpaces",
		Engine->RegisterGlobalFunction(
			"string DeletionSpaces(string)",
			asFUNCTION(DeletionSpaces), asCALL_CDECL)
	); 
	RegisterError("Autowrap",
		Engine->RegisterGlobalFunction(
			"string Autowrap(string)",
			asFUNCTION(Autowrap), asCALL_CDECL)
	);
}

std::string Replacement(std::string str, std::string Replace, std::string Change, std::string Ftext, std::string Rtext, bool F, bool R) {
	int pos = str.find(Replace);
	if (pos == std::string::npos) {
		return str;
	}
	int L = pos - Ftext.size();
	if (L >= 0) {
		pos = L;
	}
	std::string Str = str.substr(0, pos);
	std::string CStr = str.substr(pos, str.size() - pos);
	while (pos != std::string::npos)
	{
		if (TextExistsConditionFrontToRear(CStr, Replace, Ftext, Rtext, F, R)) {//判断是否符合标准
			CStr = TextReplacement(CStr, Replace, Change);
			pos = CStr.find(Replace);//判断是否还这种元素
		}
		else {//不符合标准，跳过这段字符串，继续处理后面内容
			pos = CStr.find(Replace) + Replace.size();
			Str += CStr.substr(0, pos);
			CStr = CStr.substr(pos, CStr.size() - pos);
			pos = CStr.find(Replace);
		}
	}
	Str += CStr;
	return Str;
}

std::string TextReplacement(std::string str, std::string Replace, std::string Change) {
	int qpos = str.find(Replace);
	int hpos = qpos + Replace.size();
	return (str.substr(0, qpos) + Change + str.substr(hpos, str.size()));
}

std::string TextDeletion(std::string str, std::string Deletion) {
	int qpos = str.find(Deletion);
	int hpos = qpos + Deletion.size();
	return (str.substr(0, qpos) + str.substr(hpos, str.size()));
}

bool TextExists(std::string str, std::string text) {
	if (str.find(text) == std::string::npos) {
		return false;
	}
	else {
		return true;
	}
}

bool TextExistsConditionFrontT(std::string str, std::string text, std::string Ftext) {
	int pos = str.find(text);
	if (pos == std::string::npos) {
		return false;
	}
	else {
		int size = Ftext.size();
		pos -= size;
		if (pos >= 0) {
			if (str.substr(pos, size) == Ftext) {
				return true;
			}
			else {
				return false;
			}
		}
		else
		{
			return false;
		}
	}
}

bool TextExistsConditionFrontF(std::string str, std::string text, std::string Ftext) {
	int pos = str.find(text);
	if (pos == std::string::npos) {
		return false;
	}
	else {
		int size = Ftext.size();
		pos -= size;
		if (pos >= 0) {
			if (str.substr(pos, size) != Ftext) {
				return true;
			}
			else {
				return false;
			}
		}
		else
		{
			return false;
		}
	}
}

bool TextExistsConditionRearT(std::string str, std::string text, std::string Rtext) {
	int pos = str.find(text);
	if (pos == std::string::npos) {
		return false;
	}
	else {
		int size = Rtext.size();
		pos += text.size();
		if ((pos + size) <= str.size()) {
			if (str.substr(pos, size) == Rtext) {
				return true;
			}
			else {
				return false;
			}
		}
		else
		{
			return false;
		}
	}
}

bool TextExistsConditionRearF(std::string str, std::string text, std::string Rtext) {
	int pos = str.find(text);
	if (pos == std::string::npos) {
		return false;
	}
	else {
		int size = Rtext.size();
		pos += text.size();
		if ((pos + size) <= str.size()) {
			if (str.substr(pos, size) != Rtext) {
				return true;
			}
			else {
				return false;
			}
		}
		else
		{
			return false;
		}
	}
}


bool TextExistsConditionFrontToRear(std::string str, std::string text, std::string Ftext, std::string Rtext, bool F, bool R) {
	bool G = false, P = false;
	if (Ftext.size() != 0)
	{
		if (F) {
			G = TextExistsConditionFrontT(str, text, Ftext);
		}
		else {
			G = TextExistsConditionFrontF(str, text, Ftext);
		}
	}
	if (Rtext.size() != 0) {
		if (R) {
			P = TextExistsConditionRearT(str, text, Rtext);
		}
		else {
			P = TextExistsConditionRearF(str, text, Rtext);
		}
	}
	
	if ((Ftext.size() == 0) && (Rtext.size() == 0)) {
		return true;
	}
	if (Ftext.size() == 0)
	{
		return P;
	}
	else if (Rtext.size() == 0) {
		return G;
	}
	else {
		return (G && P);
	}
}


std::string LeaveOnlyLetters(std::string str) {
	std::regex regexp("[a-zA-Z0-9]");
	std::string input;
	std::smatch result; // 保存匹配结果的容器
	for (size_t i = 0; i < str.size(); i++)
	{
		input = str[i];
		if (!std::regex_search(input, result, regexp)) {
			str[i] = ' ';
		}
	}
	return str;
}

std::string WordSeparation(std::string str) {
	std::regex regexp("[A-Z]");
	std::regex regexp2("[a-z]");
	std::string input;
	std::smatch result; // 保存匹配结果的容器
	for (size_t i = 1; i < str.size(); i++)
	{
		input = str[i];
		if (std::regex_search(input, result, regexp)) {
			input = str[i - 1];
			if (std::regex_search(input, result, regexp2)) {
				str = str.substr(0, i) + ' ' + str.substr(i, str.size() - i);
			}
		}
	}
	return str;
}


std::string RemoveExcessiveSpaces(std::string str) {
	int jie;
	if (str.size() < 1) {
		return str;
	}
	for (size_t i = 0; i < str.size() - 1; i++)
	{
		if (str[i] == ' ') {
			for (size_t x = i; x < str.size(); x++) {
				if (str[x] != ' ') {
					i++;
					jie = x;
					break;
				}
				if (x == (str.size() - 1)) {
					jie = str.size();
				}
			}
			str = str.substr(0, i) + str.substr(jie, str.size() - jie);
		}
	}
	if (str[0] == ' ')
	{
		str = str.substr(1, str.size() - 1);
	}
	return str;
}

std::string UppercaseStart(std::string str) {
	if ((str[0] >= 'a') && (str[0] <= 'z')) {
		str[0] = toupper(str[0]);//转为大写
	}
	for (size_t i = 0; i < str.size(); i++)
	{
		if (str[i] == ' ') {
			if (i < str.size() - 1) {
				if ((str[i + 1] >= 'a') && (str[i + 1] <= 'z')) {
					str[i + 1] = toupper(str[i + 1]);//转为大写
				}
			}
		}
	}
	return str;
}

std::string DeletionSpaces(std::string str) {
	while (TextExists(str, " "))
	{
		str = TextDeletion(str, " ");
	}
	return str;
}

std::string Autowrap(std::string str) {
	unsigned int Cut = 0;

	std::regex regexp("[a-zA-Z0-9!-/:-@[-`{-~]");
	std::string input;
	std::smatch result; // 保存匹配结果的容器
	for (size_t i = 0; i < str.size(); i++)
	{
		input = str[i];
		if (std::regex_search(input, result, regexp)) {
			Cut++;
		}
		else {
			Cut++;
			i += 2;
		}
		if (Cut >= Variable::WrapSize) {
			i++;
			Cut = 0;
			str = str.substr(0, i) + '\n' + str.substr(i, str.size());
		}
	}
	return str;
}