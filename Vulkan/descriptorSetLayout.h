#pragma once

#include "../base.h"
#include "device.h"
#include "description.h"

namespace GAME::VulKan {

	class DescriptorSetLayout {
	public:


		DescriptorSetLayout(Device* device);

		~DescriptorSetLayout();

		void build(const std::vector<UniformParameter*>& params);

		[[nodiscard]] VkDescriptorSetLayout getLayout() const { return mLayout; }

	private:
		VkDescriptorSetLayout mLayout{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };

		std::vector<UniformParameter*> mParams{};
	};
}