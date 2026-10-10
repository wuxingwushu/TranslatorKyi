#include "opcode.h"
#include "../Tool/Convert.h"//类型转换工具（原在本文件内重复定义了一份）

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
		std::cout << "File Open Failed!" << std::endl;
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
	//边界检查：原来 Code[0]/Code[1] 直接索引，空行或残缺指令会越界崩溃。
	//先按指令名查该指令所需的最小参数个数（含指令本身），不足就跳过这行。
	if (Code.empty()) { return str; }
	static const std::map<std::string, size_t> MinArgs = {
		{ "Bool", 2 }, { "Char", 2 }, { "Int", 2 }, { "Float", 2 }, { "Double", 2 }, { "String", 2 },
		{ "Replacement", 7 }, { "TextReplacement", 3 }, { "TextDeletion", 2 },
		{ "LeaveOnlyLetters", 1 }, { "WordSeparation", 1 }, { "RemoveExcessiveSpaces", 1 },
		{ "UppercaseStart", 1 }, { "DeletionSpaces", 1 },
		{ "NewDLL", 2 }, { "DLL", 2 }, { "DeleteDLL", 1 },
		{ "New", 3 }, { "Set", 3 }, { "Get", 3 }, { "Delete", 2 },
	};
	const auto Min = MinArgs.find(Code[0]);
	if (Min == MinArgs.end() || Code.size() < Min->second) { return str; }

	switch (Control_Param[Code[0]])
	{
		case Bool:
			STu->boolS->add(TOOL::BoolConverter(Code[1]));
			break;
		case Char:
			STu->charS->add(TOOL::Converter<char>(Code[1]));
			break;
		case Int:
			STu->intS->add(TOOL::Converter<int>(Code[1]));
			break;
		case Float:
			STu->floatS->add(TOOL::Converter<float>(Code[1]));
			break;
		case Double:
			STu->doubleS->add(TOOL::Converter<double>(Code[1]));
			break;
		case String:
			STu->stringS->add(Code[1]);
			break;
		case F_Replacement:
			str = Replacement(str, Code[1], Code[2], Code[3], Code[4], TOOL::BoolConverter(Code[5]), TOOL::BoolConverter(Code[6]));
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
				std::cout << "Open" << Code[1] << "Fail!" << std::endl;
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