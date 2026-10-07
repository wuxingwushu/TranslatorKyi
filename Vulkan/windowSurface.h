#pragma once
#include "instance.h"
#if defined(_WIN32) || defined(__ANDROID__)
#include "window.h"
#endif

namespace VulKan {

	class WindowSurface {
	public:
#if defined(_WIN32) || defined(__ANDROID__)
		//创建Surface，让VulKan和窗口链接起来（适配win,安卓,等等不同设备）
		WindowSurface(Instance* instance, Window* window);
#endif

		~WindowSurface();

		//获取Surface的指针
		[[nodiscard]] inline VkSurfaceKHR getSurface() const noexcept { return mWindowSurface; }

	private:
		VkSurfaceKHR mWindowSurface{ VK_NULL_HANDLE };
		Instance* mInstance{ nullptr };
	};
}
