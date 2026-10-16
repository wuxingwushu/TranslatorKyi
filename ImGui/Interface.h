#pragma once
#include "GUI.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/device.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"
#include "../Function/tesseract.h"
#include "../Function/Translate.h"
#include <filesystem>
#include <vector>
#include "../Function/Hitokoto.h"

namespace GAME {
	enum InterFaceEnum
	{
		No_Enum = 0,
		TranslateEnum,
		ScreenshotEnum,
		SetUpEnum,
		MenuEnum,
		HitokotoEnum
	};

	class ImGuiInterFace
	{
	public:
		ImGuiInterFace(
			VulKan::Device* device, 
			VulKan::Window* Win, 
			ImGui_ImplVulkan_InitInfo Info, 
			VulKan::RenderPass* Pass,
			VulKan::CommandBuffer* commandbuffer,
			int FormatCount
		);

		~ImGuiInterFace();

		bool InterFace();

		bool GetInterFaceBool() {
			return InterFaceBool;
		}

		void SetInterFaceBool(bool Bool) {
			InterFaceBool = Bool;
			if (!InterFaceBool) {
				ImGui_ImplVulkan_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();
				ImGui::Render();

				if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					//如果开启 Viewports 模式那么每次 ImGui::Render() 或 ImGui::EndFrame() 后都有调用下面两个函数
					ImGui::UpdatePlatformWindows();
					ImGui::RenderPlatformWindowsDefault();
				}
			}
		}

		InterFaceEnum GetInterFaceEnum() {
			return InterfaceIndexes;
		}

		int GetInterFaceEnumTime() {
			switch (InterfaceIndexes)
			{
			case GAME::TranslateEnum:
				return Variable::DisplayTime;
			case GAME::MenuEnum:
				return 5000;
			case GAME::HitokotoEnum:
				//一言的显示时长也读设置（Data.ini 的 HitokotoDisplayDuration，单位毫秒），以前写死 10000。
				//下限 1000：DoYouWantToUpdateTheScreen() 见到 time == 0 就当成「不自动关闭」，
				//填 0 会让弹窗一直挂在屏幕上。
				return (Variable::HitokotoDisplayDuration > 1000) ? Variable::HitokotoDisplayDuration : 1000;
			default:
				return 0;
			}
		}

		void SetInterFace(InterFaceEnum fi) {
			InterfaceIndexes = fi;
			SetInterFaceBool(true);
			UpdateTheScreen = 2;
			switch (InterfaceIndexes)
			{
			case No_Enum:
				SetInterFaceBool(false);
				break;
			case TranslateEnum:
				TranslateBool = true;
				TranslateWindowClosed = false;//新一轮翻译：用户之前用「X」关掉的窗口可以再弹出来
				TranslateTime = clock();//获取显示时间戳
				break;
			case ScreenshotEnum:
				ScreenshotBool = true;
				break;
			case SetUpEnum:
				SetBool = true;
				break;
			case MenuEnum:
				MenuBool = true;
				TranslateTime = clock();//获取显示时间戳
				break; 
			case HitokotoEnum:
				HitokotoBool = true;
				TranslateTime = clock();//获取显示时间戳
				break;
			default:
				break;
			}
		}

		bool EndDisplayBool = false;//结束显示开关（给外界一个信号，结束显示）

		//菜单里点了「退出」：置位后主循环（application.cpp 的 mainLoop）会像窗口被关闭一样跳出循环，
		//走 cleanUp() → main.cpp 删除窗口/Application 这条正常收尾通道。
		//以前这里直接 exit(0)：堆上的 Application/InterFace/Translate 不会被析构，后台线程
		//（AI 生成 / HTTP / OCR / 脚本）会在进程退出过程中继续跑，可能用到已经半销毁的成员。
		bool ExitRequestBool = false;

