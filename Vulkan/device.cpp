#include "device.h"
#if defined(_WIN32) || defined(__ANDROID__)
#define VMA_DEBUG_MARGIN 16
#if defined(__ANDROID__)
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#endif
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#endif
#include "../DebugLog.h"
#include "../Variable.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace VulKan {
	//instance.cpp 中的致命诊断输出（不受 TRANSLATOR_ENABLE_LOG 影响，写 stderr），在此复用。
	//VulkanDiag 是可变参数函数，转发调用不需要 <cstdarg>。
	void VulkanDiag(const char* fmt, ...);
	const char* vkResultName(VkResult result);
}

namespace VulKan {

	Device::Device(Instance* instance, WindowSurface* surface) {
		LOGD("Device::Device()");
		mInstance = instance;
		mSurface = surface;
		pickPhysicalDevice();//按设置里的"渲染设备"给所有设备评分排序，选出这一次要用的设备
		initQueueFamilies(mPhysicalDevice);//根据最高性能设备填写队列家族
		createLogicalDevice();//用最高分数设备创建ID引用
	}

	Device::~Device() {
		vmaDestroyAllocator(mAllocator);//销毁内存分配器
		vkDestroyDevice(mDevice, nullptr);//销毁设备
	}

	void Device::pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		VkResult enumResult = vkEnumeratePhysicalDevices(mInstance->getInstance(), &deviceCount, nullptr);//你有多少个显卡
		VulkanDiag("[Vulkan] vkEnumeratePhysicalDevices: VkResult=%d (%s), deviceCount=%u\n",
			(int)enumResult, vkResultName(enumResult), deviceCount);

		if (enumResult != VK_SUCCESS) {
			LOGE("Device::pickPhysicalDevice: vkEnumeratePhysicalDevices failed, VkResult=%d", (int)enumResult);
			VulkanDiag("[Vulkan] 结论: 枚举物理设备失败（VkResult=%d）—— instance 已创建但查询设备出错。\n",
				(int)enumResult);
			throw std::runtime_error("Error:failed to enumeratePhysicalDevice");
		}

