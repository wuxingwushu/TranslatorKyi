#include "Interface.h"
#include "../AngelScript/AngelScriptCode.h"
#include "../Function/WebDav.h"

namespace GAME {
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
		ImFontConfig Font_cfg;
		Font_cfg.OversampleH = 1;
		Font_cfg.FontDataOwnedByAtlas = false;
		ImFont* Font;
		if (Variable::FontBool) {
			Font = io.Fonts->AddFontFromFileTTF(Variable::FontFilePath.c_str(), Variable::FontSize, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
		}
		else {
			Font = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, Variable::FontSize, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
		}
		if (Variable::HitokotoFontBool) {
			if (!Variable::HitokotoTTFBool) {
				HitokotoFont = io.Fonts->AddFontFromFileTTF(Variable::HitokotoFont.c_str(), Variable::HitokotoFontSize, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
			}
			else {
				HitokotoFont = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, Variable::HitokotoFontSize, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
			}
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


		//ImGui 风格设置
		auto Color = Style.Colors;


		Style.ChildRounding = 0.0f;
		Style.FrameRounding = 0.0f;//是否圆润按键

		Color[ImGuiCol_Button] = ImColor(10, 105, 56, 255);//按键颜色
		Color[ImGuiCol_ButtonHovered] = ImColor(30, 125, 76, 255);//鼠标悬停颜色
		Color[ImGuiCol_ButtonActive] = ImColor(0, 95, 46, 255);//鼠标点击颜色

		Color[ImGuiCol_FrameBg] = ImColor(54, 54, 54, 150);
		Color[ImGuiCol_FrameBgActive] = ImColor(42, 42, 42, 150);
		Color[ImGuiCol_FrameBgHovered] = ImColor(100, 100, 100, 150);

		Color[ImGuiCol_CheckMark] = ImColor(10, 105, 56, 255);

		Color[ImGuiCol_SliderGrab] = ImColor(10, 105, 56, 255);
		Color[ImGuiCol_SliderGrabActive] = ImColor(0, 95, 46, 255);

		Color[ImGuiCol_Header] = ImColor(10, 105, 56, 255);
		Color[ImGuiCol_HeaderHovered] = ImColor(30, 125, 76, 255);
		Color[ImGuiCol_HeaderActive] = ImColor(0, 95, 46, 255);

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

	
	

	void ImGuiInterFace::TranslateInterface()
	{
		if (TranslateBool) {
			TranslateBool = false;
			POINT MousePos = { 0,0 };
			GetCursorPos(&MousePos);//获取鼠标位置
			ImGui::SetNextWindowPos({ float(MousePos.x), float(MousePos.y) });//设置窗口生成位置
			Variable::WrapSize = kuangshu / int(Variable::FontSize);
		}
		ImGui::Begin(u8"TranslateUI", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);//创建窗口
		
		if (InputCursorBool) {
			// 在窗口打开时自动将焦点设置到多行文本输入框上
			ImGui::SetKeyboardFocusHere();
		}
		TranslateInputBool = true;
		ImGui::InputTextMultiline("##eng", eng, IM_ARRAYSIZE(eng), ImVec2(kuangshu, ImGui::GetTextLineHeight() * RowsNumber), flags, MyText);
		if (InputCursorBool) {
			InputTextMultilineText();//将剪贴板内容粘贴到输入光标位置
		}
		ImGui::SameLine();//让一个元素并排
		ImGui::BeginGroup();
		if (ImGui::Button(Language::TranslationKey.c_str())) {
			Variable::zhong = mTranslate->TranslateAPI(eng);
			//zhong 是 1MB 的定长缓冲区，长文翻译结果直接 memcpy 会写爆且结尾没有 '\0'
			memset(zhong, 0, sizeof(zhong));
			TOOL::CopyToBuffer(zhong, sizeof(zhong), Variable::zhong);
		}
		if (ImGui::Button(Language::From.c_str())) {
			ChildWindowBool = !ChildWindowBool;
			WhoBool = true;
		}
		ImGui::EndGroup();
		TranslateInputBool = false;
		ImGui::InputTextMultiline("##zhong", zhong, IM_ARRAYSIZE(zhong), ImVec2(kuangshu, ImGui::GetTextLineHeight() * RowsNumber), flags, MyText);
		ImGui::SameLine();//让一个元素并排
		ImGui::BeginGroup();
		if (ImGui::Button(Language::To.c_str())) {
			ChildWindowBool = !ChildWindowBool;
			WhoBool = false;
		}
		//翻译API更换
		if (ImGui::Button(mTranslate->TranslateName[mTranslate->mTranslate])) {
			mTranslate->mTranslate++;
			if (mTranslate->mTranslate > 2) {
				mTranslate->mTranslate = 0;
			}
		}
		ImGui::EndGroup();
		ImGui::SetWindowSize(ImVec2(BeginWindowSizeX, BeginWindowSizeY));
		BeginWindowPosX = ImGui::GetWindowPos().x;
		BeginWindowPosY = ImGui::GetWindowPos().y;
		//当鼠标点击时更新窗口大小
		if (GetKeyState(VK_LBUTTON) < 0) {
			BeginWindowSizeX = ImGui::GetWindowWidth();
			BeginWindowSizeY = ImGui::GetWindowHeight();
			WindowRenewBool = true;
		}
		//判断鼠标是否在翻译界面上
		/*if ((m_io->MousePos.x > BeginWindowPosX) && (m_io->MousePos.y > BeginWindowPosY) && (m_io->MousePos.x < (BeginWindowPosX + BeginWindowSizeX)) && (m_io->MousePos.y < (BeginWindowPosY + BeginWindowSizeY))) {
			TranslateTime = clock();
			fanbool = true;
		}
		else if ((clock() - TranslateTime) > Variable::DisplayTime) {
			EndDisplayBool = true;
			InterFaceBool = false;
			ChildWindowBool = false;
		}*/
		ImGui::End();

		
		if (ChildWindowBool) {
			ImGui::Begin(u8"ToFromUI", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
			ImGui::SetWindowPos(ImVec2(BeginWindowPosX + BeginWindowSizeX, BeginWindowPosY));
			if (WhoBool) {
				if (ImGui::BeginListBox("From", ImVec2(-FLT_MIN, RowsNumber * 2 * (ImGui::GetTextLineHeightWithSpacing() - 4))))
				{
					for (int n = 0; n < Variable::Baiduitems.size(); n++)
					{
						const bool is_selected = (mTranslate->mFrom == n);
						if (ImGui::Selectable(Variable::BaiduitemsName[n].c_str(), is_selected))
							mTranslate->mFrom = n;
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndListBox();
				}
			}
			else {
				if (ImGui::BeginListBox("To", ImVec2(-FLT_MIN, RowsNumber * 2 * (ImGui::GetTextLineHeightWithSpacing() - 4))))
				{
					for (int n = 0; n < Variable::Baiduitems.size() - 1; n++)
					{
						const bool is_selected = (mTranslate->mTo == n + 1);
						if (ImGui::Selectable(Variable::BaiduitemsName[n + 1].c_str(), is_selected))
							mTranslate->mTo = n + 1;
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndListBox();
				}
			}
			BeginWindowSizeX_2 = ImGui::GetTextLineHeightWithSpacing() * 5 + 20;
			ImGui::SetWindowSize(ImVec2(BeginWindowSizeX_2, BeginWindowSizeY));
			//if ((m_io->MousePos.x > ImGui::GetWindowPos().x) && (m_io->MousePos.y > ImGui::GetWindowPos().y) && (m_io->MousePos.x < (ImGui::GetWindowPos().x + ImGui::GetWindowWidth())) && (m_io->MousePos.y < (ImGui::GetWindowPos().y + ImGui::GetWindowHeight()))) {
			//	TranslateTime = clock();
			//	fanbool = true;
			//}
			//else if ((clock() - TranslateTime) > Variable::DisplayTime) {
			//	EndDisplayBool = true;
			//	InterFaceBool = false;
			//	ChildWindowBool = false;
			//}
			//else if ((GetKeyState(VK_LBUTTON) < 0) || (GetKeyState(VK_RBUTTON) < 0)) {//鼠标点击窗口之外的地方关闭窗口
			//	ChildWindowBool = false;
			//}
			ImGui::End();
		}

		//std::cout << "ImGui::GetTextLineHeight()" << ImGui::GetTextLineHeight() << std::endl;
		//std::cout << "ImGui::GetTextLineHeightWithSpacing()" << ImGui::GetTextLineHeightWithSpacing() << std::endl;

		//当鼠标松开时 将窗口大小更新到合适大小
		if ((GetKeyState(VK_LBUTTON) >= 0) && WindowRenewBool) {
			WindowRenewBool = false;
			RowsNumber = (BeginWindowSizeY - ImGui::GetTextLineHeightWithSpacing()) / (int(ImGui::GetTextLineHeight()) * 2);
			if (RowsNumber < 3) {
				RowsNumber = 3;
			}
			BeginWindowSizeY = (RowsNumber * int(ImGui::GetTextLineHeight()) * 2) + ImGui::GetTextLineHeightWithSpacing();
			kuangshu = BeginWindowSizeX - (ImGui::GetTextLineHeightWithSpacing() * 3 + 16);

			if (kuangshu < ImGui::GetTextLineHeightWithSpacing()) {
				RowsNumber = ImGui::GetTextLineHeightWithSpacing();
				BeginWindowSizeX = ImGui::GetTextLineHeightWithSpacing() * 4 + 16;
			}
			UpdateTheScreen = 2;//强制在刷新一次画面
		}
		

		// 获取窗口句柄
		HWND hwnd = FindWindow(NULL, "TranslateUI");
		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
		hwnd = FindWindow(NULL, "ToFromUI");
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

				Variable::eng = mTesseract->IdentifyPictures(x, y, w, h, TData);

				
				if (AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->GetOpenBool() && Variable::ScriptBool) {
					//执行脚本
					AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->RunFunction(
						AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->ScreenshotFunction
					);
				}
				else
				{
					Variable::zhong = mTranslate->TranslateAPI(Variable::eng);//翻译内容
				}


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

		
		ImGui::Begin("SetUI", &SetBool, ImGuiWindowFlags_NoTitleBar);

		if (ImGui::Button(Language::jianguoyunWebDav.c_str())) {
			ShellExecute(NULL, "open", "https://www.jianguoyun.com/", NULL, NULL, SW_SHOWMAXIMIZED);
		}
		InputInfo.LText = SetWebDav_url;
		ImGui::InputText(Language::ServerAddress.c_str(), SetWebDav_url, IM_ARRAYSIZE(SetWebDav_url), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetWebDav_username;
		ImGui::InputText(Language::Account.c_str(), SetWebDav_username, IM_ARRAYSIZE(SetWebDav_username), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetWebDav_password;
		ImGui::InputText(Language::SecretKey.c_str(), SetWebDav_password, IM_ARRAYSIZE(SetWebDav_password), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetWebDav_WebFile;
		ImGui::InputText(Language::ApplyName.c_str(), SetWebDav_WebFile, IM_ARRAYSIZE(SetWebDav_WebFile), flags, &InputKeyEvent, &InputInfo);

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
		

		ImGui::Text(Language::AccountKey.c_str());
		InputInfo.LText = SetBaiduID;
		ImGui::InputText(Language::BaiduID.c_str(), SetBaiduID, IM_ARRAYSIZE(SetBaiduID), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetBaiduKey;
		ImGui::InputText(Language::BaiduKey.c_str(), SetBaiduKey, IM_ARRAYSIZE(SetBaiduKey), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetYoudaoID;
		ImGui::InputText(Language::YoudaoID.c_str(), SetYoudaoID, IM_ARRAYSIZE(SetYoudaoID), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetYoudaoKey;
		ImGui::InputText(Language::YoudaoKey.c_str(), SetYoudaoKey, IM_ARRAYSIZE(SetYoudaoKey), flags, &InputKeyEvent, &InputInfo);
		ImGui::Text(Language::ShortcutKeys.c_str());
		if (ImGui::BeginCombo(Language::KeyCombination.c_str(), CharMakeUpS[SetMakeUp], flags))
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
		ImGui::InputText(Language::ScreenshotTranslation.c_str(), SetScreenshotkey, IM_ARRAYSIZE(SetScreenshotkey));
		ImGui::InputText(Language::SelectTranslation.c_str(), SetChoicekey, IM_ARRAYSIZE(SetChoicekey));
		ImGui::InputText(Language::ReplaceTranslation.c_str(), SetReplacekey, IM_ARRAYSIZE(SetReplacekey));
		ImGui::Text(Language::Set.c_str());
		ImGui::Checkbox(Language::Startup.c_str(), &Variable::Startup);
		ImGui::InputInt(Language::ResidenceTime.c_str(), &Variable::DisplayTime);
		ImGui::InputFloat(Language::FontSize.c_str(), &Variable::FontSize, 0.1f, 1.0f);
		InputText();

		if (ModelS.size() != 0) {
			if (ImGui::BeginCombo(Language::TesseractModel.c_str(), ModelS[ModelIndex].c_str(), flags))
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
		ImGui::Checkbox(Language::UseTTF_Typeface.c_str(), &Variable::FontBool);
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
		if (Variable::FontBool) {
			if (FontS.size() != 0) {
				if (ImGui::BeginCombo(Language::TTF_Typeface.c_str(), FontS[FontIndex].c_str(), flags))
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

		if (ImGui::BeginCombo(Language::ReplaceLanguage.c_str(), Variable::BaiduitemsName[Variable::ReplaceLanguage].c_str(), flags))
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


		if (ImGui::BeginCombo(Language::Language.c_str(), LanguageS[LanguageIndex].c_str(), flags))
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

		//渲染设备选择。下拉框的内容每帧重建：前三项固定，后面每项对应一台识别到的设备。
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
			if (ImGui::BeginCombo(Language::RenderDevice.c_str(), RenderDeviceItems[SetVulkanDeviceMode], flags))
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
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(Language::RenderDeviceHelp1.c_str());
				ImGui::TextUnformatted(Language::RenderDeviceHelp2.c_str());
				ImGui::TextUnformatted(Language::RenderDeviceHelp3.c_str());
				ImGui::TextUnformatted(Language::RenderDeviceHelp4.c_str());
				ImGui::TextUnformatted(Language::RenderDeviceHelp5.c_str());
				ImGui::TextUnformatted(Language::RenderDeviceHelp6.c_str());
				ImGui::TextUnformatted(Language::RenderDeviceHelp7.c_str());
				ImGui::EndTooltip();
			}
		}

		ImGui::ColorEdit4(Language::ScreenshotColor.c_str(), (float*)&ScreenshotColor, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);

		
		ImGui::Checkbox((Language::Script + " ").c_str(), &LScriptBool);//同名会出现冲突
		if (LScriptBool) {
			if (ScriptS.size() != 0) {
				if (ImGui::BeginCombo(Language::Script.c_str(), ScriptS[ScriptIndex].c_str(), flags))
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

		ImGui::Checkbox(Language::PopUpNotification.c_str(), &SetPopUpNotificationBool);
		if (SetPopUpNotificationBool) {
			ImGui::SameLine();
			ImGui::Checkbox(Language::IndependentTypeface.c_str(), &SetHitokotoFontBool);
			if (SetHitokotoFontBool) {
				ImGui::SameLine();
				ImGui::Checkbox(Language::InternalFontPattern.c_str(), &SetHitokotoTTFBool);
				if (!SetHitokotoTTFBool) {
					if (FontS.size() != 0) {
						if (ImGui::BeginCombo(Language::TTF_Typeface.c_str(), FontS[SetHitokotoFontIndex].c_str(), flags))
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
			}
			ImGui::SliderFloat(Language::PositionX.c_str(), &SetHitokotoPosX, 0.0f, 1.0f);
			ImGui::SliderFloat(Language::PositionY.c_str(), &SetHitokotoPosY, 0.0f, 1.0f);
			ImGui::InputInt(Language::HitokotoTimeInterval.c_str(), &SetHitokotoTimeInterval);
			ImGui::InputInt(Language::HitokotoDisplayDuration.c_str(), &SetHitokotoDisplayDuration);
			ImGui::InputFloat(Language::HitokotoFontSize.c_str(), &SetHitokotoFontSize);
		}
		
		if (ImGui::Button(Language::Save.c_str())) {
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

		ImGui::Text(Hitokoto.c_str());

		BeginWindowPosX = ImGui::GetWindowPos().x;
		BeginWindowPosY = ImGui::GetWindowPos().y;
		BeginWindowSizeX = ImGui::GetWindowWidth();
		BeginWindowSizeY = ImGui::GetWindowHeight();
		ImGui::End();
	}

	void ImGuiInterFace::MenuInterface()
	{
		//设置生成位置在鼠标的右上角
		if (MenuBool) {
			static POINT pt = { 0,0 };
			GetCursorPos(&pt);//获取鼠标位置
			ImGui::SetNextWindowPos({ float(pt.x), float(pt.y) - (ImGui::GetTextLineHeightWithSpacing() * 3) - 24 });//设置窗口生成位置
			MenuBool = false;
		}
		ImGui::Begin("MenuUI", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);//创建窗口
		//设置按键
		if (ImGui::Button(Language::Set.c_str())) {
			SetInterFace(SetUpEnum);
		}
		if (Variable::PopUpNotificationBool) {
			if (ImGui::Button(Language::ShutUp.c_str())) {
				Variable::PopUpNotificationBool = false;
				Variable::SaveFile();
			}
		}
		else {
			if (ImGui::Button(Language::Speak.c_str())) {
				Variable::PopUpNotificationBool = true;
				Variable::SaveFile();
			}
		}
		if (ImGui::Button(Language::Exit.c_str())) {
			exit(0);
		}
		BeginWindowPosX = ImGui::GetWindowPos().x;
		BeginWindowPosY = ImGui::GetWindowPos().y;
		BeginWindowSizeX = ImGui::GetWindowWidth();
		BeginWindowSizeY = ImGui::GetWindowHeight();
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
