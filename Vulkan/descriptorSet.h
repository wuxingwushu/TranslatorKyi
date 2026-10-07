#pragma once
#include "device.h"
#include "descriptorSetLayout.h"
#include "descriptorPool.h"

namespace VulKan {
	/*
	* 对于每一个模型的渲染，都需要绑定一个DescriptorSet，绑定的位置就是在CommandBuffer
	* 一个DescriptorSet里面，都对应着一个vp矩阵使用的buffer，一个model矩阵使用的buffer，等等,其中也包括
	* binding size等等的描述信息
	* 由于交换链的存在，多帧有可能并行渲染，所以我们需要为每一个交换链的图片，对应生成一个DescriptorSet
	*/

	class DescriptorSet {
	public:
		

		DescriptorSet(
			Device* device,
			const std::vector<UniformParameter*> params,
			const VkDescriptorSetLayout layout,
			const DescriptorPool* pool,
			int frameCount,
			std::mutex* wMutex = nullptr
		);

		~DescriptorSet();

		//更新绑定的图片
		void UpDataImagePicture(unsigned int Index, VkDescriptorImageInfo* ImageInfo);
		void UpDataBufferPicture(unsigned int Index, VkDescriptorBufferInfo* BufferInfo);

		[[nodiscard]] inline VkDescriptorSet getDescriptorSet(int frameCount) const { return mDescriptorSets[frameCount]; }

	private:
		unsigned int wFrameCount = 0;
		unsigned int mDescriptorSize = 0;
		const DescriptorPool* mDescriptorPool;
		// 对每个DescriptorSet，我们需要把params里面的描述信息，写入其中
		// VkWriteDescriptorSet 存储 pBufferInfo 指针，但源 Buffer 可能被销毁导致悬空指针
		// 因此将 VkDescriptorBufferInfo 按值复制到 mBufferInfoStorage 中保存
		std::vector<std::vector<VkWriteDescriptorSet>> descriptorSetWrites{};
		std::vector<VkDescriptorSet> mDescriptorSets{};
		std::vector<std::vector<VkDescriptorBufferInfo>> mBufferInfoStorage;      // 本地存储避免悬空指针
		std::vector<VkDescriptorBufferInfo> mUpdateBufferInfoStorage;            // UpDataBufferPicture 本地存储
		Device* mDevice{ nullptr };
	};
}