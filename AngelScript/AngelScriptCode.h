#pragma once
#include <angelscript.h>
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


		void RunFunction(asIScriptFunction* Function);

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

		//脚本开关
		bool mOpen = false;

		// AngelScript引擎
		asIScriptEngine* engine = nullptr;
		// 创建一个新的脚本上下文
		asIScriptContext* context = nullptr;
		// 编译脚本
		asIScriptModule* module = nullptr;
		CScriptBuilder* builder = nullptr;
	};

}