		//取出当前这个 ImGui 窗口对应的系统窗口句柄（必须在 ImGui::Begin/End 之间调用）。
		//以前各面板是在 ImGui::End() 之后用 FindWindow(NULL, "xxxUI") 按标题找窗口再置顶：
		//多视口模式下这些系统窗口是 ImGui 自己建的（标题也是 ImGui 设的、还可能被加上序号），
		//FindWindow 会找到别的进程/别的实例的同名窗口，找不到时就更不会置顶了。
		//ImGuiViewport::PlatformHandleRaw 在 GLFW 后端里就是 HWND。
		HWND WindowTopMostHandle();

		//统一的翻译入口。显示模式下所有翻译源都是异步的：窗口立刻出来显示「翻译中…」，
		//请求交给后台线程（百度/爬虫/有道走 HTTP 线程，本地 AI 走模型线程），结果由 UpdateTranslateTask() 每帧落实。
		//只有 ReplaceClipboard = true（Ctrl+Alt+R）是同步的：返回 true 时 Variable::zhong 里就是译文，
		//由调用方粘贴，并把 ClipboardBackup 还原回剪贴板。
		bool RequestTranslate(const std::string& English, bool ReplaceClipboard = false, const std::string& ClipboardBackup = std::string());

		//「选择翻译」（Alt/Ctrl+Q）的入口：和截图翻译一样，窗口必须**一按就出来**。
		//以前脚本模式是在主循环里同步 RunFunction(ChoiceFunction)：脚本里的 TranslateAPI() 是同步接口，
		//要等翻译回来才返回，于是 SetInterFace(TranslateEnum) 得等译文算完才执行 —— 用户看到的就是
		//「按下快捷键后卡一下，窗口才出来」。现在把顺序倒过来：先填原文 + 「翻译中…」并显示窗口，
		//脚本 / HTTP / AI 翻译都在它背后跑，结果由 UpdateTranslateTask() 每帧填回界面。
		void BeginSelectionTranslate(const std::string& English);

		//主循环每帧调用一次（KeyBoardEvents() 之后）：刷新「翻译中」提示、取回后台 AI 翻译结果
		void UpdateTranslateTask();
		//是否正在等一次「替换」的 AI 翻译（这次粘贴由 UpdateTranslateTask() 做，调用方不要再贴一遍）
		bool AiReplaceTaskRunning() const { return AiTaskRunning && AiTaskReplace; }

		//快捷键「唤出翻译窗口」（Data.ini 的 Key/Translatekey，默认 Alt+T）：
		//把上一次的原文/译文清掉，显示一个**空白**的翻译内容界面，方便直接打字或粘贴。
		//public：由主循环的 KeyBoardEvents() 调用。
		void ShowBlankTranslateWindow() {
			//界面画的是这两个成员，全局的那两份也一起清：不然点「翻译」时又会把上次的原文翻一遍
			eng.clear();
			zhong.clear();
			Variable::eng.clear();
			Variable::zhong.clear();
			TranslateFocusInput = true;//唤出来就把光标放进原文框，可以直接打字
			//走 SetInterFace()：它会顺带把 TranslateWindowClosed 清掉，
			//所以刚用「X」关掉过窗口也能再唤出来（这正是快捷键的用途）。
			SetInterFace(TranslateEnum);
		}

		const VkCommandBuffer GetCommandBuffer(int i, VkCommandBufferInheritanceInfo info);

		ImGuiIO* m_io;

		Tesseract* mTesseract = nullptr;

		Translate* mTranslate = nullptr;

		int UpdateTheScreen = false;
		//窗口是否是第一次显示
		bool GetUpdateTheScreen() {
			if (UpdateTheScreen > 0) {
				--UpdateTheScreen;
				return true;
			}
			else {
				return false;
			}
		}

		bool DragWindowSizeBool = false;

