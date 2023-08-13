#pragma once

#include "../base.h"

namespace GAME {
	class Application;
}

namespace VulKan {

	class Window {
	public:


		//构建窗口
		Window(const int& width, const int& height, bool MouseBool, bool FullScreen);

		//解析释放
		~Window();

		//判断窗口是否被关闭
		bool shouldClose();

		//窗口获取事件
		void pollEvents();

		[[nodiscard]] GLFWwindow* getWindow() const { return mWindow; }

		void setApp(GAME::Application* app);

		void processEvent();

		void SystemTray();//创建系统托盘

	public:
		bool mWindowResized{ false };
		GAME::Application* mApp;

	private:
		bool MouseDisabled = false;
		int mWidth{ 0 };//储存窗口宽度
		int mHeight{ 0 };//储存窗口高度
		GLFWwindow* mWindow{ NULL };//储存窗口指针
	};
}