		if (deviceCount == 0) {
			LOGE("Device::pickPhysicalDevice: no physical devices found");
			VulkanDiag("[Vulkan] 结论: 系统报告 0 个物理设备。\n"
					   "[Vulkan] 说明: instance 创建成功但没有任何设备可用，几乎总是驱动/ICD 安装不完整，\n"
					   "[Vulkan]       或 ICD 注册表项指向的文件已丢失。请更新显卡驱动后重试。\n");
			throw std::runtime_error("Error:failed to enumeratePhysicalDevice");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		VkResult enumResult2 = vkEnumeratePhysicalDevices(mInstance->getInstance(), &deviceCount, devices.data());//获取这些显卡的信息
		if (enumResult2 != VK_SUCCESS) {
			//第二次调用时设备数量可能变少（例如 VK_ERROR_INCOMPLETE），deviceCount 已被回写
			devices.resize(deviceCount);
			VulkanDiag("[Vulkan] 警告: 第二次 vkEnumeratePhysicalDevices 返回 %d (%s)，按 %u 个设备继续。\n",
				(int)enumResult2, vkResultName(enumResult2), deviceCount);
			if (deviceCount == 0) {
				throw std::runtime_error("Error:failed to enumeratePhysicalDevice");
			}
		}

		//注意：rateDevice() 对不满足硬性要求的设备会返回 0，0 分不能等同于"性能差"，
		//必须结合 isDeviceSuitable() 一起判断，所以这里把两件事分开做。
		struct Candidate {
			int score;
			VkPhysicalDevice device;
			bool suitable;
			bool isCpu;
			std::string name;
			std::string rejectReason;
		};
		std::vector<Candidate> candidates;
		candidates.reserve(devices.size());

		for (const auto& device : devices) {
			Candidate c;
			c.device = device;
			c.score = rateDevice(device);//获取每一张显卡的评分
			c.suitable = isDeviceSuitable(device);
			c.rejectReason = c.suitable ? std::string() : describeDeviceRejection(device);
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);
			c.isCpu = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);
			c.name = props.deviceName;
			candidates.push_back(std::move(c));
		}

		//按设置里的"渲染设备"决定选取策略：
		//  0 AutoBest  自动选择最高性能 —— 评分最高的设备
		//  1 AutoWorst 自动选择最低性能 —— 评分最低的设备
		//  2 CPU       只用 CPU 软件渲染(SwiftShader)
		//  3 Specific  用 Variable::VulkanDeviceName 指定的那一台
		//0/1/3 都必须让硬件设备严格优先于 CPU 软件设备：只要机器上有显卡，就绝不能因为分数
		//把软件设备选出来（否则又会出现"选了显卡却还是 CPU 渲染"）。
		const bool wantCpuDevice = (Variable::VulkanDeviceMode == Variable::VulkanDeviceModeEnum::CPU);
		const bool wantSpecificDevice = Variable::IsSpecificDeviceMode();
		const bool wantLowestScore = (Variable::VulkanDeviceMode == Variable::VulkanDeviceModeEnum::AutoWorst);

		VulkanDiag("[Vulkan] 渲染设备设置=%d (%s)，选取策略: %s\n",
			(int)Variable::VulkanDeviceMode,
			wantCpuDevice ? "CPU 软件渲染"
				: (wantSpecificDevice ? "指定设备"
					: (wantLowestScore ? "自动选择最低性能" : "自动选择最高性能")),
			wantCpuDevice ? "只用 CPU 软件设备"
				: (wantSpecificDevice ? "优先用设置里指定的那一台设备；没识别到就回退到最高性能"
					: (wantLowestScore ? "硬件设备优先，其中评分最低的优先；没有硬件设备时才用 CPU 软件设备"
						: "硬件设备优先，其中评分最高的优先；没有硬件设备时才用 CPU 软件设备")));

		//可用的设备排前面，再按设置挑"最高性能 / 最低性能 / 指定的那一台"
		std::stable_sort(candidates.begin(), candidates.end(),
			[wantCpuDevice, wantSpecificDevice, wantLowestScore](const Candidate& a, const Candidate& b) {
				if (a.suitable != b.suitable) return a.suitable;//可用的排前面

				if (wantCpuDevice) {
					//CPU 软件渲染：软件设备排最前
					if (a.isCpu != b.isCpu) return a.isCpu;
					return a.score > b.score;
				}

				//其余模式：硬件设备一律排在 CPU 软件设备前面
				if (a.isCpu != b.isCpu) return b.isCpu;

				if (wantSpecificDevice) {
					//设置里指定了设备名：匹配的排最前（名字对不上就退化成"最高性能"）
					const bool aWanted = (a.name == Variable::VulkanDeviceName);
					const bool bWanted = (b.name == Variable::VulkanDeviceName);
					if (aWanted != bWanted) return aWanted;
				}

				return wantLowestScore ? (a.score < b.score) : (a.score > b.score);
			});

		VulkanDiag("[Vulkan] 发现 %zu 个物理设备:\n", candidates.size());
		for (size_t i = 0; i < candidates.size(); i++) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(candidates[i].device, &props);
			VulkanDiag("[Vulkan]   [%zu] %s%s (score=%d, apiVersion=%u.%u.%u, type=%d) -> %s\n",
				i, props.deviceName,
				(wantSpecificDevice && candidates[i].name == Variable::VulkanDeviceName) ? " [设置指定]" : "",
				candidates[i].score,
				VK_VERSION_MAJOR(props.apiVersion),
				VK_VERSION_MINOR(props.apiVersion),
				VK_VERSION_PATCH(props.apiVersion),
				(int)props.deviceType,
				candidates[i].suitable ? "可用" : candidates[i].rejectReason.c_str());
		}

		//回退逻辑：不再只看最高分那一个，而是按优先级取第一个真正可用的设备
		for (const auto& candidate : candidates) {
			if (!candidate.suitable) continue;

			mPhysicalDevice = candidate.device;//获取那张显卡
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);

			//记录几何着色器能力：CPU 软件设备没有它，管线要走顶点着色器实例化展块的回退分支
			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(mPhysicalDevice, &features);
			mSupportsGeometryShader = (features.geometryShader == VK_TRUE);

			VulkanDiag("[Vulkan] 选用物理设备: %s (score=%d)\n", props.deviceName, candidate.score);
			VulkanDiag("[Vulkan] 几何着色器(geometryShader): %s\n",
				mSupportsGeometryShader ? "支持" : "不支持 —— UVDynamicDiagram 管线将改用顶点着色器实例化展块");

			//设置里点名要用的设备没被识别到（换驱动、拔显卡、换机器）：说清楚已经改用别的设备，
			//不要静默替换，也别让用户以为"选了却没生效"
			if (wantSpecificDevice && props.deviceName != Variable::VulkanDeviceName) {
				VulkanDiag("[Vulkan] 警告: 设置里指定的渲染设备「%s」本次没有被 loader 识别到，"
					"已按「自动选择最高性能」改用上面这台设备。\n"
					"[Vulkan]       要固定用它，请重新在设置界面里选一次。\n",
					Variable::VulkanDeviceName.c_str());
			}

			//告诉界面层"现在到底是不是 CPU 软件渲染"，主界面左上角要据此提醒用户
			Variable::RunningOnSoftwareRenderer = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);
			Variable::RunningDeviceName = props.deviceName;
			if (Variable::RunningOnSoftwareRenderer) {
				VulkanDiag("[Vulkan] 提示: 当前是 CPU 软件渲染设备，帧率会很低，仅用于在没有 Vulkan 显卡驱动时验证程序可跑。\n");
			}
			return;
		}

		LOGE("Device::pickPhysicalDevice: no suitable physical device");
		//一个可用的都没有：把每个设备的落选原因讲清楚，否则只剩下"failed to get physical device"
		VulkanDiag("[Vulkan] 结论: %zu 个物理设备均不满足本程序的最低要求（各向异性采样 samplerAnisotropy）。\n",
			candidates.size());
		for (size_t i = 0; i < candidates.size(); i++) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(candidates[i].device, &props);
			VulkanDiag("[Vulkan]   设备 [%zu] %s: %s\n", i, props.deviceName,
				candidates[i].rejectReason.c_str());
		}
		VulkanDiag("[Vulkan] 说明: 若列表里出现 \"SwiftShader\" 等 CPU 软件渲染设备，本程序已支持在其上运行\n"
				   "[Vulkan]       （几何着色器已改为可选），前提是该软件 ICD 已注册；否则请安装带 Vulkan 的显卡驱动。\n");
		throw std::runtime_error("Error:failed to get physical device");
	}

	//某台物理设备是否满足本程序的最低要求；不满足时通过 reasonOut 说明原因。
	//判据必须与 createLogicalDevice() 里"真正会致命"的东西一致：
	//  1) 各向异性采样(samplerAnisotropy)：采样器/管线要用（见 isDeviceSuitable 的历史要求）；
	//  2) VK_KHR_swapchain：没有它根本没法呈现到窗口（createLogicalDevice 里会直接抛异常）。
	//几何着色器与 VK_KHR_maintenance1 是可选能力（不支持时自动回退），不列为硬性要求。
	bool physicalDeviceMeetsMinimumRequirements(VkPhysicalDevice device, std::string* reasonOut) {
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		std::vector<std::string> reasons;
		if (!deviceFeatures.samplerAnisotropy) reasons.push_back("不支持各向异性采样(samplerAnisotropy)");

		//设备级扩展里必须有 VK_KHR_swapchain
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> available(extensionCount);
		if (extensionCount > 0) {
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());
		}
		bool hasSwapchain = false;
		for (const auto& ext : available) {
			if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
				hasSwapchain = true;
				break;
			}
		}
		if (!hasSwapchain) reasons.push_back(std::string("缺少设备扩展 ") + VK_KHR_SWAPCHAIN_EXTENSION_NAME + "（无法呈现到窗口）");

		if (reasons.empty()) return true;

		if (reasonOut) {
			std::string result;
			for (size_t i = 0; i < reasons.size(); i++) {
				if (i > 0) result += "、";
				result += reasons[i];
			}
			*reasonOut = result;
		}
		return false;
	}

	//说明某个物理设备为什么不满足要求；满足要求时返回空串
	std::string Device::describeDeviceRejection(VkPhysicalDevice device) {
		VkPhysicalDeviceProperties deviceProp;
		vkGetPhysicalDeviceProperties(device, &deviceProp);

		std::string result;
		if (physicalDeviceMeetsMinimumRequirements(device, &result)) return std::string();

		result += " [deviceType=" + std::to_string((int)deviceProp.deviceType)
			+ ", apiVersion=" + std::to_string(VK_VERSION_MAJOR(deviceProp.apiVersion))
			+ "." + std::to_string(VK_VERSION_MINOR(deviceProp.apiVersion))
			+ "." + std::to_string(VK_VERSION_PATCH(deviceProp.apiVersion)) + "]";
		return result;
	}

	int Device::rateDevice(VkPhysicalDevice device) {
		int score = 0;

		//设备名称 类型 支持vulkan的版本
		VkPhysicalDeviceProperties  deviceProp;
		vkGetPhysicalDeviceProperties(device, &deviceProp);

		//纹理压缩 浮点数运算特性 多视口渲染
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		if (deviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {//是否是独显，
			score += 1000;
		}

		score += deviceProp.limits.maxImageDimension2D;

		//几何着色器不再作为硬性要求（CPU 软件设备没有它，管线会自动回退）。
		//这里只影响排序：同等条件下优先选支持几何着色器的设备。0 分仅表示"能力受限"，不代表设备不可用。
		if (!deviceFeatures.geometryShader) {
			score -= 100000;
		}

		return score;
	}

	bool Device::isDeviceSuitable(VkPhysicalDevice device) {
		//判据统一放在 physicalDeviceMeetsMinimumRequirements() 里（与探测阶段共用同一套），
		//避免"探测说能用、选设备时又说不能用"这种自相矛盾。
		//注意：不再要求独显；几何着色器也不是硬性要求（没有它时 UVDynamicDiagram 管线
		//走顶点着色器实例化展块的回退分支）。
		return physicalDeviceMeetsMinimumRequirements(device, nullptr);
	}

	void Device::initQueueFamilies(VkPhysicalDevice device) {
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {//判断是否是需要渲染的
				mGraphicQueueFamily = i;
			}

			//寻找支持显示的队列族
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface->getSurface(), &presentSupport);

			if (presentSupport) {
				mPresentQueueFamily = i;
			}

			//判断设备是否完整
			if (isQueueFamilyComplete()) {//判断 mGraphicQueueFamily 和 mPresentQueueFamily 的 ID 是否相等
				break;
			}

			++i;
		}
	}

	void Device::createLogicalDevice() {
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

		std::set<uint32_t> queueFamilies = {mGraphicQueueFamily.value(), mPresentQueueFamily.value()};

		float queuePriority = 1.0;//队列执行度等级，越大越高

		for (uint32_t queueFamily : queueFamilies) {
			//填写创建信息
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;//创建多少个队列
			queueCreateInfo.pQueuePriorities = &queuePriority;

			queueCreateInfos.push_back(queueCreateInfo);
		}	

		//填写逻辑设备创建信息
		//注意：只能申请物理设备真正支持的 feature，申请不支持的 feature 会导致 vkCreateDevice 失败。
		//CPU 软件设备（SwiftShader）没有几何着色器，此时不申请，管线会走回退分支。
		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(mPhysicalDevice, &supportedFeatures);

		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.samplerAnisotropy = supportedFeatures.samplerAnisotropy;//开启各向异性
		deviceFeatures.geometryShader = supportedFeatures.geometryShader;//几何着色器（可选）
		deviceFeatures.shaderFloat64 = VK_FALSE;//GPU开启 Float64

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;//创建多少个队列
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		//申请设备扩展前先过滤：申请设备不支持的扩展会导致整个 vkCreateDevice 失败。
		//VK_KHR_swapchain 是硬性要求；VK_KHR_maintenance1 只影响 Vulkan 坐标系，缺失就跳过。
		uint32_t deviceExtCount = 0;
		vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &deviceExtCount, nullptr);
		std::vector<VkExtensionProperties> deviceExts(deviceExtCount);
		if (deviceExtCount > 0) {
			vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &deviceExtCount, deviceExts.data());
		}

		std::vector<const char*> enabledDeviceExtensions;
		for (const char* required : deviceRequiredExtensions) {
			if (!required) continue;
			bool found = false;
			for (const auto& ext : deviceExts) {
				if (std::strcmp(ext.extensionName, required) == 0) { found = true; break; }
			}
			if (found) {
				enabledDeviceExtensions.push_back(required);
			}
			else {
				VulkanDiag("[Vulkan] 警告: 设备不支持扩展 %s，已跳过（不影响基本渲染）。\n", required);
			}
		}
		if (enabledDeviceExtensions.empty()) {
			VulkanDiag("[Vulkan] 结论: 设备连 VK_KHR_swapchain 都不支持，无法呈现到窗口。\n");
			throw std::runtime_error("Error:failed to create logical device");
		}

		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();

		//layer层
		if (mInstance->getEnableValidationLayer()) {//是否开启了检测
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			deviceCreateInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice) != VK_SUCCESS) {
			LOGE("Device::createLogicalDevice: failed to create logical device");
			throw std::runtime_error("Error:failed to create logical device");
		}

		//内存分配器的信息
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
		allocatorInfo.physicalDevice = mPhysicalDevice;
		allocatorInfo.device = mDevice;
		allocatorInfo.instance = mInstance->getInstance();
		//allocatorInfo.flags = 

