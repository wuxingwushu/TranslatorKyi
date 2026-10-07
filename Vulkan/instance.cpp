#include "instance.h"
#include  "../Tool/Tool.h"
#include  "../Variable.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <atomic>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <system_error>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GLFW/glfw3.h>
#elif defined(__ANDROID__)
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#endif
#include "../DebugLog.h"

#ifndef VK_API_VERSION_1_1
#define VK_API_VERSION_1_1 VK_MAKE_API_VERSION(0, 1, 1, 0)
#endif

#ifndef VK_API_VERSION_1_2
#define VK_API_VERSION_1_2 VK_MAKE_API_VERSION(0, 1, 2, 0)
#endif

#ifndef VK_API_VERSION_1_3
#define VK_API_VERSION_1_3 VK_MAKE_API_VERSION(0, 1, 3, 0)
#endif

namespace VulKan {
	//--------------------------------------------------------------------------------------------------
	// 致命诊断输出：不受 DebugLog.h 的 TRANSLATOR_ENABLE_LOG 开关影响。
	// 原因：TRANSLATOR_ENABLE_LOG=0 时 LOGE/LOGW 全部是 ((void)0)，而 Vulkan 初始化失败恰恰
	//       只有这些日志能说明 VkResult 是什么，导致报错只剩一句"Vulkan not available"。
	//
	// 去向（两处都写）：
	//   1) logs/Error.txt —— 主通道。走 TOOL::logger（TranslatorKyi 的 spdlog logger，由
	//      TOOL::SpdLogInit() 建立），和 "Error:failed to create instance, Vulkan not available"
	//      落在同一个文件里，用户看 logs/Error.txt 就能看到完整因果链。
	//   2) stderr —— 兜底。logger 还没建好（TOOL::SpdLogInit 之前，或它自己抛异常）时，
	//      这里用 fopen 追加写 Logs/VulKanError.txt；控制台/IDE 输出窗口也能同步看到。
	//--------------------------------------------------------------------------------------------------
	static void VulkanDiagWrite(const char* utf8Text) {
		if (!utf8Text) return;

		bool routed = false;

		//TOOL::logger 在 main() 里 TOOL::SpdLogInit() 时就已创建，早于 Vulkan 初始化；
		//但仍要判空：SpdLogInit 可能失败并把它留在 nullptr 上。
		if (TOOL::logger != nullptr) {
			try {
				//统一走日志系统
				TOOL::logger->error("{}", utf8Text);
				routed = true;
			}
			catch (...) {
				routed = false;//日志系统自己出问题时不能连累 Vulkan 诊断
			}
		}

		if (!routed) {
			//logger 不可用的兜底：直接追加写 Logs/VulKanError.txt
			std::error_code ec;
			std::filesystem::create_directories("Logs", ec);
			if (FILE* fp = std::fopen("Logs/VulKanError.txt", "ab")) {
				std::fputs(utf8Text, fp);
				std::fclose(fp);
				routed = true;
			}
		}

		//同时保留控制台输出：日志文件是事后看的，控制台是当场看的
#if defined(_WIN32)
		int wideLen = ::MultiByteToWideChar(CP_UTF8, 0, utf8Text, -1, nullptr, 0);
		if (wideLen > 1) {
			std::wstring wide(static_cast<size_t>(wideLen - 1), L'\0');
			::MultiByteToWideChar(CP_UTF8, 0, utf8Text, -1, wide.data(), wideLen);
			// 控制台可能是 UTF-16 语义，用 WriteConsoleW 才不会出现乱码
			if (::GetConsoleWindow() != nullptr) {
				HANDLE h = ::GetStdHandle(STD_ERROR_HANDLE);
				DWORD mode = 0;
				if (h != INVALID_HANDLE_VALUE && h != nullptr && ::GetConsoleMode(h, &mode)) {
					DWORD written = 0;
					::WriteConsoleW(h, wide.c_str(), static_cast<DWORD>(wide.size()), &written, nullptr);
					return;
				}
			}
			// 被重定向到文件/管道时用 UTF-8 字节流
			std::fputs(utf8Text, stderr);
			return;
		}
		std::fputs(utf8Text, stderr);
#else
#if defined(__ANDROID__)
		//Android 没有可用的 stderr，致命诊断转到 logcat
		__android_log_write(ANDROID_LOG_ERROR, "TranslatorKyiVulkan", utf8Text);
#endif
		std::fputs(utf8Text, stderr);
#endif
		(void)routed;
	}

	//外部链接：Vulkan/device.cpp 也调用它做诊断
	void VulkanDiag(const char* fmt, ...) {
		char buffer[2048];
		va_list args;
		va_start(args, fmt);
		int n = std::vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
		if (n < 0) return;
		if (static_cast<size_t>(n) >= sizeof(buffer)) {
			buffer[sizeof(buffer) - 1] = '\0';
		}
		VulkanDiagWrite(buffer);
	}

