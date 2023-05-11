#include "Interface.h"
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
		Font_cfg.FontDataOwnedByAtlas = false;
		ImFont* Font;
		if (Variable::FontBool) {
			Font = io.Fonts->AddFontFromFileTTF(Variable::FontFilePath.c_str(), Variable::FontSize, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
		}
		else {
			Font = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, Variable::FontSize, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
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
		bool kai = false;//下一帧是否要更新
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		//ImGui::ShowDemoWindow();
		switch (InterfaceIndexes)
		{
		case 0:
			kai = TranslateInterface();
			break;
		case 1:
			ScreenshotInterface();
			kai = true;//截图实时更新下一帧
			break;
		case 2:
			SetUpInterface();
			break;
		case 3:
			MenuInterface();
			break;
		}
		ImGui::Render();

		return kai;
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
		if (data->HasSelection() && ((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('C') < 0)))
		{
			char selected_text[10000];
			if (data->SelectionEnd > data->SelectionStart) {
				memcpy(selected_text, &data->Buf[data->SelectionStart], (data->SelectionEnd - data->SelectionStart));
				selected_text[data->SelectionEnd + 1] = '\0';//加上终止符
			}
			else {
				memcpy(selected_text, &data->Buf[data->SelectionEnd], (data->SelectionStart - data->SelectionEnd));
				selected_text[data->SelectionStart + 1] = '\0';//加上终止符
			}
			TOOL::CopyToClipboard(TOOL::Utf8ToUnicode(selected_text));
		}
		else if(!ImGui::IsItemDeactivated() && (GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)){
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
			char selected_text[10000];
			int Len = mTextLen - mCursorPos;
			memcpy(selected_text, &eng[mCursorPos], Len);
			ImGui::ClearActiveID();//失去焦点，粘贴的内容才会被保存
			memcpy(&eng[mCursorPos], ClipboardText.c_str(), ClipboardText.size());
			mCursorPos += ClipboardText.size();
			memcpy(&eng[mCursorPos], selected_text, Len);
			//Variable::eng = eng;
			
			//memcpy(eng, Variable::eng.c_str(), Variable::eng.size());
		}
	}

	
	

	bool ImGuiInterFace::TranslateInterface()
	{
		if (TranslateBool) {
			POINT MousePos = { 0,0 };
			GetCursorPos(&MousePos);//获取鼠标位置
			ImGui::SetNextWindowPos({ float(MousePos.x), float(MousePos.y) });//设置窗口生成位置
			TranslateBool = false;
		}
		int BeginWindowPosX, BeginWindowPosY;
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
		if (ImGui::Button(u8"翻译键")) {
			Variable::zhong = mTranslate->TranslateAPI(eng);
			memset(zhong, 0, sizeof(zhong));
			memcpy(zhong, Variable::zhong.c_str(), Variable::zhong.size());
		}
		if (ImGui::Button(u8"From")) {
			ChildWindowBool = !ChildWindowBool;
			WhoBool = true;
		}
		ImGui::EndGroup();
		TranslateInputBool = false;
		ImGui::InputTextMultiline("##zhong", zhong, IM_ARRAYSIZE(zhong), ImVec2(kuangshu, ImGui::GetTextLineHeight() * RowsNumber), flags, MyText);
		ImGui::SameLine();//让一个元素并排
		ImGui::BeginGroup();
		if (ImGui::Button(u8"To")) {
			ChildWindowBool = !ChildWindowBool;
			WhoBool = false;
		}
		//翻译API更换
		if (ImGui::Button(mTranslate->TranslateName[mTranslate->mTranslate])) {
			if (mTranslate->mTranslate) {
				mTranslate->mTranslate = 0;
			}
			else {
				mTranslate->mTranslate = 1;
			}
		}
		ImGui::EndGroup();
		ImGui::SetWindowSize(ImVec2(BeginWindowSizeX, BeginWindowSizeY));
		bool fanbool = false;
		BeginWindowPosX = ImGui::GetWindowPos().x;
		BeginWindowPosY = ImGui::GetWindowPos().y;
		//当鼠标点击时更新窗口大小
		if (GetKeyState(VK_LBUTTON) < 0) {
			BeginWindowSizeX = ImGui::GetWindowWidth();
			BeginWindowSizeY = ImGui::GetWindowHeight();
			WindowRenewBool = true;
		}
		//判断鼠标是否在翻译界面上
		if ((m_io->MousePos.x > BeginWindowPosX) && (m_io->MousePos.y > BeginWindowPosY) && (m_io->MousePos.x < (BeginWindowPosX + BeginWindowSizeX)) && (m_io->MousePos.y < (BeginWindowPosY + BeginWindowSizeY))) {
			TranslateTime = clock();
			fanbool = true;
		}
		else if ((clock() - TranslateTime) > Variable::DisplayTime) {
			EndDisplayBool = true;
			InterFaceBool = false;
			ChildWindowBool = false;
		}
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
			ImGui::SetWindowSize(ImVec2(ImGui::GetTextLineHeightWithSpacing()*5+20, BeginWindowSizeY));
			if ((m_io->MousePos.x > ImGui::GetWindowPos().x) && (m_io->MousePos.y > ImGui::GetWindowPos().y) && (m_io->MousePos.x < (ImGui::GetWindowPos().x + ImGui::GetWindowWidth())) && (m_io->MousePos.y < (ImGui::GetWindowPos().y + ImGui::GetWindowHeight()))) {
				TranslateTime = clock();
				fanbool = true;
			}
			else if ((clock() - TranslateTime) > Variable::DisplayTime) {
				EndDisplayBool = true;
				InterFaceBool = false;
				ChildWindowBool = false;
			}
			else if ((GetKeyState(VK_LBUTTON) < 0) || (GetKeyState(VK_RBUTTON) < 0)) {//鼠标点击窗口之外的地方关闭窗口
				ChildWindowBool = false;
			}
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
		return fanbool;
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
				Variable::zhong = mTranslate->TranslateAPI(Variable::eng);

				memset(eng, 0, sizeof(eng));
				memset(zhong, 0, sizeof(zhong));
				memcpy(eng, Variable::eng.c_str(), Variable::eng.size());
				memcpy(zhong, Variable::zhong.c_str(), Variable::zhong.size());

				x = 0;
				w = 0;
				y = 0;
				h = 0;

				//开启翻译
				TranslateTime = clock();
				InterfaceIndexes = 0;
				ScreenshotBool = true;
				TranslateBool = true;
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
			IM_COL32(0, 0, 0, 100));

		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(x + w, 0),
			ImVec2(Variable::windows_Width, 0),
			ImVec2(Variable::windows_Width, y + h),
			ImVec2(x + w, y + h),
			//颜色
			IM_COL32(0, 0, 0, 100));

		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(x, y + h),
			ImVec2(Variable::windows_Width, y + h),
			ImVec2(Variable::windows_Width, Variable::windows_Heigth),
			ImVec2(x, Variable::windows_Heigth),
			//颜色
			IM_COL32(0, 0, 0, 100));

		draw_list->AddQuadFilled(
			//矩形框的四个点（顺时针）
			ImVec2(0, y),
			ImVec2(x, y),
			ImVec2(x, Variable::windows_Heigth),
			ImVec2(0, Variable::windows_Heigth),
			//颜色
			IM_COL32(0, 0, 0, 100));


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
		bool FFO;
		char* LText;
	};
	InputBoxInfo InputInfo;

	void InputText() {
		if (!((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)) && InputInfo.FFO) {
			std::string ClipboardText = TOOL::UnicodeToUtf8(TOOL::ClipboardTochar());
			char selected_text[10000];
			int Len = InputInfo.Len - InputInfo.Pos;
			memcpy(selected_text, &InputInfo.Text[InputInfo.Pos], Len);
			ImGui::ClearActiveID();//失去焦点，粘贴的内容才会被保存
			memcpy(&InputInfo.Text[InputInfo.Pos], ClipboardText.c_str(), ClipboardText.size());
			InputInfo.Pos += ClipboardText.size();
			memcpy(&InputInfo.Text[InputInfo.Pos], selected_text, Len);
			InputInfo.FFO = false;
		}
	}

	int InputKeyEvent(ImGuiInputTextCallbackData* data) {
		InputBoxInfo* i = static_cast<InputBoxInfo*>(data->UserData);
		if ((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)) {
			InputInfo.Text = InputInfo.LText;
			InputInfo.Len = data->BufTextLen;
			InputInfo.Pos = data->CursorPos;
			InputInfo.FFO = true;
		}
		return 0;
	}


	void ImGuiInterFace::SetUpInterface()
	{
		static std::string Hitokoto;

		static char SetBaiduID[128];
		static char SetBaiduKey[128];
		static char SetYoudaoID[128];
		static char SetYoudaoKey[128];

		static char SetScreenshotkey[2];
		static char SetChoicekey[2];
		static char SetReplacekey[2];

		static int ModelIndex;
		static std::vector<std::string> ModelS;

		static int FontIndex;
		static int MyFontSize;
		static std::vector<std::string> FontS;

		static float LFontSize;
		static bool LFontBool;

		if (SetBool) {
			SetBool = false;

			Hitokoto = GetHitokoto();

			memcpy(SetBaiduID, Variable::BaiduAppid.c_str(), Variable::BaiduAppid.size());
			memcpy(SetBaiduKey, Variable::BaiduSecret_key.c_str(), Variable::BaiduSecret_key.size());
			memcpy(SetYoudaoID, Variable::YoudaoAppid.c_str(), Variable::YoudaoAppid.size());
			memcpy(SetYoudaoKey, Variable::YoudaoSecret_key.c_str(), Variable::YoudaoSecret_key.size());

			memcpy(SetScreenshotkey, Variable::Screenshotkey.c_str(), 1);
			memcpy(SetChoicekey, Variable::Choicekey.c_str(), 1);
			memcpy(SetReplacekey, Variable::Replacekey.c_str(), 1);

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

			//ImGui::InputTextWithHint("input text (w/ hint)", "enter text here", str1, IM_ARRAYSIZE(str1));
		}

		
		ImGui::Begin("SetUI", &SetBool, ImGuiWindowFlags_NoTitleBar);
		ImGui::Text(u8"翻译密钥");
		InputInfo.LText = SetBaiduID;
		ImGui::InputText(u8"百度ID", SetBaiduID, IM_ARRAYSIZE(SetBaiduID), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetBaiduKey;
		ImGui::InputText(u8"百度Key", SetBaiduKey, IM_ARRAYSIZE(SetBaiduKey), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetYoudaoID;
		ImGui::InputText(u8"有道ID", SetYoudaoID, IM_ARRAYSIZE(SetYoudaoID), flags, &InputKeyEvent, &InputInfo);
		InputInfo.LText = SetYoudaoKey;
		ImGui::InputText(u8"有道Key", SetYoudaoKey, IM_ARRAYSIZE(SetYoudaoKey), flags, &InputKeyEvent, &InputInfo);
		ImGui::Text(u8"快捷键");
		ImGui::InputText(u8"截图翻译", SetScreenshotkey, IM_ARRAYSIZE(SetScreenshotkey));
		ImGui::InputText(u8"选择翻译", SetChoicekey, IM_ARRAYSIZE(SetChoicekey));
		ImGui::InputText(u8"替换翻译", SetReplacekey, IM_ARRAYSIZE(SetReplacekey));
		ImGui::Text(u8"设置");
		ImGui::Checkbox("开机启动", &Variable::Startup);
		ImGui::InputInt(u8"滞留时间（ms）", &Variable::DisplayTime);
		ImGui::InputFloat(u8"字体大小", &Variable::FontSize, 0.1f, 1.0f);
		InputText();

		if (ModelS.size() != 0) {
			if (ImGui::BeginCombo(u8"Tesseract模型", ModelS[ModelIndex].c_str(), flags))
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
			ImGui::Text(u8"你没有Tesseract模型，模型放在当前程序位置的TessData");
		}
		ImGui::Checkbox("使用TTF字体", &Variable::FontBool);
		ImGui::SameLine();
		if (ImGui::Button(u8"TTF文件夹")) {
			TCHAR buffer[MAX_PATH] = { 0 };
			GetCurrentDirectory(MAX_PATH, buffer);//获取启动器路径
			//拼接为绝对路径
			ShellExecute(NULL, "open", (std::string(buffer) + "\\TTF").c_str(), NULL, NULL, SW_SHOWDEFAULT);//打开文件夹
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"TessData文件夹")) {
			TCHAR buffer[MAX_PATH] = { 0 };
			GetCurrentDirectory(MAX_PATH, buffer);//获取启动器路径
			//拼接为绝对路径
			ShellExecute(NULL, "open", (std::string(buffer) + "\\TessData").c_str(), NULL, NULL, SW_SHOWDEFAULT);//打开文件夹
		}
		if (Variable::FontBool) {
			if (FontS.size() != 0) {
				if (ImGui::BeginCombo(u8"TTF字体", FontS[FontIndex].c_str(), flags))
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
				ImGui::Text(u8"你没有TTF字体，字体放在当前程序位置的TTF");
			}
		}

		if (ImGui::BeginCombo(u8"替换语言", Variable::BaiduitemsName[Variable::ReplaceLanguage].c_str(), flags))
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


		if (ImGui::Button(u8"保存")) {
			Variable::BaiduAppid = SetBaiduID;
			Variable::BaiduSecret_key = SetBaiduKey;
			Variable::YoudaoAppid = SetYoudaoID;
			Variable::YoudaoSecret_key = SetYoudaoKey;

			//转为大写
			Variable::Screenshotkey = toupper(SetScreenshotkey[0]);
			Variable::Choicekey = toupper(SetChoicekey[0]);
			Variable::Replacekey = toupper(SetReplacekey[0]);

			if (ModelS.size() != 0) {
				if (Variable::Model != ModelS[ModelIndex]) {
					mTesseract->~Tesseract();
					mTesseract->Tesseract::Tesseract(ModelS[ModelIndex].c_str());
				}
				Variable::Model = ModelS[ModelIndex];
			}

			bool updata = false;//判断是否要重启软件
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

			TOOL::SetModifyRegedit("TranslatorKyi", Variable::Startup);

			
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
			SetBool = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"GitHub")) {
			ShellExecute(NULL, "open", "https://github.com/wuxingwushu/TranslatorKyi", NULL, NULL, SW_SHOWMAXIMIZED);//打开链接
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"关闭")) {
			EndDisplayBool = true;
			SetBool = true;
		}

		ImGui::Text(Hitokoto.c_str());

		ImGui::End();
	}

	void ImGuiInterFace::MenuInterface()
	{
		//设置生成位置在鼠标的右上角
		if (MenuBool) {
			static POINT pt = { 0,0 };
			GetCursorPos(&pt);//获取鼠标位置
			ImGui::SetNextWindowPos({ float(pt.x), float(pt.y) - (ImGui::GetTextLineHeightWithSpacing() * 2) - 24 });//设置窗口生成位置
			MenuBool = false;
		}
		ImGui::Begin("MenuUI", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);//创建窗口
		//设置按键
		if (ImGui::Button(u8"设置")) {
			SetInterFace(2);
		}
		if (ImGui::Button(u8"退出")) {
			exit(0);
		}
		bool fanbool = false;
		if ((m_io->MousePos.x > ImGui::GetWindowPos().x) && (m_io->MousePos.y > ImGui::GetWindowPos().y) && (m_io->MousePos.x < (ImGui::GetWindowPos().x + ImGui::GetWindowWidth())) && (m_io->MousePos.y < (ImGui::GetWindowPos().y + ImGui::GetWindowHeight()))) {
			TranslateTime = clock();
			fanbool = true;
		}
		else if ((clock() - TranslateTime) > Variable::DisplayTime) {
			EndDisplayBool = true;
			InterFaceBool = false;
		}
		else if ((GetKeyState(VK_LBUTTON) < 0) || (GetKeyState(VK_RBUTTON) < 0)) {//鼠标点击窗口之外的地方关闭窗口
			EndDisplayBool = true;
			InterFaceBool = false;
		}
		ImGui::End();
		// 获取窗口句柄
		HWND hwnd = FindWindow(NULL, "MenuUI");
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
		tex_data->Width = Variable::windows_Width;
		tex_data->Height = Variable::windows_Heigth;

		// Calculate allocation size (in number of bytes)
		size_t image_size = tex_data->Width * tex_data->Height * tex_data->Channels;

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
