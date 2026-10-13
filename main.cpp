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
	//控制台的中文显示：程序内部（含 spdlog 的控制台 sink 和日志文件）统一用的是 UTF-8，
	//而中文系统新建的控制台默认代码页是 936 —— 不改这里，屏幕上看到的日志就是
	//“鎺㈡祴”这种乱码（文件里的字节其实是对的，只有屏幕显示错）。
	//没有控制台（比如当纯 GUI 启动）时这两个调用只是失败返回，不影响后面的逻辑。
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	//防止软件多开：用命名互斥量判定，不能用 FindWindow(NULL, "TranslatorKyi")。
	//原因（实测踩过）：FindWindow 只按窗口标题匹配，会命中标题恰好也叫 "TranslatorKyi" 的
	//资源管理器文件夹窗口（class=CabinetWClass）——于是明明没有任何本程序实例在跑，
	//程序也会弹 "Software Started" 然后直接退出；而且这一步在 SpdLogInit() 之前，
	//连日志都不会留下，现场表现为"启动约 3 秒后静默退出、logs/Error.txt 一字节不增"。
	//互斥量的语义正好是这里要的：
	//  1) 只认本程序创建的命名内核对象，任何同名窗口（Explorer、记事本……）都干扰不了；
	//  2) 进程无论正常退出还是崩溃，句柄都由内核回收，不会像锁文件那样留下"僵尸锁"。
	//不用"按窗口类名 FindWindow"是因为那要依赖 GLFW 的内部类名（"GLFW30" 之类），
	//属于实现细节，GLFW 升级就可能变；互斥量没有这个耦合。
	//句柄故意不关闭：要一直持有到进程结束，提前 CloseHandle 等于提前放行第二个实例。
	static HANDLE SingleInstanceMutex = CreateMutexW(NULL, TRUE, L"TranslatorKyi.SingleInstance");
	if (SingleInstanceMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
	{
		//已有实例持有同名互斥量。注意 CreateMutex 在"已存在"时并不会把所有权给我们，
		//所以这里只能关句柄，不能 ReleaseMutex。
		CloseHandle(SingleInstanceMutex);
		ShowUtf8MessageBox("Software Started", "Error");
		return FALSE;
	}
	//CreateMutex 失败（极罕见，比如内核对象耗尽）时不拦启动：宁可多开一个，
	//也不要让用户完全打不开程序。

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
		TOOL::logger->error("主循环异常退出：{}", e.what());
	}
	catch (...) {
		//不是 std::exception 的异常（比如别的库里抛的字符串/整数）原来会一路穿到
		//std::terminate → abort()，表现为 ucrtbase.dll 的 0xc0000409 静默崩溃。
		ShowUtf8MessageBox("Unknown exception", "main");
		TOOL::logger->error("主循环异常退出：不是 std::exception 类型的异常");
	}

	//日志立刻刷盘：spdlog 默认不自动 flush，不刷的话退出前的最后几行会随进程一起消失
	TOOL::logger->flush();

	//退出路标（直接写文件，见 Tool/Log.cpp）：崩溃时靠它定位崩在哪一步
	TOOL::LogStep("main: 删除窗口");
	delete mWin;
	TOOL::LogStep("main: 删除 Application");
	delete app;
	TOOL::LogStep("main: 结束");

	return 0;
}