	//VkResult 的名字：失败时直接打数字没人看得懂（外部链接，Vulkan/device.cpp 也使用）
	const char* vkResultName(VkResult result) {
		switch (result) {
		case VK_SUCCESS: return "VK_SUCCESS";
		case VK_NOT_READY: return "VK_NOT_READY";
		case VK_TIMEOUT: return "VK_TIMEOUT";
		case VK_EVENT_SET: return "VK_EVENT_SET";
		case VK_EVENT_RESET: return "VK_EVENT_RESET";
		case VK_INCOMPLETE: return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
		case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
		default: return "(unknown VkResult)";
		}
	}

#if defined(_WIN32)
	//--------------------------------------------------------------------------------------------------
	// 没有显卡 Vulkan 驱动时，自动改用 CPU 软件渲染（SwiftShader）。
	//
	// 背景：本程序的渲染用了几何着色器之外的东西都能软件实现，但 Windows 上只有
	//       "注册过 ICD 的驱动" 才提供 Vulkan 实现。没有独显/核显驱动时 loader 直接
	//       返回 VK_ERROR_INCOMPATIBLE_DRIVER。SwiftShader 是纯 CPU 的 Vulkan 实现
	//       （Edge 自带一份），只要通过 VK_ICD_FILENAMES / VK_DRIVER_FILES 指给 loader 就能用，
	//       不需要管理员权限，也不改系统注册表。
	//
	// 调用时机：必须在任何 Vulkan 调用之前（main 里 InitSpdLog 之后、Application 构造之前）。
	//           本函数自己 setenv，只影响当前进程。
	//--------------------------------------------------------------------------------------------------
	static bool registryDriversKeyExists() {
		//64 位进程要读 64 位视图，32 位进程系统会自动重定向到 Wow6432Node
		HKEY key = nullptr;
		LSTATUS st = ::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\Vulkan\\Drivers", 0, KEY_READ, &key);
		if (st != ERROR_SUCCESS) return false;

		bool hasEntry = false;
		char valueName[512];
		DWORD nameLen = 0;
		for (DWORD index = 0; ; ++index) {
			nameLen = static_cast<DWORD>(sizeof(valueName));
			LSTATUS es = ::RegEnumValueA(key, index, valueName, &nameLen, nullptr, nullptr, nullptr, nullptr);
			if (es == ERROR_SUCCESS) {
				hasEntry = true;
				break;
			}
			if (es == ERROR_NO_MORE_ITEMS) break;
			if (es != ERROR_MORE_DATA) break;
		}
		::RegCloseKey(key);
		return hasEntry;
	}

	static bool envVarIsSet(const char* name) {
		char buffer[8] = { 0 };
		DWORD n = ::GetEnvironmentVariableA(name, buffer, static_cast<DWORD>(sizeof(buffer)));
		return n > 0;
	}

	//--------------------------------------------------------------------------------------------------
	// 直接问 Vulkan loader"现在能不能拿到显卡"，不要再靠注册表猜。
	//
	// 为什么不能只看注册表：Windows 上 ICD 的注册方式有两代。
	//   旧式：HKLM\SOFTWARE\Khronos\Vulkan\Drivers 下逐条登记 icd json 路径
	//         （registryDriversKeyExists() 查的就是它）。
	//   新式：驱动的 INF 把 icd manifest 注册在设备的 PnP 注册表里，loader 1.4 起会自己去
	//         DriverStore\FileRepository\<显卡 inf 目录>\ 找（本机实测：旧式键不存在，
	//         但 vulkaninfo --summary 能看到 GPU0 = NVIDIA GeForce RTX 2070）。
	//   只看旧式键会把"按新式方式注册的显卡驱动"误判成"没有显卡"，于是「自动选择最高性能」
	//   被错误地切成 SwiftShader，整进程锁死在 CPU 软件渲染上（本 bug 的根因）。
	//
	// 所以这里改成实证：建一个最小的临时 VkInstance，枚举物理设备，只要有一台非 CPU 设备
	// 就算有显卡驱动。必须在设置 VK_ICD_FILENAMES 之前调用，否则 loader 只看我们指定的软件 ICD。
	// 返回 false 说明 loader 自己都拿不到驱动（vkCreateInstance 会返回 VK_ERROR_INCOMPATIBLE_DRIVER）。
	//
	// 同时把枚举到的硬件设备缓存进 Variable::VulkanDetectedDevices，供设置界面的"渲染设备"下拉框
	// 列出"识别到的显卡"（用户可以直接指定用哪一台），以及 Vulkan/device.cpp 按名字匹配。
	//--------------------------------------------------------------------------------------------------
	//device.cpp 里的实现：某台物理设备是否满足本程序的最低要求（探测阶段与真正选设备时共用同一套判据）
	bool physicalDeviceMeetsMinimumRequirements(VkPhysicalDevice device, std::string* reasonOut);

