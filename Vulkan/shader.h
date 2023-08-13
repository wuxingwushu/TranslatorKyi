#pragma once

#include "../base.h"
#include "device.h"

namespace VulKan {

	class Shader {
	public:
		Shader(Device* device, const std::string &fileName, VkShaderStageFlagBits shaderStage, const std::string &entryPoint);

		~Shader();

		[[nodiscard]] auto getShaderStage() const noexcept { return mShaderStage; }
		[[nodiscard]] auto& getShaderEntryPoint() const noexcept { return mEntryPoint; }
		[[nodiscard]] auto getShaderModule() const noexcept { return mShaderModule; }

	private:
		VkShaderModule mShaderModule{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };
		std::string mEntryPoint;
		VkShaderStageFlagBits mShaderStage;
	};
}