#include "shader.h"

namespace GAME::VulKan {

	static std::vector<char> readBinary(const std::string& fileName) {
		std::ifstream file(fileName.c_str(), std::ios::ate | std::ios::binary | std::ios::in);

		if (!file) {
			throw std::runtime_error("Error: failed to open shader file");
		}

		const size_t fileSize = file.tellg();//获取有多少字节
		std::vector<char> buffer(fileSize);

		file.seekg(0);//回到开头
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	Shader::Shader(Device* device, const std::string& fileName, VkShaderStageFlagBits shaderStage, const std::string& entryPoint) {
		mDevice = device;
		mShaderStage = shaderStage;
		mEntryPoint = entryPoint;

		std::vector<char> codeBuffer = readBinary(fileName);

		VkShaderModuleCreateInfo shaderCreateInfo{};
		shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderCreateInfo.codeSize = codeBuffer.size();
		shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(codeBuffer.data());//数据强制转换为uint32_t

		if (vkCreateShaderModule(mDevice->getDevice(), &shaderCreateInfo, nullptr, &mShaderModule) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create shader");
		}

		codeBuffer.clear();
	}

	Shader::~Shader() {
		if (mShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(mDevice->getDevice(), mShaderModule, nullptr);
		}
	}
}