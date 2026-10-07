#pragma once
#include <vector>//动态数组
#include <array>//静态数组
#include <map>
#include <string>//字符串
#include <optional>
#include <set>
#include <fstream>
#include <vulkan/vulkan.h>//VulKan API

//开启的测试模式
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"//测试类型
};

namespace VulKan {
	//决定这次运行用显卡还是用 CPU 软件渲染(SwiftShader)，必要时设置进程内的 VK_ICD_FILENAMES。
	//必须在任何 Vulkan 调用之前调用（main 里 InitSpdLog 之后、创建 Application 之前）。
	//判据是"临时建一个 VkInstance 让 loader 自己枚举设备"，不再只看旧式注册表键
	//HKLM\SOFTWARE\Khronos\Vulkan\Drivers（loader 1.4 起 ICD 也可以由驱动的 PnP 注册信息提供，
	//只看旧键会把这类显卡误判成没有驱动，导致「自动选择最高性能」被错误地切成 CPU 软件渲染）。
	//这次枚举到的硬件设备会写进 Variable::VulkanDetectedDevices，供设置界面列出"识别到的显卡"。
	//返回 false 表示既没有可用的显卡驱动也没找到 SwiftShader，此时创建 instance 一定失败。
	bool ensureVulkanIcdAvailable();
	
	class Instance {
	public:
		Instance(bool enableValidationLayer);

		~Instance();



		//得到符合要求实例扩展
		std::vector<const char*> getRequiredExtensions();



		//检查验证层是否支持
		bool checkValidationLayerSupport();
		//设置调试器的返回那些信息
		void setupDebugger();
		//判断是否开启了检测
		inline bool getEnableValidationLayer() const noexcept { return mEnableValidationLayer; }

		

		//获取VulKan的实列
		[[nodiscard]] inline VkInstance getInstance() const noexcept { return mInstance; }

		[[nodiscard]] inline std::vector<const char*> getextensions() const noexcept { return extensions; }

		

	private:
		void printAvailableExtensions();
		std::vector<const char*> getAvailableExtensionNames();
		std::vector<const char*> filterExtensions(const std::vector<const char*>& required, const std::vector<const char*>& available);

	private:
		VkInstance mInstance{ VK_NULL_HANDLE };//实列指针
		bool mEnableValidationLayer{ false };//是否开启了校验层
		VkDebugUtilsMessengerEXT mDebugger{ VK_NULL_HANDLE };//调试信使指针


		std::vector<const char*> extensions;
	};
}