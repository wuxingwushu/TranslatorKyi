#pragma once
#include "device.h"
#include "description.h"

namespace VulKan {

	class DescriptorPool {
	public:
		DescriptorPool(Device* device);

		~DescriptorPool();

		void build(std::vector<UniformParameter*>& params, const int &frameCount, unsigned int shuliang = 1);

		[[nodiscard]] inline VkDescriptorPool getPool() const noexcept { return mPool; }

	private:
		//需要知道，每一种uniform都有多少个，才能为其预留分配空间，所谓空间并不是uniformbuffer的大小
		//而是，根据每种uniform不同，那么描述符就不同，所以说空间指的是描述符的大小，蕴含在系统内部的
		VkDescriptorPool mPool{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };
	};
}