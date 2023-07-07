#include "application.h"
/*
* 换了电脑编译说有语法错误：
*		目前知道的是：中文输出的问题。
*		解决方法：在中文结尾后加一个 “空格” 或  字母  。
*/
/*
  ／l、
（ﾟ､ 。７
　l、  ~ヽ
　じしf_, )ノ 
*/
int main() {
	Variable::ReadFile(iniData);
	Language::ReadFile(Variable::Language);

	//防止软件多开
	if (FindWindow(NULL, "TranslatorKyi"))
	{
		MessageBoxEx(NULL, TEXT("ヽ(*。>Д<)o゜"), TEXT(Language::FindWin.c_str()), MB_OK, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
		return FALSE;
	}

	TOOL::SpdLogInit();

	GAME::Application* app = new GAME::Application();

	GAME::VulKan::Window* mWin = new GAME::VulKan::Window(app->mWidth, app->mHeight, 0, 0);

	mWin->setApp(app);

	try {
		app->run(mWin);
	}
	catch (const std::exception& e) {
		MessageBoxEx(NULL, TEXT(e.what()), TEXT("main"), MB_OK, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
		TOOL::logger->error(e.what());
		//std::cout << "main: " << e.what() << std::endl;
	}

	delete mWin;
	delete app;

	return 0;
}