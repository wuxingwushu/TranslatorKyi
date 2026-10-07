#include "descriptorSet.h"
#include <mutex>
#include "../DebugLog.h"

namespace VulKan {

	DescriptorSet::DescriptorSet(
		Device* device,
		const std::vector<UniformParameter*> params,
		const VkDescriptorSetLayout layout,
		const DescriptorPool* pool,
		int frameCount,
		std::mutex* wMutex
	) :
		mDevice(device),
		mDescriptorPool(pool),
		wFrameCount(frameCount)
	{
		LOGD("DescriptorSet::DescriptorSet(frameCount=%d)", frameCount);
		std::vector<VkDescriptorSetLayout> layouts(frameCount, layout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool->getPool();
		allocInfo.descriptorSetCount = frameCount;
		allocInfo.pSetLayouts = layouts.data();
		
		mDescriptorSets.resize(frameCount);
		if (wMutex != nullptr) {
			std::lock_guard<std::mutex> lock(*wMutex);
			VkResult result = vkAllocateDescriptorSets(mDevice->getDevice(), &allocInfo, mDescriptorSets.data());
			if (result != VK_SUCCESS) {
				LOGE("DescriptorSet::DescriptorSet: failed to allocate descriptor sets (mutex), VkResult=%d", result);
				throw std::runtime_error("Error: failed to allocate descriptor sets");
			}
		}
		else {
			VkResult result = vkAllocateDescriptorSets(mDevice->getDevice(), &allocInfo, mDescriptorSets.data());
			if (result != VK_SUCCESS) {
				LOGE("DescriptorSet::DescriptorSet: failed to allocate descriptor sets, VkResult=%d", result);
				throw std::runtime_error("Error: failed to allocate descriptor sets");
			}
		}
		
		mDescriptorSize = params.size();

		// 预分配本地 BufferInfo 存储，避免悬空指针引用已被销毁的 Buffer
		mBufferInfoStorage.resize(frameCount, std::vector<VkDescriptorBufferInfo>(params.size()));

		for (int i = 0; i < frameCount; ++i) {
			std::vector<VkWriteDescriptorSet> LSdescriptorSetWrites;
			int paramIndex = 0;
			for (const auto& param : params) {
				VkWriteDescriptorSet descriptorSetWrite{};
				descriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorSetWrite.dstSet = mDescriptorSets[i];
				descriptorSetWrite.dstArrayElement = 0;
				descriptorSetWrite.descriptorType = param->mDescriptorType;
				descriptorSetWrite.descriptorCount = param->mCount;
				descriptorSetWrite.dstBinding = param->mBinding;
				
				if ((param->mDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (param->mDescriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
					if (i < (int)param->mBuffers.size() && param->mBuffers[i] != nullptr) {
						mBufferInfoStorage[i][paramIndex] = param->mBuffers[i]->getBufferInfo();
						descriptorSetWrite.pBufferInfo = &mBufferInfoStorage[i][paramIndex];
					} else {
						LOGE("DescriptorSet: param[%d] mBuffers[%d] is null or out of range (mBuffers.size=%zu)", paramIndex, i, param->mBuffers.size());
						mBufferInfoStorage[i][paramIndex] = {};
						descriptorSetWrite.pBufferInfo = &mBufferInfoStorage[i][paramIndex];
					}
				}
				else if ((param->mDescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) || (param->mDescriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)) {
					if (param->mTexture != nullptr) {
						descriptorSetWrite.pImageInfo = &param->mTexture->getImageInfo();
					} else {
						LOGE("DescriptorSet: param[%d] mTexture is null (type=%d)", paramIndex, (int)param->mDescriptorType);
					}
				}

				LSdescriptorSetWrites.push_back(descriptorSetWrite);
				paramIndex++;
			}
			descriptorSetWrites.push_back(LSdescriptorSetWrites);

			vkUpdateDescriptorSets(mDevice->getDevice(), static_cast<uint32_t>(LSdescriptorSetWrites.size()), LSdescriptorSetWrites.data(), 0, nullptr);
		}
	}

	void DescriptorSet::UpDataImagePicture(unsigned int Index, VkDescriptorImageInfo* ImageInfo) {
		for (auto& DescriptorSetWrites : descriptorSetWrites) {
			if ((DescriptorSetWrites[Index].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) || (DescriptorSetWrites[Index].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)) {
				DescriptorSetWrites[Index].pImageInfo = ImageInfo;
			}
			vkUpdateDescriptorSets(mDevice->getDevice(), static_cast<uint32_t>(DescriptorSetWrites.size()), DescriptorSetWrites.data(), 0, nullptr);
		}
	}

	void DescriptorSet::UpDataBufferPicture(unsigned int Index, VkDescriptorBufferInfo* BufferInfo) {
		if (BufferInfo == nullptr) {
			LOGE("DescriptorSet::UpDataBufferPicture: BufferInfo is null");
			return;
		}
		mUpdateBufferInfoStorage.resize(1);
		mUpdateBufferInfoStorage[0] = *BufferInfo;
		for (auto& DescriptorSetWrites : descriptorSetWrites) {
			if (DescriptorSetWrites[Index].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
				DescriptorSetWrites[Index].pBufferInfo = &mUpdateBufferInfoStorage[0];
			}
			vkUpdateDescriptorSets(mDevice->getDevice(), static_cast<uint32_t>(DescriptorSetWrites.size()), DescriptorSetWrites.data(), 0, nullptr);
		}
	}

	DescriptorSet::~DescriptorSet() {
		vkFreeDescriptorSets(mDevice->getDevice(), mDescriptorPool->getPool(), mDescriptorSets.size(), mDescriptorSets.data());
	}
}