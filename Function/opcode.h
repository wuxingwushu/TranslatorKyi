#pragma once
#include "../base.h"
#include <regex>//正则表达式
#include "PileUp.h"

template <typename T>
T Converter(const std::string& s);

bool BoolConverter(std::string s);


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
std::string UppercaseStart(std::string str);//开头字母大写化(空格后面那个字母大写化)
std::string DeletionSpaces(std::string str);//删除所有空格


enum Fenum {
    Bool,
    Char,
    Int,
    Float,
    Double,
    String,
    F_Replacement,
    F_TextReplacement,
    F_TextDeletion,
    F_LeaveOnlyLetters,
    F_WordSeparation,
    F_RemoveExcessiveSpaces,
    F_UppercaseStart,
    F_DeletionSpaces,
    NewDLL,
    DLL,
    DeleteDLL,
    New,
    Set,
    Get,
    Delete,
};

extern std::map<std::string, Fenum>Control_Param;

extern Stu* STu;
extern HINSTANCE Dll;
typedef void (*MyFunction)(Stu*);
extern MyFunction myFunction;
void InitOpcode(unsigned int Max);
std::string Opcode(std::string str, const char* CodeMod);//操作码读取
std::string CodeExplain(std::string str, std::vector<std::string> Code);//操作码解释器
