#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION//stb_image.h �ĺ궨��
#include "../stb_image.h"

namespace GAME {

	Texture::Texture(
		VulKan::Device* device, 
		const VulKan::CommandPool* commandPool,
		VulKan::Sampler* sampler, 
		int texWidth, 
		int texHeight, 
		char* data
	) {
		mDevice = device;

		int texSize = texWidth * texHeight * 4;

		

		mImage = new VulKan::Image(
			mDevice, texWidth, texHeight,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		VkImageSubresourceRange region{};
		region.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		region.baseArrayLayer = 0;
		region.layerCount = 1;

		region.baseMipLevel = 0;
		region.levelCount = 1;

		mImage->setImageLayout(
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			region,
			commandPool
		);

		mImage->fillImageData(texSize, (void*)data);

		mImage->setImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			region,
			commandPool
		);

		

		mImageInfo.imageLayout = mImage->getLayout();
		mImageInfo.imageView = mImage->getImageView();
		mImageInfo.sampler = sampler->getSampler();
	}

	Texture::~Texture() {
		mImage->~Image();
		
	}
}