#pragma once

#include "../base.h"
#include "../VulKan/image.h"//����ͼƬ
#include "../VulKan/sampler.h"//����ͼƬ������
#include "../VulKan/device.h"
#include "../VulKan/commandPool.h"



namespace GAME {

	class Texture {
	public:
		Texture(
			VulKan::Device* device,
			const VulKan::CommandPool* commandPool,
			VulKan::Sampler* sampler,
			int texWidth,
			int texHeight,
			char* data
		);

		~Texture();

		[[nodiscard]] auto getImage() const { return mImage; }
		
		//[[nodiscard]] auto getSampler() const { return mSampler; }

		[[nodiscard]] VkDescriptorImageInfo& getImageInfo() { return mImageInfo; }

	private:
		VulKan::Device* mDevice{ nullptr };
		VulKan::Image* mImage{ nullptr };
		VkDescriptorImageInfo mImageInfo{};
	};

}