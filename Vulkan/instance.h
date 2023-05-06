#pragma once

#include "../base.h"

namespace GAME::VulKan {
	
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
		[[nodiscard]] bool getEnableValidationLayer() const { return mEnableValidationLayer; }

		

		//获取VulKan的实列
		[[nodiscard]] VkInstance getInstance() const { return mInstance; }

		[[nodiscard]] std::vector<const char*> getextensions() const { return extensions; }

		

	private:
		//打印所以扩展名
		void printAvailableExtensions();

	private:
		VkInstance mInstance{ VK_NULL_HANDLE };//实列指针
		bool mEnableValidationLayer{ false };//是否开启了校验层
		VkDebugUtilsMessengerEXT mDebugger{ VK_NULL_HANDLE };//调试信使指针


		std::vector<const char*> extensions;
	};
}