		//翻译窗口里如果正开着下拉框（语言的弹出列表），鼠标其实落在弹出窗口上，
		//主窗口的矩形命中测试会失败；TranslateInterface() 每帧把这里置位，
		//DoYouWantToUpdateTheScreen() 见到就把滞留计时续上，否则弹出列表会跟着窗口一起被自动隐藏。
		bool KeepAliveBool = false;

		bool DoYouWantToUpdateTheScreen(int time) {
			if (GetKeyState(VK_LBUTTON) >= 0) {
				DragWindowSizeBool = false;
			}
			//KeepAliveBool 只在 TranslateInterface() 里被赋值，别的面板不会去清它。
			//点「X」关窗口时鼠标必然停在窗口上，那一帧它被置成 true 就再也没人清，
			//于是接着弹出的一言等会自动关闭的窗口会被这里一直续时、永远不关。
			//所以只认翻译窗口这一种情况，其余面板一律忽略。
			if (KeepAliveBool && (InterfaceIndexes == TranslateEnum)) {
				TranslateTime = clock();
				return true;
			}
			if ((clock() - TranslateTime) > time && (time != 0)) {
				EndDisplayBool = true;
				InterFaceBool = false;
				ChildWindowBool = false;
				return false;
			}
			if ((BeginWindowPosX - 1 < m_io->MousePos.x) &&
				(m_io->MousePos.x < (BeginWindowPosX + BeginWindowSizeX + 1 + (ChildWindowBool ? BeginWindowSizeX_2 : 0))) &&
				(BeginWindowPosY - 1 < m_io->MousePos.y) &&
				(m_io->MousePos.y < (BeginWindowPosY + BeginWindowSizeY + 1))
				) 
			{
				TranslateTime = clock();
				if (GetKeyState(VK_LBUTTON) < 0) {
					DragWindowSizeBool = true;
				}
				return true;
			}
			else {
				if (DragWindowSizeBool || ((InterfaceIndexes == TranslateEnum) ? WindowRenewBool : false)) {
					return true;
				}
				if (InterfaceIndexes == MenuEnum) {
					if ((GetKeyState(VK_LBUTTON) < 0) || (GetKeyState(VK_RBUTTON) < 0)) {//鼠标点击窗口之外的地方关闭窗口
						EndDisplayBool = true;
						InterFaceBool = false;
					}
				}
				if (ChildWindowBool) {
					if ((GetKeyState(VK_LBUTTON) < 0) || (GetKeyState(VK_RBUTTON) < 0)) {//鼠标点击窗口之外的地方关闭窗口
						ChildWindowBool = false;
					}
					return true;
				}
				return (UpdateTheScreen > 0);
			}
		}

	private:

		VkDescriptorPool			g_DescriptorPool = VK_NULL_HANDLE;//给 ImGui 创建的 DescriptorPool 记得销毁
		int							g_MinImageCount = 3;
		int mFormatCount;
		VulKan::Window* mWindown{ nullptr };
		VulKan::Device* mDevice{ nullptr };
		ImGui_ImplVulkan_InitInfo ImGuiVulkanInfo;

		InterFaceEnum InterfaceIndexes = No_Enum;
		bool InterFaceBool = false;//

		VulKan::CommandPool** ImGuiCommandPoolS;
		VulKan::CommandBuffer** ImGuiCommandBufferS;

		
		void InputTextMultilineText();//向翻译窗口粘贴文本


		int BeginWindowPosX = 0, BeginWindowPosY = 0;
		int BeginWindowSizeX = 280, BeginWindowSizeY = 148;//翻译窗口的宽高
		int BeginWindowSizeX_2 = 280;


		void TranslateInterface();//翻译内容显示界面
		bool TranslateBool;//翻译界面是否是刚显示
		//用户点过翻译窗口里的「X」之后置位：后台翻译/识别的结果回来时不要再把窗口弹出来（否则等于关不掉）。
		//SetInterFace(TranslateEnum)（= 新一轮翻译）会把它清掉，所以下次翻译照样能弹窗。
		bool TranslateWindowClosed = false;
		//后台任务出结果时用它把窗口显示出来：用户刚点过「X」就不弹（见 TranslateWindowClosed）。
		//注意它只在「任务结果回来」这条路上用；用户自己发起的新翻译仍然直接 SetInterFace(TranslateEnum)。
		void ShowTranslateWindow() {
			if (TranslateWindowClosed) { return; }
			SetInterFace(TranslateEnum);
		}