	//探测结果：机器上有没有显卡、其中有没有"能用"的显卡。
	//"能用"的判据见 device.cpp 的 physicalDeviceMeetsMinimumRequirements()——探测阶段和真正选设备时
	//共用同一套，避免出现"探测说能用、真选设备时又说不能用"。
	struct VulkanProbeResult {
		bool anyHardware = false;			//loader 报告了至少一台硬件设备（不含 CPU 软件设备）
		bool anyUsableHardware = false;		//其中至少一台满足本程序的最低要求
		std::string firstHardwareName;		//第一台硬件设备的名称（诊断用）
		std::string firstUsableHardwareName;	//第一台可用硬件设备的名称
		std::vector<std::string> hardwareRejectReasons;	//"设备名: 原因"，供"有显卡但都不能用"的警告使用
	};

	static VulkanProbeResult probeVulkanDevices() {
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "TranslatorKyi ICD probe";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "NO ENGINE";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;//探测不需要高版本，1.0 就够枚举设备

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		//不启用任何扩展/校验层：这里只想问"有没有 ICD"，条件越少越能反映真实情况

		VkInstance probeInstance = VK_NULL_HANDLE;
		VkResult result = vkCreateInstance(&createInfo, nullptr, &probeInstance);
		if (result != VK_SUCCESS) {
			VulkanDiag("[Vulkan] 探测: 临时 vkCreateInstance 返回 %d (%s) —— loader 拿不到任何驱动。\n",
				(int)result, vkResultName(result));
			return VulkanProbeResult();
		}

		uint32_t deviceCount = 0;
		result = vkEnumeratePhysicalDevices(probeInstance, &deviceCount, nullptr);
		VulkanProbeResult probeResult;
		Variable::VulkanDetectedDevices.clear();//探测结果就是界面下拉框的"识别到的显卡"列表
		if (result == VK_SUCCESS && deviceCount > 0) {
			std::vector<VkPhysicalDevice> devices(deviceCount);
			if (vkEnumeratePhysicalDevices(probeInstance, &deviceCount, devices.data()) == VK_SUCCESS) {
				VulkanDiag("[Vulkan] 探测: loader 报告 %u 个物理设备:\n", deviceCount);
				for (uint32_t i = 0; i < deviceCount; i++) {
					VkPhysicalDeviceProperties props;
					vkGetPhysicalDeviceProperties(devices[i], &props);
					const bool isCpuDevice = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);

					if (isCpuDevice) {
						//CPU 软件设备不算"识别到的显卡"，界面上由「CPU 软件渲染」代表
						VulkanDiag("[Vulkan]   [%u] %s (type=%d, CPU 软件渲染设备)\n",
							i, props.deviceName, (int)props.deviceType);
						continue;
					}

					//是否满足本程序的最低要求；不满足时把原因一起记下来，稍后要打给用户看
					std::string rejectReason;
					const bool usable = physicalDeviceMeetsMinimumRequirements(devices[i], &rejectReason);

					VulkanDiag("[Vulkan]   [%u] %s (type=%d, 硬件设备, %s)\n",
						i, props.deviceName, (int)props.deviceType,
						usable ? "满足最低要求" : std::string("不满足最低要求: " + rejectReason).c_str());

					Variable::VulkanDeviceInfo info;
					info.name = props.deviceName;
					info.deviceType = (int)props.deviceType;
					info.usable = usable;
					Variable::VulkanDetectedDevices.push_back(std::move(info));

					if (!probeResult.anyHardware) {
						probeResult.anyHardware = true;
						probeResult.firstHardwareName = props.deviceName;
					}
					if (usable) {
						if (!probeResult.anyUsableHardware) {
							probeResult.anyUsableHardware = true;
							probeResult.firstUsableHardwareName = props.deviceName;
						}
					}
					else {
						probeResult.hardwareRejectReasons.push_back(std::string(props.deviceName) + ": " + rejectReason);
					}
				}
			}
		}
		else {
			VulkanDiag("[Vulkan] 探测: vkEnumeratePhysicalDevices 返回 %d (%s), deviceCount=%u\n",
				(int)result, vkResultName(result), deviceCount);
		}

		//探路用的实例马上销毁，真正的 VkInstance 由 Application 创建
		vkDestroyInstance(probeInstance, nullptr);

