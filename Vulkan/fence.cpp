#include "fence.h"

namespace GAME::VulKan {

	Fence::Fence(Device* device, bool signaled) {
		mDevice = device;

		VkFenceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;//�Ƿ��Ǽ���̬

		if (vkCreateFence(mDevice->getDevice(), &createInfo, nullptr, &mFence) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to create fence");
		}
	}

	Fence::~Fence() {
		if (mFence != VK_NULL_HANDLE) {
			vkDestroyFence(mDevice->getDevice(), mFence, nullptr);
		}
	}

	void Fence::resetFence() {
		vkResetFences(mDevice->getDevice(), 1, &mFence);
	}

	void Fence::block(uint64_t timeout) {
		vkWaitForFences(mDevice->getDevice(), 1, &mFence, VK_TRUE, timeout);
	}
}