#pragma once
#include "device.h"

namespace VulKan {

	class Sampler {
	public:
		Sampler(Device* device);

		~Sampler();

		[[nodiscard]] inline VkSampler getSampler() const noexcept { return mSampler; }

	private:
		Device* mDevice{ nullptr };
		VkSampler mSampler{ VK_NULL_HANDLE };
	};
}