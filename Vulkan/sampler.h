#pragma once

#include "../base.h"
#include "device.h"

namespace GAME::VulKan {

	class Sampler {
	public:
		Sampler(Device* device);

		~Sampler();

		[[nodiscard]] auto getSampler() const { return mSampler; }

	private:
		Device* mDevice{ nullptr };
		VkSampler mSampler{ VK_NULL_HANDLE };
	};
}