		return probeResult;
	}

	//确认 icd json 存在，并且它指向的 dll 也在（不然交给 loader 只是换个错误码）
	static bool icdManifestLooksUsable(const std::filesystem::path& manifestPath) {
		std::error_code ec;
		if (!std::filesystem::is_regular_file(manifestPath, ec)) return false;
		std::ifstream in(manifestPath);
		if (!in.is_open()) return true;//读不开就当它没问题，不去拦一个可能可用的驱动
		std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		const std::string marker = "\"library_path\"";
		size_t pos = text.find(marker);
		if (pos == std::string::npos) return true;
		pos = text.find('"', pos + marker.size());
		if (pos == std::string::npos) return true;
		size_t end = text.find('"', pos + 1);
		if (end == std::string::npos) return true;
		std::string lib = text.substr(pos + 1, end - pos - 1);
		while (!lib.empty() && (lib.front() == '.' || lib.front() == '\\' || lib.front() == '/')) {
			lib.erase(lib.begin());
		}
		if (lib.empty()) return true;
		return std::filesystem::is_regular_file(manifestPath.parent_path() / lib, ec);
	}

	static const std::vector<std::filesystem::path>& swiftShaderCandidateRoots() {
		static const std::vector<std::filesystem::path> roots = {
			"C:\\Program Files (x86)\\Microsoft\\EdgeCore\\Optimized",
			"C:\\Program Files\\Microsoft\\EdgeCore\\Optimized",
			"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application",
			"C:\\Program Files\\Microsoft\\EdgeWebView\\Application",
			"C:\\Program Files\\Microsoft\\Edge\\Application"
		};
		return roots;
	}

	static bool tryEnableSwiftShaderIcd() {
		std::error_code ec;
		for (const auto& root : swiftShaderCandidateRoots()) {
			if (!std::filesystem::is_directory(root, ec)) continue;
			//Optimized 目录下没有版本号，EdgeWebView/Edge 目录下带版本号子目录，所以递归找
			for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
				it != end; it.increment(ec)) {
				if (ec) break;
				if (!it->is_regular_file(ec)) continue;
				if (it->path().filename() != "vk_swiftshader_icd.json") continue;
				if (!icdManifestLooksUsable(it->path())) continue;
				::SetEnvironmentVariableA("VK_ICD_FILENAMES", it->path().string().c_str());
				VulkanDiag("[Vulkan] 已自动启用 SwiftShader（CPU 软件渲染）：%s\n", it->path().string().c_str());
				return true;
			}
		}
		return false;
	}