		//快捷键唤出空白窗口时，下一帧把键盘焦点放进原文框（见 TranslateInterface）。
		bool TranslateFocusInput = false;

		//翻译记录（上限 20 条，新的在最前面）。实现见 Interface/TranslatePanel.cpp。
		//存放在 Data.ini 同目录的 TranslateHistory.dat：用独立文件而不是塞进 Data.ini，
		//因为 ini 的 vector 值是空格分隔的，长文本/带空格的译文存进去会被拆成好几段。
		struct TranslateHistoryItem {
			std::string Source;//原文
			std::string Target;//译文
		};
		std::vector<TranslateHistoryItem> TranslateHistoryList;
		bool TranslateHistoryLoaded = false;//惰性加载：第一次需要记录时才读文件（翻译窗口的「记录」或设置页的「翻译记录」）
		bool TranslateHistoryOpen = false;//记录列表是否展开
		void TranslateHistoryLoad();
		void TranslateHistorySave();
		void TranslateHistoryAdd(const std::string& Source, const std::string& Target);
		void TranslateHistoryPopup();//「记录」按钮弹出的列表

		//本地 AI 模型翻译的后台任务状态（见 RequestTranslate/UpdateTranslateTask）
		bool AiTaskRunning = false;			//有后台 AI 翻译在跑
		bool AiTaskReplace = false;			//完成后是「替换」而不是「显示」（Ctrl+Alt+R）
		std::string AiTaskSourceText;		//原文，结果回来时一起填进 eng 缓冲
		std::string AiTaskClipboardBackup;	//替换模式下原来剪贴板里的内容，贴完还原
		clock_t AiTaskStartTime = 0;		//开始时间（用来显示已用秒数）
		//普通翻译源（百度/爬虫/有道）的显示模式：窗口先出来，HTTP 请求交给后台线程
		bool WebTaskPending = false;		//正在等一次后台的普通翻译源（百度/爬虫/有道）结果
		std::string WebTaskSourceText;		//这次后台翻译的原文（结果回来时用它回填原文框）
		bool OcrTaskPending = false;		//正在等一次后台的截图 OCR 识别结果（识别完接着翻译）
		bool ScriptAfterOcr = false;		//本次截图识别完后要跑用户的截图脚本（脚本模式）
		bool ScriptPending = false;			//脚本等这一帧把识别出的原文画出来之后，下一帧再跑（脚本是同步接口）
		bool ScriptRunning = false;			//脚本已经交给后台线程在跑，跑完由 UpdateTranslateTask() 把结果同步上界面
		//「选择翻译」排队：脚本共用一个 asIScriptContext，一次只能跑一个。上一次还在后台跑时，
		//既不能并发再起一个（BeginRun 会拒绝），也**不能等它跑完**——那是主线程上的 join，
		//AI 源一次要几十秒，界面就卡死了。所以把这次要翻的文本先存下来，
		//等 UpdateTranslateTask() 看到脚本跑完再发出去（用户连按多次时只保留最后一次）。
		bool SelectionPending = false;
		std::string PendingSelectionText;
		//把一次「选择翻译」真正发出去（原文已经摆好，且此刻没有脚本在跑）。见 BeginSelectionTranslate。
		void StartSelectionTask(const std::string& English);
		//把排队中的那次「选择翻译」发出去（上一次脚本已经结束、轮到它了）。
		void StartPendingSelection() {
			SelectionPending = false;
			const std::string Text = PendingSelectionText;
			PendingSelectionText.clear();
			//界面上已经摆着新原文 +「翻译中…」（用户按下快捷键时写的），这里只负责把它跑起来
			StartSelectionTask(Text);
		}
		std::string AiTargetLangCode() const;//当前要翻译成哪种语言（Data.ini 里的代码）
		bool ChildWindowBool = false;//右侧窗口是否显示
		bool WhoBool;//右侧窗口显示 From 还是 To
		bool WindowRenewBool = true;//窗口大小是否调整过
		int RowsNumber = 4;//文本显示多行
	public:
		int kuangshu = 200;//文本有多少像素宽度
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways;
	public:
		clock_t TranslateTime;//显示时间
		//原文 / 译文：以前是两块各 1MB 的定长 char 缓冲区（到处 memset + 截断拷贝），
		//现在就是普通字符串，赋值即同步。
		std::string eng;
		std::string zhong;

