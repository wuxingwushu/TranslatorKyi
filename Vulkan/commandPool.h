#pragma once
#include "device.h"

namespace VulKan {

	class CommandPool {
	public:
		CommandPool(Device* device, VkCommandPoolCreateFlagBits flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		~CommandPool();

		[[nodiscard]] inline VkCommandPool getCommandPool() const noexcept { return mCommandPool; }

		void reset() {
			if (mCommandPool != VK_NULL_HANDLE) {
				vkResetCommandPool(mDevice->getDevice(), mCommandPool, 0);
			}
		}

	private:
		VkCommandPool mCommandPool{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };
	};
}