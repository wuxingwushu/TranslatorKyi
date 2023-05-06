#pragma once

#include "../base.h"
#include "device.h"

namespace GAME::VulKan {

	class Shader {
	public:
		Shader(Device* device, const std::string &fileName, VkShaderStageFlagBits shaderStage, const std::string &entryPoint);

		~Shader();

		[[nodiscard]] auto getShaderStage() const { return mShaderStage; }
		[[nodiscard]] auto& getShaderEntryPoint() const { return mEntryPoint; }
		[[nodiscard]] auto getShaderModule() const { return mShaderModule; }

	private:
		VkShaderModule mShaderModule{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };
		std::string mEntryPoint;
		VkShaderStageFlagBits mShaderStage;
	};
}