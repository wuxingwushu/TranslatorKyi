#pragma once

#include "../base.h"
#include "device.h"
#include "window.h"
#include "windowSurface.h"
#include "renderPass.h"
#include "image.h"
#include "commandPool.h"

namespace GAME::VulKan {

	struct SwapChainSupportInfo {
		VkSurfaceCapabilitiesKHR mCapabilities;
		std::vector<VkSurfaceFormatKHR> mFormats;
		std::vector<VkPresentModeKHR> mPresentModes;
	};

	//创建一张MutiSample的图片，并且加入到FrameBuffer里面
	class SwapChain {
	public:
		SwapChain(
			Device* device, 
			Window* window, 
			WindowSurface* surface,
			CommandPool* commandPool
		);

		~SwapChain();

		//查找交换链支持信息
		SwapChainSupportInfo querySwapChainSupportInfo();
		//选择表面格式
		VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
		//选择表面呈现模式
		VkPresentModeKHR chooseSurfacePresentMode(const std::vector<VkPresentModeKHR> &availablePresenstModes);
		//选择范围
		VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR & capabilities);

		void createFrameBuffers(const RenderPass* renderPass);

	public:
		[[nodiscard]] VkFormat getFormat() const { return mSwapChainFormat; }

		//获取GPU画布的数量
		[[nodiscard]] auto getImageCount() const { return mImageCount; }

		[[nodiscard]] auto getSwapChain() const { return mSwapChain; }

		//获得是哪一个GPU画布
		[[nodiscard]] auto getFrameBuffer(const int index) const { return mSwapChainFrameBuffers[index]; }

		//获得屏幕的长宽
		[[nodiscard]] auto getExtent() const { return mSwapChainExtent; }

	private:
		//创建imageView
		VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1);

	private:
		VkSwapchainKHR mSwapChain{ VK_NULL_HANDLE };

		VkFormat  mSwapChainFormat;
		VkExtent2D mSwapChainExtent;
		uint32_t mImageCount{ 0 };

		//vkimage由swapchain创建，销毁也要交给swapchain
		std::vector<VkImage> mSwapChainImages{};

		//对图像的管理器。管理框架
		std::vector<VkImageView> mSwapChainImageViews{};

		std::vector<VkFramebuffer> mSwapChainFrameBuffers{};

		//深度图片
		std::vector<Image*> mDepthImages{};

		//多重采样，中间图片
		std::vector<Image*> mMutiSampleImages{};

		Device* mDevice{ nullptr };
		Window* mWindow{ nullptr };
		WindowSurface* mSurface{ nullptr };
	};
}