#endif

	//没装 Vulkan 驱动时自动切到 SwiftShader。返回是否切换成功。
	bool ensureVulkanIcdAvailable() {
#if defined(_WIN32)
		//先探明机器上到底有哪些 Vulkan 设备（纯查询，不改变任何东西）。必须放在最前面，有两个原因：
		//  1) 设置界面的"渲染设备"下拉框要用这份列表列出"识别到的显卡"，让用户能直接指定用哪一台；
		//  2) 一旦为 CPU 软件渲染设上 VK_ICD_FILENAMES，loader 就只看软件 ICD 了，那时再探就看不到显卡。
		const VulkanProbeResult probe = probeVulkanDevices();

		const bool wantCpu = Variable::IsCpuRenderingMode();

		//设置里明确选了"只用 CPU 软件渲染"：不管有没有显卡驱动都用 SwiftShader
		if (wantCpu) {
			Variable::CpuSoftwareRenderReason = "设置里选择了「CPU 软件渲染」";
			if (envVarIsSet("VK_DRIVER_FILES") || envVarIsSet("VK_ICD_FILENAMES")) {
				Variable::CpuSoftwareRenderReason = "设置里选择了「CPU 软件渲染」（沿用外部已指定的 ICD 环境变量）";
				VulkanDiag("[Vulkan] 已按设置使用 CPU 软件渲染（沿用外部已指定的 ICD 环境变量）。\n");
				return true;
			}
			VulkanDiag("[Vulkan] 设置中选择了「CPU 软件渲染」，尝试启用 SwiftShader...\n");
			if (tryEnableSwiftShaderIcd()) return true;
			VulkanDiag("[Vulkan] 启用 SwiftShader 失败：没有找到 vk_swiftshader_icd.json，"
				"请把它改成「自动选择最高性能」或安装支持 Vulkan 的显卡驱动。\n");
			return false;
		}

		if (envVarIsSet("VK_DRIVER_FILES") || envVarIsSet("VK_ICD_FILENAMES")) {
			return true; //用户自己指定了 ICD，尊重用户的选择
		}

		//有能用的显卡：走硬件。让 loader 自己回答"有没有显卡"，不要用注册表键去猜（见 probeVulkanDevices 的注释）。
		if (probe.anyUsableHardware) {
			VulkanDiag("[Vulkan] 检测到可用的显卡 Vulkan 驱动: %s（旧式注册表键 HKLM\\SOFTWARE\\Khronos\\Vulkan\\Drivers %s，"
				"本程序不再以它为判据）。\n",
				probe.firstUsableHardwareName.c_str(),
				registryDriversKeyExists() ? "存在" : "不存在，ICD 由驱动 PnP 注册信息提供");
			return true;
		}

		//枚举到了显卡但没有一台满足最低要求：这是最容易被误解成"程序不支持我的显卡"的情况，
		//所以把每一台的落选原因用醒目的框打出来（LOGE 的开关关掉时，也只有这里能看到原因）。
		if (probe.anyHardware) {
			VulkanDiag("[Vulkan] ================================================================\n");
			VulkanDiag("[Vulkan] 警告: 机器上枚举到 %zu 台显卡，但没有一台满足本程序的最低要求！\n",
				probe.hardwareRejectReasons.size());
			for (const auto& reason : probe.hardwareRejectReasons) {
				VulkanDiag("[Vulkan]   落选: %s\n", reason.c_str());
			}
			VulkanDiag("[Vulkan] 最低要求: 各向异性采样(samplerAnisotropy) + 设备扩展 VK_KHR_swapchain。\n");
			VulkanDiag("[Vulkan] ================================================================\n");
		}

		//设置里明确"指定了某一台设备"：不能用就直说，不偷偷换成软件渲染
		//（指定了显卡却悄悄跑在 CPU 上，正是本功能要避免的事）
		if (Variable::VulkanDeviceMode == Variable::VulkanDeviceModeEnum::Specific) {
			if (probe.anyHardware) {
				VulkanDiag("[Vulkan] 设置中指定了渲染设备「%s」，但识别到的显卡都不满足本程序的最低要求，"
					"不会回退到 CPU 软件渲染。\n"
					"[Vulkan]       要么换一张满足最低要求的显卡并安装驱动，要么把设置里的渲染设备改为"
					"「自动选择最高性能」或「CPU 软件渲染」。\n",
					Variable::VulkanDeviceName.c_str());
			}
			else {
				VulkanDiag("[Vulkan] 设置中指定了渲染设备「%s」，但 loader 报告没有任何可用的显卡 Vulkan 驱动(ICD)，"
					"不会回退到 CPU 软件渲染。\n"
					"[Vulkan]       要么安装支持 Vulkan 的显卡驱动，要么把设置里的渲染设备改为「自动选择最高性能」或「CPU 软件渲染」。\n",
					Variable::VulkanDeviceName.c_str());
			}
			return false;
		}

		//自动兜底的最后一层：没有显卡，或者有显卡但都不能用 —— 都降到 CPU 软件渲染，保证程序能起来
		if (probe.anyHardware) {
			VulkanDiag("[Vulkan] 以上显卡都无法使用，自动降级到 CPU 软件渲染(SwiftShader) —— 帧率会非常低！\n"
				"[Vulkan]       要恢复硬件渲染：更新现有显卡驱动，或换一张满足最低要求的显卡。\n");
			Variable::CpuSoftwareRenderReason = "识别到的显卡均不满足最低要求，已自动降级";
		}
		else {
			VulkanDiag("[Vulkan] loader 报告没有任何可用的显卡 Vulkan 驱动(ICD)，尝试自动启用 CPU 软件渲染(SwiftShader)...\n");
			Variable::CpuSoftwareRenderReason = "未检测到显卡 Vulkan 驱动";
		}
		if (tryEnableSwiftShaderIcd()) {
			VulkanDiag("[Vulkan] 提示: 当前使用 CPU 软件渲染，帧率会很低，仅用于验证程序可跑；\n"
				"[Vulkan]       要恢复硬件渲染，装一张支持 Vulkan 的显卡驱动即可（本程序会自动优先使用它）。\n");
			return true;
		}

		VulkanDiag("[Vulkan] 自动启用 SwiftShader 失败：没有找到 vk_swiftshader_icd.json。\n"
			"[Vulkan]       需要在 system32 或程序目录放一个 ICD json（例如 SwiftShader 的\n"
			"[Vulkan]       vk_swiftshader_icd.json + vk_swiftshader.dll），或者安装显卡驱动。\n");
		return false;
#else
		return true; //Android 的 ICD 是系统自带的
#endif
	}

	//VK_ERROR_INCOMPATIBLE_DRIVER 是"系统里一个 Vulkan 驱动(ICD)都没有"的专用信号：
	//loader 会返回它（Windows loader 的提示原文是 "vkCreateInstance: Found no drivers!"）。
	static bool isNoIcdDriverError(VkResult result) {
		if (result == VK_ERROR_INCOMPATIBLE_DRIVER) return true;
		if (result == VK_ERROR_INITIALIZATION_FAILED) return true;
		return false;
	}

	//validation layer 回调函数
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pMessageData,
		void* pUserData) {

		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			LOGW("ValidationLayer: %s", pMessageData->pMessage);
			if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				if (TOOL::logger) TOOL::logger->warn(pMessageData->pMessage);
			}else if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				if (TOOL::logger) TOOL::logger->error(pMessageData->pMessage);
			}
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
		LOGD("Instance::Instance(enableValidationLayer=%d)", enableValidationLayer);
		mEnableValidationLayer = enableValidationLayer;

		//崩溃诊断：始终输出（不受 TRANSLATOR_ENABLE_LOG 影响），方便定位初始化失败原因
		VulkanDiag("[Vulkan] === Instance 初始化开始 (validationLayer=%d) ===\n", (int)enableValidationLayer);

		uint32_t instanceVersion = VK_API_VERSION_1_0;
		PFN_vkEnumerateInstanceVersion pfnEnumerateInstanceVersion = 
			reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
		bool versionQueried = false;
		if (pfnEnumerateInstanceVersion) {
			if (pfnEnumerateInstanceVersion(&instanceVersion) == VK_SUCCESS) {
				versionQueried = true;
				LOGI("Vulkan instance version: %u.%u.%u",
					VK_VERSION_MAJOR(instanceVersion),
					VK_VERSION_MINOR(instanceVersion),
					VK_VERSION_PATCH(instanceVersion));
			}
		}
		VulkanDiag("[Vulkan] loader 报告的最高 instance 版本: %u.%u.%u%s\n",
			VK_VERSION_MAJOR(instanceVersion), VK_VERSION_MINOR(instanceVersion),
			VK_VERSION_PATCH(instanceVersion),
			versionQueried ? "" : " (vkEnumerateInstanceVersion 不可用，按 1.0 处理)");

		if (mEnableValidationLayer) {
			if (!checkValidationLayerSupport()) {
				mEnableValidationLayer = false;
				LOGW("Validation layer is not supported, disabled");
			}
		}

		printAvailableExtensions();

		std::vector<const char*> availableExtNames = getAvailableExtensionNames();

		extensions = getRequiredExtensions();
		if (extensions.empty()) {
			//GLFW 未初始化成功（glfwGetRequiredInstanceExtensions 返回空）时会出现这种情况，
			//这种失败在窗口系统层面，和 Vulkan 驱动无关，需要单独指出。
			VulkanDiag("[Vulkan] 警告: 必需的实例扩展列表为空 —— 通常是 glfwInit() 失败，"
					   "请检查 glfwGetError()。\n");
		}
		std::vector<const char*> filteredExtensions = filterExtensions(extensions, availableExtNames);

		uint32_t apiVersions[] = {
			(instanceVersion < VK_API_VERSION_1_3) ? instanceVersion : VK_API_VERSION_1_3,
			VK_API_VERSION_1_2,
			VK_API_VERSION_1_1,
			VK_API_VERSION_1_0
		};

		struct RetryConfig {
			uint32_t apiVersion;
			std::vector<const char*> exts;
			bool useValidation;
			const char* desc;
		};

		std::vector<RetryConfig> retryConfigs;

		retryConfigs.push_back({apiVersions[0], extensions, mEnableValidationLayer, "detected API + all required extensions"});
		retryConfigs.push_back({apiVersions[0], filteredExtensions, mEnableValidationLayer, "detected API + filtered extensions"});
		retryConfigs.push_back({VK_API_VERSION_1_0, extensions, mEnableValidationLayer, "API 1.0 + all required extensions"});
		retryConfigs.push_back({VK_API_VERSION_1_0, filteredExtensions, mEnableValidationLayer, "API 1.0 + filtered extensions"});
		retryConfigs.push_back({VK_API_VERSION_1_0, filteredExtensions, false, "API 1.0 + filtered extensions + no validation"});

		VkResult result = VK_ERROR_INITIALIZATION_FAILED;
		VkResult lastResult = VK_ERROR_INITIALIZATION_FAILED;
		bool created = false;
		//记录最后一次真正提交给 vkCreateInstance 的扩展列表，供失败诊断逐条核对
		std::vector<const char*> lastAttemptExtensions;

		for (size_t attempt = 0; attempt < retryConfigs.size(); attempt++) {
			const auto& cfg = retryConfigs[attempt];
			LOGI("Attempt %zu: %s (apiVersion=%u.%u.%u, %u extensions, validation=%d)",
				attempt, cfg.desc,
				VK_VERSION_MAJOR(cfg.apiVersion),
				VK_VERSION_MINOR(cfg.apiVersion),
				VK_VERSION_PATCH(cfg.apiVersion),
				(uint32_t)cfg.exts.size(), cfg.useValidation);
			VulkanDiag("[Vulkan] 尝试 %zu/%zu: %s (apiVersion=%u.%u.%u, 扩展=%u, 验证层=%d)\n",
				attempt + 1, retryConfigs.size(), cfg.desc,
				VK_VERSION_MAJOR(cfg.apiVersion),
				VK_VERSION_MINOR(cfg.apiVersion),
				VK_VERSION_PATCH(cfg.apiVersion),
				(uint32_t)cfg.exts.size(), (int)cfg.useValidation);

			VkApplicationInfo appInfo = {};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "vulkanLession";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "NO ENGINE";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = cfg.apiVersion;

			VkInstanceCreateInfo instCreateInfo = {};
			instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instCreateInfo.pApplicationInfo = &appInfo;
			instCreateInfo.enabledExtensionCount = static_cast<uint32_t>(cfg.exts.size());
			instCreateInfo.ppEnabledExtensionNames = cfg.exts.data();

			if (cfg.useValidation) {
				instCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
				instCreateInfo.ppEnabledLayerNames = validationLayers.data();
			}
			else {
				instCreateInfo.enabledLayerCount = 0;
			}

			result = vkCreateInstance(&instCreateInfo, nullptr, &mInstance);
			lastResult = result;
			lastAttemptExtensions = cfg.exts;
			if (result == VK_SUCCESS) {
				LOGI("vkCreateInstance succeeded on attempt %zu: %s", attempt, cfg.desc);
				VulkanDiag("[Vulkan] 第 %zu 次尝试成功: %s\n", attempt + 1, cfg.desc);
				extensions = cfg.exts;
				if (!cfg.useValidation) {
					mEnableValidationLayer = false;
				}
				created = true;
				break;
			}
			else {
				LOGW("Attempt %zu failed: VkResult = %d", attempt, (int)result);
				VulkanDiag("[Vulkan] 第 %zu 次尝试失败: VkResult=%d (%s)\n",
					attempt + 1, (int)result, vkResultName(result));
			}
		}

		if (!created) {
			LOGE("All vkCreateInstance attempts failed. Vulkan is not available on this device.");
			LOGE("  VK_ERROR_INCOMPATIBLE_DRIVER=%d, VK_ERROR_EXTENSION_NOT_PRESENT=%d, "
				 "VK_ERROR_LAYER_NOT_PRESENT=%d, VK_ERROR_INITIALIZATION_FAILED=%d",
				(int)VK_ERROR_INCOMPATIBLE_DRIVER, (int)VK_ERROR_EXTENSION_NOT_PRESENT,
				(int)VK_ERROR_LAYER_NOT_PRESENT, (int)VK_ERROR_INITIALIZATION_FAILED);

			//------------------------------------------------------------------------------------------
			// 以下诊断始终输出到 stderr（不受 TRANSLATOR_ENABLE_LOG 影响），用来解释"为什么失败"。
			//------------------------------------------------------------------------------------------
			VulkanDiag("\n[Vulkan] === vkCreateInstance 全部 %zu 次尝试失败，最后一次 VkResult=%d (%s) ===\n",
				retryConfigs.size(), (int)lastResult, vkResultName(lastResult));

			uint32_t icdExtCount = 0;
			VkResult enumIcdResult = vkEnumerateInstanceExtensionProperties(nullptr, &icdExtCount, nullptr);

			if (isNoIcdDriverError(lastResult)) {
				VulkanDiag("[Vulkan] 结论: 未安装 Vulkan 驱动(ICD)。\n");
				if (lastResult == VK_ERROR_INCOMPATIBLE_DRIVER) {
					VulkanDiag("[Vulkan] 判据: loader 返回 VK_ERROR_INCOMPATIBLE_DRIVER，其内部提示为\n"
							   "[Vulkan]       \"Found no drivers!\"（没有任何 ICD manifest 被注册）。\n");
				}
			}
			else if (lastResult == VK_ERROR_EXTENSION_NOT_PRESENT) {
				VulkanDiag("[Vulkan] 结论: loader 可用，但缺少必需的实例扩展。\n");
			}
			else if (lastResult == VK_ERROR_LAYER_NOT_PRESENT) {
				VulkanDiag("[Vulkan] 结论: 请求的校验层(layer)未安装。\n");
			}
			else if (lastResult == VK_ERROR_OUT_OF_HOST_MEMORY) {
				VulkanDiag("[Vulkan] 结论: 主机内存不足，无法初始化 Vulkan。\n");
			}
			else if (lastResult == VK_ERROR_INITIALIZATION_FAILED) {
				VulkanDiag("[Vulkan] 结论: 初始化失败。loader 通常也用它表示\"找不到可用驱动\"，\n");
			}
			else {
				VulkanDiag("[Vulkan] 结论: 未归类的失败（VkResult=%d），请对照 Vulkan 规范检查上述尝试记录。\n",
					(int)lastResult);
			}

			//loader 暴露的扩展数量是判断"有没有驱动"的直接旁证：扩展几乎全部来自 ICD，
			//一个 ICD 都没有时只剩 loader 自己那几条（本机实测为 4 条）。
			const bool suspiciouslyFewExtensions = (icdExtCount <= 8);
			VulkanDiag("[Vulkan] loader 暴露的实例扩展数量: %u%s\n", icdExtCount,
				(enumIcdResult != VK_SUCCESS) ? " (枚举本身失败)" :
				(suspiciouslyFewExtensions ? "  <-- 数量异常少，几乎没有驱动在提供扩展，基本可确定没有装 Vulkan 驱动" :
											 "  (正常数量，说明至少有一个 ICD 在工作)"));
			VulkanDiag("[Vulkan] 本次尝试使用的实例扩展: %zu 个\n", lastAttemptExtensions.size());
			for (size_t i = 0; i < lastAttemptExtensions.size(); i++) {
				const char* name = lastAttemptExtensions[i] ? lastAttemptExtensions[i] : "(null)";
				bool available = false;
				for (uint32_t j = 0; j < availableExtNames.size(); j++) {
					if (availableExtNames[j] && std::strcmp(availableExtNames[j], name) == 0) {
						available = true;
						break;
					}
				}
				VulkanDiag("[Vulkan]   %s  %s\n", available ? "[可用]" : "[缺失]", name);
			}
			if (mEnableValidationLayer) {
				VulkanDiag("[Vulkan] 校验层当前为开启状态；若缺少 VK_LAYER_KHRONOS_validation，"
						   "可在设置中关闭校验层后重试。\n");
			}
			if (isNoIcdDriverError(lastResult)) {
				VulkanDiag("[Vulkan] 修复建议:\n"
						   "[Vulkan]   1) 安装带 Vulkan 运行时的显卡驱动。注意 Intel 第 4/5 代核显\n"
						   "[Vulkan]      （HD 4200/4400/4600/5000/5500/6000、Iris 6100/6200）在 Windows 10 上\n"
						   "[Vulkan]      官方从未提供 Vulkan ICD（Intel 支持表里 Vulkan 一栏为 N/A），\n"
						   "[Vulkan]      装 15.40 等旧版驱动也不会有 igvk64.dll，请不要再回滚驱动。\n"
						   "[Vulkan]   2) 加装一张支持 Vulkan 的独立显卡（如 GTX 1050 / RX 550 级别即可）。\n"
						   "[Vulkan]   3) 用 CPU 软件渲染兜底：本程序已支持在 SwiftShader 上运行，\n"
						   "[Vulkan]      设置环境变量 VK_ICD_FILENAMES 指向 vk_swiftshader_icd.json 即可，\n"
						   "[Vulkan]      例如 Edge 自带的那份：\n"
						   "[Vulkan]      C:\\Program Files (x86)\\Microsoft\\EdgeCore\\Optimized\\vk_swiftshader_icd.json\n"
						   "[Vulkan]      （帧率很低，仅用于验证程序可跑；几何着色器已改为可选，缺失时自动回退。）\n"
						   "[Vulkan]   验证: 命令行执行 vulkaninfo --summary，能看到 GPU0 才算有 ICD。\n"
						   "[Vulkan]   排查: 注册表 HKLM\\SOFTWARE\\Khronos\\Vulkan\\Drivers 应存在且指向 icd json 文件。\n");
			}
			VulkanDiag("[Vulkan] === 诊断结束 ===\n\n");

			if (TOOL::logger) TOOL::logger->error("Error:failed to create instance, Vulkan not available");
			throw std::runtime_error("Error:failed to create instance");
		}

		if (mEnableValidationLayer) {
			setupDebugger();
		}
	}

	Instance::~Instance() {
		if (mEnableValidationLayer) {
			DestroyDebugUtilsMessengerEXT(mInstance, mDebugger, nullptr);
		}

		vkDestroyInstance(mInstance, nullptr);
	}

	void Instance::printAvailableExtensions() {
		uint32_t extensionCount = 0;
		VkResult enumResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		if (enumResult != VK_SUCCESS) {
			LOGE("printAvailableExtensions: vkEnumerateInstanceExtensionProperties failed, VkResult=%d", (int)enumResult);
			return;
		}

		std::vector<VkExtensionProperties> extensions(extensionCount);
		enumResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		if (enumResult != VK_SUCCESS) {
			LOGE("printAvailableExtensions: vkEnumerateInstanceExtensionProperties(2nd call) failed, VkResult=%d", (int)enumResult);
			return;
		}

		LOGI("Available instance extensions (%u):", extensionCount);
		for (uint32_t i = 0; i < extensionCount; i++) {
			LOGI("  [%u] %s (spec %u.%u.%u)", i, extensions[i].extensionName,
				VK_VERSION_MAJOR(extensions[i].specVersion),
				VK_VERSION_MINOR(extensions[i].specVersion),
				VK_VERSION_PATCH(extensions[i].specVersion));
		}
	}

	std::vector<const char*> Instance::getAvailableExtensionNames() {
		std::vector<const char*> names;
		uint32_t extensionCount = 0;
		VkResult enumResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		if (enumResult != VK_SUCCESS || extensionCount == 0) {
			return names;
		}
		std::vector<VkExtensionProperties> props(extensionCount);
		enumResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, props.data());
		if (enumResult != VK_SUCCESS) {
			return names;
		}
		names.reserve(extensionCount);
		for (uint32_t i = 0; i < extensionCount; i++) {
			names.push_back(props[i].extensionName);
		}
		return names;
	}

	std::vector<const char*> Instance::filterExtensions(const std::vector<const char*>& required, const std::vector<const char*>& available) {
		std::vector<const char*> filtered;
		for (const auto& req : required) {
			bool found = false;
			for (const auto& avail : available) {
				if (strcmp(req, avail) == 0) {
					found = true;
					break;
				}
			}
			if (found) {
				filtered.push_back(req);
			}
			else {
				LOGW("Extension '%s' not available, skipping", req);
			}
		}
		return filtered;
	}

	std::vector<const char*> Instance::getRequiredExtensions() {
#if defined(_WIN32)
		uint32_t glfwExtensionCount = 0;

		//得到要求实例扩展
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#elif defined(__ANDROID__)
		std::vector<const char*> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			"VK_KHR_android_surface"
		};
#endif

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

		if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugger) != VK_SUCCESS) {
			LOGE("Instance::setupDebugger: failed to create debugger");
			throw std::runtime_error("Error:VulKan Instance . failed to create debugger");
		}
	}
}