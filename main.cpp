#include "application.h"
/*
  ／l、
（ﾟ､ 。７
　l、  ~ヽ
　じしf_, )ノ 
*/
int main() {
	//防止软件多开
	if (FindWindow(NULL, "TranslatorKyi"))
	{
		MessageBoxEx(NULL, TEXT("ヽ(*。>Д<)o゜"), TEXT("已经启动啦！"), MB_OK, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
		return FALSE;
	}

	TOOL::SpdLogInit();

	Variable::ReadFile(iniData);

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