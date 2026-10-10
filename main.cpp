#include "application.h"
#include "Vulkan/instance.h"
#include "Tool/Charset.h"//TOOL::Utf8ToWide：错误弹窗的 UTF-8 → 宽字符
#include <windows.h>
#include <string>
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
//错误弹窗用的 UTF-8 文本转宽字符封装。
//原因：CMake 里没有定义 UNICODE/_UNICODE，TEXT() 就是普通窄字符串、MessageBoxEx 会选到 ANSI 版，
//而程序内部的错误信息（e.what()、日志里的中文）都是 UTF-8 字节 —— 交给 ANSI 版就等于让系统
//按 936 代码页去解释 UTF-8，弹出来的中文必然是乱码。所以统一转成宽字符再用 MessageBoxExW。
static int ShowUtf8MessageBox(const char* Text, const char* Title)
{
	const std::wstring WideText = (Text != nullptr) ? TOOL::Utf8ToWide(Text) : std::wstring();
	const std::wstring WideTitle = (Title != nullptr) ? TOOL::Utf8ToWide(Title) : std::wstring();
	return (int)MessageBoxExW(NULL, WideText.c_str(), WideTitle.c_str(), MB_OK, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
}

int main() {
	/*std::cout << "结果：" << translate("Variable", "auto", "ja");
	return 0;*/

	//控制台的中文显示：程序内部（含 spdlog 的控制台 sink 和日志文件）统一用的是 UTF-8，
	//而中文系统新建的控制台默认代码页是 936 —— 不改这里，屏幕上看到的日志就是
	//“鎺㈡祴”这种乱码（文件里的字节其实是对的，只有屏幕显示错）。
	//没有控制台（比如当纯 GUI 启动）时这两个调用只是失败返回，不影响后面的逻辑。
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	//防止软件多开
	if (FindWindow(NULL, "TranslatorKyi"))
	{
		ShowUtf8MessageBox("Software Started", "Error");
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
		ShowUtf8MessageBox(e.what(), "main");
		TOOL::logger->error(e.what());
		//std::cout << "main: " << e.what() << std::endl;
	}

	delete mWin;
	delete app;

	return 0;
}