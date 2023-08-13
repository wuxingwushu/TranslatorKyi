#include "instance.h"

namespace VulKan {
	//validation layer 回调函数
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,//消息等级
		VkDebugUtilsMessageTypeFlagsEXT messageType,//消息类型
		const VkDebugUtilsMessengerCallbackDataEXT* pMessageData,//消息本体
		void* pUserData) {

		if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			std::cout << "ValidationLayer: " << pMessageData->pMessage << std::endl;
		}
		
		return VK_FALSE;
	}

	//辅助函数			辅助创建监听功能
	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* debugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

		if (func != nullptr) {//判断创建成功没
			return func(instance, pCreateInfo, pAllocator, debugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	//回收创建的监听功能
	static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
		VkDebugUtilsMessengerEXT  debugMessenger,
		const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

		if (func != nullptr) {
			return func(instance, debugMessenger, pAllocator);
		}
	}


	Instance::Instance(bool enableValidationLayer) {
		mEnableValidationLayer = enableValidationLayer;//存储当前是否开启验证层

		if (mEnableValidationLayer && !checkValidationLayerSupport()) {//判断测试功能开启成功没
			mEnableValidationLayer = false;
			throw std::runtime_error("Error: validation layer is not supported");
		}

		//打印所有扩展名字
		//printAvailableExtensions();

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;//sType 成员变量来显式指定结构体类型
		appInfo.pApplicationName = "vulkanLession";//名字
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);//版本
		appInfo.pEngineName = "NO ENGINE";//是否有引擎，对引擎优化
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);//引擎版本
		appInfo.apiVersion = VK_API_VERSION_1_3;//API 版本

		VkInstanceCreateInfo instCreateInfo = {}; 
		instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instCreateInfo.pApplicationInfo = &appInfo;

		//扩展相关
		extensions = getRequiredExtensions();//根据返回来的扩展
		instCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());//设置扩展字符串大小
		instCreateInfo.ppEnabledExtensionNames = extensions.data();//传递需要开启的扩展

		//layer相关
		if (mEnableValidationLayer) {//判断是否开启检测（主要是在写代码是测试用的）
			instCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());//获取字符串长度
			instCreateInfo.ppEnabledLayerNames = validationLayers.data();//开启对应测试功能
		}
		else {
			instCreateInfo.enabledLayerCount = 0;//不开启测试
		}

		if (vkCreateInstance(&instCreateInfo, nullptr, &mInstance) != VK_SUCCESS) {//用上面信息创建VulKan的实例
			throw std::runtime_error("Error:failed to create instance");
		}

		setupDebugger();
	}

	Instance::~Instance() {
		if (mEnableValidationLayer) {
			DestroyDebugUtilsMessengerEXT(mInstance, mDebugger, nullptr);
		}

		vkDestroyInstance(mInstance, nullptr);
	}

	void Instance::printAvailableExtensions() {
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);//获取可以扩展的数量

		std::vector<VkExtensionProperties> extensions(extensionCount);//创建储存扩展名的数组
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());//获取扩展名数组

		std::cout << "Available extensions:" << std::endl;

		for (const auto& extension : extensions) {//遍历数组
			std::cout << extension.extensionName << std::endl;//打印所以扩展名
		}
	}

	std::vector<const char*> Instance::getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;

		//得到要求实例扩展
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		//添加校验层扩展
		if (mEnableValidationLayer) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); //开启调试功能
		}

		return extensions;
	}

	bool Instance::checkValidationLayerSupport() {
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);//获取全部测试功能数量

		std::vector<VkLayerProperties> availableLayers(layerCount);//创建储存全部测试功能的数组
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());//获取全部测试功能的名字

		for (const auto& layerName : validationLayers) {//遍历要开启的测试功能
			bool layerFound = false;

			for (const auto& layerProp : availableLayers) {//和全部测试功能对比是否有对应的测试功能
				if (std::strcmp(layerName, layerProp.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	void Instance::setupDebugger() {
		if (!mEnableValidationLayer) { return; }

		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |	//监听那些类型
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

		createInfo.messageType = 
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |	//监听那些类型
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

		createInfo.pfnUserCallback = debugCallBack;//设置监听的回调函数
		createInfo.pUserData = nullptr;

		if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugger) != VK_SUCCESS) {//创建返回错误信息或警告的信使
			throw std::runtime_error("Error:VulKan Instance . failed to create debugger");
		}
	}
}