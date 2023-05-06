#include "commandPool.h"

namespace GAME::VulKan {

	CommandPool::CommandPool(Device* device, VkCommandPoolCreateFlagBits flag) {
		mDevice = device;

		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.queueFamilyIndex = device->getGraphicQueueFamily().value();

		//指令修改的属性、指令池的内存属性
		//VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: 分配出来的CommandBuffer可以单独更新、单独重置
		//VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: 每一个CommandBuffer不能单独reset，必须集体重置，vkResetCommandPool
		createInfo.flags = flag;

		if (vkCreateCommandPool(mDevice->getDevice(), &createInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
			throw std::runtime_error("Error:  failed to create command pool");
		}
	}

	CommandPool::~CommandPool() {
		if (mCommandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(mDevice->getDevice(), mCommandPool, nullptr);
		}
	}
}