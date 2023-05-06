#pragma once

#include "../base.h"
#include "device.h"

//用来同步，
//列：通知GPU是否画完画布，或 ，画布是否显示完毕

namespace GAME::VulKan {

	class Semaphore {
	public:
		Semaphore(Device* device);

		~Semaphore();

		[[nodiscard]] auto getSemaphore() const { return mSemaphore; }
	private:
		VkSemaphore mSemaphore{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };
	};
}