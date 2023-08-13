#pragma once
#include "../base.h"
#include "PileUp.h"
#include "../AngelScript/FunctionalFunctions.h"

template <typename T>
T Converter(const std::string& s);

bool BoolConverter(std::string s);

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
