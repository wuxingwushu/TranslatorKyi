#include "application.h"
#include "Vulkan/instance.h"
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

	//Vulkan ICD 自动兜底：机器上没装显卡的 Vulkan 驱动时，把系统里自带的 SwiftShader
	//（Edge/Chrome/Android SDK 带的那份）指给 loader，自动切到 CPU 软件渲染，保证程序能起来。
	//调用时机是硬要求，必须在这里：
	//  1) 早于 Application 里的 Vulkan 初始化（那才是真正 vkCreateInstance 的地方）；
	//  2) 早于 Window 构造里的 glfwInit()——GLFW 在"有 loader 但没有 ICD"的机器上会
	//     vulkan.c 里直接 return GLFW_FALSE，导致 glfwGetRequiredInstanceExtensions 返回 NULL，
	//     Instance 拿到一张空的扩展表（这就是本次 VK_ERROR_INCOMPATIBLE_DRIVER 的完整因果链）。
	//它自己会先探测机器上有没有可用的显卡，有显卡就什么都不改（只在没显卡/设置选了 CPU 时才动）。
	VulKan::ensureVulkanIcdAvailable();

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