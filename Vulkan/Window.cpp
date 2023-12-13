#include "window.h"
#include "../application.h"

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
		//strncpy_s(nidApp.szTip, TEXT("MyNameIsTranslatorJi!"), sizeof(TEXT("MyNameIsTranslatorJi!")));//鼠标停止系统托盘上的提示
		nidApp.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
		nidApp.uCallbackMessage = WM_USER + 1;
		nidApp.hIcon = (HICON)LoadImage(NULL, TEXT("product.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);//ico 图片 只支持32x32  ,  16x16
		Shell_NotifyIcon(NIM_ADD, &nidApp);
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