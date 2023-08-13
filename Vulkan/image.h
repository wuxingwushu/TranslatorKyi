#pragma once

#include "../base.h"
#include "device.h"
#include "commandPool.h"
#include "commandBuffer.h"
#include "buffer.h"

namespace VulKan {
	/*
	* 分析：如果我们需要做一张被用于纹理采样的图片，那么我们首先
	* 需要从undefinedLayout变换成为TransferDst， 然后在数据拷贝
	* 完毕之后，再转换称为ShaderReadOnly
	*/
	
	class Image {
	public:
		//工具函数,静态
		static Image* createDepthImage(
			Device* device, 
			const int& width,
			const int& height,
			VkSampleCountFlagBits sample
		);

		static Image* createRenderTargetImage(
			Device* device,
			const int& width,
			const int& height,
			VkFormat format
		);

	public:

		Image(
			Device* device,
			const int& width,
			const int& height,
			const VkFormat &format,
			const VkImageType &imageType,//是2D纹理，还是3D纹理（体素）
			const VkImageTiling &tiling,
			const VkImageUsageFlags &usage,
			const VkSampleCountFlagBits sample,
			const VkMemoryPropertyFlags &properties,//memory
			const VkImageAspectFlags &aspectFlags//view
		);

		~Image();

		//此处属于便捷写法，封装性比较好，但是可以独立作为一个工具函数
		//写到Tool的类里面
		void setImageLayout(
			VkImageLayout newLayout, 
			VkPipelineStageFlags srcStageMask, 
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresrouceRange,
			const CommandPool* commandPool
			);

		//多线程设置ImageLayout
		void ThreadSetImageLayout(
			VkImageLayout newLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresrouceRange,
			CommandBuffer* commandbuffer
		);


		void fillImageData(size_t size, void* pData);

		void ThreadFillImageData(size_t size, void* pData);

		void ThreadGetImageCommandBuffer(CommandBuffer* commandbuffer);

		void updateBufferByMap(void* data, size_t size);

		[[nodiscard]] auto getImage() const noexcept { return mImage; }

		[[nodiscard]] auto getLayout() const noexcept { return mLayout; }

		[[nodiscard]] auto getWidth() const noexcept { return mWidth; }

		[[nodiscard]] auto getHeight() const noexcept { return mHeight; }

		[[nodiscard]] auto getImageView() const noexcept { return mImageView; }

		//[[nodiscard]] auto getImageBuffer() const { return fillstageBuffer; }

	public:
		static VkFormat findDepthFormat(const Device* device);
		static VkFormat findSupportedFormat(
			const Device* device,
			const std::vector<VkFormat>& candidates,
			VkImageTiling tiling,
			VkFormatFeatureFlags features
		);

		bool hasStencilComponent(VkFormat format);

	private:
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

		

	private:
		size_t				mWidth{ 0 };
		size_t				mHeight{ 0 };
		Device*			    mDevice{ nullptr };
		VkImage				mImage{ VK_NULL_HANDLE };//CPU端的内存描述
		VmaAllocation mAllocation;// vma 的GPU内存描述
		//VkDeviceMemory		mImageMemory{ VK_NULL_HANDLE };//GPU端的内存描述，（废弃，专用 vma）
		VkImageView			mImageView{ VK_NULL_HANDLE };
		VkFormat			mFormat;

		VkImageLayout		mLayout{VK_IMAGE_LAYOUT_UNDEFINED};


		
	
	private://这些数据是临时的，用来上传数据用的
		CommandBuffer* LayoutcommandBuffer{ VK_NULL_HANDLE };
		Buffer* fillstageBuffer{ VK_NULL_HANDLE };
	};
}