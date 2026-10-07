#pragma once
//#define VMA_DEBUG_MARGIN 16//边距（Margins）https://blog.csdn.net/weixin_50523841/article/details/122506850
#if defined(_WIN32) || defined(__ANDROID__)
#if defined(__ANDROID__)
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#endif
#include "../vk_mem_alloc.h"//仓库根目录的同一个头文件（VMA_IMPLEMENTATION 在 device.cpp 里展开）
#endif
#include "instance.h"
#include "windowSurface.h"
#include <optional>


namespace VulKan {

	const std::vector<const char*> deviceRequiredExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,//开启 VulKan 坐标系扩展
		//VK_NV_FRAMEBUFFER_MIXED_SAMPLES_EXTENSION_NAME
	};

	//判断某台物理设备是否满足本程序的最低要求（各向异性采样 + VK_KHR_swapchain）。
	//不满足时通过 reasonOut 说明原因（reasonOut 传 nullptr 表示不需要原因）。
	//做成不依赖 Device 实例的自由函数：Vulkan/instance.cpp 在创建真实 VkInstance 之前就要用它
	//判断"机器上到底有没有一台能用的显卡"（有显卡但都不能用时自动降级到 CPU 软件渲染）。
	bool physicalDeviceMeetsMinimumRequirements(VkPhysicalDevice device, std::string* reasonOut);

	struct GPUComputeCapabilities {
		uint32_t smCount;
		uint32_t subgroupSize;
		uint32_t maxWorkGroupInvocations;
		uint32_t maxWorkGroupCount[3];
		bool     hasSMCountExtension;
	};

	class Device {
	public:
		Device(Instance* instance, WindowSurface* surface);

		~Device();

		//按设置里的"渲染设备"（自动最高性能/自动最低性能/CPU 软件渲染/指定设备）选出这一次要用的物理设备
		void pickPhysicalDevice();

		//给设备评分
		int rateDevice(VkPhysicalDevice device);

		//判断设备是否符合要求
		bool isDeviceSuitable(VkPhysicalDevice device);

		//说明设备为何不满足要求（诊断用，满足时返回空串）
		std::string describeDeviceRejection(VkPhysicalDevice device);

		//当前选中的物理设备是否支持几何着色器。
		//CPU 软件设备（SwiftShader / llvmpipe 等）不支持几何着色器，此时
		//UVDynamicDiagram 管线会退化成"顶点着色器实例化展块"，见 CreatePipeline.cpp。
		[[nodiscard]] inline bool supportsGeometryShader() const noexcept { return mSupportsGeometryShader; }

		//初始化队列族
		void initQueueFamilies(VkPhysicalDevice device);

		//获取逻辑设备，创建对应引用ID
		void createLogicalDevice();

		//判断设备是否完整
		bool isQueueFamilyComplete();

		VkSampleCountFlagBits getMaxUsableSampleCount();

		#if defined(_WIN32) || defined(__ANDROID__)
		[[nodiscard]] inline VmaAllocator getAllocator() const noexcept { return mAllocator; }//获取内存分配器
#endif

		[[nodiscard]] inline VkDevice getDevice() const noexcept { return mDevice; }
		[[nodiscard]] inline VkPhysicalDevice getPhysicalDevice() const noexcept { return mPhysicalDevice; }

		[[nodiscard]] GPUComputeCapabilities getComputeCapabilities() const;

		[[nodiscard]] inline std::optional<uint32_t> getGraphicQueueFamily() const noexcept { return mGraphicQueueFamily; }
		[[nodiscard]] inline std::optional<uint32_t> getPresentQueueFamily() const noexcept { return mPresentQueueFamily; }

		[[nodiscard]] inline VkQueue getGraphicQueue() const noexcept { return mGraphicQueue; }
		[[nodiscard]] inline VkQueue getPresentQueue() const noexcept { return mPresentQueue; }

	private:
		VkPhysicalDevice mPhysicalDevice{ VK_NULL_HANDLE };//获得的详细设备信息
		Instance* mInstance{ nullptr };
		WindowSurface* mSurface{ nullptr };

		//***  Vulkan 将诸如绘制指令、内存操作提交到VkQueue 中，进行异步执行。

		//存储当前渲染任务队列族的id
		std::optional<uint32_t> mGraphicQueueFamily;
		VkQueue	mGraphicQueue{ VK_NULL_HANDLE };

		std::optional<uint32_t> mPresentQueueFamily;
		VkQueue mPresentQueue{ VK_NULL_HANDLE };

		//逻辑设备
		VkDevice mDevice{ VK_NULL_HANDLE };

		//选中的物理设备是否支持几何着色器（在 pickPhysicalDevice 里填写）
		bool mSupportsGeometryShader{ false };

		#if defined(_WIN32) || defined(__ANDROID__)
		//创建的内存分配器
		VmaAllocator mAllocator{ VK_NULL_HANDLE };
		#endif
		
	};
}