#if VMA_DYNAMIC_VULKAN_FUNCTIONS
		static VmaVulkanFunctions vulkanFunctions = {};
		vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
		allocatorInfo.pVulkanFunctions = &vulkanFunctions;
#endif


		if (vmaCreateAllocator(&allocatorInfo, &mAllocator) != VK_SUCCESS) {
			LOGE("Device::createLogicalDevice: failed to create VMA allocator");
			throw std::runtime_error("Error: failed to create VMA allocator");
		}


		vkGetDeviceQueue(mDevice, mGraphicQueueFamily.value(), 0, &mGraphicQueue);
		vkGetDeviceQueue(mDevice, mPresentQueueFamily.value(), 0, &mPresentQueue);
	}

	bool Device::isQueueFamilyComplete() {
		return mGraphicQueueFamily.has_value() && mPresentQueueFamily.has_value();
	}

	VkSampleCountFlagBits Device::getMaxUsableSampleCount() {
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);

		VkSampleCountFlags counts = std::min(
			props.limits.framebufferColorSampleCounts,
			props.limits.framebufferDepthSampleCounts
		);

		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

	GPUComputeCapabilities Device::getComputeCapabilities() const {
		GPUComputeCapabilities caps = {};
		caps.hasSMCountExtension = false;

		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
		caps.maxWorkGroupInvocations = props.limits.maxComputeWorkGroupInvocations;
		caps.maxWorkGroupCount[0] = props.limits.maxComputeWorkGroupCount[0];
		caps.maxWorkGroupCount[1] = props.limits.maxComputeWorkGroupCount[1];
		caps.maxWorkGroupCount[2] = props.limits.maxComputeWorkGroupCount[2];

		auto vkGetPhysicalDeviceProperties2 =
			reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
				vkGetInstanceProcAddr(mInstance->getInstance(), "vkGetPhysicalDeviceProperties2"));

		if (!vkGetPhysicalDeviceProperties2) {
			caps.subgroupSize = 0;
			caps.smCount = std::max(4u, caps.maxWorkGroupInvocations / 1024u);
			return caps;
		}

		VkPhysicalDeviceSubgroupProperties subgroup = {};
		subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

		VkPhysicalDeviceProperties2 props2 = {};
		props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

		VkPhysicalDeviceShaderSMBuiltinsPropertiesNV smPropsNV = {};
		smPropsNV.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV;

		props2.pNext = &subgroup;
		subgroup.pNext = &smPropsNV;

		vkGetPhysicalDeviceProperties2(mPhysicalDevice, &props2);

		caps.subgroupSize = subgroup.subgroupSize;

		if (smPropsNV.shaderSMCount > 0) {
			caps.smCount = smPropsNV.shaderSMCount;
			caps.hasSMCountExtension = true;
		} else {
			VkPhysicalDeviceShaderCorePropertiesAMD corePropsAMD = {};
			corePropsAMD.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD;

			props2.pNext = &subgroup;
			subgroup.pNext = &corePropsAMD;
			vkGetPhysicalDeviceProperties2(mPhysicalDevice, &props2);

			if (corePropsAMD.shaderEngineCount > 0 && corePropsAMD.shaderArraysPerEngineCount > 0) {
				caps.smCount = corePropsAMD.shaderEngineCount * corePropsAMD.shaderArraysPerEngineCount;
				caps.hasSMCountExtension = true;
			} else {
				caps.smCount = std::max(4u, caps.maxWorkGroupInvocations / 1024u);
			}
		}

		return caps;
	}
}