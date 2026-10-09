#pragma once
#include <angelscript.h>
#include <thread>
#include <atomic>
#include "scriptbuilder.h"      //拓展 #include
#include "../Function/Translate.h"

extern Translate* AngelScriptTranslate;

namespace AngelScriptOpcode {

	class AngelScriptCode
	{
	public:
		static AngelScriptCode* GetAngelScriptCode() {
			if (mAngelScriptCode == nullptr) {
				mAngelScriptCode = new AngelScriptCode();
			}
			return mAngelScriptCode;
		}

		~AngelScriptCode();


		//同步跑脚本（替换模式 Ctrl+Alt+R：必须立刻拿到译文才能粘贴）
		void RunFunction(asIScriptFunction* Function);

		//异步跑脚本（截图/划词）：整个脚本在后台线程里跑完，主循环不阻塞，
		//所以「翻译中」这段时间窗口照样能拖、能关。
		//注意不能用 context->Suspend() 等后台翻译结果：AngelScript 恢复时不会重新调用
		//被挂起的系统函数（TranslateAPI），脚本拿到的是挂起前那个空返回值。
		bool BeginRun(asIScriptFunction* Function);
		bool IsRunning() const { return mScriptRunning.load(); }
		//脚本还在后台跑就等它跑完（别的快捷键要用 Variable::eng/zhong，别和脚本线程抢）
		void WaitRunning() { if (mScriptRunning.load() && mScriptThread.joinable()) { mScriptThread.join(); } }

		bool GetOpenBool() {
			return mOpen;
		}


		// 获取脚本函数句柄
		//替换
		asIScriptFunction* ReplaceFunction = nullptr;
		//截图
		asIScriptFunction* ScreenshotFunction = nullptr;
		//选择
		asIScriptFunction* ChoiceFunction = nullptr;
	private:
		static AngelScriptCode* mAngelScriptCode;
		AngelScriptCode();

		void Register();

		void GetFunction(asIScriptFunction** Function, std::string str);

		//真正执行脚本（RunFunction 和 BeginRun 的后台线程都调它）
		void RunScriptFunction(asIScriptFunction* Function);

		//脚本开关
		bool mOpen = false;

		//后台脚本线程
		std::thread mScriptThread;
		std::atomic<bool> mScriptRunning{ false };

		// AngelScript引擎
		asIScriptEngine* engine = nullptr;
		// 创建一个新的脚本上下文
		asIScriptContext* context = nullptr;
		// 编译脚本
		asIScriptModule* module = nullptr;
		CScriptBuilder* builder = nullptr;
	};

}