#include "Interface.h"
#include "../AngelScript/AngelScriptCode.h"
#include "../Function/WebDav.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace GAME {
	//=== 字体加载 ==============================================================
	//Font.h（内嵌的 Test.ttf 字模）已经删除，字体一律从 TTF 文件里读：
	//  1. 设置里选中的字体文件（Variable::FontFilePath / Variable::HitokotoFont）
	//  2. 程序目录 ./TTF 下的默认字体（约定 SmileySans-Oblique.ttf，没有就取目录里第一个 ttf）
	//  3. ImGui 自带字模（只有 ASCII，纯兜底，保证界面不会一个字体都没有）
	//注意：ImGui 的 AddFontFromFileTTF 读不到文件时会直接 IM_ASSERT 失败（Debug 下一运行就弹框），
	//所以这里必须先自己确认文件在不在，不能把路径直接丢给 ImGui。
	static const char* const DefaultFontFileName = "./TTF/SmileySans-Oblique.ttf";

	static bool FontFileReadable(const std::string& FilePath) {
		if (FilePath.empty()) {
			return false;
		}
		std::error_code ec;
		return std::filesystem::is_regular_file(FilePath, ec) && !ec;
	}

	//./TTF 目录里按文件名排序的第一个 ttf（用户换字体文件后不用改代码）
	static std::string FirstFontInTTFFolder() {
		std::error_code ec;
		std::filesystem::directory_iterator Iterator("./TTF", ec);
		if (ec) {
			return std::string();
		}
		std::vector<std::string> Files;
		for (const auto& Entry : Iterator) {
			std::error_code EntryEc;
			if (!Entry.is_regular_file(EntryEc)) {
				continue;
			}
			std::string Extension = Entry.path().extension().string();
			for (size_t i = 0; i < Extension.size(); i++) {
				Extension[i] = (char)std::tolower((unsigned char)Extension[i]);
			}
			if (Extension == ".ttf") {
				Files.push_back(Entry.path().string());
			}
		}
		if (Files.empty()) {
			return std::string();
		}
		std::sort(Files.begin(), Files.end());
		return Files.front();
	}

	//当前实际会使用的默认字模路径（设置界面里显示给用户看）
	static std::string DefaultTypefacePath() {
		if (FontFileReadable(DefaultFontFileName)) {
			return DefaultFontFileName;
		}
		return FirstFontInTTFFolder();
	}

	//加载字体：文件不存在或读不出来就自动退回默认字模，永远返回一个可用字体（ImGui::PushFont 不接受空指针）
	static ImFont* LoadTypeface(ImGuiIO& io, const std::string& WantedPath, float Size, const ImFontConfig* FontCfg, const ImWchar* Ranges) {
		if (FontFileReadable(WantedPath)) {
			if (ImFont* Font = io.Fonts->AddFontFromFileTTF(WantedPath.c_str(), Size, FontCfg, Ranges)) {
				return Font;
			}
		}
		else if (!WantedPath.empty()) {
			TOOL::logger->warn("Typeface file not usable, fallback to default typeface: " + WantedPath);
		}
		std::string DefaultPath = DefaultTypefacePath();
		if (FontFileReadable(DefaultPath)) {
			if (ImFont* Font = io.Fonts->AddFontFromFileTTF(DefaultPath.c_str(), Size, FontCfg, Ranges)) {
				return Font;
			}
		}
		TOOL::logger->warn("No usable TTF typeface found, using ImGui built-in typeface");
		return io.Fonts->AddFontDefault();
	}

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
		//销毁ImGui
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		if (g_DescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(mDevice->getDevice(), g_DescriptorPool, nullptr);//销毁专门创建给ImGui用的DescriptorPool
		}

		for (int i = 0; i < mFormatCount; i++)
		{
			delete ImGuiCommandBufferS[i];
			delete ImGuiCommandPoolS[i];
		}
		delete ImGuiCommandBufferS;
		delete ImGuiCommandPoolS;

		delete mTesseract;
		delete mTranslate;
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
	
	// InputTextMultiline 的 回调函数
	int mCursorPos, mTextLen;//返回输入框的光标位置，文本长度
	bool TranslateInputBool;//让 Ctrl + c 只作用于被翻译的文本框
	bool InputCursorBool;
	int MyText(ImGuiInputTextCallbackData* data) {
		if (!ImGui::IsItemDeactivated())
		{
			if (InputCursorBool) {
				data->CursorPos = mCursorPos;
				mCursorPos = 0;
				InputCursorBool = false;
			}
		}
		if (data->HasSelection() && ((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('C') < 0)))//判断是否有选中的文本
		{
			//选中的文本可能非常长（eng/zhong 是 1MB 的缓冲区），必须按目标栈数组的长度截断。
			//旧代码把终止符写在 [长度+1]（越界一字节），而长度本身没有任何上限。
			char selected_text[10000];
			const int SelectionBegin = (data->SelectionEnd > data->SelectionStart) ? data->SelectionStart : data->SelectionEnd;
			const int SelectionEndPos = (data->SelectionEnd > data->SelectionStart) ? data->SelectionEnd : data->SelectionStart;
			int SelectionLen = SelectionEndPos - SelectionBegin;//判断选中的文本是从左往右选还是从右往左选
			if (SelectionLen > (int)sizeof(selected_text) - 1) { SelectionLen = (int)sizeof(selected_text) - 1; }
			if (SelectionLen < 0) { SelectionLen = 0; }
			memcpy(selected_text, &data->Buf[SelectionBegin], SelectionLen);//复制选中的文本
			selected_text[SelectionLen] = '\0';//加上终止符
			TOOL::CopyToClipboard(TOOL::Utf8ToUnicode(selected_text));//复制到剪贴板
		}
		else if(!ImGui::IsItemDeactivated() && (GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)){//判断是否有选中的文本
			if (TranslateInputBool) {
				mCursorPos = data->CursorPos;
				mTextLen = data->BufTextLen;
				InputCursorBool = true;
			}
			
		}
		return 0;
	}

	void ImGuiInterFace::InputTextMultilineText() {
		if ((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)) {
			while ((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0))
			{
				mWindown->pollEvents();
			}
			std::string ClipboardText = TOOL::UnicodeToUtf8(TOOL::ClipboardTochar());
			//旧写法是在栈上的定长数组里手工拼三段：选中尾部（Len）和剪贴板内容都没有长度上限，
			//既能写爆 selected_text[10000]，也能写爆 eng 这块 1MB 的堆缓冲区。
			//改成先在 std::string 上拼接，再按容量截断拷回去。
			const std::string TextCopy(eng, strnlen(eng, sizeof(eng)));//eng 是定长缓冲区，按实际长度取
			const int TextLen = (int)TextCopy.size();
			int PastePos = (mCursorPos < TextLen) ? mCursorPos : TextLen;
			if (PastePos < 0) { PastePos = 0; }
			const std::string ResultText = TextCopy.substr(0, PastePos) + ClipboardText + TextCopy.substr(PastePos);
			ImGui::ClearActiveID();//失去焦点，粘贴的内容才会被保存
			TOOL::CopyToBuffer(eng, sizeof(eng), ResultText);
			mCursorPos = PastePos + (int)ClipboardText.size();
			if (mCursorPos > (int)sizeof(eng) - 1) { mCursorPos = (int)sizeof(eng) - 1; }
			//Variable::eng = eng;
			
			//memcpy(eng, Variable::eng.c_str(), Variable::eng.size());
		}
	}

	
	

	//语言文件里存的是带占位符的模板（%d 显示秒数、%s 显示文本），这里做替换，
	//这样代码里不用出现非 ASCII 字面量。
	static std::string AiTextWithNumber(const std::string& Tpl, int Value)
	{
		const std::string Num = std::to_string(Value);
		std::string Result;
		Result.reserve(Tpl.size() + Num.size());
		for (size_t i = 0; i < Tpl.size(); i++)
		{
			if (Tpl[i] == '%' && (i + 1) < Tpl.size() && Tpl[i + 1] == 'd')
			{
				Result += Num;
				i++;
			}
			else
			{
				Result += Tpl[i];
			}
		}
		return Result;
	}

	static std::string AiTextWithString(const std::string& Tpl, const std::string& Value)
	{
		std::string Result;
		Result.reserve(Tpl.size() + Value.size());
		for (size_t i = 0; i < Tpl.size(); i++)
		{
			if (Tpl[i] == '%' && (i + 1) < Tpl.size() && Tpl[i + 1] == 's')
			{
				Result += Value;
				i++;
			}
			else
			{
				Result += Tpl[i];
			}
		}
		return Result;
	}

	//当前要翻译成哪种语言（Data.ini 里 Baidu_items 那套代码：zh、cht、jp…）
	std::string ImGuiInterFace::AiTargetLangCode() const
	{
		if (mTranslate == nullptr) { return std::string(); }
		const int Target = mTranslate->mTo;
		if (Target >= 0 && Target < (int)Variable::Baiduitems.size()) { return Variable::Baiduitems[Target]; }
		return std::string();
	}

	//统一的翻译入口：显示模式下都交给后台线程（普通源走 HTTP 线程，AI 走模型线程），结果由 UpdateTranslateTask() 落实；
	bool ImGuiInterFace::RequestTranslate(const std::string& English, bool ReplaceClipboard, const std::string& ClipboardBackup)
	{
		if (mTranslate == nullptr) { return true; }

		if (!mTranslate->IsAiTranslate())
		{
			//百度/爬虫/有道：走后台线程（和 AI 那条路一样）。
			//「替换」模式（Ctrl+Alt+R）不弹窗口，还是同步做完，粘贴/还原剪贴板由调用方做（和以前一样）。
			if (ReplaceClipboard)
			{
				Variable::zhong = mTranslate->TranslateAPI(English);
				return true;
			}

			//显示模式：窗口必须「一按就出来」，不能等请求回来。所以这里先把窗口和「翻译中…」摆上，
			//请求直接交给后台线程（WebBeginTranslation），结果由 UpdateTranslateTask() 每帧取。
			Variable::eng = English;
			Variable::zhong = Language::Translating;
			memset(eng, 0, sizeof(eng));
			memset(zhong, 0, sizeof(zhong));
			TOOL::CopyToBuffer(eng, sizeof(eng), Variable::eng);
			TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);
			if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum))
			{
				TranslateTime = clock();
			}
			else
			{
				SetInterFace(TranslateEnum);
			}

			//引擎和语言在这一刻定死再交给线程（Ctrl+Alt+R 会临时改 mTo，调用方随后就还原了）
			if (mTranslate->WebBeginTranslation(English, mTranslate->mTranslate, mTranslate->mFrom, mTranslate->mTo))
			{
				WebTaskPending = true;
				WebTaskSourceText = English;
			}
			return false;
		}

		if (AiTaskRunning) { return false; }//上一个 AI 翻译还没结束，这次不等它

		AiTaskRunning = true;
		AiTaskReplace = ReplaceClipboard;
		AiTaskSourceText = English;
		AiTaskClipboardBackup = ClipboardBackup;
		AiTaskStartTime = clock();
		Variable::eng = English;

		if (AiTaskReplace)
		{
			//替换模式不弹窗口，算完直接粘贴（在 UpdateTranslateTask 里做）
			mTranslate->AiBeginTranslation(English, AiTargetLangCode());
			return false;
		}

		//显示模式：先把「正在加载/翻译中」写进去，窗口立刻显示进度，主循环里每帧刷新
		Variable::zhong = AiTextWithNumber(Language::AILoading, 0);
		memset(eng, 0, sizeof(eng));
		memset(zhong, 0, sizeof(zhong));
		TOOL::CopyToBuffer(eng, sizeof(eng), Variable::eng);
		TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);
		//窗口已经开着就只把滞留计时续上；再调 SetInterFace() 会把窗口重新挪到鼠标位置（只有 AI 源走这条异步路径，所以只有它会跳）
		if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum))
		{
			TranslateTime = clock();
		}
		else
		{
			SetInterFace(TranslateEnum);
		}

		mTranslate->AiBeginTranslation(English, AiTargetLangCode());
		return false;
	}

	//主循环每帧一次：刷新进度文案 + 取回结果
	void ImGuiInterFace::UpdateTranslateTask()
	{
		//模型闲置卸载要每帧检查，所以放在最前面（下面有空任务时的提前 return）
		if (mTranslate != nullptr) { mTranslate->AiPollIdle(); }

		//脚本已经交给后台线程在跑：主循环照常转，所以「翻译中」这段时间窗口能拖、能关。
		//跑完了再把脚本写好的原文/译文同步到界面。
		if (ScriptRunning)
		{
			//跑完之前把滞留计时续上，别让窗口被 DoYouWantToUpdateTheScreen() 提前关掉
			if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum)) { TranslateTime = clock(); }
			if (AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->IsRunning()) { return; }//还没跑完

			//跑完了：脚本里的 SetInput()/SetOutput() 已经把原文/译文写好，同步到界面并开始计时
			ScriptRunning = false;
			memset(eng, 0, sizeof(eng));
			memset(zhong, 0, sizeof(zhong));
			TOOL::CopyToBuffer(eng, sizeof(eng), Variable::eng);
			TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);
			if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum)) { TranslateTime = clock(); }
			return;
		}

		//脚本模式：识别出的原文已经在上一帧画到界面上了，现在才把脚本交给后台线程跑。
		//（脚本里的 TranslateAPI() 是同步接口，跑在后台线程里，不会再卡住主循环。）
		if (ScriptPending)
		{
			ScriptPending = false;
			ScriptRunning = true;
			AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->BeginRun(
				AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->ScreenshotFunction
			);
			return;
		}

		//截图翻译：OCR 还在后台跑就继续显示「识别中…」；文本出来后再走一次正常翻译流程。
		if (OcrTaskPending)
		{
			//识别期间把滞留计时续上，别让「识别中…」被 DoYouWantToUpdateTheScreen() 提前关掉
			if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum)) { TranslateTime = clock(); }

			std::string Text;
			const bool OcrDone = (mTesseract != nullptr) ? mTesseract->OcrTakeResult(Text) : true;
			if (!OcrDone) { return; }//还没识别完，下一帧再来

			OcrTaskPending = false;
			if (Text.empty()) { TOOL::logger->warn("screenshot ocr: empty text"); }

			//识别出来的原文先填进原文框（脚本模式要用它）：界面此时已经显示着「识别中…」，
			//所以下面无论跑脚本还是起翻译线程，都不会再挡住窗口出现
			Variable::eng = Text;
			memset(eng, 0, sizeof(eng));
			TOOL::CopyToBuffer(eng, sizeof(eng), Variable::eng);

			if (ScriptAfterOcr)
			{
				//脚本模式：原文先显示出来（译文框留「翻译中…」），脚本等**下一帧**再跑。
				//脚本里的 TranslateAPI() 是同步接口，跑起来主循环会卡住，不能让原文跟着一起等结果。
				ScriptAfterOcr = false;
				ScriptPending = true;
				Variable::zhong = Language::Translating;
				memset(zhong, 0, sizeof(zhong));
				TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);
				if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum)) { TranslateTime = clock(); }
				return;
			}

			//识别出来的原文交给统一入口：它会填原文框、把译文框置成「翻译中…」并起后台线程
			RequestTranslate(Text);
			return;
		}

		//普通翻译源（百度/爬虫/有道）：后台线程在跑就一直显示「翻译中…」，结果回来再更新界面。
		if (mTranslate != nullptr && WebTaskPending)
		{
			//请求期间把滞留计时续上：正式计时从结果写进界面那一刻才重新开始，
			//不然请求慢的时候「翻译中…」会被 DoYouWantToUpdateTheScreen() 提前关掉。
			if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum)) { TranslateTime = clock(); }

			std::string Result;
			if (!mTranslate->WebTakeResult(Result)) { return; }//还没算完，下一帧再来

			const std::string Source = WebTaskSourceText;
			WebTaskSourceText.clear();
			WebTaskPending = false;

			if (Result.empty()) { TOOL::logger->warn("web translate failed: empty result"); }

			Variable::eng = Source;
			Variable::zhong = Result;
			memset(eng, 0, sizeof(eng));
			memset(zhong, 0, sizeof(zhong));
			TOOL::CopyToBuffer(eng, sizeof(eng), Variable::eng);
			TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);
			//译文已经写进界面了：显示时长（Variable::DisplayTime）从这一刻才开始算。
			if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum))
			{
				TranslateTime = clock();
			}
			else
			{
				SetInterFace(TranslateEnum);
			}
			return;
		}

		if (!AiTaskRunning || mTranslate == nullptr) { return; }

		const int ElapsedSeconds = (int)((clock() - AiTaskStartTime) / CLOCKS_PER_SEC);

		if (!AiTaskReplace)
		{
			if (mTranslate->AiStage() == Translate::AiStageLoading)
			{
				Variable::zhong = AiTextWithNumber(Language::AILoading, ElapsedSeconds);
			}
			else
			{
				Variable::zhong = AiTextWithNumber(Language::AITranslating, ElapsedSeconds);
			}
			memset(zhong, 0, sizeof(zhong));
			TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);

			//本地模型要算好几秒，而窗口滞留时间默认只有 5 秒（Variable::DisplayTime），
			//不把计时器续上的话，结果还没出来窗口就被 DoYouWantToUpdateTheScreen() 关掉了。
			TranslateTime = clock();
		}

		std::string Result;
		if (!mTranslate->AiTakeResult(Result)) { return; }
		AiTaskRunning = false;

		const std::string Error = mTranslate->AiLastError();
		if (Result.empty())
		{
			if (Error.empty()) { TOOL::logger->error("AI translate failed: empty result"); }
			else { TOOL::logger->error("AI translate failed: {}", Error); }

			if (AiTaskReplace)
			{
				//替换模式失败时不要把错误说明贴进用户的文档里
				return;
			}
			Variable::zhong = Error.empty() ? Language::AIFailedEmpty : AiTextWithString(Language::AIFailed, Error);
		}
		else
		{
			Variable::zhong = Result;
		}

		if (AiTaskReplace)
		{
			TOOL::CopyToClipboard(TOOL::Utf8ToUnicode(Variable::zhong.c_str()));
			TOOL::CtrlAndV();//粘贴出去 ctrl + v
			Sleep(5);
			TOOL::CopyToClipboard(AiTaskClipboardBackup);//还原原来剪切板的内容
			return;
		}

		Variable::eng = AiTaskSourceText;
		memset(eng, 0, sizeof(eng));
		memset(zhong, 0, sizeof(zhong));
		TOOL::CopyToBuffer(eng, sizeof(eng), Variable::eng);
		TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);

		if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum))
		{
			//窗口已经开着：只续时间，别再 SetInterFace()（那会把窗口重新挪到鼠标位置）
			TranslateTime = clock();
		}
		else
		{
			SetInterFace(TranslateEnum);
		}
	}

	void ImGuiInterFace::TranslateInterface()
	{
		//自适应尺寸：宽度可以拖（记在 TranslateWinWidth，默认 380），高度每帧按内容算出来
		static float TranslateWinWidth = 380.0f;//窗口宽度（拖动后被记住）
		static float TranslateWinHeight = 320.0f;//窗口高度（每帧按内容重算，不用手拖）
		static float TrChromeLast = 0.0f;//上一帧实测的「非文本框部分」高度（顶上那几行 + 内外边距），用来算窗口还放不放得下
		if (TranslateBool) {
			TranslateBool = false;
			POINT MousePos = { 0,0 };
			GetCursorPos(&MousePos);//获取鼠标位置
			ImGui::SetNextWindowPos({ float(MousePos.x), float(MousePos.y) });//设置窗口生成位置
			Variable::WrapSize = kuangshu / int(std::max(1.0f, Variable::FontSize));
		}
		ImGui::SetNextWindowSize(ImVec2(TranslateWinWidth, TranslateWinHeight));
		ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 160.0f), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin(u8"TranslateUI", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);//创建窗口

		//下拉框展开时鼠标落在弹出窗口上，主窗口的命中测试会失败——用 KeepAliveBool 把滞留计时续上
		KeepAliveBool = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) || ImGui::IsWindowHovered();

		//Data.ini 里的语言列表可能被改短，下标兜个底
		const int LangCount = (int)Variable::Baiduitems.size();
		if (LangCount > 0) {
			if (mTranslate->mFrom < 0 || mTranslate->mFrom >= LangCount) { mTranslate->mFrom = 0; }
			if (mTranslate->mTo < 1 || mTranslate->mTo >= LangCount) { mTranslate->mTo = (LangCount > 1) ? 1 : 0; }
		}
		const char* FromName = (LangCount > 0) ? Variable::BaiduitemsName[mTranslate->mFrom].c_str() : "";
		const char* ToName = (LangCount > 0) ? Variable::BaiduitemsName[mTranslate->mTo].c_str() : "";

		//顶行：源语言 / 互换 / 目标语言 …… 右边是翻译源（点一下换下一个）
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::BeginCombo("##from", FromName)) {
			for (int n = 0; n < LangCount; ++n) {
				const bool Selected = (mTranslate->mFrom == n);
				if (ImGui::Selectable(Variable::BaiduitemsName[n].c_str(), Selected)) { mTranslate->mFrom = n; }
				if (Selected) { ImGui::SetItemDefaultFocus(); }
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		const bool CanSwap = (mTranslate->mFrom > 0) && (mTranslate->mTo > 0) && (mTranslate->mFrom < LangCount) && (mTranslate->mTo < LangCount);
		ImGui::BeginDisabled(!CanSwap);
		if (ImGui::Button(Language::SwapLanguage.c_str())) {
			const int FromLanguage = mTranslate->mFrom;
			mTranslate->mFrom = mTranslate->mTo;
			mTranslate->mTo = FromLanguage;
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", CanSwap ? Language::SwapLanguage.c_str() : Language::SourceLanguage.c_str());
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::BeginCombo("##to", ToName)) {
			for (int n = 1; n < LangCount; ++n) {
				const bool Selected = (mTranslate->mTo == n);
				if (ImGui::Selectable(Variable::BaiduitemsName[n].c_str(), Selected)) { mTranslate->mTo = n; }
				if (Selected) { ImGui::SetItemDefaultFocus(); }
			}
			ImGui::EndCombo();
		}
		//翻译源：点一下循环切换（百度 / 爬虫 / 有道 / AI模型）
		const std::string EngineLabel = Language::Engine + ": " + mTranslate->TranslateName[mTranslate->mTranslate];
		const float EngineWidth = ImGui::CalcTextSize(EngineLabel.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		//自适应：右边放得下就靠右同行，放不下就换到下一行，免得和语言下拉挤在一起
		const float EngineRightX = ImGui::GetWindowWidth() - EngineWidth - ImGui::GetStyle().WindowPadding.x;
		if (EngineRightX > ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x) {
			ImGui::SameLine(EngineRightX);
		}
		if (ImGui::Button(EngineLabel.c_str())) {
			mTranslate->mTranslate++;
			if (mTranslate->mTranslate > Translate::AiTranslate) { mTranslate->mTranslate = 0; }
			//翻译源是模式开关：点一下立刻写回 Data.ini，不然重启后又变回百度
			Variable::Translate = mTranslate->mTranslate;
			Variable::SaveFile();
		}

		//文本框宽度（顺便算自动换行宽度，AngelScript 的 Autowrap 用）
		kuangshu = (int)ImGui::GetContentRegionAvail().x;
		Variable::WrapSize = kuangshu / int(std::max(1.0f, Variable::FontSize));

		//文本框高度自适应：按文本在「文本框自身宽度」下换行后的真实高度算；长文到上限后交给文本框自己滚动
		const ImGuiStyle& UiStyle = ImGui::GetStyle();
		const float LineHeight = ImGui::GetTextLineHeight();
		const float TextWrapWidth = std::max(40.0f, ImGui::GetContentRegionAvail().x - UiStyle.FramePadding.x * 2.0f);
		const float MinBoxHeight = LineHeight * 3.0f + UiStyle.FramePadding.y * 2.0f;
		const float TrScreenBottom = (float)GetSystemMetrics(SM_CYSCREEN) - 8.0f; const float TrChrome = (TrChromeLast > 1.0f) ? TrChromeLast : (LineHeight * 6.0f + UiStyle.ItemSpacing.y * 6.0f + UiStyle.WindowPadding.y * 2.0f); const float TrRoomForBoxes = TrScreenBottom - ImGui::GetWindowPos().y - TrChrome; float MaxBoxHeight = float(Variable::windows_Heigth) * 0.4f; if (TrRoomForBoxes > MinBoxHeight * 2.0f) { MaxBoxHeight = std::min(MaxBoxHeight, TrRoomForBoxes * 0.5f); } MaxBoxHeight = std::max(MinBoxHeight, MaxBoxHeight);//文本框高度上限：正常按窗口高度的 40%；窗口贴着屏幕底部弹出时再按「顶到屏幕底还剩多少」收一次，保证按钮和文本框都在屏幕里（ImGui 的 DisplaySize / GetMainViewport 在多视口下是 1x1，只能用系统屏幕尺寸）
		auto AdaptiveBoxHeight = [&](const char* Text) -> float {
			float Height = MinBoxHeight;
			if (Text[0] != '\0') {
				Height = ImGui::CalcTextSize(Text, NULL, false, TextWrapWidth).y + UiStyle.FramePadding.y * 2.0f + LineHeight * 0.5f;
			}
			return std::min(std::max(Height, MinBoxHeight), MaxBoxHeight);
		};
		const float EngBoxHeight = AdaptiveBoxHeight(eng);
		const float ZhongBoxHeight = AdaptiveBoxHeight(zhong);

		//原文
		if (InputCursorBool) {
			ImGui::SetKeyboardFocusHere();//窗口打开时把焦点放到原文框上
		}
		TranslateInputBool = true;
		ImGui::InputTextMultiline("##eng", eng, IM_ARRAYSIZE(eng), ImVec2(-FLT_MIN, EngBoxHeight), flags, MyText);
		if (InputCursorBool) {
			InputTextMultilineText();//将剪贴板内容粘贴到输入光标位置
		}
		TranslateInputBool = false;

		//按钮行
		if (ImGui::Button(Language::TranslationKey.c_str(), ImVec2(96.0f, 0.0f))) {
			//普通翻译源（百度/爬虫/有道）在这里同步出结果；
			//本地 AI 模型是后台算的，结果由 UpdateTranslateTask() 填进来
			RequestTranslate(eng);
			//zhong 是 1MB 的定长缓冲区，长文翻译结果直接 memcpy 会写爆且结尾没有 '\0'
			memset(zhong, 0, sizeof(zhong));
			TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);
		}
		ImGui::SameLine();
		if (ImGui::Button(Language::Clear.c_str(), ImVec2(80.0f, 0.0f))) {
			memset(eng, 0, sizeof(eng));
			memset(zhong, 0, sizeof(zhong));
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(zhong[0] == '\0');
		if (ImGui::Button(Language::CopyResult.c_str(), ImVec2(110.0f, 0.0f))) {
			TOOL::CopyToClipboard(TOOL::Utf8ToUnicode(zhong));
		}
		ImGui::EndDisabled();

		//译文
		ImGui::InputTextMultiline("##zhong", zhong, IM_ARRAYSIZE(zhong), ImVec2(-FLT_MIN, ZhongBoxHeight), flags, MyText);

		//自适应尺寸：宽度只在拖动时记（默认 380，最小 300），高度永远等于内容实际需要的高度
		if (GetKeyState(VK_LBUTTON) < 0) {
			const float WinW = ImGui::GetWindowWidth();
			if (WinW > 1.0f) { TranslateWinWidth = WinW; }
		}
		if (TranslateWinWidth < 300.0f) { TranslateWinWidth = 300.0f; }
		TranslateWinHeight = ImGui::GetCursorPosY() + UiStyle.WindowPadding.y;
		TrChromeLast = TranslateWinHeight - (EngBoxHeight + ZhongBoxHeight);//量一下这一帧「非文本框部分」到底有多高，下一帧用它算上限（第一帧先用估算值）
		ImGui::SetWindowSize(ImVec2(TranslateWinWidth, TranslateWinHeight));
		BeginWindowPosX = (int)ImGui::GetWindowPos().x;
		BeginWindowPosY = (int)ImGui::GetWindowPos().y;
		ImGui::End();

		// 获取窗口句柄
		HWND hwnd = FindWindow(NULL, "TranslateUI");
		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

	void ImGuiInterFace::ScreenshotInterface()
	{
		if (ScreenshotBool) {
			//左键点击事件
			if (GetKeyState(VK_LBUTTON) < 0) {
				//获取左键位置
				GetCursorPos(&MousePosition_1);
				GetCursorPos(&MousePosition_2);
				ScreenshotBool = false;
			}

			//右键点击事件
			if (GetKeyState(VK_RBUTTON) < 0) {
				//获取鼠标右键点击事件，取消截图
				EndDisplayBool = true;
				ScreenshotBool = true;
			}
		}
		else {
			//更新鼠标移动结束位置
			GetCursorPos(&MousePosition_2);

			if (MousePosition_1.x < MousePosition_2.x) {
				x = MousePosition_1.x;
				w = MousePosition_2.x - MousePosition_1.x;
			}
			else {
				x = MousePosition_2.x;
				w = MousePosition_1.x - MousePosition_2.x;
			}
			if (MousePosition_1.y < MousePosition_2.y) {
				y = MousePosition_1.y;
				h = MousePosition_2.y - MousePosition_1.y;
			}
			else {
				y = MousePosition_2.y;
				h = MousePosition_1.y - MousePosition_2.y;
			}

			//鼠标左键松开事件
			if (GetKeyState(VK_LBUTTON) >= 0) {

				//先把界面切到翻译窗口：窗口**立刻**出现，识别/脚本/翻译都在它背后进行，结果回来再往界面上写
				Variable::eng.clear();
				Variable::zhong = Language::Recognizing;
				//脚本模式（AngelScript 开着）也走后台识别：文本出来后再由 UpdateTranslateTask() 跑脚本，
				//这样脚本里同步做的翻译不会再挡住「翻译内容显示界面」的出现
				ScriptAfterOcr = AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->GetOpenBool() && Variable::ScriptBool;

				//截图翻译：识别丢到后台线程，界面立刻显示「识别中…」，
				//识别完由 UpdateTranslateTask() 接着发起翻译（普通源起 HTTP 线程，AI 源起模型线程）
				if (!mTesseract->OcrBegin(x, y, w, h, TData))
				{
					Variable::zhong.clear();
					TOOL::logger->warn("screenshot ocr: failed to start");
				}
				OcrTaskPending = mTesseract->OcrRunning();

				//eng/zhong 各 1MB，长文识别+翻译结果直接 memcpy 会写爆数组，且结尾没有 '\0'
				memset(eng, 0, sizeof(eng));
				memset(zhong, 0, sizeof(zhong));
				TOOL::CopyToBuffer(eng, sizeof(eng), Variable::eng);
				TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);

				x = 0;
				w = 0;
				y = 0;
				h = 0;

				//开启翻译
				SetInterFace(TranslateEnum);
			}
		}

		ImGui::Begin("ScreenshotUI", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar);
		
		ImGui::SetWindowPos({ -8, -8 });//设置显示位置
		ImGui::SetWindowSize(ImVec2(Variable::windows_Width + 16, Variable::windows_Heigth + 16));//设置显示大小
		//显示图片
		ImGui::Image((ImTextureID)mTextureData.DS, ImVec2(Variable::windows_Width, Variable::windows_Heigth), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0, 0, 0, 0));

		//创建画布,用于把框选之外的画面变暗。
		ImDrawList* draw_list = ImGui::GetWindowDrawList();

		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(0, 0),
			ImVec2(0, y),
			ImVec2(x + w, y),
			ImVec2(x + w, 0),
			//颜色
			IM_COL32(Variable::ScreenshotColor[0], Variable::ScreenshotColor[1], Variable::ScreenshotColor[2], Variable::ScreenshotColor[3]));

		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(x + w, 0),
			ImVec2(Variable::windows_Width, 0),
			ImVec2(Variable::windows_Width, y + h),
			ImVec2(x + w, y + h),
			//颜色
			IM_COL32(Variable::ScreenshotColor[0], Variable::ScreenshotColor[1], Variable::ScreenshotColor[2], Variable::ScreenshotColor[3]));

		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(x, y + h),
			ImVec2(Variable::windows_Width, y + h),
			ImVec2(Variable::windows_Width, Variable::windows_Heigth),
			ImVec2(x, Variable::windows_Heigth),
			//颜色
			IM_COL32(Variable::ScreenshotColor[0], Variable::ScreenshotColor[1], Variable::ScreenshotColor[2], Variable::ScreenshotColor[3]));

		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(0, y),
			ImVec2(x, y),
			ImVec2(x, Variable::windows_Heigth),
			ImVec2(0, Variable::windows_Heigth),
			//颜色
			IM_COL32(Variable::ScreenshotColor[0], Variable::ScreenshotColor[1], Variable::ScreenshotColor[2], Variable::ScreenshotColor[3]));


		static POINT pt = { 0,0 };
		GetCursorPos(&pt);//获取鼠标位置
		ImGui::SetCursorPosX(pt.x + 9);
		ImGui::SetCursorPosY(pt.y - 56);
		//显示放大位置
		ImGui::Image((ImTextureID)mTextureData.DS, ImVec2(64, 64),
			ImVec2(float(pt.x - 16) / float(Variable::windows_Width), float(pt.y - 16) / float(Variable::windows_Heigth)),
			ImVec2(float(pt.x + 16) / float(Variable::windows_Width), float(pt.y + 16) / float(Variable::windows_Heigth)));
		//画十字准心
		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(pt.x + 1, pt.y - 33),
			ImVec2(pt.x + 65, pt.y - 33),
			ImVec2(pt.x + 65, pt.y - 32),
			ImVec2(pt.x + 1, pt.y - 32),
			//颜色
			IM_COL32(255, 0, 0, 100));

		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(pt.x + 32, pt.y - 64),
			ImVec2(pt.x + 33, pt.y - 64),
			ImVec2(pt.x + 33, pt.y - 0),
			ImVec2(pt.x + 32, pt.y - 0),
			//颜色
			IM_COL32(255, 0, 0, 100));

		ImGui::End();
		// 获取窗口句柄
		HWND hwnd = FindWindow(NULL, "ScreenshotUI");
		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

	struct InputBoxInfo {
		char* Text;
		int Pos;
		int Len;
		int Cap;//缓冲区的真实容量（字节），粘贴时必须按它截断
		bool FFO;
		char* LText;
	};
	InputBoxInfo InputInfo;

	void InputText() {
		if (!((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)) && InputInfo.FFO) {
			//旧写法把光标后的尾部内容拷进栈上的 selected_text[10000] 再拼回去：
			//长度既没按 selected_text 截断，也没按目标缓冲区（这些输入框都只有 128 字节）截断，
			//往一个 128 字节的输入框里粘贴一段长文本，就会连着写爆栈数组和堆缓冲区。
			std::string ClipboardText = TOOL::UnicodeToUtf8(TOOL::ClipboardTochar());
			const int Cap = (InputInfo.Cap > 0) ? InputInfo.Cap : 128;
			const std::string TextCopy(InputInfo.LText, strnlen(InputInfo.LText, (size_t)Cap));
			const int TextLen = (int)TextCopy.size();
			int PastePos = (InputInfo.Pos < TextLen) ? InputInfo.Pos : TextLen;
			if (PastePos < 0) { PastePos = 0; }
			const std::string ResultText = TextCopy.substr(0, PastePos) + ClipboardText + TextCopy.substr(PastePos);
			ImGui::ClearActiveID();//失去焦点，粘贴的内容才会被保存
			TOOL::CopyToBuffer(InputInfo.Text, (size_t)Cap, ResultText);
			InputInfo.Pos = PastePos + (int)ClipboardText.size();
			if (InputInfo.Pos > Cap - 1) { InputInfo.Pos = Cap - 1; }
			InputInfo.FFO = false;
		}
	}

	int InputKeyEvent(ImGuiInputTextCallbackData* data) {
		InputBoxInfo* i = static_cast<InputBoxInfo*>(data->UserData);
		if ((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)) {
			InputInfo.Text = InputInfo.LText;
			InputInfo.Len = data->BufTextLen;
			InputInfo.Cap = data->BufSize;//ImGui 拿到的就是数组真实容量（调用点都传了 IM_ARRAYSIZE）
			InputInfo.Pos = data->CursorPos;
			InputInfo.FFO = true;
		}
		return 0;
	}


	void ImGuiInterFace::SetUpInterface()
	{
		static std::string Hitokoto;

		static char SetWebDav_url[128];
		static char SetWebDav_username[128];
		static char SetWebDav_password[128];
		static char SetWebDav_WebFile[128];

		static char SetBaiduID[128];
		static char SetBaiduKey[128];
		static char SetYoudaoID[128];
		static char SetYoudaoKey[128];

		//本地 AI 模型（llama.cpp）：模型路径 + 推理参数
		static char SetAiModelPath[260];
		static int SetAiThreads;
		static int SetAiNCtx;
		static int SetAiMaxTokens;
		static float SetAiTemperature;
		static int SetAiIdleUnload;
		static int SetTranslateSource;//设置界面里选的翻译源（0=百度 1=爬虫 2=有道 3=AI 模型）

		static int SetMakeUp;
		char* CharMakeUpS[2] = { "Alt","Ctrl" };
		int MakeUpS[2] = { 18,17};
		static char SetScreenshotkey[2];
		static char SetChoicekey[2];
		static char SetReplacekey[2];

		static int ModelIndex;
		static std::vector<std::string> ModelS;

		static int FontIndex;
		static int MyFontSize;
		static std::vector<std::string> FontS;

		static int LanguageIndex;
		static std::vector<std::string> LanguageS;

		static float LFontSize;
		static bool LFontBool;

		static float ScreenshotColor[4];

		static int ScriptIndex;
		static std::vector<std::string> ScriptS;
		static bool LScriptBool;

		static bool RecoveryWindow;//恢复窗口选项
		static std::vector<std::string> RecoveryList;
		static int RecoveryIndex = 0;
		static unsigned char RecoveryChoice = 0;

		static bool LDirectory_Opcode;
		static bool LDirectory_Language;
		static bool LDirectory_TessData;
		static bool LDirectory_TTF;

		static bool SetPopUpNotificationBool;
		static int SetHitokotoTimeInterval;
		static int SetHitokotoDisplayDuration;
		static float SetHitokotoPosX;
		static float SetHitokotoPosY;
		static float SetHitokotoFontSize;
		static bool SetHitokotoTTFBool;
		static bool SetHitokotoFontBool;
		static int SetHitokotoFontIndex;
		//渲染设备选择：先改这几个"待保存"的局部量，按下保存才写回 Variable::（和 PixelClean 一致）
		static int SetVulkanDeviceMode = (int)Variable::VulkanDeviceModeEnum::AutoBest;
		static bool VulkanDeviceModeChanged = false;
		static Variable::VulkanDeviceModeEnum PendingVulkanDeviceMode = Variable::VulkanDeviceModeEnum::AutoBest;
		static std::string PendingVulkanDeviceName = "";
		//语言文件里本来就是 UTF-8 中文，标点也一起放进语言文件当模板，
		//这样代码里不用出现非 ASCII 字面量。这里把模板里前 N 个 %s 依次换成给定文本。
		auto FillDeviceText = [](const std::string& Tpl, const std::vector<std::string>& Values) -> std::string {
			std::string Result;
			Result.reserve(Tpl.size() + 32);
			size_t ValueIndex = 0;
			for (size_t i = 0; i < Tpl.size(); i++)
			{
				if (Tpl[i] == '%' && i + 1 < Tpl.size() && Tpl[i + 1] == 's' && ValueIndex < Values.size())
				{
					Result += Values[ValueIndex];
					ValueIndex++;
					i++;//跳过 s
				}
				else
				{
					Result += Tpl[i];
				}
			}
			return Result;
		};
		if (SetBool) {
			SetBool = false;

			Hitokoto = GetHitokoto();

			//这些目标全是 128 字节的定长数组。旧写法直接 memcpy(..., str.size())：
			//配置里存了长文本就会写爆数组，而且刚好写满 128 字节时连终止符都没有，
			//ImGui 后面按 C 字符串读它就会一路读到相邻内存。CopyToBuffer 负责截断并补 '\0'。
			TOOL::CopyToBuffer(SetWebDav_url, sizeof(SetWebDav_url), Variable::WebDav_url);
			TOOL::CopyToBuffer(SetWebDav_username, sizeof(SetWebDav_username), Variable::WebDav_username);
			TOOL::CopyToBuffer(SetWebDav_password, sizeof(SetWebDav_password), Variable::WebDav_password);
			TOOL::CopyToBuffer(SetWebDav_WebFile, sizeof(SetWebDav_WebFile), Variable::WebDav_WebFile);

			TOOL::CopyToBuffer(SetBaiduID, sizeof(SetBaiduID), Variable::BaiduAppid);
			TOOL::CopyToBuffer(SetBaiduKey, sizeof(SetBaiduKey), Variable::BaiduSecret_key);
			TOOL::CopyToBuffer(SetYoudaoID, sizeof(SetYoudaoID), Variable::YoudaoAppid);
			TOOL::CopyToBuffer(SetYoudaoKey, sizeof(SetYoudaoKey), Variable::YoudaoSecret_key);

			TOOL::CopyToBuffer(SetAiModelPath, sizeof(SetAiModelPath), Variable::AiModelPath);
			SetAiThreads = Variable::AiThreads;
			SetAiNCtx = Variable::AiNCtx;
			SetAiMaxTokens = Variable::AiMaxTokens;
			SetAiTemperature = Variable::AiTemperature;
			SetAiIdleUnload = Variable::AiIdleUnload;
			SetTranslateSource = mTranslate->mTranslate;//打开设置界面时按当前实际生效的翻译源初始化

			if (Variable::MakeUp == 17) { SetMakeUp = 1; }
			TOOL::CopyToBuffer(SetScreenshotkey, sizeof(SetScreenshotkey), Variable::Screenshotkey);
			TOOL::CopyToBuffer(SetChoicekey, sizeof(SetChoicekey), Variable::Choicekey);
			TOOL::CopyToBuffer(SetReplacekey, sizeof(SetReplacekey), Variable::Replacekey);

			LFontSize = Variable::FontSize;
			LFontBool = Variable::FontBool;

			ModelS.clear();
			ModelIndex = 0;
			TOOL::FilePath("./TessData", &ModelS, "traineddata", TOOL::StrName(Variable::Model).c_str(), &ModelIndex);

			FontS.clear();
			FontIndex = 0;
			TOOL::FilePath("./TTF", &FontS, "ttf", TOOL::StrName(Variable::FontFilePath).c_str(), &FontIndex);
			MyFontSize = FontS.size();
			TOOL::FilePath("C:\\Windows\\Fonts", &FontS, "ttf", TOOL::StrName(Variable::FontFilePath).c_str(), &FontIndex);

			LanguageS.clear();
			LanguageIndex = 0;
			TOOL::FilePath("./Language", &LanguageS, "ini", TOOL::StrName(Variable::Language).c_str(), &LanguageIndex);
			//ImGui::InputTextWithHint("input text (w/ hint)", "enter text here", str1, IM_ARRAYSIZE(str1));

			for (size_t i = 0; i < 4; i++)
			{
				ScreenshotColor[i] = float(Variable::ScreenshotColor[i]) / 255.0f;
			}
			
			ScriptS.clear();
			ScriptIndex = 0;
			TOOL::FilePath("./Opcode", &ScriptS, "as", TOOL::StrName(Variable::Script).c_str(), &ScriptIndex);
			LScriptBool = Variable::ScriptBool;

			//渲染设备选择：把当前设置换算成下拉框下标。
			//0..2 是三项固定的（自动最高/自动最低/CPU），3 开始依次对应识别到的设备。
			if (Variable::VulkanDeviceMode == Variable::VulkanDeviceModeEnum::Specific)
			{
				SetVulkanDeviceMode = 0;//指定的设备这次没识别到就先显示第一项
				for (size_t i = 0; i < Variable::VulkanDetectedDevices.size(); i++)
				{
					if (Variable::VulkanDetectedDevices[i].name == Variable::VulkanDeviceName)
					{
						SetVulkanDeviceMode = (int)i + 3;
						break;
					}
				}
			}
			else
			{
				SetVulkanDeviceMode = (int)Variable::VulkanDeviceMode;
			}
			PendingVulkanDeviceMode = Variable::VulkanDeviceMode;
			PendingVulkanDeviceName = Variable::VulkanDeviceName;
			VulkanDeviceModeChanged = false;

			RecoveryWindow = false;
			RecoveryIndex = 0;
			RecoveryChoice = 0;

			LDirectory_Opcode = Variable::OpcodeBool;
			LDirectory_Language = Variable::LanguageBool;
			LDirectory_TessData = Variable::TessDataBool;
			LDirectory_TTF = Variable::TTFBool;

			SetPopUpNotificationBool = Variable::PopUpNotificationBool;
			SetHitokotoTimeInterval = Variable::HitokotoTimeInterval;
			SetHitokotoDisplayDuration = Variable::HitokotoDisplayDuration;
			SetHitokotoFontSize = Variable::HitokotoFontSize;
			SetHitokotoTTFBool = Variable::HitokotoTTFBool;
			SetHitokotoFontBool = Variable::HitokotoFontBool;
			SetHitokotoPosX = Variable::HitokotoPosX;
			SetHitokotoPosY = Variable::HitokotoPosY;
			SetHitokotoFontIndex = 0;
			for (size_t i = 0; i < FontS.size(); i++)
			{
				if (FontS[i] == Variable::HitokotoFont) {
					SetHitokotoFontIndex = i;
				}
			}
			
		}

		//当前页（左侧导航选中项）
		static int SetPage = 0;
		//「已保存」提示的显示时刻
		static clock_t SavedTime = 0;
		//工程的版本号没有对应变量，先写死一个
		const char* VersionText = "v2.0.0";
		//左侧导航：8 个分类
		const char* NavItems[8] = {
			Language::NavTranslate.c_str(), Language::NavAI.c_str(), Language::NavHotkey.c_str(), Language::NavGeneral.c_str(),
			Language::NavInterface.c_str(), Language::NavHitokoto.c_str(), Language::NavBackup.c_str(), Language::NavAbout.c_str()
		};
		//每页第一行的小标题：AI/快捷键沿用原来的段标题，其余用导航名
		const char* PageTitles[8] = {
			Language::NavTranslate.c_str(), Language::AIModel.c_str(), Language::ShortcutKeys.c_str(), Language::NavGeneral.c_str(),
			Language::NavInterface.c_str(), Language::NavHitokoto.c_str(), Language::NavBackup.c_str(), Language::NavAbout.c_str()
		};

		//保存动作：底部的「保存」按钮和 Ctrl+S 都走这一段（原来就是一整块，搬进 lambda 里不改逻辑）
		auto DoSave = [&]()
		{
			bool updata = false;//判断是否要重启软件
			Variable::PopUpNotificationBool = SetPopUpNotificationBool;
			Variable::HitokotoTimeInterval = SetHitokotoTimeInterval;
			Variable::HitokotoDisplayDuration = SetHitokotoDisplayDuration;
			Variable::HitokotoPosX = SetHitokotoPosX;
			Variable::HitokotoPosY = SetHitokotoPosY;
			if (Variable::HitokotoFontSize != SetHitokotoFontSize) {
				updata = true;
				Variable::HitokotoFontSize = SetHitokotoFontSize;
			}
			if (Variable::HitokotoTTFBool != SetHitokotoTTFBool) {
				updata = true;
				Variable::HitokotoTTFBool = SetHitokotoTTFBool;
			}
			if (Variable::HitokotoFontBool != SetHitokotoFontBool) {
				updata = true;
				Variable::HitokotoFontBool = SetHitokotoFontBool;
			}
			if ((FontS.size() != 0) && !Variable::HitokotoTTFBool) {
				std::string LFontFilePath;
				if (MyFontSize > SetHitokotoFontIndex) {
					LFontFilePath = "./TTF/" + FontS[SetHitokotoFontIndex] + ".ttf";
				}
				else {
					LFontFilePath = "C:\\Windows\\Fonts\\" + FontS[SetHitokotoFontIndex] + ".ttf";
				}

				if (LFontFilePath != Variable::HitokotoFont) {//更换字体
					updata = true;
					Variable::HitokotoFont = LFontFilePath;
				}
			}
			else {
				Variable::HitokotoTTFBool = true;
			}
			

			Variable::WebDav_url = SetWebDav_url;
			Variable::WebDav_username = SetWebDav_username;
			Variable::WebDav_password = SetWebDav_password;
			Variable::WebDav_WebFile = SetWebDav_WebFile;

			Variable::OpcodeBool = LDirectory_Opcode;
			Variable::LanguageBool = LDirectory_Language;
			Variable::TessDataBool = LDirectory_TessData;
			Variable::TTFBool = LDirectory_TTF;

			//渲染设备选择
			Variable::VulkanDeviceMode = PendingVulkanDeviceMode;
			Variable::VulkanDeviceName = PendingVulkanDeviceName;

			Variable::BaiduAppid = SetBaiduID;
			Variable::BaiduSecret_key = SetBaiduKey;
			Variable::YoudaoAppid = SetYoudaoID;
			Variable::YoudaoSecret_key = SetYoudaoKey;
			//翻译源：保存后立刻生效，并写回 Data.ini 的 [FT] Translate
			Variable::Translate = SetTranslateSource;
			mTranslate->SetTranslate(SetTranslateSource);

			//AI 模型：路径或推理参数改了就把已加载的模型卸掉，下次翻译按新设置重新加载
			const bool AiSettingChanged = (Variable::AiModelPath != SetAiModelPath) || (Variable::AiThreads != SetAiThreads) ||
				(Variable::AiNCtx != SetAiNCtx) || (Variable::AiMaxTokens != SetAiMaxTokens) ||
				(Variable::AiTemperature != SetAiTemperature);
			Variable::AiModelPath = SetAiModelPath;
			Variable::AiThreads = SetAiThreads;
			Variable::AiNCtx = SetAiNCtx;
			Variable::AiMaxTokens = SetAiMaxTokens;
			Variable::AiTemperature = SetAiTemperature;
			//闲置卸载时间只是给主循环判断用的，不算「推理参数变了」，不必重载模型
			Variable::AiIdleUnload = SetAiIdleUnload;
			if (AiSettingChanged) { mTranslate->AiUnloadModel(); }
			
			//转为大写
			Variable::MakeUp = MakeUpS[SetMakeUp];
			Variable::Screenshotkey = toupper(SetScreenshotkey[0]);
			Variable::Choicekey = toupper(SetChoicekey[0]);
			Variable::Replacekey = toupper(SetReplacekey[0]);

			for (size_t i = 0; i < 4; i++)
			{
				Variable::ScreenshotColor[i] = int(ScreenshotColor[i] * 255);
			}

			if (ModelS.size() != 0) {
				if (Variable::Model != ModelS[ModelIndex]) {
					mTesseract->~Tesseract();
					mTesseract->Tesseract::Tesseract(ModelS[ModelIndex].c_str());
				}
				Variable::Model = ModelS[ModelIndex];
			}

			Variable::ScriptBool = LScriptBool;
			if (ScriptS.size() != 0) {
				if (Variable::Script != ScriptS[ScriptIndex]) {
					delete AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode();
				}
				Variable::Script = ScriptS[ScriptIndex];
			}

			if (LanguageS.size() != 0) {
				if (Variable::Language != LanguageS[LanguageIndex]) {
					Language::ReadFile(LanguageS[LanguageIndex]);
				}
				Variable::Language = LanguageS[LanguageIndex];
			}

			
			if ((FontS.size() != 0) && Variable::FontBool) {
				std::string LFontFilePath;
				if (MyFontSize > FontIndex) {
					LFontFilePath = "./TTF/" + FontS[FontIndex] + ".ttf";
				}
				else {
					LFontFilePath = "C:\\Windows\\Fonts\\" + FontS[FontIndex] + ".ttf";
				}
				
				if (LFontFilePath != Variable::FontFilePath) {//更换字体
					updata = true;
				}
				Variable::FontFilePath = LFontFilePath;
			}
			else {
				Variable::FontBool = false;
			}

			if ((Variable::FontBool != LFontBool) || Variable::FontSize != LFontSize) {//更换字体或字体大小
				updata = true;
			}

			if (TOOL::SetModifyRegedit("TranslatorKyi", Variable::Startup)) {
				TOOL::logger->error("SetModifyRegedit(): Error");
			}

			
			Variable::SaveFile();

			if (updata) {
				delete mWindown;
				char path[MAX_PATH];
				GetModuleFileName(NULL, path, MAX_PATH);
				ShellExecute(NULL, NULL, path, NULL, NULL, SW_SHOWDEFAULT);
				exit(0);
			}

			mTranslate->SetBaiduAppID(Variable::BaiduAppid.c_str());
			mTranslate->SetBaiduSecretkey(Variable::BaiduSecret_key.c_str());
				
			mTranslate->SetYoudaoAppID(Variable::YoudaoAppid.c_str());
			mTranslate->SetYoudaoSecretkey(Variable::YoudaoSecret_key.c_str());

			EndDisplayBool = true;
			InterFaceBool = false;
			SetBool = true;
			SavedTime = clock();
		};

		ImGui::SetNextWindowSize(ImVec2(760.0f, 640.0f), ImGuiCond_FirstUseEver);//第一次显示时的默认大小
		ImGui::SetNextWindowSizeConstraints(ImVec2(560.0f, 380.0f), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("SetUI", &SetBool, ImGuiWindowFlags_NoTitleBar);//创建窗口

		//主题色（标题条竖线 / 导航选中项 / 「已保存」提示）
		const ImVec4 AccentColor(0.282f, 0.780f, 0.486f, 1.0f);
		const ImU32 AccentU32 = ImGui::GetColorU32(AccentColor);

		//标题条：一条绿色竖条 + 标题 + 右侧灰色版本号
		{
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			const ImVec2 Pos = ImGui::GetCursorScreenPos();
			const float Width = ImGui::GetContentRegionAvail().x;
			const float Height = ImGui::GetTextLineHeight() + 8.0f;
			DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y + 5.0f), ImVec2(Pos.x + 3.0f, Pos.y + Height - 3.0f), AccentU32, 2.0f);
			DrawList->AddText(ImVec2(Pos.x + 11.0f, Pos.y + 4.0f), ImGui::GetColorU32(ImGuiCol_Text), Language::Set.c_str());
			const float VersionWidth = ImGui::CalcTextSize(VersionText).x;
			DrawList->AddText(ImVec2(Pos.x + Width - VersionWidth, Pos.y + 4.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), VersionText);
			ImGui::Dummy(ImVec2(Width, Height));
		}
		ImGui::Separator();

		//底部栏高度：一行按钮 + 一点边距；左右两块内容区都按它留出底部空间
		const float FooterH = ImGui::GetFrameHeightWithSpacing() + 8.0f;
		//两列表格：左列固定宽度的标签，右列控件
		auto BeginSettingsTable = [](const char* Id) -> bool
		{
			if (!ImGui::BeginTable(Id, 2, ImGuiTableFlags_SizingStretchProp)) {
				return false;
			}
			ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthStretch, 0.38f);
			ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, 0.62f);
			return true;
		};
		auto RowLabel = [](const char* Label)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextWrapped(Label);
			ImGui::TableNextColumn();
		};

		//左侧分类导航
		ImGui::BeginChild("##nav", ImVec2(132.0f, -FooterH), true);
		{
			for (int i = 0; i < 8; i++)
			{
				ImGui::PushID(i);
				const bool Selected = (SetPage == i);
				if (Selected) {
					//selected item: translucent accent background, bright text stays readable
					ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(AccentColor.x, AccentColor.y, AccentColor.z, 0.22f));
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(AccentColor.x, AccentColor.y, AccentColor.z, 0.50f));
					ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(AccentColor.x, AccentColor.y, AccentColor.z, 0.65f));
				}
				const ImVec2 ItemPos = ImGui::GetCursorScreenPos();
				if (ImGui::Selectable(NavItems[i], Selected, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0.0f, 26.0f))) {
					SetPage = i;
				}
				if (Selected) {
					ImGui::PopStyleColor(3);
					//accent bar drawn inside the child window left padding
					ImDrawList* DrawList = ImGui::GetWindowDrawList();
					DrawList->AddRectFilled(ImVec2(ItemPos.x - 9.0f, ItemPos.y + 5.0f), ImVec2(ItemPos.x - 6.0f, ItemPos.y + 21.0f), AccentU32, 2.0f);
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		ImGui::SameLine();

		//右侧内容区
		ImGui::BeginChild("##page", ImVec2(0.0f, -FooterH));
		{
			//页面小标题
			ImGui::TextUnformatted(PageTitles[SetPage]);
			ImGui::Separator();

			switch (SetPage)
			{
			case 0://翻译服务：百度 / 有道的账号密钥
			{
				ImGui::TextUnformatted(Language::AccountKey.c_str());
				//翻译源：决定按快捷键 / 点「翻译」时用哪个引擎（翻译窗口右上角的按钮和托盘菜单里的「翻译源」都是这个值）
				if (BeginSettingsTable("##tbl_source"))
				{
					RowLabel(Language::Engine.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::BeginCombo("##translate_source", mTranslate->TranslateName[SetTranslateSource]))
					{
						for (int i = 0; i <= Translate::AiTranslate; i++)
						{
							const bool SourceChosen = (SetTranslateSource == i);
							if (ImGui::Selectable(mTranslate->TranslateName[i], SourceChosen)) {
								SetTranslateSource = i;
								//翻译源是模式开关：选中就立刻生效并写盘，不用等「保存」
								Variable::Translate = i;
								mTranslate->SetTranslate(i);
								Variable::SaveFile();
							}
							if (SourceChosen) { ImGui::SetItemDefaultFocus(); }
						}
						ImGui::EndCombo();
					}
					ImGui::EndTable();
				}
				ImGui::TextWrapped(Language::EngineHint.c_str());
				ImGui::Spacing();
				if (BeginSettingsTable("##tbl_translate"))
				{
					RowLabel(Language::BaiduID.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputInfo.LText = SetBaiduID;
					ImGui::InputText("##baidu_id", SetBaiduID, IM_ARRAYSIZE(SetBaiduID), flags, &InputKeyEvent, &InputInfo);
					RowLabel(Language::BaiduKey.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputInfo.LText = SetBaiduKey;
					ImGui::InputText("##baidu_key", SetBaiduKey, IM_ARRAYSIZE(SetBaiduKey), flags, &InputKeyEvent, &InputInfo);
					RowLabel(Language::YoudaoID.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputInfo.LText = SetYoudaoID;
					ImGui::InputText("##youdao_id", SetYoudaoID, IM_ARRAYSIZE(SetYoudaoID), flags, &InputKeyEvent, &InputInfo);
					RowLabel(Language::YoudaoKey.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputInfo.LText = SetYoudaoKey;
					ImGui::InputText("##youdao_key", SetYoudaoKey, IM_ARRAYSIZE(SetYoudaoKey), flags, &InputKeyEvent, &InputInfo);
					ImGui::EndTable();
				}
				break;
			}
			case 1://AI 模型（本地 llama.cpp）
			{
				if (BeginSettingsTable("##tbl_ai"))
				{
					RowLabel(Language::AIModelPath.c_str());
					ImGui::SetNextItemWidth(-96.0f);
					InputInfo.LText = SetAiModelPath;
					ImGui::InputText("##ai_path", SetAiModelPath, IM_ARRAYSIZE(SetAiModelPath), flags, &InputKeyEvent, &InputInfo);
					ImGui::SameLine();
					if (ImGui::Button(Language::AIModelDefault.c_str(), ImVec2(88.0f, 0.0f))) {
						TOOL::CopyToBuffer(SetAiModelPath, sizeof(SetAiModelPath), Translate::DefaultAiModelPath());
					}
					RowLabel(Language::AIThreads.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##ai_threads", &SetAiThreads);
					if (SetAiThreads < 0) { SetAiThreads = 0; }
					RowLabel(Language::AINCtx.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##ai_nctx", &SetAiNCtx);
					if (SetAiNCtx < 256) { SetAiNCtx = 256; }
					RowLabel(Language::AIMaxTokens.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##ai_maxtokens", &SetAiMaxTokens);
					if (SetAiMaxTokens < 16) { SetAiMaxTokens = 16; }
					RowLabel(Language::AITemperature.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputFloat("##ai_temperature", &SetAiTemperature, 0.05f, 0.1f);
					if (SetAiTemperature < 0.01f || SetAiTemperature > 2.0f) { SetAiTemperature = 0.7f; }
					//模型闲置多久自动卸载（秒，0 = 一直留着）；只影响主循环的判断，改了不用重新加载模型
					RowLabel(Language::AIIdleUnload.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##ai_idleunload", &SetAiIdleUnload);
					if (SetAiIdleUnload < 0) { SetAiIdleUnload = 0; }
					if (SetAiIdleUnload > 86400) { SetAiIdleUnload = 86400; }
					ImGui::EndTable();
				}
				ImGui::Text(Language::AIHint.c_str());
				if (mTranslate != nullptr) {
					//状态行：没加载 / 正在加载 / 正在翻译 / 已加载（模型常驻内存，只有退出或换参数才卸载）
					if (mTranslate->AiStage() == Translate::AiStageLoading) {
						ImGui::Text(Language::AIStatusLoading.c_str());
					}
					else if (mTranslate->AiStage() == Translate::AiStageGenerating) {
						ImGui::Text(Language::AIStatusGenerating.c_str());
					}
					else if (mTranslate->AiModelLoaded()) {
						std::string Status = AiTextWithString(Language::AIStatusLoaded, mTranslate->AiModelDesc());
						//开了「闲置卸载」就把倒计时一起显示，方便确认它真的会到点卸载
						const int IdleLeft = mTranslate->AiIdleRemaining();
						if (IdleLeft >= 0) { Status += AiTextWithNumber(Language::AIIdleLeft, IdleLeft); }
						ImGui::Text("%s", Status.c_str());
					}
					else {
						ImGui::Text(Language::AIStatusNotLoaded.c_str());
					}
					if (ImGui::Button(Language::AILoad.c_str())) {
						mTranslate->AiBeginLoad();//后台加载，界面不卡
					}
					ImGui::SameLine();
					if (ImGui::Button(Language::AIUnload.c_str())) {
						mTranslate->AiUnloadModel();//正在翻译时会忽略这次点击
					}
				}
				break;
			}
			case 2://快捷键
			{
				if (BeginSettingsTable("##tbl_hotkey"))
				{
					RowLabel(Language::KeyCombination.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::BeginCombo("##makeup", CharMakeUpS[SetMakeUp], flags))
					{
						for (int n = 0; n < 2; n++)
						{
							const bool is_selected = (SetMakeUp == n);
							if (ImGui::Selectable(CharMakeUpS[n], is_selected))
								SetMakeUp = n;
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					RowLabel(Language::ScreenshotTranslation.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputText("##screenshot_key", SetScreenshotkey, IM_ARRAYSIZE(SetScreenshotkey));
					RowLabel(Language::SelectTranslation.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputText("##choice_key", SetChoicekey, IM_ARRAYSIZE(SetChoicekey));
					RowLabel(Language::ReplaceTranslation.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputText("##replace_key", SetReplacekey, IM_ARRAYSIZE(SetReplacekey));
					ImGui::EndTable();
				}
				break;
			}
			case 3://常规
			{
				if (BeginSettingsTable("##tbl_general"))
				{
					RowLabel(Language::Startup.c_str());
					ImGui::Checkbox("##startup", &Variable::Startup);
					RowLabel(Language::ResidenceTime.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##displaytime", &Variable::DisplayTime);
					RowLabel(Language::FontSize.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputFloat("##fontsize", &Variable::FontSize, 0.1f, 1.0f);
					ImGui::EndTable();
				}
				break;
			}
			case 4://界面
			{
				if (BeginSettingsTable("##tbl_interface"))
				{
					RowLabel(Language::TesseractModel.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ModelS.size() != 0) {
						if (ImGui::BeginCombo("##tesseract_model", ModelS[ModelIndex].c_str(), flags))
						{
							for (int n = 0; n < ModelS.size(); n++)
							{
								const bool is_selected = (ModelIndex == n);
								if (ImGui::Selectable(ModelS[n].c_str(), is_selected))
									ModelIndex = n;
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
					else {
						ImGui::Text(Language::NotTesseractModelText.c_str());
					}
					RowLabel(Language::UseTTF_Typeface.c_str());
					ImGui::Checkbox("##use_ttf", &Variable::FontBool);
					ImGui::SameLine();
					if (ImGui::Button(Language::TTF_Folder.c_str())) {
						TCHAR buffer[MAX_PATH] = { 0 };
						GetCurrentDirectory(MAX_PATH, buffer);//获取启动器路径
						//拼接为绝对路径
						ShellExecute(NULL, "open", (std::string(buffer) + "\\TTF").c_str(), NULL, NULL, SW_SHOWDEFAULT);//打开文件夹
					}
					ImGui::SameLine();
					if (ImGui::Button(Language::TessDataFolder.c_str())) {
						TCHAR buffer[MAX_PATH] = { 0 };
						GetCurrentDirectory(MAX_PATH, buffer);//获取启动器路径
						//拼接为绝对路径
						ShellExecute(NULL, "open", (std::string(buffer) + "\\TessData").c_str(), NULL, NULL, SW_SHOWDEFAULT);//打开文件夹
					}
					RowLabel(Language::TTF_Typeface.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (Variable::FontBool) {
						if (FontS.size() != 0) {
							if (ImGui::BeginCombo("##ttf_typeface", FontS[FontIndex].c_str(), flags))
							{
								for (int n = 0; n < FontS.size(); n++)
								{
									const bool is_selected = (FontIndex == n);
									if (ImGui::Selectable(FontS[n].c_str(), is_selected))
										FontIndex = n;
									if (is_selected)
										ImGui::SetItemDefaultFocus();
								}
								ImGui::EndCombo();
							}
						}
						else {
							ImGui::Text(Language::NotTTF_TypefaceText.c_str());
						}
					}
					else {
						//没有内嵌字模了：不勾这一项时，用的是程序目录 ./TTF 里的默认字体，这里把它显示出来
						ImGui::Text(Language::DefaultTypeface.c_str(), DefaultTypefacePath().c_str());
					}
					RowLabel(Language::ReplaceLanguage.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::BeginCombo("##replace_language", Variable::BaiduitemsName[Variable::ReplaceLanguage].c_str(), flags))
					{
						for (int n = 0; n < Variable::BaiduitemsName.size()-1; n++)
						{
							const bool is_selected = (Variable::ReplaceLanguage == n);
							if (ImGui::Selectable(Variable::BaiduitemsName[n].c_str(), is_selected))
								Variable::ReplaceLanguage = n;
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					RowLabel(Language::Language.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::BeginCombo("##interface_language", LanguageS[LanguageIndex].c_str(), flags))
					{
						for (int n = 0; n < LanguageS.size(); n++)
						{
							const bool is_selected = (LanguageIndex == n);
							if (ImGui::Selectable(LanguageS[n].c_str(), is_selected))
								LanguageIndex = n;
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					ImGui::EndTable();
				}

				//渲染设备选择。下拉框的内容每帧重建：前三项固定，后面每项对应一台识别到的设备。
				if (BeginSettingsTable("##tbl_renderdevice"))
				{
					RowLabel(Language::RenderDevice.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					{
						static std::vector<std::string> RenderDeviceLabels;
						static std::vector<const char*> RenderDeviceItems;
						RenderDeviceLabels.clear();
						RenderDeviceItems.clear();
						RenderDeviceLabels.push_back(Language::RenderDeviceAutoBest);
						RenderDeviceLabels.push_back(Language::RenderDeviceAutoWorst);
						RenderDeviceLabels.push_back(Language::RenderDeviceCPU);
						for (size_t i = 0; i < Variable::VulkanDetectedDevices.size(); i++)
						{
							const Variable::VulkanDeviceInfo& Device = Variable::VulkanDetectedDevices[i];
							std::string TypeText;
							switch (Device.deviceType)
							{
							case 1: TypeText = Language::RenderDeviceTypeIGPU; break;
							case 2: TypeText = Language::RenderDeviceTypeDGPU; break;
							case 3: TypeText = Language::RenderDeviceTypeVirtual; break;
							case 4: TypeText = Language::RenderDeviceTypeCPU; break;
							default: TypeText = Language::RenderDeviceTypeOther; break;
							}
							if (Device.usable)
							{
								RenderDeviceLabels.push_back(FillDeviceText(Language::RenderDeviceItem, { Device.name, TypeText }));
							}
							else
							{
								RenderDeviceLabels.push_back(FillDeviceText(Language::RenderDeviceItemBad, { Device.name, TypeText, Language::RenderDeviceUnusable }));
							}
						}
						for (size_t i = 0; i < RenderDeviceLabels.size(); i++)
						{
							RenderDeviceItems.push_back(RenderDeviceLabels[i].c_str());
						}
						if (SetVulkanDeviceMode < 0 || SetVulkanDeviceMode >= (int)RenderDeviceItems.size())
						{
							SetVulkanDeviceMode = 0;//列表变了（比如换了显卡）就退回第一项，避免越界
						}
						if (ImGui::BeginCombo("##render_device", RenderDeviceItems[SetVulkanDeviceMode], flags))
						{
							for (int n = 0; n < (int)RenderDeviceItems.size(); n++)
							{
								const bool is_selected = (SetVulkanDeviceMode == n);
								if (ImGui::Selectable(RenderDeviceItems[n], is_selected))
								{
									SetVulkanDeviceMode = n;
									if (n >= 3 && (size_t)(n - 3) < Variable::VulkanDetectedDevices.size())
									{
										//第 3 项往后都是具体设备，记下名字，重启后按名字找
										PendingVulkanDeviceMode = Variable::VulkanDeviceModeEnum::Specific;
										PendingVulkanDeviceName = Variable::VulkanDetectedDevices[n - 3].name;
									}
									else
									{
										const int Mode = (n < 0) ? 0 : ((n > 2) ? 2 : n);
										PendingVulkanDeviceMode = (Variable::VulkanDeviceModeEnum)Mode;
									}
								}
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						VulkanDeviceModeChanged = (PendingVulkanDeviceMode != Variable::VulkanDeviceMode)
							|| (PendingVulkanDeviceMode == Variable::VulkanDeviceModeEnum::Specific && PendingVulkanDeviceName != Variable::VulkanDeviceName);
					}
					ImGui::EndTable();
				}
				if (VulkanDeviceModeChanged)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s", Language::RenderDeviceRestart.c_str());
				}
				else if (Variable::IsSpecificDeviceMode())
				{
					//设置里指定了设备，但这轮探测没看到它，Vulkan 层会自动改用最高性能的那台
					bool DeviceFound = false;
					for (size_t i = 0; i < Variable::VulkanDetectedDevices.size(); i++)
					{
						if (Variable::VulkanDetectedDevices[i].name == Variable::VulkanDeviceName)
						{
							DeviceFound = true;
							break;
						}
					}
					if (!DeviceFound)
					{
						ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s", FillDeviceText(Language::RenderDeviceMissing, { Variable::VulkanDeviceName }).c_str());
					}
				}
				//这次实际跑在哪台设备上
				if (Variable::RunningOnSoftwareRenderer)
				{
					ImGui::Text("%s", FillDeviceText(Language::RenderDeviceCurrentCPU, { Variable::RunningDeviceName }).c_str());
				}
				else if (Variable::IsSpecificDeviceMode())
				{
					ImGui::Text("%s", FillDeviceText(Language::RenderDeviceCurrentSpecific, { Variable::RunningDeviceName }).c_str());
				}
				else
				{
					ImGui::Text("%s", FillDeviceText(Language::RenderDeviceCurrentGPU, { Variable::RunningDeviceName }).c_str());
				}
				if (Variable::RunningOnSoftwareRenderer && !Variable::CpuSoftwareRenderReason.empty())
				{
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s", FillDeviceText(Language::RenderDeviceDegrade, { Variable::CpuSoftwareRenderReason }).c_str());
				}
				ImGui::SameLine();
				{
					//7 条提示拼成一个多行 tooltip，交给已有的 HelpMarker
					const std::string HelpText = Language::RenderDeviceHelp1 + "\n" + Language::RenderDeviceHelp2 + "\n"
						+ Language::RenderDeviceHelp3 + "\n" + Language::RenderDeviceHelp4 + "\n"
						+ Language::RenderDeviceHelp5 + "\n" + Language::RenderDeviceHelp6 + "\n" + Language::RenderDeviceHelp7;
					HelpMarker(HelpText.c_str());
				}

				if (BeginSettingsTable("##tbl_interface2"))
				{
					RowLabel(Language::ScreenshotColor.c_str());
					ImGui::ColorEdit4("##screenshot_color", (float*)&ScreenshotColor, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);
					RowLabel(Language::Script.c_str());
					ImGui::Checkbox("##script", &LScriptBool);//同名会出现冲突
					if (LScriptBool) {
						if (ScriptS.size() != 0) {
							ImGui::SameLine();
							ImGui::SetNextItemWidth(-FLT_MIN);
							if (ImGui::BeginCombo("##script_combo", ScriptS[ScriptIndex].c_str(), flags))
							{
								for (int n = 0; n < ScriptS.size(); n++)
								{
									const bool is_selected = (ScriptIndex == n);
									if (ImGui::Selectable(ScriptS[n].c_str(), is_selected))
										ScriptIndex = n;
									if (is_selected)
										ImGui::SetItemDefaultFocus();
								}
								ImGui::EndCombo();
							}
						}
						else {
							ImGui::Text(Language::NotScript.c_str());
						}
					}
					ImGui::EndTable();
				}
				break;
			}
			case 5://一言
			{
				if (BeginSettingsTable("##tbl_hitokoto"))
				{
					RowLabel(Language::PopUpNotification.c_str());
					ImGui::Checkbox("##popup_notification", &SetPopUpNotificationBool);
					if (SetPopUpNotificationBool) {
						RowLabel(Language::IndependentTypeface.c_str());
						ImGui::Checkbox("##hitokoto_font", &SetHitokotoFontBool);
						if (SetHitokotoFontBool) {
							RowLabel(Language::InternalFontPattern.c_str());
							ImGui::Checkbox("##hitokoto_ttf", &SetHitokotoTTFBool);
							RowLabel(Language::TTF_Typeface.c_str());
							ImGui::SetNextItemWidth(-FLT_MIN);
							if (!SetHitokotoTTFBool) {
								if (FontS.size() != 0) {
									if (ImGui::BeginCombo("##hitokoto_typeface", FontS[SetHitokotoFontIndex].c_str(), flags))
									{
										for (int n = 0; n < FontS.size(); n++)
										{
											const bool is_selected = (SetHitokotoFontIndex == n);
											if (ImGui::Selectable(FontS[n].c_str(), is_selected))
												SetHitokotoFontIndex = n;
											if (is_selected)
												ImGui::SetItemDefaultFocus();
										}
										ImGui::EndCombo();
									}
								}
								else {
									ImGui::Text(Language::NotTTF_TypefaceText.c_str());
								}
							}
							else {
								//「默认字模」= 程序目录 ./TTF 里的字体
								ImGui::Text(Language::DefaultTypeface.c_str(), DefaultTypefacePath().c_str());
							}
						}
						RowLabel(Language::PositionX.c_str());
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("##hitokoto_posx", &SetHitokotoPosX, 0.0f, 1.0f);
						RowLabel(Language::PositionY.c_str());
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("##hitokoto_posy", &SetHitokotoPosY, 0.0f, 1.0f);
						RowLabel(Language::HitokotoTimeInterval.c_str());
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::InputInt("##hitokoto_interval", &SetHitokotoTimeInterval);
						RowLabel(Language::HitokotoDisplayDuration.c_str());
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::InputInt("##hitokoto_duration", &SetHitokotoDisplayDuration);
						RowLabel(Language::HitokotoFontSize.c_str());
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::InputFloat("##hitokoto_fontsize", &SetHitokotoFontSize);
					}
					ImGui::EndTable();
				}
				break;
			}
			case 6://备份（坚果云 WebDav）
			{
				if (ImGui::Button(Language::jianguoyunWebDav.c_str())) {
					ShellExecute(NULL, "open", "https://www.jianguoyun.com/", NULL, NULL, SW_SHOWMAXIMIZED);
				}
				if (BeginSettingsTable("##tbl_backup"))
				{
					RowLabel(Language::ServerAddress.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputInfo.LText = SetWebDav_url;
					ImGui::InputText("##webdav_url", SetWebDav_url, IM_ARRAYSIZE(SetWebDav_url), flags, &InputKeyEvent, &InputInfo);
					RowLabel(Language::Account.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputInfo.LText = SetWebDav_username;
					ImGui::InputText("##webdav_username", SetWebDav_username, IM_ARRAYSIZE(SetWebDav_username), flags, &InputKeyEvent, &InputInfo);
					RowLabel(Language::SecretKey.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputInfo.LText = SetWebDav_password;
					ImGui::InputText("##webdav_password", SetWebDav_password, IM_ARRAYSIZE(SetWebDav_password), flags, &InputKeyEvent, &InputInfo);
					RowLabel(Language::ApplyName.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputInfo.LText = SetWebDav_WebFile;
					ImGui::InputText("##webdav_webfile", SetWebDav_WebFile, IM_ARRAYSIZE(SetWebDav_WebFile), flags, &InputKeyEvent, &InputInfo);
					ImGui::EndTable();
				}

				ImGui::Checkbox("Opcode/", &LDirectory_Opcode);
				ImGui::SameLine();
				ImGui::Checkbox("Language/", &LDirectory_Language);
				ImGui::SameLine();
				ImGui::Checkbox("TessData/", &LDirectory_TessData);
				ImGui::SameLine();
				ImGui::Checkbox("TTF/", &LDirectory_TTF);
				ImGui::SameLine();
				ImGui::Text(Language::BackupsFolder.c_str());

				if (ImGui::Button(Language::Backups.c_str())) {
					
					// 获取当前时间的时间戳
					std::time_t now = std::time(nullptr);

					// 使用本地时间进行格式化
					std::tm* localTime = std::localtime(&now);

					// 获取年份、月份和时间
					int year = localTime->tm_year + 1900;  // 年份需要加上 1900
					int month = localTime->tm_mon + 1;     // 月份从 0 开始，需要加上 1
					int day = localTime->tm_mday;           // 当月的第几天
					int hour = localTime->tm_hour;          // 小时
					int minute = localTime->tm_min;         // 分钟
					int second = localTime->tm_sec;         // 秒钟

					char computerName[MAX_COMPUTERNAME_LENGTH + 1];
					DWORD size = sizeof(computerName);

					GetComputerNameA(computerName, &size);

					std::string BackupsName = std::string(computerName) + "_" + toString(year) + "_" + toString(month) + "_" + toString(day)
						/* + "_" + toString(hour) + "." + toString(minute) + "." + toString(second) */ ;
					
					std::cout << BackupsName << std::endl;

					if (!WebDav_Directory(SetWebDav_WebFile, BackupsName)) {
						WebDav_CreateFolder(BackupsName);
					}

					WebDav_Upload("./Data.ini", BackupsName);

					char path_exe[MAX_PATH];
					GetModuleFileName(NULL, path_exe, MAX_PATH);
					std::string Exename = path_exe;
					for (int i = Exename.size() - 1; i >= 0; i--)
					{
						if (Exename[i] == '\\') {
							Exename = Exename.substr(0, i + 1);
							break;
						}
					}
					if (LDirectory_Opcode)WebDav_UploadDirectory(Exename + "Opcode\\", BackupsName, "Opcode");
					if (LDirectory_Language)WebDav_UploadDirectory(Exename + "Language\\", BackupsName, "Language");
					if (LDirectory_TessData)WebDav_UploadDirectory(Exename + "TessData\\", BackupsName, "TessData");
					if (LDirectory_TTF)WebDav_UploadDirectory(Exename + "TTF\\", BackupsName, "TTF");
				}
				ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f);
				if (ImGui::Button(RecoveryWindow ? Language::Return.c_str() : Language::Recovery.c_str())) {
					if (RecoveryWindow) {
						RecoveryWindow = false;
					}
					else {
						RecoveryWindow = true;
						RecoveryList = WebDav_List(Variable::WebDav_WebFile + "/");
						RecoveryIndex = 0;
						RecoveryChoice = 0;

						Variable::WebDav_url = SetWebDav_url;
						Variable::WebDav_username = SetWebDav_username;
						Variable::WebDav_password = SetWebDav_password;
						Variable::WebDav_WebFile = SetWebDav_WebFile;
					}
				}

				if (RecoveryWindow && (RecoveryList.size() != 0))
				{
					if (ImGui::BeginCombo(Language::RecoveryList.c_str(), RecoveryList[RecoveryIndex].c_str(), flags))
					{
						for (int n = 0; n < RecoveryList.size(); n++)
						{
							const bool is_selected = (RecoveryIndex == n);
							if (ImGui::Selectable(RecoveryList[n].c_str(), is_selected))
								RecoveryIndex = n;
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					if (RecoveryChoice == 0) {
						ImGui::SameLine();
						if (ImGui::Button(Language::Restoration.c_str())) {
							RecoveryChoice = 1;
						}
						ImGui::SameLine();
						if (ImGui::Button(Language::Delete.c_str())) {
							RecoveryChoice = 2;
						}
					}
					else if(RecoveryChoice == 1){
						ImGui::SameLine();
						if (ImGui::Button(Language::Cancel.c_str())) {
							RecoveryChoice = 0;
						}
						ImGui::SameLine();
						if (ImGui::Button(Language::Confirm.c_str())) {
							RecoveryChoice = 0;
							RecoveryWindow = false;
							WebDav_DownloadDirectory(RecoveryList[RecoveryIndex]);

							SetBool = true;
							Variable::ReadFile(iniData);
						}
					}
					else {
						ImGui::SameLine();
						if (ImGui::Button(Language::Confirm.c_str())) {
							RecoveryChoice = 0;
							WebDav_Delete(RecoveryList[RecoveryIndex].c_str());
							RecoveryList[RecoveryIndex] = RecoveryList.back();
							RecoveryList.pop_back();
							RecoveryIndex = 0;
							if (RecoveryList.size() == 0) {
								RecoveryWindow = false;
							}
						}
						ImGui::SameLine();
						if (ImGui::Button(Language::Cancel.c_str())) {
							RecoveryChoice = 0;
						}
					}
					
				}
				break;
			}
			case 7://关于
			{
				ImGui::TextUnformatted(Language::AboutText.c_str());
				ImGui::Separator();
				ImGui::TextUnformatted(VersionText);
				if (ImGui::Button(u8"GitHub")) {
					ShellExecute(NULL, "open", "https://github.com/wuxingwushu/TranslatorKyi", NULL, NULL, SW_SHOWMAXIMIZED);//打开链接
				}
				ImGui::Spacing();
				break;
			}
			default:
			{
				SetPage = 0;
				break;
			}
			}

			//所有输入框都画完了：延迟粘贴（Ctrl+V）统一在这里落到当前输入框里
			InputText();
		}
		ImGui::EndChild();

		//底部固定栏：保存 / GitHub / 关闭 + 「已保存」提示
		ImGui::Separator();
		if (ImGui::Button(Language::Save.c_str(), ImVec2(96.0f, 0.0f))) {
			DoSave();
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"GitHub")) {
			ShellExecute(NULL, "open", "https://github.com/wuxingwushu/TranslatorKyi", NULL, NULL, SW_SHOWMAXIMIZED);//打开链接
		}
		ImGui::SameLine();
		if (ImGui::Button(Language::Close.c_str())) {
			EndDisplayBool = true;
			InterFaceBool = false;
			SetBool = true;
		}
		ImGui::SameLine();
		ImGui::Text(Hitokoto.c_str());
		if ((SavedTime != 0) && ((clock() - SavedTime) < (2 * CLOCKS_PER_SEC))) {
			//右对齐显示「已保存」
			const float SavedWidth = ImGui::CalcTextSize(Language::Saved.c_str()).x;
			ImGui::SameLine(ImGui::GetWindowWidth() - SavedWidth - ImGui::GetStyle().WindowPadding.x);
			ImGui::TextColored(AccentColor, "%s", Language::Saved.c_str());
		}
		//Ctrl+S 也触发保存（焦点在窗口里，含子窗口）
		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsWindowFocused(ImGuiHoveredFlags_ChildWindows) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
			DoSave();
		}

		BeginWindowPosX = (int)ImGui::GetWindowPos().x;
		BeginWindowPosY = (int)ImGui::GetWindowPos().y;
		BeginWindowSizeX = (int)ImGui::GetWindowWidth();
		BeginWindowSizeY = (int)ImGui::GetWindowHeight();
		ImGui::End();
	}

	void ImGuiInterFace::MenuInterface()
	{
		if (MenuBool) {
			MenuBool = false;
			POINT pt = { 0,0 };
			GetCursorPos(&pt);//获取鼠标位置
			//托盘在屏幕右下角，菜单往鼠标左上方长，并保证不跑出屏幕
			const float MenuWidth = 120.0f;
			const float MenuHeight = ImGui::GetTextLineHeightWithSpacing() * 5.0f + 46.0f;
			const ImGuiViewport* Viewport = ImGui::GetMainViewport();
			float PosX = float(pt.x) - MenuWidth;
			float PosY = float(pt.y) - MenuHeight;
			if (PosX < Viewport->WorkPos.x) { PosX = Viewport->WorkPos.x; }
			if (PosY < Viewport->WorkPos.y) { PosY = Viewport->WorkPos.y; }
			ImGui::SetNextWindowPos(ImVec2(PosX, PosY));
			ImGui::SetNextWindowSize(ImVec2(MenuWidth, 0.0f));//宽度固定，高度自适应
		}
		ImGui::Begin("MenuUI", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);//创建窗口

		ImGui::Separator();

		//一行菜单项：整行可点、悬浮整行高亮、左边一条绿色竖条、右边灰色提示
		auto MenuRow = [](const char* Label, const char* Hint, bool Accent) -> bool
		{
			ImGui::PushID(Label);
			const float RowHeight = 26.0f;
			const ImVec2 Pos = ImGui::GetCursorScreenPos();
			const float Width = ImGui::GetContentRegionAvail().x;
			const bool Clicked = ImGui::Selectable("##row", false, ImGuiSelectableFlags_None, ImVec2(Width, RowHeight));
			const bool Hovered = ImGui::IsItemHovered();
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			if (Hovered) {
				DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + RowHeight), ImGui::GetColorU32(ImGuiCol_Header), 4.0f);
			}
			const float TextY = Pos.y + (RowHeight - ImGui::GetTextLineHeight()) * 0.5f;
			DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y + 5.0f), ImVec2(Pos.x + 3.0f, Pos.y + RowHeight - 5.0f),
				Accent ? IM_COL32(72, 199, 124, 255) : IM_COL32(72, 199, 124, 110), 2.0f);
			DrawList->AddText(ImVec2(Pos.x + 12.0f, TextY), ImGui::GetColorU32(ImGuiCol_Text), Label);
			if ((Hint != nullptr) && (Hint[0] != '\0')) {
				const float HintWidth = ImGui::CalcTextSize(Hint).x;
				DrawList->AddText(ImVec2(Pos.x + Width - HintWidth - 10.0f, TextY), ImGui::GetColorU32(ImGuiCol_TextDisabled), Hint);
			}
			ImGui::PopID();
			return Clicked;
		};

		//设置
		if (MenuRow(Language::Set.c_str(), "", true)) {
			SetInterFace(SetUpEnum);
		}
		//翻译源：点一下换下一个（菜单不关，方便连点）
		if (MenuRow(Language::Engine.c_str(), mTranslate->TranslateName[mTranslate->mTranslate], false)) {
			mTranslate->mTranslate++;
			if (mTranslate->mTranslate > Translate::AiTranslate) { mTranslate->mTranslate = 0; }
			//翻译源是模式开关：点一下立刻写回 Data.ini
			Variable::Translate = mTranslate->mTranslate;
			Variable::SaveFile();
		}
		//弹窗通知开关（按钮文字是「接下来要做的操作」）
		if (Variable::PopUpNotificationBool) {
			if (MenuRow(Language::ShutUp.c_str(), "", false)) {
				Variable::PopUpNotificationBool = false;
				Variable::SaveFile();
			}
		}
		else {
			if (MenuRow(Language::Speak.c_str(), "", false)) {
				Variable::PopUpNotificationBool = true;
				Variable::SaveFile();
			}
		}
		//退出
		if (MenuRow(Language::Exit.c_str(), "", false)) {
			exit(0);
		}

		BeginWindowPosX = (int)ImGui::GetWindowPos().x;
		BeginWindowPosY = (int)ImGui::GetWindowPos().y;
		BeginWindowSizeX = (int)ImGui::GetWindowWidth();
		BeginWindowSizeY = (int)ImGui::GetWindowHeight();
		ImGui::End();

		// 获取窗口句柄
		HWND hwnd = FindWindow(NULL, "MenuUI");
		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

	void ImGuiInterFace::HitokotoSentence() {
		static std::string Hitokoto;
		static float lenH = 0;
		if (HitokotoBool) {
			HitokotoBool = false;
			Hitokoto = GetHitokoto();

			if (Variable::HitokotoFontBool) {
				ImVec2 textSize = HitokotoFont->CalcTextSizeA(HitokotoFont->FontSize, FLT_MAX, 0, Hitokoto.c_str());
				lenH = textSize.x + HitokotoFont->FontSize;
			}
			else {
				ImFont* font = ImGui::GetFont();
				ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, Hitokoto.c_str());
				lenH = (textSize.x + font->FontSize) * (Variable::HitokotoFontSize / Variable::FontSize);
			}
			
		}
		
		ImGui::Begin("HitokotoUI", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);//创建窗口
		// 在此处切换字体
		if (Variable::HitokotoFontBool) { ImGui::PushFont(HitokotoFont); }
		else{ ImGui::SetWindowFontScale(Variable::HitokotoFontSize / Variable::FontSize); }
		ImGui::SetWindowPos(ImVec2((Variable::windows_Width * Variable::HitokotoPosX) - lenH, (Variable::windows_Heigth * Variable::HitokotoPosY) - ImGui::GetFont()->FontSize));
		ImGui::SetWindowSize(ImVec2(lenH, -1));
		ImGui::Text(Hitokoto.c_str());
		BeginWindowPosX = 1;
		BeginWindowPosY = 1;
		BeginWindowSizeX = -1;
		BeginWindowSizeY = -1;
		// 在完成绘制后恢复默认字体
		if (Variable::HitokotoFontBool)ImGui::PopFont();
		ImGui::End();
	
		// 获取窗口句柄
		HWND hwnd = FindWindow(NULL, "HitokotoUI");
		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}



	// Helper function to find Vulkan memory type bits. See ImGui_ImplVulkan_MemoryType() in imgui_impl_vulkan.cpp
	uint32_t ImGuiInterFace::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties mem_properties;
		vkGetPhysicalDeviceMemoryProperties(ImGuiVulkanInfo.PhysicalDevice, &mem_properties);

		for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
			if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;

		return 0xFFFFFFFF; // Unable to find memoryType
	}

	// Helper function to load an image with common settings and return a MyTextureData with a VkDescriptorSet as a sort of Vulkan pointer
	bool ImGuiInterFace::LoadTextureFromFile(char* Texturedata, MyTextureData* tex_data)
	{
		if (tex_data->DS != nullptr) {
			RemoveTexture(tex_data);
		}

		TData = Texturedata;

		// Specifying 4 channels forces stb to load the image in RGBA which is an easy format for Vulkan
		tex_data->Channels = 4;
		//尺寸必须用"上一次截图"的那一对：Texturedata 就是 TOOL::screen 交出来的那块缓冲区，
		//按 windows_* 算的话，截图之后用户换了分辨率，这里就会按新尺寸去读旧缓冲区（越界读堆）。
		tex_data->Width = Variable::ScreenShot_Width;
		tex_data->Height = Variable::ScreenShot_Heigth;

		// Calculate allocation size (in number of bytes)
		size_t image_size = (size_t)tex_data->Width * (size_t)tex_data->Height * (size_t)tex_data->Channels;

		VkResult err;

		// Create the Vulkan image.
		{
			VkImageCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.format = VK_FORMAT_R8G8B8A8_UNORM;
			info.extent.width = tex_data->Width;
			info.extent.height = tex_data->Height;
			info.extent.depth = 1;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			err = vkCreateImage(ImGuiVulkanInfo.Device, &info, ImGuiVulkanInfo.Allocator, &tex_data->Image);
			//check_vk_result(err);
			VkMemoryRequirements req;
			vkGetImageMemoryRequirements(ImGuiVulkanInfo.Device, tex_data->Image, &req);
			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = req.size;
			alloc_info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			err = vkAllocateMemory(ImGuiVulkanInfo.Device, &alloc_info, ImGuiVulkanInfo.Allocator, &tex_data->ImageMemory);
			//check_vk_result(err);
			err = vkBindImageMemory(ImGuiVulkanInfo.Device, tex_data->Image, tex_data->ImageMemory, 0);
			//check_vk_result(err);
		}

		// Create the Image View
		{
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = tex_data->Image;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = VK_FORMAT_B8G8R8A8_UNORM;
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.layerCount = 1;
			err = vkCreateImageView(ImGuiVulkanInfo.Device, &info, ImGuiVulkanInfo.Allocator, &tex_data->ImageView);
			//check_vk_result(err);
		}

		// Create Sampler
		{
			VkSamplerCreateInfo sampler_info{};
			sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler_info.magFilter = VK_FILTER_LINEAR;
			sampler_info.minFilter = VK_FILTER_LINEAR;
			sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
			sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_info.minLod = -1000;
			sampler_info.maxLod = 1000;
			sampler_info.maxAnisotropy = 1.0f;
			err = vkCreateSampler(ImGuiVulkanInfo.Device, &sampler_info, ImGuiVulkanInfo.Allocator, &tex_data->Sampler);
			//check_vk_result(err);
		}

		// Create Descriptor Set using ImGUI's implementation
		tex_data->DS = ImGui_ImplVulkan_AddTexture(tex_data->Sampler, tex_data->ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// Create Upload Buffer
		{
			VkBufferCreateInfo buffer_info = {};
			buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_info.size = image_size;
			buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			err = vkCreateBuffer(ImGuiVulkanInfo.Device, &buffer_info, ImGuiVulkanInfo.Allocator, &tex_data->UploadBuffer);
			//check_vk_result(err);
			VkMemoryRequirements req;
			vkGetBufferMemoryRequirements(ImGuiVulkanInfo.Device, tex_data->UploadBuffer, &req);
			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = req.size;
			alloc_info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			err = vkAllocateMemory(ImGuiVulkanInfo.Device, &alloc_info, ImGuiVulkanInfo.Allocator, &tex_data->UploadBufferMemory);
			//check_vk_result(err);
			err = vkBindBufferMemory(ImGuiVulkanInfo.Device, tex_data->UploadBuffer, tex_data->UploadBufferMemory, 0);
			//check_vk_result(err);
		}

		// Upload to Buffer:
		{
			void* map = NULL;
			err = vkMapMemory(ImGuiVulkanInfo.Device, tex_data->UploadBufferMemory, 0, image_size, 0, &map);
			//check_vk_result(err);
			memcpy(map, Texturedata, image_size);
			VkMappedMemoryRange range[1] = {};
			range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			range[0].memory = tex_data->UploadBufferMemory;
			range[0].size = image_size;
			err = vkFlushMappedMemoryRanges(ImGuiVulkanInfo.Device, 1, range);
			//check_vk_result(err);
			vkUnmapMemory(ImGuiVulkanInfo.Device, tex_data->UploadBufferMemory);
		}


		// Create a command buffer that will perform following steps when hit in the command queue.
		// TODO: this works in the example, but may need input if this is an acceptable way to access the pool/create the command buffer.
		VkCommandPool command_pool = ImGuiCommandPoolS[0]->getCommandPool();
		VkCommandBuffer command_buffer;
		{
			VkCommandBufferAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc_info.commandPool = command_pool;
			alloc_info.commandBufferCount = 1;

			err = vkAllocateCommandBuffers(ImGuiVulkanInfo.Device, &alloc_info, &command_buffer);
			//check_vk_result(err);

			VkCommandBufferBeginInfo begin_info = {};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			err = vkBeginCommandBuffer(command_buffer, &begin_info);
			//check_vk_result(err);
		}

		// Copy to Image
		{
			VkImageMemoryBarrier copy_barrier[1] = {};
			copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copy_barrier[0].image = tex_data->Image;
			copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy_barrier[0].subresourceRange.levelCount = 1;
			copy_barrier[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

			VkBufferImageCopy region = {};
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = tex_data->Width;
			region.imageExtent.height = tex_data->Height;
			region.imageExtent.depth = 1;
			vkCmdCopyBufferToImage(command_buffer, tex_data->UploadBuffer, tex_data->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			VkImageMemoryBarrier use_barrier[1] = {};
			use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			use_barrier[0].image = tex_data->Image;
			use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			use_barrier[0].subresourceRange.levelCount = 1;
			use_barrier[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
		}

		// End command buffer
		{
			VkSubmitInfo end_info = {};
			end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			end_info.commandBufferCount = 1;
			end_info.pCommandBuffers = &command_buffer;
			err = vkEndCommandBuffer(command_buffer);
			//check_vk_result(err);
			err = vkQueueSubmit(ImGuiVulkanInfo.Queue, 1, &end_info, VK_NULL_HANDLE);
			//check_vk_result(err);
			err = vkDeviceWaitIdle(ImGuiVulkanInfo.Device);
			//check_vk_result(err);
		}

		return true;
	}

	// Helper function to cleanup an image loaded with LoadTextureFromFile
	void ImGuiInterFace::RemoveTexture(MyTextureData* tex_data)
	{
		vkFreeMemory(ImGuiVulkanInfo.Device, tex_data->UploadBufferMemory, nullptr);
		vkDestroyBuffer(ImGuiVulkanInfo.Device, tex_data->UploadBuffer, nullptr);
		vkDestroySampler(ImGuiVulkanInfo.Device, tex_data->Sampler, nullptr);
		vkDestroyImageView(ImGuiVulkanInfo.Device, tex_data->ImageView, nullptr);
		vkDestroyImage(ImGuiVulkanInfo.Device, tex_data->Image, nullptr);
		vkFreeMemory(ImGuiVulkanInfo.Device, tex_data->ImageMemory, nullptr);
		ImGui_ImplVulkan_RemoveTexture(tex_data->DS);
	}
}
