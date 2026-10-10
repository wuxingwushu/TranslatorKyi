#include "AngelScriptCode.h"
#include "scriptstdstring.h"    //拓展 string
#include "scriptbuilder.h"      //拓展 #include
#include "../Variable.h"
#include <iostream>
#include "FunctionalFunctions.h"
#include "../Tool/Tool.h"



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

//脚本里调用的翻译接口：脚本现在整个跑在后台线程里（AngelScriptCode::BeginRun），
//所以这里直接同步翻译，主循环不会被它占住。
//注意：不能用 context->Suspend() 去等后台结果 —— AngelScript 恢复执行时不会重新调用
//被挂起的系统函数，脚本拿到的是挂起前那个返回值（空串），译文就会是空的。
std::string TranslateAPI(std::string str) {
    if (AngelScriptTranslate == nullptr) { return str; }
    return AngelScriptTranslate->TranslateAPI(str);
}


namespace AngelScriptOpcode {

    //单例的唯一持有者：函数内 static，进程退出时由运行时析构（不再像原来那样只 new 不 delete）
    std::unique_ptr<AngelScriptCode>& AngelScriptCode::InstancePtr() {
        static std::unique_ptr<AngelScriptCode> sInstance{ new AngelScriptCode() };
        return sInstance;
    }

    AngelScriptCode* AngelScriptCode::GetAngelScriptCode() {
        return InstancePtr().get();
    }

    void AngelScriptCode::ResetAngelScriptCode() {
        //先析构旧的（会 join 后台脚本线程并 Release context/engine），再按新脚本文件重建
        InstancePtr().reset(new AngelScriptCode());
    }

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

    //真正执行脚本：RunFunction（主线程，同步）和 BeginRun 的后台线程都调它
    void AngelScriptCode::RunScriptFunction(asIScriptFunction* Function) {
        if (Function == nullptr || context == nullptr) { return; }

        // 准备执行脚本
        int r = context->Prepare(Function);
        if (r < 0) {
            std::cout << "Failed to prepare script context" << std::endl;
            TOOL::logger->warn("script prepare failed, r={}", r);
            return;
        }

        // 执行脚本（脚本里的 TranslateAPI() 是同步的，可能跑很久）
        r = context->Execute();
        if (r != asEXECUTION_FINISHED) {
            std::cout << "Failed to execute script" << std::endl;
            TOOL::logger->warn("script execute failed, r={}", r);
        }
    }

    void AngelScriptCode::RunFunction(asIScriptFunction* Function) {
        //同步入口（替换模式）：要是后台脚本还在跑，先等它跑完，免得两个线程同时用一个 context
        WaitRunning();
        RunScriptFunction(Function);
    }

    bool AngelScriptCode::BeginRun(asIScriptFunction* Function) {
        if (Function == nullptr || context == nullptr) { return false; }
        if (mScriptRunning.load()) { return false; }//上一次还在跑（AI 翻译很慢），别用同一个 context 再开一个
        if (mScriptThread.joinable()) { mScriptThread.join(); }

        mScriptRunning = true;
        mScriptThread = std::thread([this, Function]() {
            //线程里逃出的异常会走 std::terminate → abort()，在 Windows 上就是
            //「出错模块 ucrtbase.dll、异常代码 0xc0000409」这种静默崩溃。
            //脚本里的 TranslateAPI 会调到翻译/网络/AI 那套代码，必须在这里兜住，
            //否则一旦抛异常，日志里什么都看不到，界面还会一直以为脚本在跑。
            try {
                RunScriptFunction(Function);
            }
            catch (const std::exception& e) {
                if (TOOL::logger) { TOOL::logger->error("脚本线程异常：{}", e.what()); }
            }
            catch (...) {
                if (TOOL::logger) { TOOL::logger->error("脚本线程异常：不是 std::exception 类型的异常"); }
            }
            mScriptRunning = false;
        });
        return true;
    }


	AngelScriptCode::~AngelScriptCode() {
        //后台脚本可能还在跑（比如 AI 翻译很慢）：先等它跑完，再释放 context/engine，免得踩空
        if (mScriptThread.joinable()) { mScriptThread.join(); }
        // 释放资源
        if (context != nullptr) { context->Release(); }
        if (engine != nullptr) { engine->ShutDownAndRelease(); }
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