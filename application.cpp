#include "application.h"
#include "AngelScript/AngelScriptCode.h"





namespace GAME {

	//ImGui 错误信息回调函数
	static void check_vk_result(VkResult err)
	{
		if (err == 0)
			return;
		fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

	//总初始化
	void Application::run(VulKan::Window* w) {
		mWindow = w;
		//initWindow();//初始化窗口
		initVulkan();//初始化Vulkan
		initImGui();//初始化ImGui
		mainLoop();//开启主循环main
		cleanUp();//回收资源
	}



	//窗口的初始化
	void Application::initWindow() {
		//mWindow = new VulKan::Window(mWidth, mHeight,0,0);
		//mWindow->setApp(shared_from_this());//把 application 本体指针传给 Window ，便于调用 Camera
	}

	//初始化Vulkan
	//1 rendePass 加入pipeline 2 生成FrameBuffer
	void Application::initVulkan() {
		mInstance = new VulKan::Instance(false);//实列化需要的VulKan功能APi （还是不知道哪里错了，开始检测就一直报错，所以只可以关掉了。修不好 （；´д｀）ゞ  ）
		mWindowSurface = new VulKan::WindowSurface(mInstance, mWindow);//获取你在什么平台运行调用不同的API（比如：Window，Android）
		mDevice = new VulKan::Device(mInstance, mWindowSurface);//获取电脑的硬件设备
		mCommandPool = new VulKan::CommandPool(mDevice);//创建指令池，给CommandBuffer用的
		mSwapChain = new VulKan::SwapChain(mDevice, mWindow, mWindowSurface, mCommandPool);//设置VulKan的工作细节
		mWidth = mSwapChain->getExtent().width;
		mHeight = mSwapChain->getExtent().height;
		mRenderPass = new VulKan::RenderPass(mDevice);
		createRenderPass();
		mSwapChain->createFrameBuffers(mRenderPass);

		

		//主指令缓存录制
		mCommandBuffers.resize(mSwapChain->getImageCount());
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			mCommandBuffers[i] = new VulKan::CommandBuffer(mDevice, mCommandPool);//创建主指令缓存
		}
		createSyncObjects();
	}
	
	void GAME::Application::initImGui()
	{
		//准备填写需要的信息
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = mInstance->getInstance();
		init_info.PhysicalDevice = mDevice->getPhysicalDevice();
		init_info.Device = mDevice->getDevice();
		init_info.QueueFamily = mDevice->getGraphicQueueFamily().value();
		init_info.Queue = mDevice->getGraphicQueue();
		init_info.PipelineCache = nullptr;
		//init_info.DescriptorPool = g_DescriptorPool;
		init_info.Subpass = 0;
		//init_info.MinImageCount = g_MinImageCount;
		init_info.ImageCount = mSwapChain->getImageCount();
		init_info.MSAASamples = mDevice->getMaxUsableSampleCount();
		init_info.Allocator = nullptr;//内存分配器
		init_info.CheckVkResultFn = check_vk_result;//错误处理


		InterFace = new ImGuiInterFace(mDevice, mWindow, init_info, mRenderPass, mCommandBuffers[0], mSwapChain->getImageCount());

		AngelScriptTranslate = InterFace->mTranslate;
	}


	void Application::createRenderPass() {
		//0：最终输出图片 1：Resolve图片（MutiSample） 2：Depth图片

		//0号位：是SwapChain原来那张图片，是Resolve的目标点，即需要设置到SubPass的Resolve当中
		VkAttachmentDescription finalAttachmentDes{};
		finalAttachmentDes.format = mSwapChain->getFormat();//获取图片格式
		finalAttachmentDes.samples = VK_SAMPLE_COUNT_1_BIT;//采样
		finalAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//原来上一张的图片，应该怎么处理（这里是不管直接清空）
		finalAttachmentDes.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//输出图片怎么处理（这里是保存）
		finalAttachmentDes.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;//我们没有用到他，所以不关心
		finalAttachmentDes.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;//我们没有用到他，所以不关心
		finalAttachmentDes.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//
		finalAttachmentDes.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//保存的格式，是一直适合显示的格式

		mRenderPass->addAttachment(finalAttachmentDes);

		

		//对于画布的索引设置以及格式要求
		VkAttachmentReference finalAttachmentRef{};
		finalAttachmentRef.attachment = 0;//画布的索引
		finalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//我们希望的格式

		//创建子流程
		VulKan::SubPass subPass{};
		subPass.addColorAttachmentReference(finalAttachmentRef);

		subPass.buildSubPassDescription();

		mRenderPass->addSubPass(subPass);

		//子流程之间的依赖关系
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;//vulkan 给的开始的虚拟流程的
		dependency.dstSubpass = 0;//代表的是 mRenderPass 数组的第 0 个 SubPass
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//我上一个流程，进行到那个一步可以进行下一个子流程
		dependency.srcAccessMask = 0;//上一个流程到那一步算结束（0 代表我不关心，下一个流程直接开始）
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//下一个流程要在哪里等待上一个流程结束
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;//我到了 图片读写操作是要等待上一个子流程结束

		mRenderPass->addDependency(dependency);

		mRenderPass->buildRenderPass();
	}
	
