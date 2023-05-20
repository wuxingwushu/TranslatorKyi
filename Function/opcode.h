#include "../base.h"
#include <regex>//正则表达式




std::string Replacement(std::string str, std::string Replace, std::string Change, std::string Ftext, std::string Rtext, bool F, bool R);//对符合条件的字符串替换

std::string TextReplacement(std::string str, std::string Replace, std::string Change);//将检测到的的一个替换掉
std::string TextDeletion(std::string str, std::string Deletion);//将检测到的的一个删除

bool TextExists(std::string str, std::string text);//判断是否存在字符串
bool TextExistsConditionFrontT(std::string str, std::string text, std::string Ftext);//判断是否存在字符串且前面字符串符合条件（是）
bool TextExistsConditionFrontF(std::string str, std::string text, std::string Ftext);//判断是否存在字符串且前面字符串符合条件(不是)
bool TextExistsConditionRearT(std::string str, std::string text, std::string Rtext);//判断是否存在字符串且后面字符串符合条件（是）
bool TextExistsConditionRearF(std::string str, std::string text, std::string Rtext);//判断是否存在字符串且后面字符串符合条件(不是)
bool TextExistsConditionFrontToRear(std::string str, std::string text, std::string Ftext, std::string Rtext, bool F, bool R);



std::string LeaveOnlyLetters(std::string str);//把字母数字以外的符号删除
std::string WordSeparation(std::string str);//将符合条件的大写字母前面加空格
std::string RemoveExcessiveSpaces(std::string str);//删除多余的空格

extern std::map<std::string, int>Control_Param;//函数库枚举
void InitParam();
std::string Opcode(std::string str, const char* CodeMod);//操作码读取
std::string CodeExplain(std::string str, std::vector<std::string> Code);//操作码解释器
