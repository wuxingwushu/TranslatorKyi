#pragma once
#include "device.h"

namespace VulKan {

	class Shader {
	public:
		Shader(Device* device, const std::string &fileName, VkShaderStageFlagBits shaderStage, const std::string &entryPoint);

		~Shader();

		[[nodiscard]] inline VkShaderStageFlagBits getShaderStage() const noexcept { return mShaderStage; }
		[[nodiscard]] inline auto& getShaderEntryPoint() const noexcept { return mEntryPoint; }
		[[nodiscard]] inline VkShaderModule getShaderModule() const noexcept { return mShaderModule; }

	private:
		VkShaderModule mShaderModule{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };
		std::string mEntryPoint;
		VkShaderStageFlagBits mShaderStage;
	};
}