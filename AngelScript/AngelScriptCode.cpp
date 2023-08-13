#include "AngelScriptCode.h"
#include "scriptstdstring.h"    //拓展 string
#include "scriptbuilder.h"      //拓展 #include
#include "../Variable.h"
#include <iostream>
#include "FunctionalFunctions.h"



void print(std::string str) {
    std::cout << str << std::endl;
}

std::string GetInput() {
    return Variable::eng;
}

void SetInput(std::string str) {
    Variable::eng = str;
}

void SetOutput(std::string str) {
    Variable::zhong = str;
}

Translate* AngelScriptTranslate = nullptr;

std::string TranslateAPI(std::string str) {
    return AngelScriptTranslate->TranslateAPI(str);
}

HINSTANCE DLL;
void LoadingDLL(std::string str) {
    DLL = LoadLibrary(str.c_str());
    if (!DLL) {
        std::cout << "Open" << str << "Fail!" << std::endl;
    }
}

FARPROC GetDLLFunction(std::string str) {
    return GetProcAddress(DLL, str.c_str());
}

void DeleteDLL() {
    if (DLL) {
        FreeLibrary(DLL);
        DLL = NULL;
    }
}


namespace AngelScriptOpcode {

    AngelScriptCode* AngelScriptCode::mAngelScriptCode = nullptr;

    void AngelScriptMessage(asSMessageInfo* msg, void* param) {
        if (msg->type == asMSGTYPE_ERROR) {
            std::cout << "Error:[" << msg->message << "]" << "Row:[" << msg->row << "]" << std::endl;
        }

        if (msg->type == asMSGTYPE_INFORMATION) {
            std::cout << "Info:[" << msg->message << "]" << "Row:[" << msg->row << "]" << std::endl;
        }

        if (msg->type == asMSGTYPE_WARNING) {
            std::cout << "Warning:[" << msg->message << "]" << "Row:[" << msg->row << "]" << std::endl;
        }
    }

    void AngelScriptCode::GetFunction(asIScriptFunction** Function, std::string str) {
        (*Function) = builder->GetModule()->GetFunctionByDecl(str.c_str());
        if (!(*Function)) {
            std::cout << "Failed to get script function: " << str <<  std::endl;
            context->Release();
            engine->ShutDownAndRelease();
            return;
        }
    }

	AngelScriptCode::AngelScriptCode() {
        // 创建AngelScript引擎
        engine = asCreateScriptEngine();
        //AngelScript引擎添加 string 类拓展
        RegisterStdString(engine);
        

        // 消息回调 脚本执行错误之类的
        int r = engine->SetMessageCallback(asFUNCTION(AngelScriptMessage), 0, asCALL_CDECL);
        if (r < 0) {
            std::cout << "Failed to set message callback" << std::endl;
            return;
        }

        //注册信息
        Register();

        // 创建一个新的脚本上下文
        context = engine->CreateContext();

        // 读取脚本文件
        std::ifstream file("./Opcode/" + Variable::Script + ".as");
        if (!file.is_open()) {
            std::cout << "Failed to open script file" << std::endl;
            context->Release();
            engine->ShutDownAndRelease();
            mOpen = false;
            return;
        }
        else
        {
            mOpen = true;
        }

        // 将脚本内容读取到字符串中
        std::string scriptCode((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // 创建 ScriptBuilder
        builder = new CScriptBuilder();
        // 绑定 ScriptBuilder 到引擎
        builder->StartNewModule(engine, "MyModule");
        // 导入代码
        builder->AddSectionFromMemory("MyModule", &scriptCode[0]);
        // 编译脚本
        builder->BuildModule();

        // 获取脚本函数句柄
        GetFunction(&ReplaceFunction, "void ReplaceFunction()");
        GetFunction(&ScreenshotFunction, "void ScreenshotFunction()");
        GetFunction(&ChoiceFunction, "void ChoiceFunction()");

        delete builder;
	}

    void AngelScriptCode::RunFunction(asIScriptFunction* Function) {
        int r;
        // 准备执行脚本
        r = context->Prepare(Function);
        if (r < 0) {
            std::cout << "Failed to prepare script context" << std::endl;
            context->Release();
            engine->ShutDownAndRelease();
            return;
        }

        // 执行脚本
        r = context->Execute();
        if (r != asEXECUTION_FINISHED) {
            std::cout << "Failed to execute script" << std::endl;
        }
    }


	AngelScriptCode::~AngelScriptCode() {
        // 释放资源
        context->Release();
        engine->ShutDownAndRelease();
        mAngelScriptCode = nullptr;
	}


    void AngelScriptCode::Register() {
        AngelScriptRegister(engine);

        int r;
        // 注册要在脚本中调用的函数
        r = engine->RegisterGlobalFunction("void print(string)", asFUNCTION(print), asCALL_CDECL);
        if (r < 0) {
            std::cout << "Failed to register global function: print" << std::endl;
            engine->ShutDownAndRelease();
            return;
        }

        /*LoadingDLL("./Opcode/D.dll");

        r = engine->RegisterGlobalFunction("void ErrorLog()", asFUNCTION(GetDLLFunction("ErrorLog")), asCALL_CDECL);
        if (r < 0) {
            std::cout << "Failed to register global function: ErrorLog" << std::endl;
            engine->ShutDownAndRelease();
            return;
        }

        r = engine->RegisterGlobalFunction("void print(int64)", asFUNCTION(GetDLLFunction("print")), asCALL_CDECL);
        if (r < 0) {
            std::cout << "Failed to register global function: DLLprint" << std::endl;
            engine->ShutDownAndRelease();
            return;
        }*/


        r = engine->RegisterGlobalFunction("void SetOutput(string)", asFUNCTION(SetOutput), asCALL_CDECL);
        if (r < 0) {
            std::cout << "Failed to register global function: SetOutput" << std::endl;
            engine->ShutDownAndRelease();
            return;
        }

        r = engine->RegisterGlobalFunction("void SetInput(string)", asFUNCTION(SetInput), asCALL_CDECL);
        if (r < 0) {
            std::cout << "Failed to register global function: SetInput" << std::endl;
            engine->ShutDownAndRelease();
            return;
        }

        r = engine->RegisterGlobalFunction("string GetInput()", asFUNCTION(GetInput), asCALL_CDECL);
        if (r < 0) {
            std::cout << "Failed to register global function: GetInput" << std::endl;
            engine->ShutDownAndRelease();
            return;
        }

        r = engine->RegisterGlobalFunction("string TranslateAPI(string)", asFUNCTION(TranslateAPI), asCALL_CDECL);
        if (r < 0) {
            std::cout << "Failed to register global function: TranslateAPI" << std::endl;
            engine->ShutDownAndRelease();
            return;
        }


        //r = engine->RegisterGlobalProperty("string eng", &Variable::eng);//注册变量
        //if (r < 0) {
        //    std::cout << "Failed to register global Property: string eng" << std::endl;
        //    engine->ShutDownAndRelease();
        //    return;
        //}
    }

}