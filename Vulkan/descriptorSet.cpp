#include "descriptorSet.h"

namespace VulKan {

	DescriptorSet::DescriptorSet(
		Device* device,
		const std::vector<UniformParameter*> params,
		const VkDescriptorSetLayout layout,
		const DescriptorPool* pool,
		int frameCount
	) {
		mDescriptorPool = pool;
		mDevice = device;

		std::vector<VkDescriptorSetLayout> layouts(frameCount, layout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool->getPool();
		allocInfo.descriptorSetCount = frameCount;
		allocInfo.pSetLayouts = layouts.data();

		mDescriptorSets.resize(frameCount);
		if (vkAllocateDescriptorSets(mDevice->getDevice(), &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to allocate descriptor sets");
		}

		for (int i = 0; i < frameCount; ++i) {
			//对每个DescriptorSet，我们需要把params里面的描述信息，写入其中
			std::vector<VkWriteDescriptorSet> descriptorSetWrites{};
			for (const auto& param : params) {
				VkWriteDescriptorSet descriptorSetWrite{};
				descriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorSetWrite.dstSet = mDescriptorSets[i];//
				descriptorSetWrite.dstArrayElement = 0;
				descriptorSetWrite.descriptorType = param->mDescriptorType;//数据类型
				descriptorSetWrite.descriptorCount = param->mCount;//多少个这样的数据
				descriptorSetWrite.dstBinding = param->mBinding;//绑定到那个Binding
				
				if (param->mDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
					descriptorSetWrite.pBufferInfo = &param->mBuffers[i]->getBufferInfo();
				}

				if (param->mDescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
					descriptorSetWrite.pImageInfo = &param->mTexture->getImageInfo();
				}

				descriptorSetWrites.push_back(descriptorSetWrite);
			}

			vkUpdateDescriptorSets(mDevice->getDevice(), static_cast<uint32_t>(descriptorSetWrites.size()), descriptorSetWrites.data(), 0, nullptr);
		}
	}

	DescriptorSet::~DescriptorSet() {
		vkFreeDescriptorSets(mDevice->getDevice(), mDescriptorPool->getPool(), mDescriptorSets.size(), mDescriptorSets.data());
	}
}