#include "sampler.h"

namespace GAME::VulKan {

	Sampler::Sampler(Device* device) {
		mDevice = device;

		VkSamplerCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		createInfo.magFilter = VK_FILTER_NEAREST;//图片被放大时怎么采样
		createInfo.minFilter = VK_FILTER_NEAREST;//图片被缩小时怎么采样
		createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		createInfo.anisotropyEnable = VK_TRUE;
		createInfo.maxAnisotropy = 16;//最多采样多少个像素点，（支持到16的最多）

		createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;//UV值超过了 1.0f 的话怎么办。（我们这里用黑色填充）

		//我们是否使用非归一化的uvw坐标体系0-1
		createInfo.unnormalizedCoordinates = VK_FALSE;

		//采样得到的样本值需要跟一个值进行比较，比较的方式也可以设置，比较通过了测试，才采样，否则不采用
		createInfo.compareEnable = VK_FALSE;
		createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		createInfo.mipLodBias = 0.0f;
		createInfo.minLod = 0.0f;
		createInfo.maxLod = 0.0f;

		if (vkCreateSampler(mDevice->getDevice(), &createInfo, nullptr, &mSampler) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create sampler");
		}
	}

	Sampler::~Sampler() {
		if(mSampler != VK_NULL_HANDLE) {
			vkDestroySampler(mDevice->getDevice(), mSampler, nullptr);
		}
	}
}