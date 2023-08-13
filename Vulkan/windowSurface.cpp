#include"windowSurface.h"

namespace VulKan {
	//创建Surface，让VulKan和窗口链接起来（适配win,安卓,等等不同设备）
	WindowSurface::WindowSurface(Instance* instance, Window* window) {
		mInstance = instance;
		if (glfwCreateWindowSurface(instance->getInstance(), window->getWindow(), nullptr, &mWindowSurface) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create surface");
		}
	}

	WindowSurface::~WindowSurface() {
		vkDestroySurfaceKHR(mInstance->getInstance(), mWindowSurface, nullptr);
	}

}