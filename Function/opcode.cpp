#include "opcode.h"

template <typename T>
T Converter(const std::string& s) {
	try {
		T v{};
		std::istringstream _{ s };
		_.exceptions(std::ios::failbit);
		_ >> v;
		return v;
	}
	catch (std::exception& e) {
		throw std::runtime_error("cannot parse value '" + s + "' to type<T>.");
	};
}

bool BoolConverter(std::string s) {
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
			if (i < str.size()-1) {
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

Stu* STu;
HINSTANCE Dll;
MyFunction myFunction;
std::map<std::string, Fenum>Control_Param;

void InitOpcode(unsigned int Max) {

	STu = new Stu;
	STu->boolS = new PileUp<bool>(Max);
	STu->charS = new PileUp<char>(Max);
	STu->intS = new PileUp<int>(Max);
	STu->floatS = new PileUp<float>(Max);
	STu->doubleS = new PileUp<double>(Max);
	STu->stringS = new PileUp<std::string>(Max);

	Control_Param = {
		{ "Bool", Bool },
		{ "Char", Char },
		{ "Int",Int },
		{ "Float", Float },
		{ "Double", Double },
		{ "String", String },
		{ "Replacement", F_Replacement },
		{ "TextReplacement", F_TextReplacement },
		{ "TextDeletion",F_TextDeletion },
		{ "LeaveOnlyLetters", F_LeaveOnlyLetters },
		{ "WordSeparation", F_WordSeparation },
		{ "RemoveExcessiveSpaces", F_RemoveExcessiveSpaces },
		{ "UppercaseStart", F_UppercaseStart },
		{ "DeletionSpaces", F_DeletionSpaces },
		{ "NewDLL", NewDLL },
		{ "DLL", DLL },
		{ "DeleteDLL", DeleteDLL },
		{ "New", New },
		{ "Set", Set },
		{ "Get",Get },
		{ "Delete", Delete },
	};
}

std::string Opcode(std::string str, const char* CodeMod) {
	std::ifstream infile(CodeMod);//打开指令文本
	// 检查文件是否成功打开
	if (!infile.is_open()) {
		std::cout << "文件打开失败！" << std::endl;
		// 关闭文件
		infile.close();
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
	// 关闭文件
    infile.close();
	return str;
}

std::string CodeExplain(std::string str, std::vector<std::string> Code) {
	switch (Control_Param[Code[0]])
	{
		case Bool:
			STu->boolS->add(BoolConverter(Code[1]));
			break;
		case Char:
			STu->charS->add(Converter<char>(Code[1]));
			break;
		case Int:
			STu->intS->add(Converter<int>(Code[1]));
			break;
		case Float:
			STu->floatS->add(Converter<float>(Code[1]));
			break;
		case Double:
			STu->doubleS->add(Converter<double>(Code[1]));
			break;
		case String:
			STu->stringS->add(Code[1]);
			break;
		case F_Replacement:
			str = Replacement(str, Code[1], Code[2], Code[3], Code[4], BoolConverter(Code[5]), BoolConverter(Code[6]));
			break;
		case F_TextReplacement:
			str = TextReplacement(str, Code[1], Code[2]);
			break;
		case F_TextDeletion:
			str = TextDeletion(str, Code[1]);
			break;
		case F_LeaveOnlyLetters:
			str = LeaveOnlyLetters(str);
			break;
		case F_WordSeparation:
			str = WordSeparation(str);
			break;
		case F_RemoveExcessiveSpaces:
			str = RemoveExcessiveSpaces(str);
			break;
		case F_UppercaseStart:
			str = UppercaseStart(str);
			break;
		case F_DeletionSpaces:
			str = DeletionSpaces(str);
			break;
		case NewDLL:
			std::cout << Code[1] << std::endl;
			Dll = LoadLibrary(Code[1].c_str());
			if (!Dll) {
				std::cout << "打开" << Code[1] << "失败！" << std::endl;
			}
			break;
		case DLL:
			myFunction = (MyFunction)GetProcAddress(Dll, Code[1].c_str());
			if (myFunction != NULL) {
				myFunction(STu);
			}
			else {
				std::cout << "DLL" << Code[1] << "Error" << std::endl;
			}
			break;
		case DeleteDLL:
			if (Dll) {
				FreeLibrary(Dll);
				Dll = NULL;
			}
			break;
		default:
			break;
	}
	return str;
}