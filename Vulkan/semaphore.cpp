#include "semaphore.h"
#include "../DebugLog.h"

namespace VulKan {

	Semaphore::Semaphore(Device* device) {
		LOGD("Semaphore::Semaphore()");
		mDevice = device;

		VkSemaphoreCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(mDevice->getDevice(), &createInfo, nullptr, &mSemaphore) != VK_SUCCESS) {
			LOGE("Semaphore::Semaphore: failed to create semaphore");
			throw std::runtime_error("Error: failed to create Semaphore");
		}
	}

	Semaphore::~Semaphore() {
		if (mSemaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(mDevice->getDevice(), mSemaphore, nullptr);
		}
	}
}