	void Application::createCommandBuffers(unsigned int i)
	{
		//ThreadCommandBufferS.clear();//清空显示队列
		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = mRenderPass->getRenderPass();
		InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(i);
		

		VkRenderPassBeginInfo renderBeginInfo{};
		renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderBeginInfo.renderPass = mRenderPass->getRenderPass();//获得画布信息
		renderBeginInfo.framebuffer = mSwapChain->getFrameBuffer(i);//设置是那个GPU画布
		renderBeginInfo.renderArea.offset = { 0, 0 };//画布从哪里开始画
		renderBeginInfo.renderArea.extent = mSwapChain->getExtent();//画到哪里结束

		//0：final   1：muti   2：depth
		std::vector< VkClearValue> clearColors{};//在使用画布前，用什么颜色清理他，
		VkClearValue finalClearColor{};
		finalClearColor.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		clearColors.push_back(finalClearColor);


		renderBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());//有多少个
		renderBeginInfo.pClearValues = clearColors.data();//用来清理的颜色数据

		mCommandBuffers[i]->begin();//开始录制主指令

		mCommandBuffers[i]->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);//关于画布信息  !!!这个只有主指令才有


		VkCommandBuffer BF = InterFace->GetCommandBuffer(i, InheritanceInfo);
		vkCmdExecuteCommands(mCommandBuffers[i]->getCommandBuffer(), 1, &BF);

		

		mCommandBuffers[i]->endRenderPass();//结束RenderPass

		mCommandBuffers[i]->end();//结束
	}

	void Application::createSyncObjects() {
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			VulKan::Semaphore* imageSemaphore = new VulKan::Semaphore(mDevice);
			mImageAvailableSemaphores.push_back(imageSemaphore);

			VulKan::Semaphore* renderSemaphore = new VulKan::Semaphore(mDevice);
			mRenderFinishedSemaphores.push_back(renderSemaphore);

			VulKan::Fence* fence = new VulKan::Fence(mDevice);
			mFences.push_back(fence);
		}
	}


	//主循环main
	void Application::mainLoop() {
		while (!mWindow->shouldClose()) {//窗口被关闭结束循环

			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			mWindow->pollEvents();
			KeyBoardEvents();//监听键盘

			ImGuiIO& io = ImGui::GetIO();
			double CursorPosX, CursorPosY;
			glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
			io.MousePos.x = CursorPosX;
			io.MousePos.y = CursorPosY;
			
			if (InterFace->EndDisplayBool) {
				InterFace->EndDisplayBool = false;
				InterFace->SetInterFaceBool(false);
				InterFace->SetInterFace(No_Enum);
				render();
			}

			if (InterFace->GetInterFaceBool()) {
				if ((InterFace->GetInterFaceEnum() == TranslateEnum) && !InterFace->DoYouWantToUpdateTheScreen()) {
					continue;
				}
				//ImGui显示录制
				if (InterFace->InterFace() || InterFace->GetUpdateTheScreen()) {
					render();//根据录制的主指令缓存显示画面
				}
				else {
					if (InterFace->m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
					{
						ImGui::UpdatePlatformWindows();
						ImGui::RenderPlatformWindowsDefault();
					}
				}
			}
		}

		//等待设备所以命令执行完毕才可以开始销毁，
		vkDeviceWaitIdle(mDevice->getDevice());//等待命令执行完毕
	}

	void Application::KeyBoardEvents() {
		if ((GetKeyState(Variable::MakeUp) < 0)) {
			if ((GetKeyState(Variable::Choicekey[0]) < 0)) {
				while ((GetKeyState(Variable::MakeUp) < 0) || (GetKeyState(Variable::Choicekey[0]) < 0)) {
					mWindow->pollEvents();
					Sleep(20);
				}
				std::string strS = TOOL::ClipboardTochar();//读取当前剪切板的内容，保存起来

				//获取当前选择的内容 ctrl + c
				TOOL::CtrlAndC();
				Sleep(5);//等待上面的内容复制到剪切板上
				
				Variable::eng = TOOL::UnicodeToUtf8(TOOL::ClipboardTochar());//获取选中的文本
				if (AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->GetOpenBool() && Variable::ScriptBool) {
					//执行脚本
					AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->RunFunction(
						AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->ChoiceFunction
					);
				}
				else
				{
					Variable::zhong = InterFace->mTranslate->TranslateAPI(Variable::eng);//翻译内容
				}
				
				memset(InterFace->eng, 0, sizeof(InterFace->eng));
				memset(InterFace->zhong, 0, sizeof(InterFace->zhong));
				memcpy(InterFace->eng, Variable::eng.c_str(), Variable::eng.size());
				memcpy(InterFace->zhong, Variable::zhong.c_str(), Variable::zhong.size());
				InterFace->SetInterFace(TranslateEnum);//设置显示类

				TOOL::CopyToClipboard(strS);//还原原来剪切板的内容
				return;
			}

			if ((GetKeyState(Variable::Replacekey[0]) < 0)) {
				while ((GetKeyState(Variable::MakeUp) < 0) || (GetKeyState(Variable::Replacekey[0]) < 0)) {
					mWindow->pollEvents();
					Sleep(20);
				}
				std::string strS = TOOL::ClipboardTochar();//读取当前剪切板的内容，保存起来

				//获取当前选择的内容 ctrl + c
				TOOL::CtrlAndC();
				Sleep(5);//等待上面的内容复制到剪切板上

				Variable::eng = TOOL::UnicodeToUtf8(TOOL::ClipboardTochar());//获取选中的文本


				int LTo = InterFace->mTranslate->mTo;
				InterFace->mTranslate->mTo = Variable::ReplaceLanguage;
				if (AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->GetOpenBool() && Variable::ScriptBool) {
					//执行脚本
					AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->RunFunction(
						AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->ReplaceFunction
					);
				}
				else
				{
					Variable::zhong = InterFace->mTranslate->TranslateAPI(Variable::eng);//翻译内容
				}
				InterFace->mTranslate->mTo = LTo;


				TOOL::CopyToClipboard(TOOL::Utf8ToUnicode(Variable::zhong.c_str()));
				//粘贴出去 ctrl + v
				TOOL::CtrlAndV();
				Sleep(5);
				TOOL::CopyToClipboard(strS);//还原原来剪切板的内容
				return;
			}

			if ((GetKeyState(Variable::Screenshotkey[0]) < 0) && ((GetKeyState(Variable::Screenshotkey[0]) < 0) != mButton)) {
				buffer = TOOL::screen(buffer);//获取截图数据
				InterFace->LoadTextureFromFile(buffer, &InterFace->mTextureData);//生成图片ID
				InterFace->SetInterFace(ScreenshotEnum);//设置显示类
			}
			mButton = (GetKeyState(Variable::Screenshotkey[0]) < 0);
		}
	}

	void Application::render() {
		//等待当前要提交的CommandBuffer执行完毕
		mFences[mCurrentFrame]->block();

		

		//获取交换链当中的下一帧
		uint32_t imageIndex{ 0 };
		VkResult result = vkAcquireNextImageKHR(
			mDevice->getDevice(),
			mSwapChain->getSwapChain(),
			UINT64_MAX,
			mImageAvailableSemaphores[mCurrentFrame]->getSemaphore(),
			VK_NULL_HANDLE,
			&imageIndex);


		//重新录制指令
		createCommandBuffers(imageIndex);
		

		//构建提交信息
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		

		//同步信息，渲染对于显示图像的依赖，显示完毕后，才能输出颜色
		VkSemaphore waitSemaphores[] = { mImageAvailableSemaphores[mCurrentFrame]->getSemaphore() };//获取储存信号量类
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };//管线颜色输出阶段，等待信号量

		submitInfo.waitSemaphoreCount = 1;//等待多少个信号量
		submitInfo.pWaitSemaphores = waitSemaphores;//储存到那个信号量
		submitInfo.pWaitDstStageMask = waitStages;//那个阶段等待信号量


		//指定提交哪些命令
		VkCommandBuffer commandBuffers[1] = { mCommandBuffers[imageIndex]->getCommandBuffer()};
		submitInfo.commandBufferCount = (uint32_t)1;
		submitInfo.pCommandBuffers = commandBuffers;

		VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphores[mCurrentFrame]->getSemaphore()};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		mFences[mCurrentFrame]->resetFence();
		if (vkQueueSubmit(mDevice->getGraphicQueue(), 1, &submitInfo, mFences[mCurrentFrame]->getFence()) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to submit renderCommand");
		}


		if (InterFace->m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = {mSwapChain->getSwapChain()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
						
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);//开始渲染


		mCurrentFrame = (mCurrentFrame + 1) % mSwapChain->getImageCount();
	}


	//回收资源
	void Application::cleanUp() {

		delete InterFace;
		
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			delete mCommandBuffers[i];//必须先销毁 CommandBuffer ，才可以销毁绑定的 CommandPool。
			delete mImageAvailableSemaphores[i];
			delete mRenderFinishedSemaphores[i];
			delete mFences[i];
		}
		delete mRenderPass;
		delete mSwapChain;
		delete mCommandPool;
		delete mDevice;
		delete mWindowSurface;
		delete mInstance;
		delete mWindow;
	}
}