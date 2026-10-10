#include "Interface.h"
#include "Interface/InterfaceInternal.h"//跨拆分文件共享的内部辅助（字体/模型列表/模板替换）
#include "../AngelScript/AngelScriptCode.h"
#include "../Function/WebDav.h"
#include "../Tool/Log.h"//TOOL::LogStep：退出路径路标
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace GAME {
	//=== 字体加载 / AI 模型列表 =================================================
	//FontFileReadable / FirstFontInTTFFolder / DefaultTypefacePath / LoadTypeface 已拆分到 Interface/Font.cpp；
	//FindAiModelIndex / CurrentAiModelName 已拆分到 Interface/ModelList.cpp（声明见 Interface/InterfaceInternal.h）。

	ImGuiInterFace::ImGuiInterFace(
		VulKan::Device* device, 
		VulKan::Window* Win, 
		ImGui_ImplVulkan_InitInfo Info, 
		VulKan::RenderPass* Pass,
		VulKan::CommandBuffer* commandbuffer,
		int FormatCount
	)
	{
		mWindown = Win;
		mDevice = device;
		mFormatCount = FormatCount;
		

		// 安装 Dear ImGui 上下文
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		m_io = &io;

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		//io.ConfigViewportsNoAutoMerge = true;
		//io.ConfigViewportsNoTaskBarIcon = true;

		// 安装 Dear ImGui 风格
		ImGui::StyleColorsDark();


		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& Style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			Style.WindowRounding = 0.0f;
			Style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// 设置字体
		//内嵌字模（Font.h）已经删除，现在统一按 TTF 文件加载：
		//FontBool 打开 → 用设置里选中的字体文件；关闭 → 用程序目录 ./TTF 里的默认字体。
		ImFontConfig Font_cfg;
		Font_cfg.OversampleH = 1;
		//FontDataOwnedByAtlas 保持默认的 true：字体数据是 ImGui 自己从文件读进来的，交给它释放。
		//（以前必须设成 false，是因为那时候指向 Font.h 里的 static 数组，不能释放。）
		ImFont* Font = LoadTypeface(io, Variable::FontBool ? Variable::FontFilePath : std::string(), Variable::FontSize, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
		if (Variable::HitokotoFontBool) {
			//HitokotoTTFBool 打开表示一言窗口也用默认字模（原来的「内部字模」）
			HitokotoFont = LoadTypeface(io, Variable::HitokotoTTFBool ? std::string() : Variable::HitokotoFont, Variable::HitokotoFontSize, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
		}
		



		// 安装 Platform/渲染器 backends
		ImGui_ImplGlfw_InitForVulkan(mWindown->getWindow(), true);

		// Create Descriptor Pool
		{
			VkDescriptorPoolSize pool_sizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			};
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
			pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
			pool_info.pPoolSizes = pool_sizes;
			if (vkCreateDescriptorPool(mDevice->getDevice(), &pool_info, nullptr, &g_DescriptorPool)) {
				throw std::runtime_error("Error: initImGui DescriptorPool 生成失败");
			}
		}

		Info.DescriptorPool = g_DescriptorPool;
		Info.MinImageCount = g_MinImageCount;
		ImGuiVulkanInfo = Info;

		ImGui_ImplVulkan_Init(&Info, Pass->getRenderPass());

		

		// 上传 DearImgui 字体
		commandbuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);//开始录制要提交的指令
		ImGui_ImplVulkan_CreateFontsTexture(commandbuffer->getCommandBuffer());//要录制的内容
		commandbuffer->end();//结束录制要提交的指令
		commandbuffer->submitSync(mDevice->getGraphicQueue());//等待指令上传结束
		ImGui_ImplVulkan_DestroyFontUploadObjects();


		//ImGui 风格设置（界面重构：深色 + 绿色主色）
		auto Color = Style.Colors;

		//窗口本身保持直角：多视口下每个窗口都是独立平台窗口，圆角会露出底下的桌面
		Style.WindowRounding = 0.0f;
		Style.ChildRounding = 5.0f;
		Style.FrameRounding = 4.0f;
		Style.PopupRounding = 5.0f;
		Style.GrabRounding = 3.0f;
		Style.TabRounding = 4.0f;
		Style.ScrollbarRounding = 8.0f;
		Style.WindowBorderSize = 1.0f;
		Style.ChildBorderSize = 1.0f;
		Style.FrameBorderSize = 0.0f;
		Style.WindowPadding = ImVec2(12.0f, 10.0f);
		Style.FramePadding = ImVec2(8.0f, 5.0f);
		Style.ItemSpacing = ImVec2(8.0f, 7.0f);
		Style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
		Style.IndentSpacing = 18.0f;
		Style.ScrollbarSize = 12.0f;
		Style.GrabMinSize = 10.0f;

		//背景
		Color[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.098f, 0.117f, 1.0f);//#161A1E
		Color[ImGuiCol_ChildBg] = ImVec4(0.070f, 0.080f, 0.098f, 1.0f);//#121419
		Color[ImGuiCol_PopupBg] = ImVec4(0.105f, 0.117f, 0.141f, 0.98f);//#1B1E24
		Color[ImGuiCol_Border] = ImVec4(0.180f, 0.200f, 0.231f, 1.0f);//#2E333B
		Color[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		Color[ImGuiCol_Text] = ImVec4(0.902f, 0.921f, 0.941f, 1.0f);
		Color[ImGuiCol_TextDisabled] = ImVec4(0.470f, 0.505f, 0.545f, 1.0f);

		//输入框 / 下拉框
		Color[ImGuiCol_FrameBg] = ImVec4(0.129f, 0.145f, 0.172f, 1.0f);//#212529
		Color[ImGuiCol_FrameBgHovered] = ImVec4(0.164f, 0.184f, 0.215f, 1.0f);
		Color[ImGuiCol_FrameBgActive] = ImVec4(0.196f, 0.219f, 0.254f, 1.0f);

		//按钮：绿色主色
		Color[ImGuiCol_Button] = ImVec4(0.113f, 0.443f, 0.278f, 1.0f);//#1D7147
		Color[ImGuiCol_ButtonHovered] = ImVec4(0.156f, 0.545f, 0.345f, 1.0f);//#288B58
		Color[ImGuiCol_ButtonActive] = ImVec4(0.086f, 0.360f, 0.227f, 1.0f);//#165C3A

		Color[ImGuiCol_CheckMark] = ImVec4(0.282f, 0.780f, 0.486f, 1.0f);//#48C77C
		Color[ImGuiCol_SliderGrab] = ImVec4(0.282f, 0.780f, 0.486f, 1.0f);
		Color[ImGuiCol_SliderGrabActive] = ImVec4(0.345f, 0.850f, 0.549f, 1.0f);

		Color[ImGuiCol_Header] = ImVec4(0.113f, 0.443f, 0.278f, 0.85f);
		Color[ImGuiCol_HeaderHovered] = ImVec4(0.156f, 0.545f, 0.345f, 0.90f);
		Color[ImGuiCol_HeaderActive] = ImVec4(0.086f, 0.360f, 0.227f, 1.0f);

		//分隔线 / 缩放手柄
		Color[ImGuiCol_Separator] = ImVec4(0.180f, 0.200f, 0.231f, 1.0f);
		Color[ImGuiCol_SeparatorHovered] = ImVec4(0.282f, 0.780f, 0.486f, 0.60f);
		Color[ImGuiCol_SeparatorActive] = ImVec4(0.282f, 0.780f, 0.486f, 0.90f);
		Color[ImGuiCol_ResizeGrip] = ImVec4(0.282f, 0.780f, 0.486f, 0.25f);
		Color[ImGuiCol_ResizeGripHovered] = ImVec4(0.282f, 0.780f, 0.486f, 0.60f);
		Color[ImGuiCol_ResizeGripActive] = ImVec4(0.282f, 0.780f, 0.486f, 0.90f);

		//滚动条
		Color[ImGuiCol_ScrollbarBg] = ImVec4(0.055f, 0.063f, 0.078f, 1.0f);
		Color[ImGuiCol_ScrollbarGrab] = ImVec4(0.216f, 0.240f, 0.278f, 1.0f);
		Color[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.282f, 0.310f, 0.353f, 1.0f);
		Color[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.282f, 0.780f, 0.486f, 0.80f);

		//标题条 / 菜单条 / 键盘导航高亮
		Color[ImGuiCol_TitleBg] = ImVec4(0.086f, 0.098f, 0.117f, 1.0f);
		Color[ImGuiCol_TitleBgActive] = ImVec4(0.113f, 0.443f, 0.278f, 1.0f);
		Color[ImGuiCol_TitleBgCollapsed] = ImVec4(0.086f, 0.098f, 0.117f, 0.75f);
		Color[ImGuiCol_MenuBarBg] = ImVec4(0.105f, 0.117f, 0.141f, 1.0f);
		Color[ImGuiCol_NavHighlight] = ImVec4(0.282f, 0.780f, 0.486f, 0.80f);

		//表格（设置界面用两列表格排「标签 + 控件」）
		Color[ImGuiCol_TableHeaderBg] = ImVec4(0.129f, 0.145f, 0.172f, 1.0f);
		Color[ImGuiCol_TableBorderStrong] = ImVec4(0.180f, 0.200f, 0.231f, 1.0f);
		Color[ImGuiCol_TableBorderLight] = ImVec4(0.145f, 0.161f, 0.188f, 1.0f);
		Color[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		Color[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);

		Color[ImGuiCol_WindowBg].w = 1.0f;//多视口下窗口背景必须不透明

		ImGuiCommandPoolS = new VulKan::CommandPool* [FormatCount];
		ImGuiCommandBufferS = new VulKan::CommandBuffer* [FormatCount];
		for (int i = 0; i < FormatCount; i++)
		{
			ImGuiCommandPoolS[i] = new VulKan::CommandPool(mDevice);
			ImGuiCommandBufferS[i] = new VulKan::CommandBuffer(mDevice, ImGuiCommandPoolS[i], true);
		}


		mTesseract = new Tesseract(Variable::Model.c_str());

		mTranslate = new Translate();
		mTranslate->SetBaiduAppID(Variable::BaiduAppid.c_str());
		mTranslate->SetBaiduSecretkey(Variable::BaiduSecret_key.c_str());

		mTranslate->SetYoudaoAppID(Variable::YoudaoAppid.c_str());
		mTranslate->SetYoudaoSecretkey(Variable::YoudaoSecret_key.c_str());

		mTranslate->SetTranslate(Variable::Translate);
		mTranslate->SetFrom(Variable::From);
		mTranslate->SetTo(Variable::To);
	}

	ImGuiInterFace::~ImGuiInterFace()
	{
		TOOL::LogStep("~ImGuiInterFace: 开始");
		//销毁ImGui
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		TOOL::LogStep("~ImGuiInterFace: ImGui 已关闭");

		if (g_DescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(mDevice->getDevice(), g_DescriptorPool, nullptr);//销毁专门创建给ImGui用的DescriptorPool
		}
		TOOL::LogStep("~ImGuiInterFace: DescriptorPool 已销毁");

		for (int i = 0; i < mFormatCount; i++)
		{
			delete ImGuiCommandBufferS[i];
			delete ImGuiCommandPoolS[i];
		}
		delete ImGuiCommandBufferS;
		delete ImGuiCommandPoolS;
		TOOL::LogStep("~ImGuiInterFace: ImGui 指令池/缓存已删除");

		delete mTesseract;
		TOOL::LogStep("~ImGuiInterFace: Tesseract 已删除");
		delete mTranslate;
		TOOL::LogStep("~ImGuiInterFace: Translate 已删除");
	}

	bool ImGuiInterFace::InterFace()
	{
		bool kai = DoYouWantToUpdateTheScreen(GetInterFaceEnumTime());//下一帧是否要更新
		if (!GetUpdateTheScreen() && !kai && (InterfaceIndexes != ScreenshotEnum)) {
			return false;
		}
		
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		//ImGui::ShowDemoWindow();
		switch (InterfaceIndexes)
		{
		case TranslateEnum:
			TranslateInterface();
			break;
		case ScreenshotEnum:
			ScreenshotInterface();
			break;
		case SetUpEnum:
			SetUpInterface();
			break;
		case MenuEnum:
			MenuInterface();
			break;
		case HitokotoEnum:
			HitokotoSentence();
			break;
		}
		ImGui::Render();

		if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			//如果开启 Viewports 模式那么每次 ImGui::Render() 或 ImGui::EndFrame() 后都有调用下面两个函数
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		return true;
	}

	const VkCommandBuffer ImGuiInterFace::GetCommandBuffer(int i, VkCommandBufferInheritanceInfo info) {
		ImGuiCommandBufferS[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, info);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ImGuiCommandBufferS[i]->getCommandBuffer());
		ImGuiCommandBufferS[i]->end();
		return ImGuiCommandBufferS[i]->getCommandBuffer();
	}
	

	// TranslateInterface / InputTextMultilineText / RequestTranslate / UpdateTranslateTask / AiTargetLangCode
	// 以及文件内的 MyText 回调已拆分到 Interface/TranslatePanel.cpp。
	// ScreenshotInterface() 已拆分到 Interface/ScreenshotPanel.cpp。
	// SetUpInterface() 及设置界面的各 Draw*Section / 定长输入框辅助已拆分到 Interface/SettingsPanel.cpp。


	// MenuInterface / HitokotoSentence / findMemoryType / LoadTextureFromFile / RemoveTexture
	// 已分别拆分到 Interface/MenuPanel.cpp、Interface/HitokotoPanel.cpp、Interface/Texture.cpp。

	}
