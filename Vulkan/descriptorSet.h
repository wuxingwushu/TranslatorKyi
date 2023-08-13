#pragma once

#include "../base.h"
#include "device.h"
#include "description.h"
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
			int frameCount
		);

		~DescriptorSet();

		[[nodiscard]] auto getDescriptorSet(int frameCount) const { return mDescriptorSets[frameCount]; }

	private:
		const DescriptorPool* mDescriptorPool;
		std::vector<VkDescriptorSet> mDescriptorSets{};
		Device* mDevice{ nullptr };
	};
}