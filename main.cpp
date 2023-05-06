#include "application.h"

int main() {
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

	mWin->~Window();
	app->~Application();

	return 0;
}