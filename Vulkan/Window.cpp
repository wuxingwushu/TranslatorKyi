#include "Window.h"
#include "../application.h"
#include "../Tool/Tool.h"

GAME::Application* mAppcpp;

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

		HWND hwnd = CreateWindowEx(
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

		//系统托盘创建
		NOTIFYICONDATA nidApp = { sizeof(nidApp) };
		nidApp.hWnd = hwnd;
		nidApp.uID = 1;
		nidApp.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
		nidApp.uCallbackMessage = WM_USER + 1;
		lstrcpyn(nidApp.szTip, TEXT("TranslatorKyi"), ARRAYSIZE(nidApp.szTip));//鼠标停在系统托盘图标上的提示（原来是空字符串）

		//图标一律优先取 exe 自己带的资源（resource.rc 里：IDI_ICON1 ICON "product.ico"）。
		//原来这里写的是 LoadImage(NULL, TEXT("product.ico"), ..., LR_LOADFROMFILE)：
		//那是「当前工作目录下的 product.ico」，而 CMake 只把 product.ico 复制到构建树根
		//（build/release），exe 所在目录（build/release/Release）里并没有这个文件 ——
		//于是 LoadImage 返回 NULL，托盘里图标位置在、右键也能弹菜单，但图完全是透明的，
		//就是「图标看不见」的原因。从资源里取图标就不会再依赖工作目录。
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
		if (TOOL::logger != nullptr) {
			if (TrayOk) {
				TOOL::logger->info("托盘图标已添加，图标来源：{}", IconSource);
			}
			else {
				TOOL::logger->error("Shell_NotifyIcon 失败，GetLastError={}，图标来源：{}", GetLastError(), IconSource);
			}
		}
	}

	//销毁Window
	Window::~Window() {
		glfwDestroyWindow(mWindow);//回收GLFW的API
		glfwTerminate();
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
			exit(0);
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
