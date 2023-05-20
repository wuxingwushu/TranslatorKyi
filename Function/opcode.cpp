#include "opcode.h"




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
	bool G, P;
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

std::string WordSeparation(std::string str){
	std::regex regexp("[A-Z]");
	std::regex regexp2("[a-z]");
	std::string input;
	std::smatch result; // 保存匹配结果的容器
	for (size_t i = 1; i < str.size(); i++)
	{
		input = str[i];
		if (std::regex_search(input, result, regexp)) {
			input = str[i-1];
			if (std::regex_search(input, result, regexp2)) {
				str = str.substr(0, i) + ' ' + str.substr(i, str.size() - i);
			}
		}
	}
	return str;
}

std::string RemoveExcessiveSpaces(std::string str) {
	int jie;
	for (size_t i = 0; i < str.size() - 1; i++)
	{
		if (str[i] == ' ') {
			i++;
			for (size_t x = i; x < str.size(); x++) {
				if (str[x] != ' ') {
					jie = x;
					break;
				}
				if (x == (str.size() - 1)) {
					jie = x;
				}
			}
			str = str.substr(0, i) + str.substr(jie, str.size() - jie);
		}
	}
	return str;
}

std::map<std::string, int>Control_Param;
void InitParam() {
	Control_Param = {
		{ "Replacement", 0 },
		{ "TextReplacement", 1 },
		{ "TextDeletion",2 },
		{ "LeaveOnlyLetters", 3 },
		{ "WordSeparation", 4 },
		{ "RemoveExcessiveSpaces", 5 },
	};
}

std::string Opcode(std::string str, const char* CodeMod) {
	std::ifstream infile(CodeMod);//打开指令文本
	// 检查文件是否成功打开
	if (!infile.is_open()) {
		std::cout << "文件打开失败！" << std::endl;
		return str;
	}
	// 逐行读取文件内容并输出
	std::string line;//读取一行指令
	std::vector<std::string> lineS;//拆分这行指令
	while (getline(infile, line)) {
		if (line.size() != 0) {//判断存在指令
			line = RemoveExcessiveSpaces(line);//把间隔空格处理的只剩下一个
			for (size_t i = 0; i < line.size(); i++)
			{
				if (line[i] == ' ') {
					lineS.push_back(line.substr(0, i));//把内容提取出来
					line = line.substr(i+1, line.size()-1-i);//把已经提取的内容出除
					i = 0;//从头开始处理
				}
			}
			lineS.push_back(line);//把最后的指令加入
			str = CodeExplain(str, lineS);//执行这行指令
			lineS.clear();//清空指令参数
		}
	}
	return str;
}

std::string CodeExplain(std::string str, std::vector<std::string> Code) {
	switch (Control_Param[Code[0]])
	{
		case 0:
			str = Replacement(str, Code[1], Code[2], Code[3], Code[4], TOOL::BoolConverter(Code[5]), TOOL::BoolConverter(Code[6]));
			break;
		case 1:
			str = TextReplacement(str, Code[1], Code[2]);
			break;
		case 2:
			str = TextDeletion(str, Code[1]);
			break;
		case 3:
			str = LeaveOnlyLetters(str);
			break;
		case 4:
			str = WordSeparation(str);
			break;
		case 5:
			str = RemoveExcessiveSpaces(str);
			break;
		default:
			break;
	}
	return str;
}