#pragma once

#include "base.h"
#include "Vulkan/instance.h"
#include "Vulkan/device.h"
#include "Vulkan/Window.h"
#include "Vulkan/windowSurface.h"
#include "Vulkan/swapChain.h"
#include "Vulkan/shader.h"
#include "Vulkan/pipeline.h"
#include "Vulkan/renderPass.h"
#include "Vulkan/commandPool.h"
#include "VulKan/commandBuffer.h"
#include "VulKan/semaphore.h"
#include "VulKan/fence.h"
#include "Vulkan/buffer.h"
#include "texture/texture.h"

#include "ImGui/Interface.h"

//#include "Function/opcode.h"


namespace GAME {
	class Application{
	public:
		unsigned int mWidth{ 1 };
		unsigned int mHeight{ 1 };

		Application() = default;

		~Application() = default;

		//总初始化
		void run(VulKan::Window* w);

		char* buffer = nullptr;

	private:
		//窗口的初始化
		void initWindow();

		//初始化Vulkan
		void initVulkan();

		//初始化ImGui
		void initImGui();

		//主循环main
		void mainLoop();

		//渲染一帧画面
		void render();

		//回收资源
		void cleanUp();

	private:

		//设置GPU画布
		void createRenderPass();
		//录入渲染指令
		void createCommandBuffers(unsigned int i);
		//创建信号量（用于渲染同步），和创建Fence（用于不重复提交指令）
		void createSyncObjects();

	private:
		void KeyBoardEvents();
		bool mButton;

	private:
		int mCurrentFrame{ 0 };//当前是渲染哪一张GPU画布
		VulKan::Window* mWindow{ nullptr };//创建窗口
		VulKan::Instance* mInstance{ nullptr };//实列化需要的VulKan功能APi
		VulKan::Device* mDevice{ nullptr };//获取电脑的硬件设备
		VulKan::WindowSurface* mWindowSurface{ nullptr };//获取你在什么平台运行调用不同的API（比如：Window，Android）
		VulKan::SwapChain* mSwapChain{ nullptr };//设置VulKan的工作细节
		VulKan::RenderPass* mRenderPass{ nullptr };//设置GPU画布
		VulKan::CommandPool* mCommandPool{ nullptr };//设置渲染指令的工作
		std::vector<VulKan::CommandBuffer*> mCommandBuffers{};//主录入渲染指令




		std::vector<VulKan::Semaphore*> mImageAvailableSemaphores{};//图片显示完毕信号量
		std::vector<VulKan::Semaphore*> mRenderFinishedSemaphores{};//图片渲染完成信号量
		std::vector<VulKan::Fence*> mFences{};//控制管线工作，比如（下一个管线需要上一个管线的图片，那就等上一个管线图片输入进来才开始工作）

	public:
		ImGuiInterFace* InterFace = nullptr; // ImGui 界面都写这里面
	};
}