#include "windowSurface.h"
#include "../DebugLog.h"

#if defined(__ANDROID__)
#include <vulkan/vulkan_android.h>
#endif

namespace VulKan {

WindowSurface::WindowSurface(Instance* instance, 
#if defined(_WIN32)
    Window* window
#elif defined(__ANDROID__)
    Window* window
#endif
) {
    LOGD("WindowSurface::WindowSurface()");
    mInstance = instance;
#if defined(_WIN32)
    if (glfwCreateWindowSurface(instance->getInstance(), window->getWindow(), nullptr, &mWindowSurface) != VK_SUCCESS) {
        LOGE("WindowSurface::WindowSurface: failed to create surface");
        throw std::runtime_error("Error: failed to create surface");
    }
#elif defined(__ANDROID__)
    VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = window->getWindow();
    if (vkCreateAndroidSurfaceKHR(instance->getInstance(), &surfaceInfo, nullptr, &mWindowSurface) != VK_SUCCESS) {
        LOGE("WindowSurface::WindowSurface: failed to create Android surface");
        throw std::runtime_error("Error: failed to create Android surface");
    }
#endif
}

WindowSurface::~WindowSurface() {
    vkDestroySurfaceKHR(mInstance->getInstance(), mWindowSurface, nullptr);
}

}