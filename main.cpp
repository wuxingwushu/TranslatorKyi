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
	/*std::cout << "结果：" << translate("Variable", "auto", "ja");
	return 0;*/
	//防止软件多开
	if (FindWindow(NULL, "TranslatorKyi"))
	{
		MessageBoxEx(NULL, TEXT("Software Started"), TEXT("Error"), MB_OK, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
		return FALSE;
	}

	Variable::ReadFile(iniData);
	Language::ReadFile(Variable::Language);

	TOOL::SpdLogInit();

	GAME::Application* app = new GAME::Application();

	VulKan::Window* mWin = new VulKan::Window(app->mWidth, app->mHeight, 0, 0);

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