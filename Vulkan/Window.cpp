#include "Window.h"
#include "../application.h"
#include "../Tool/Tool.h"
#include <cstdlib>//std::atexit

GAME::Application* mAppcpp;

namespace {
	//托盘图标的状态放在文件作用域，而不是 Window 成员：程序里现在只剩一条退出路径是 exit(0)
	//（ImGui/Interface/SettingsPanel.cpp 的 SaveSettings()「改字体→重启自己」走的 RestartSelfApplication()），
	//exit(0) 不会执行 ~Window()，只有 atexit 注册的函数一定会被调用，
	//所以清理函数必须能从文件作用域拿到 hWnd/uID。
	//（菜单里的「退出」现在只置 InterFace->ExitRequestBool、ESC 置 glfwSetWindowShouldClose，
	//  这两条都会跳出主循环、正常析构到 ~Window()。）
	HWND gTrayHwnd = NULL;
	NOTIFYICONDATA gTrayNid{};

	//摘掉托盘图标并销毁回调窗口。幂等：~Window() 和 atexit 兜底都会调它。
	void RemoveTrayIcon() {
		if (gTrayHwnd == NULL) {
			return;
		}
		gTrayNid.uFlags = 0;//NIM_DELETE 只认 hWnd/uID，uFlags 必须清零
		Shell_NotifyIcon(NIM_DELETE, &gTrayNid);
		DestroyWindow(gTrayHwnd);
		gTrayHwnd = NULL;
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_USER + 1:
		switch (lParam)
		{
		case WM_RBUTTONUP:
			mAppcpp->InterFace->SetInterFace(GAME::InterFaceEnum::MenuEnum);
			break;
		case WM_LBUTTONUP:
			//exit(0);
			break;
		}
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

namespace VulKan {

	//获取窗口大小是否改变
	static void windowResized(GLFWwindow* window, int width, int height) {
		auto pUserData = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		pUserData->mWindowResized = true;
	}

	Window::Window(const int& width, const int& height, bool MouseBool, bool FullScreen) {
		mWidth = width;
		mHeight = height;
		MouseDisabled = MouseBool;

		glfwInit();

		//设置环境，关掉opengl API 并且禁止窗口改变大小
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//关掉opengl API
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);//是否禁止窗口改变大小
		glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);//窗口显示
		//glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);//窗口置顶
		GLFWmonitor* pMonitor = FullScreen ? glfwGetPrimaryMonitor() : NULL;
		mWindow = glfwCreateWindow(mWidth, mHeight, "TranslatorKyi", pMonitor, nullptr);//创建一个窗口
		if (!mWindow) {//判断窗口是否创建成功
			std::cerr << "Error: failed to create window" << std::endl;
		}

		SystemTray();//创建系统托盘
		
		


		//glfwSetWindowAttrib(mWindow, GLFW_FLOATING, GLFW_TRUE);//窗口置顶
		//glfwSetWindowOpacity(mWindow, 0.0f);//窗口透明度
		//glViewport(0, 0, width, height);
		glfwSetWindowUserPointer(mWindow, this);
		glfwSetFramebufferSizeCallback(mWindow, windowResized);//绑定窗口大小改变事件

		glfwHideWindow(mWindow);//隐藏窗口包括任务栏

		//GLFW_CURSOR_DISABLED 禁用鼠标
		if (MouseDisabled) {
			glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else {
			glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	void Window::SystemTray() {
		// 创建窗口
		WNDCLASS wc;
		ZeroMemory(&wc, sizeof(wc));
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = GetModuleHandle(NULL);
		wc.lpszClassName = "MyWindowClass";

		RegisterClass(&wc);

		gTrayHwnd = CreateWindowEx(
			0,
			"MyWindowClass",
			"My Window",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL,
			NULL,
			wc.hInstance,
			NULL
		);
		if (gTrayHwnd == NULL) {
			if (TOOL::logger != nullptr) {
				TOOL::logger->error("托盘回调窗口创建失败，GetLastError={}", GetLastError());
			}
			return;//没有回调窗口就挂不上托盘图标，提前退出，免得再白调一次 Shell_NotifyIcon
		}
		//exit(0) 型的退出路径拿不到 ~Window()，这里再挂一层 atexit 兜底，保证图标一定被摘掉。
		std::atexit(RemoveTrayIcon);

		//系统托盘创建
		NOTIFYICONDATA nidApp = { sizeof(nidApp) };
		nidApp.hWnd = gTrayHwnd;
		nidApp.uID = 1;
		//这里绝对不能带 NIF_GUID：一旦置了 NIF_GUID，图标身份就只认 guidItem（uID 被忽略），
		//而本程序从来没给 guidItem 赋值（全 0）。全 0 的 GUID 不是合法身份，Shell 直接拒绝：
		//实测 NIM_ADD 与 NIM_MODIFY 都返回 FALSE、GetLastError=2147500037(0x80004005 E_FAIL)，
		//结果就是通知区里连图标带菜单一起消失，日志里只剩 "Shell_NotifyIcon 失败"。
		//（官方文档的 troubleshooting 也提到：GUID 注册里带着二进制路径，exe 换目录后会失败。）
		//用 hWnd+uID 识别就一切正常。
		nidApp.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
		nidApp.uCallbackMessage = WM_USER + 1;
		lstrcpyn(nidApp.szTip, TEXT("TranslatorKyi"), ARRAYSIZE(nidApp.szTip));//鼠标停在系统托盘图标上的提示（原来是空字符串）

		//图标一律优先取 exe 自己带的资源（resource.rc 里：IDI_ICON1 ICON "product.ico"），
		//不要再依赖「当前工作目录下的 product.ico」：CMake 只把 product.ico 复制到构建树根
		//（build/release），exe 所在目录（build/release/Release）里不一定有，LoadImage 返回 NULL
		//时通知区里就是一个透明的空位。从 exe 资源取图标与工作目录无关。
		//（注意：这次「托盘整个消失」不是图标加载的问题，是下面的 NIF_GUID，见上。）
		int IconCx = GetSystemMetrics(SM_CXSMICON);//按系统小图标尺寸取，高 DPI 下会自动缩放
		int IconCy = GetSystemMetrics(SM_CYSMICON);
		HINSTANCE hInstance = GetModuleHandle(NULL);
		const char* IconSource = "exe 资源 IDI_ICON1";
		nidApp.hIcon = (HICON)LoadImage(hInstance, TEXT("IDI_ICON1"), IMAGE_ICON, IconCx, IconCy, LR_DEFAULTCOLOR);
		if (nidApp.hIcon == NULL) {
			IconSource = "工作目录下的 product.ico";
			nidApp.hIcon = (HICON)LoadImage(NULL, TEXT("product.ico"), IMAGE_ICON, IconCx, IconCy, LR_LOADFROMFILE | LR_DEFAULTCOLOR);
		}
		if (nidApp.hIcon == NULL) {
			IconSource = "系统默认图标（上面两种都失败）";
			nidApp.hIcon = LoadIcon(NULL, IDI_APPLICATION);//兜底：宁可显示系统默认图标，也不要一个透明的空位
		}

		BOOL TrayOk = Shell_NotifyIcon(NIM_ADD, &nidApp);
		if (!TrayOk) {
			TrayOk = Shell_NotifyIcon(NIM_MODIFY, &nidApp);//已经有同 id 的图标时 NIM_ADD 会失败，改成更新
		}
		gTrayNid = nidApp;//退出时要用同一份数据做 NIM_DELETE
		if (TOOL::logger != nullptr) {
			if (TrayOk) {
				TOOL::logger->info("托盘图标已添加，图标来源：{}", IconSource);
			}
			else {
				TOOL::logger->error("Shell_NotifyIcon 失败，GetLastError={}，图标来源：{}（通知区不会有任何图标）", GetLastError(), IconSource);
			}
		}
	}

	//销毁Window
	Window::~Window() {
		TOOL::LogStep("~Window: 开始");
		//先摘掉托盘图标、再销毁那个隐藏的回调窗口。顺序不能反，也不能漏：
		//不摘的话退出后通知区会留下「幽灵图标」，鼠标划过才消失。
		//正常退出（main.cpp 结尾 delete mWin）走到这里；exit(0) 型的退出由 atexit 注册的同一个函数兜底。
		RemoveTrayIcon();
		glfwDestroyWindow(mWindow);//回收GLFW的API
		glfwTerminate();
		TOOL::LogStep("~Window: 结束");
	}

	//判断窗口是否被关闭
	bool Window::shouldClose() {
		return glfwWindowShouldClose(mWindow);
	}

	//窗口获取事件
	void Window::pollEvents() {
		glfwPollEvents();
	}

	void Window::setApp(GAME::Application* app) {
		mApp = app;
		mAppcpp = app;
	}

	//键盘事件
	void Window::processEvent() {

		if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			//不再 exit(0)：和点窗口右上角关闭按钮一样置上关闭标志，主循环跳出后走 cleanUp()，
			//由析构函数等后台线程收尾（exit(0) 不会析构堆上的 Application/InterFace/Translate）。
			glfwSetWindowShouldClose(mWindow, GLFW_TRUE);
		}

		//控制鼠标显示和禁用
		if (glfwGetKey(mWindow, GLFW_KEY_P) == GLFW_PRESS) {
			if (MouseDisabled) {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				MouseDisabled = false;
			}
			else {
				glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				MouseDisabled = true;
			}
		}

		if (glfwGetKey(mWindow, GLFW_KEY_W) == GLFW_PRESS) {

		}

		if (glfwGetKey(mWindow, GLFW_KEY_S) == GLFW_PRESS) {
			
		}

		if (glfwGetKey(mWindow, GLFW_KEY_A) == GLFW_PRESS) {
			
		}

		if (glfwGetKey(mWindow, GLFW_KEY_D) == GLFW_PRESS) {
			
		}
	}
}
