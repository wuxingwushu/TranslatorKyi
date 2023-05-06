#include "descriptorSetLayout.h"

namespace GAME::VulKan {

	DescriptorSetLayout::DescriptorSetLayout(Device* device) {
		mDevice = device;
	}

	DescriptorSetLayout::~DescriptorSetLayout() {
		if (mLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(mDevice->getDevice(), mLayout, nullptr);
		}
	}

	void DescriptorSetLayout::build(const std::vector<UniformParameter*>& params) {
		mParams = params;

		if (mLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(mDevice->getDevice(), mLayout, nullptr);
		}

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};

		for (const auto& param : mParams) {
			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.descriptorType = param->mDescriptorType;//绑定类型
			layoutBinding.binding = param->mBinding;//那个Binding通道
			layoutBinding.stageFlags = param->mStage;//数据大小
			layoutBinding.descriptorCount = param->mCount;//多少个数据

			layoutBindings.push_back(layoutBinding);
		}

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
		createInfo.pBindings = layoutBindings.data();

		if (vkCreateDescriptorSetLayout(mDevice->getDevice(), &createInfo, nullptr, &mLayout) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create descriptor set layout");
		}
	}

}