	private:
		void ScreenshotInterface();//截图操作界面
		POINT MousePosition_1;//鼠标点击位置
		POINT MousePosition_2;//鼠标松开位置
		bool ScreenshotBool = true;//界面是否是刚显示
		int x, y, w, h;//框的位置，大小

		void SetUpInterface();//设置界面（窗口骨架 + 按页分发；实现见 Interface/SettingsPanel.cpp）
		bool SetBool = true;//界面是否是刚显示
		//设置界面按左侧 9 个分类拆分绘制
		void InitSettingsState();//打开设置界面时把当前配置读进「待编辑」状态
		void DrawSettingsNav();//左侧分类导航
		void DrawSettingsTranslate();//页0 翻译服务
		void DrawSettingsAI();//页1 本地 AI 模型
		void DrawSettingsHotkey();//页2 快捷键
		void DrawSettingsGeneral();//页3 常规
		void DrawSettingsInterface();//页4 界面
		void DrawSettingsInterfaceFonts();//页4 子段：识别模型 / 字体 / 语言
		void DrawSettingsRenderDevice();//页4 子段：渲染设备选择
		void DrawSettingsInterfaceMisc();//页4 子段：截图颜色 / 截图脚本
		void DrawSettingsHitokoto();//页5 一言
		void DrawSettingsBackup();//页6 备份（坚果云 WebDav）
		void DrawSettingsRecovery();//页6 子段：备份恢复
		void DrawSettingsHistory();//页7 翻译记录
		void DrawSettingsAbout();//页8 关于
		void SaveSettings();//保存动作（底部按钮 / Ctrl+S 共用）

		void MenuInterface();//菜单界面
		bool MenuBool = true;//界面是否是刚显示

		void HitokotoSentence();
		bool HitokotoBool = true;//界面是否是刚显示
		ImFont* HitokotoFont = nullptr;
	public:
		char* TData;

		// A struct to manage data related to one image in vulkan
		struct MyTextureData
		{
			VkDescriptorSet DS = nullptr;         // Descriptor set: this is what you'll pass to Image()
			int             Width;
			int             Height;
			int             Channels;

			// Need to keep track of these to properly cleanup
			VkImageView     ImageView;
			VkImage         Image;
			VkDeviceMemory  ImageMemory;
			VkSampler       Sampler;
			VkBuffer        UploadBuffer;
			VkDeviceMemory  UploadBufferMemory;

			MyTextureData() { memset(this, 0, sizeof(*this)); }
		};

		MyTextureData mTextureData;

		// Helper function to find Vulkan memory type bits. See ImGui_ImplVulkan_MemoryType() in imgui_impl_vulkan.cpp
		uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

		// Helper function to load an image with common settings and return a MyTextureData with a VkDescriptorSet as a sort of Vulkan pointer
		bool LoadTextureFromFile(char* Texturedata, MyTextureData* tex_data);

		// Helper function to cleanup an image loaded with LoadTextureFromFile
		void RemoveTexture(MyTextureData* tex_data);

	};
}