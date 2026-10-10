#include "../Interface.h"
#include "InterfaceInternal.h"
#include "../../AngelScript/AngelScriptCode.h"
#include "../../Function/WebDav.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

// =====================================================================================
// 设置界面（SetUpInterface）
// -------------------------------------------------------------------------------------
// 原先 Interface.cpp 里 1186 行的单个巨型函数，现按左侧 8 个分类拆开：
//   InitSettingsState()          —— 打开设置界面时把当前配置读进「待编辑」状态
//   DrawSettingsNav()            —— 左侧分类导航
//   DrawSettingsTranslate()      —— 页0 翻译服务
//   DrawSettingsAI()             —— 页1 本地 AI 模型
//   DrawSettingsHotkey()         —— 页2 快捷键
//   DrawSettingsGeneral()        —— 页3 常规
//   DrawSettingsInterface()      —— 页4 界面（再分 字体 / 渲染设备 / 其它 三个子段）
//   DrawSettingsHitokoto()       —— 页5 一言
//   DrawSettingsBackup()         —— 页6 备份（坚果云 WebDav）+ 备份恢复
//   DrawSettingsAbout()          —— 页7 关于
//   SaveSettings()               —— 保存动作（底部按钮 / Ctrl+S 共用）
// 各段共享的「待编辑」状态集中在文件内的 SettingsState（等价于原来的函数内 static）。
// =====================================================================================
namespace GAME {
	namespace {

		// ---- 待编辑状态（原 SetUpInterface 里的 static 局部量）----
		struct SettingsState {

			char SetWebDav_url[128];
			char SetWebDav_username[128];
			char SetWebDav_password[128];
			char SetWebDav_WebFile[128];

			char SetBaiduID[128];
			char SetBaiduKey[128];
			char SetYoudaoID[128];
			char SetYoudaoKey[128];

			//本地 AI 模型（llama.cpp）：模型路径 + 推理参数
			char SetAiModelPath[260];
			int SetAiThreads;
			int SetAiNCtx;
			int SetAiMaxTokens;
			float SetAiTemperature;
			int SetAiIdleUnload;
			int SetTranslateSource;//设置界面里选的翻译源（0=百度 1=爬虫 2=有道 3=AI 模型）
			//扫描 Modes 文件夹得到的模型列表（下拉框用）和当前选中的下标（-1 = 列表里没有）
			int SetAiModelIndex;
			std::vector<std::string> SetAiModelList;
			//AI 运行设备（llama.cpp）：模式 + 指定设备名 + 这次识别到的设备列表
			int SetAiDeviceMode;
			std::string SetAiDeviceName;
			std::vector<LlamaTranslate::DeviceInfo> SetAiDeviceList;

			int SetMakeUp;
			char SetScreenshotkey[2];
			char SetChoicekey[2];
			char SetReplacekey[2];

			int ModelIndex;
			std::vector<std::string> ModelS;

			int FontIndex;
			int MyFontSize;
			std::vector<std::string> FontS;

			int LanguageIndex;
			std::vector<std::string> LanguageS;

			float LFontSize;
			bool LFontBool;

			float ScreenshotColor[4];

			int ScriptIndex;
			std::vector<std::string> ScriptS;
			bool LScriptBool;

			bool RecoveryWindow;//恢复窗口选项
			std::vector<std::string> RecoveryList;
			int RecoveryIndex = 0;
			unsigned char RecoveryChoice = 0;

			bool LDirectory_Opcode;
			bool LDirectory_Language;
			bool LDirectory_TessData;
			bool LDirectory_TTF;

			bool SetPopUpNotificationBool;
			int SetHitokotoTimeInterval;
			int SetHitokotoDisplayDuration;
			float SetHitokotoPosX;
			float SetHitokotoPosY;
			float SetHitokotoFontSize;
			bool SetHitokotoTTFBool;
			bool SetHitokotoFontBool;
			int SetHitokotoFontIndex;
			//渲染设备选择：先改这几个"待保存"的局部量，按下保存才写回 Variable::（和 PixelClean 一致）
			int SetVulkanDeviceMode = (int)Variable::VulkanDeviceModeEnum::AutoBest;
			bool VulkanDeviceModeChanged = false;
			Variable::VulkanDeviceModeEnum PendingVulkanDeviceMode = Variable::VulkanDeviceModeEnum::AutoBest;
			std::string PendingVulkanDeviceName = "";

			//当前页（左侧导航选中项）
			int SetPage = 0;
			//「已保存」提示的显示时刻
			clock_t SavedTime = 0;
		};
		SettingsState S;

		//语言文件里本来就是 UTF-8 中文，标点也一起放进语言文件当模板，
		//这样代码里不用出现非 ASCII 字面量。这里把模板里前 N 个 %s 依次换成给定文本。
		std::string FillDeviceText(const std::string& Tpl, const std::vector<std::string>& Values) {
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
		}

		//两列表格：左列固定宽度的标签，右列控件
		bool BeginSettingsTable(const char* Id)
		{
			if (!ImGui::BeginTable(Id, 2, ImGuiTableFlags_SizingStretchProp)) {
				return false;
			}
			ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthStretch, 0.38f);
			ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, 0.62f);
			return true;
		}
		void RowLabel(const char* Label)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextWrapped(Label);
			ImGui::TableNextColumn();
		}

		//自己重启自己：路径必须走宽字符 API，否则程序装在中文目录下（例如 C:\用户\...）会启动失败
		void RestartSelfApplication()
		{
			wchar_t path[MAX_PATH] = { 0 };
			GetModuleFileNameW(NULL, path, MAX_PATH);
			ShellExecuteW(NULL, NULL, path, NULL, NULL, SW_SHOWDEFAULT);
			exit(0);
		}

		// ---- 定长输入框的 Ctrl+V 延迟粘贴（原 Interface.cpp 里的文件内辅助）----
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
	}

	//打开设置界面时，把当前配置读进「待编辑」状态（原来的 if (SetBool) 初始化块）
	void ImGuiInterFace::InitSettingsState()
	{
		RequestHitokoto();//后台线程取，别在这里同步等网络：否则每点一次「设置」都要先卡 0.5 秒

		//这些目标全是 128 字节的定长数组。旧写法直接 memcpy(..., str.size())：
		//配置里存了长文本就会写爆数组，而且刚好写满 128 字节时连终止符都没有，
		//ImGui 后面按 C 字符串读它就会一路读到相邻内存。CopyToBuffer 负责截断并补 '\0'。
		TOOL::CopyToBuffer(S.SetWebDav_url, sizeof(S.SetWebDav_url), Variable::WebDav_url);
		TOOL::CopyToBuffer(S.SetWebDav_username, sizeof(S.SetWebDav_username), Variable::WebDav_username);
		TOOL::CopyToBuffer(S.SetWebDav_password, sizeof(S.SetWebDav_password), Variable::WebDav_password);
		TOOL::CopyToBuffer(S.SetWebDav_WebFile, sizeof(S.SetWebDav_WebFile), Variable::WebDav_WebFile);

		TOOL::CopyToBuffer(S.SetBaiduID, sizeof(S.SetBaiduID), Variable::BaiduAppid);
		TOOL::CopyToBuffer(S.SetBaiduKey, sizeof(S.SetBaiduKey), Variable::BaiduSecret_key);
		TOOL::CopyToBuffer(S.SetYoudaoID, sizeof(S.SetYoudaoID), Variable::YoudaoAppid);
		TOOL::CopyToBuffer(S.SetYoudaoKey, sizeof(S.SetYoudaoKey), Variable::YoudaoSecret_key);

		TOOL::CopyToBuffer(S.SetAiModelPath, sizeof(S.SetAiModelPath), Variable::AiModelPath);
		S.SetAiThreads = Variable::AiThreads;
		S.SetAiNCtx = Variable::AiNCtx;
		S.SetAiMaxTokens = Variable::AiMaxTokens;
		S.SetAiTemperature = Variable::AiTemperature;
		S.SetAiIdleUnload = Variable::AiIdleUnload;
		S.SetTranslateSource = mTranslate->mTranslate;//打开设置界面时按当前实际生效的翻译源初始化

		//Modes 文件夹里的模型（逐级向上找），并按当前生效的模型定位下拉框选项
		S.SetAiModelList = Translate::AiModelFiles();
		S.SetAiModelIndex = FindAiModelIndex(S.SetAiModelList, S.SetAiModelPath);

		//AI 运行设备：三项固定（自动最高 / 自动最低 / CPU）+ 每台识别到的设备；
		//配置被改坏（模式越界）时退回「自动最高性能」
		S.SetAiDeviceList = Translate::AiDeviceList();
		S.SetAiDeviceMode = Variable::AiDeviceMode;
		if (S.SetAiDeviceMode < (int)LlamaTranslate::DeviceMode::AutoBest ||
			S.SetAiDeviceMode > (int)LlamaTranslate::DeviceMode::Specific)
		{
			S.SetAiDeviceMode = (int)LlamaTranslate::DeviceMode::AutoBest;
		}
		S.SetAiDeviceName = Variable::AiDeviceName;

		if (Variable::MakeUp == 17) { S.SetMakeUp = 1; }
		TOOL::CopyToBuffer(S.SetScreenshotkey, sizeof(S.SetScreenshotkey), Variable::Screenshotkey);
		TOOL::CopyToBuffer(S.SetChoicekey, sizeof(S.SetChoicekey), Variable::Choicekey);
		TOOL::CopyToBuffer(S.SetReplacekey, sizeof(S.SetReplacekey), Variable::Replacekey);

		S.LFontSize = Variable::FontSize;
		S.LFontBool = Variable::FontBool;

		S.ModelS.clear();
		S.ModelIndex = 0;
		TOOL::FilePath("./TessData", &S.ModelS, "traineddata", TOOL::FileStem(Variable::Model).c_str(), &S.ModelIndex);

		S.FontS.clear();
		S.FontIndex = 0;
		TOOL::FilePath("./TTF", &S.FontS, "ttf", TOOL::FileStem(Variable::FontFilePath).c_str(), &S.FontIndex);
		S.MyFontSize = S.FontS.size();
		TOOL::FilePath("C:\\Windows\\Fonts", &S.FontS, "ttf", TOOL::FileStem(Variable::FontFilePath).c_str(), &S.FontIndex);

		S.LanguageS.clear();
		S.LanguageIndex = 0;
		TOOL::FilePath("./Language", &S.LanguageS, "ini", TOOL::FileStem(Variable::Language).c_str(), &S.LanguageIndex);
		//ImGui::InputTextWithHint("input text (w/ hint)", "enter text here", str1, IM_ARRAYSIZE(str1));

		for (size_t i = 0; i < 4; i++)
		{
			S.ScreenshotColor[i] = float(Variable::ScreenshotColor[i]) / 255.0f;
		}

		S.ScriptS.clear();
		S.ScriptIndex = 0;
		TOOL::FilePath("./Opcode", &S.ScriptS, "as", TOOL::FileStem(Variable::Script).c_str(), &S.ScriptIndex);
		S.LScriptBool = Variable::ScriptBool;

		//渲染设备选择：把当前设置换算成下拉框下标。
		//0..2 是三项固定的（自动最高/自动最低/CPU），3 开始依次对应识别到的设备。
		if (Variable::VulkanDeviceMode == Variable::VulkanDeviceModeEnum::Specific)
		{
			S.SetVulkanDeviceMode = 0;//指定的设备这次没识别到就先显示第一项
			for (size_t i = 0; i < Variable::VulkanDetectedDevices.size(); i++)
			{
				if (Variable::VulkanDetectedDevices[i].name == Variable::VulkanDeviceName)
				{
					S.SetVulkanDeviceMode = (int)i + 3;
					break;
				}
			}
		}
		else
		{
			S.SetVulkanDeviceMode = (int)Variable::VulkanDeviceMode;
		}
		S.PendingVulkanDeviceMode = Variable::VulkanDeviceMode;
		S.PendingVulkanDeviceName = Variable::VulkanDeviceName;
		S.VulkanDeviceModeChanged = false;

		S.RecoveryWindow = false;
		S.RecoveryIndex = 0;
		S.RecoveryChoice = 0;

		S.LDirectory_Opcode = Variable::OpcodeBool;
		S.LDirectory_Language = Variable::LanguageBool;
		S.LDirectory_TessData = Variable::TessDataBool;
		S.LDirectory_TTF = Variable::TTFBool;

		S.SetPopUpNotificationBool = Variable::PopUpNotificationBool;
		S.SetHitokotoTimeInterval = Variable::HitokotoTimeInterval;
		S.SetHitokotoDisplayDuration = Variable::HitokotoDisplayDuration;
		S.SetHitokotoFontSize = Variable::HitokotoFontSize;
		S.SetHitokotoTTFBool = Variable::HitokotoTTFBool;
		S.SetHitokotoFontBool = Variable::HitokotoFontBool;
		S.SetHitokotoPosX = Variable::HitokotoPosX;
		S.SetHitokotoPosY = Variable::HitokotoPosY;
		S.SetHitokotoFontIndex = 0;
		for (size_t i = 0; i < S.FontS.size(); i++)
		{
			if (S.FontS[i] == Variable::HitokotoFont) {
				S.SetHitokotoFontIndex = i;
			}
		}
	}

	//把设置界面的「待编辑」状态写回 Variable:: 并落盘（底部「保存」按钮和 Ctrl+S 共用）
	void ImGuiInterFace::SaveSettings()
	{
		bool updata = false;//判断是否要重启软件
		Variable::PopUpNotificationBool = S.SetPopUpNotificationBool;
		Variable::HitokotoTimeInterval = S.SetHitokotoTimeInterval;
		Variable::HitokotoDisplayDuration = S.SetHitokotoDisplayDuration;
		Variable::HitokotoPosX = S.SetHitokotoPosX;
		Variable::HitokotoPosY = S.SetHitokotoPosY;
		if (Variable::HitokotoFontSize != S.SetHitokotoFontSize) {
			updata = true;
			Variable::HitokotoFontSize = S.SetHitokotoFontSize;
		}
		if (Variable::HitokotoTTFBool != S.SetHitokotoTTFBool) {
			updata = true;
			Variable::HitokotoTTFBool = S.SetHitokotoTTFBool;
		}
		if (Variable::HitokotoFontBool != S.SetHitokotoFontBool) {
			updata = true;
			Variable::HitokotoFontBool = S.SetHitokotoFontBool;
		}
		if ((S.FontS.size() != 0) && !Variable::HitokotoTTFBool) {
			std::string LFontFilePath;
			if (S.MyFontSize > S.SetHitokotoFontIndex) {
				LFontFilePath = "./TTF/" + S.FontS[S.SetHitokotoFontIndex] + ".ttf";
			}
			else {
				LFontFilePath = "C:\\Windows\\Fonts\\" + S.FontS[S.SetHitokotoFontIndex] + ".ttf";
			}

			if (LFontFilePath != Variable::HitokotoFont) {//更换字体
				updata = true;
				Variable::HitokotoFont = LFontFilePath;
			}
		}
		else {
			Variable::HitokotoTTFBool = true;
		}


		Variable::WebDav_url = S.SetWebDav_url;
		Variable::WebDav_username = S.SetWebDav_username;
		Variable::WebDav_password = S.SetWebDav_password;
		Variable::WebDav_WebFile = S.SetWebDav_WebFile;

		Variable::OpcodeBool = S.LDirectory_Opcode;
		Variable::LanguageBool = S.LDirectory_Language;
		Variable::TessDataBool = S.LDirectory_TessData;
		Variable::TTFBool = S.LDirectory_TTF;

		//渲染设备选择
		Variable::VulkanDeviceMode = S.PendingVulkanDeviceMode;
		Variable::VulkanDeviceName = S.PendingVulkanDeviceName;

		Variable::BaiduAppid = S.SetBaiduID;
		Variable::BaiduSecret_key = S.SetBaiduKey;
		Variable::YoudaoAppid = S.SetYoudaoID;
		Variable::YoudaoSecret_key = S.SetYoudaoKey;
		//翻译源：保存后立刻生效，并写回 Data.ini 的 [FT] Translate
		Variable::Translate = S.SetTranslateSource;
		mTranslate->SetTranslate(S.SetTranslateSource);

		//AI 模型：路径、推理参数或运行设备改了就把已加载的模型卸掉，下次翻译按新设置重新加载
		const bool AiSettingChanged = (Variable::AiModelPath != S.SetAiModelPath) || (Variable::AiThreads != S.SetAiThreads) ||
			(Variable::AiNCtx != S.SetAiNCtx) || (Variable::AiMaxTokens != S.SetAiMaxTokens) ||
			(Variable::AiTemperature != S.SetAiTemperature) ||
			(Variable::AiDeviceMode != S.SetAiDeviceMode) || (Variable::AiDeviceName != S.SetAiDeviceName);
		Variable::AiModelPath = S.SetAiModelPath;
		Variable::AiThreads = S.SetAiThreads;
		Variable::AiNCtx = S.SetAiNCtx;
		Variable::AiMaxTokens = S.SetAiMaxTokens;
		Variable::AiTemperature = S.SetAiTemperature;
		Variable::AiDeviceMode = S.SetAiDeviceMode;
		Variable::AiDeviceName = S.SetAiDeviceName;
		//闲置卸载时间只是给主循环判断用的，不算「推理参数变了」，不必重载模型
		Variable::AiIdleUnload = S.SetAiIdleUnload;
		if (AiSettingChanged) { mTranslate->AiUnloadModel(); }

		//转为大写
		int MakeUpS[2] = { 18,17 };
		Variable::MakeUp = MakeUpS[S.SetMakeUp];
		Variable::Screenshotkey = toupper(S.SetScreenshotkey[0]);
		Variable::Choicekey = toupper(S.SetChoicekey[0]);
		Variable::Replacekey = toupper(S.SetReplacekey[0]);

		for (size_t i = 0; i < 4; i++)
		{
			Variable::ScreenshotColor[i] = int(S.ScreenshotColor[i] * 255);
		}

		if (S.ModelS.size() != 0) {
			if (Variable::Model != S.ModelS[S.ModelIndex]) {
				mTesseract->~Tesseract();
				mTesseract->Tesseract::Tesseract(S.ModelS[S.ModelIndex].c_str());
			}
			Variable::Model = S.ModelS[S.ModelIndex];
		}

		Variable::ScriptBool = S.LScriptBool;
		if (S.ScriptS.size() != 0) {
			if (Variable::Script != S.ScriptS[S.ScriptIndex]) {
				AngelScriptOpcode::AngelScriptCode::ResetAngelScriptCode();
			}
			Variable::Script = S.ScriptS[S.ScriptIndex];
		}

		if (S.LanguageS.size() != 0) {
			if (Variable::Language != S.LanguageS[S.LanguageIndex]) {
				Language::ReadFile(S.LanguageS[S.LanguageIndex]);
			}
			Variable::Language = S.LanguageS[S.LanguageIndex];
		}


		if ((S.FontS.size() != 0) && Variable::FontBool) {
			std::string LFontFilePath;
			if (S.MyFontSize > S.FontIndex) {
				LFontFilePath = "./TTF/" + S.FontS[S.FontIndex] + ".ttf";
			}
			else {
				LFontFilePath = "C:\\Windows\\Fonts\\" + S.FontS[S.FontIndex] + ".ttf";
			}

			if (LFontFilePath != Variable::FontFilePath) {//更换字体
				updata = true;
			}
			Variable::FontFilePath = LFontFilePath;
		}
		else {
			Variable::FontBool = false;
		}

		if ((Variable::FontBool != S.LFontBool) || Variable::FontSize != S.LFontSize) {//更换字体或字体大小
			updata = true;
		}

		//SetModifyRegedit 返回 true 表示写入/删除成功。原来这里少了「!」，
		//于是每次保存设置（成功时）都会打出一条假的 "SetModifyRegedit(): Error"。
		if (!TOOL::SetModifyRegedit("TranslatorKyi", Variable::Startup)) {
			TOOL::logger->error("SetModifyRegedit(): Error");
		}


		Variable::SaveFile();

		if (updata) {
			//重启走的是 exit(0)：堆上的对象不会被析构，后台线程（AI 生成 / HTTP 翻译 / OCR / 脚本）
			//必须先停干净再退出，否则新进程起来后旧线程还在读写这些成员。
			if (mTranslate != nullptr) { mTranslate->StopBackgroundWork(); }
			if (mTesseract != nullptr) { mTesseract->OcrWait(); }
			AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->WaitRunning();
			TOOL::LogStep("SaveSettings: 后台线程已停止，准备重启自己");
			delete mWindown;
			RestartSelfApplication();//自己重启自己
		}

		mTranslate->SetBaiduAppID(Variable::BaiduAppid.c_str());
		mTranslate->SetBaiduSecretkey(Variable::BaiduSecret_key.c_str());

		mTranslate->SetYoudaoAppID(Variable::YoudaoAppid.c_str());
		mTranslate->SetYoudaoSecretkey(Variable::YoudaoSecret_key.c_str());

		EndDisplayBool = true;
		InterFaceBool = false;
		SetBool = true;
		S.SavedTime = clock();
	}

	//左侧分类导航（8 个分类）
	void ImGuiInterFace::DrawSettingsNav()
	{
		const ImVec4 AccentColor(0.282f, 0.780f, 0.486f, 1.0f);
		const ImU32 AccentU32 = ImGui::GetColorU32(AccentColor);
		const float FooterH = ImGui::GetFrameHeightWithSpacing() + 8.0f;
		const char* NavItems[8] = {
			Language::NavTranslate.c_str(), Language::NavAI.c_str(), Language::NavHotkey.c_str(), Language::NavGeneral.c_str(),
			Language::NavInterface.c_str(), Language::NavHitokoto.c_str(), Language::NavBackup.c_str(), Language::NavAbout.c_str()
		};

		ImGui::BeginChild("##nav", ImVec2(132.0f, -FooterH), true);
		{
			for (int i = 0; i < 8; i++)
			{
				ImGui::PushID(i);
				const bool Selected = (S.SetPage == i);
				if (Selected) {
					//selected item: translucent accent background, bright text stays readable
					ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(AccentColor.x, AccentColor.y, AccentColor.z, 0.22f));
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(AccentColor.x, AccentColor.y, AccentColor.z, 0.50f));
					ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(AccentColor.x, AccentColor.y, AccentColor.z, 0.65f));
				}
				const ImVec2 ItemPos = ImGui::GetCursorScreenPos();
				if (ImGui::Selectable(NavItems[i], Selected, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0.0f, 26.0f))) {
					S.SetPage = i;
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
	}

	//页0 翻译服务：百度 / 有道的账号密钥
	void ImGuiInterFace::DrawSettingsTranslate()
	{
		ImGui::TextUnformatted(Language::AccountKey.c_str());
		//翻译源：决定按快捷键 / 点「翻译」时用哪个引擎（翻译窗口右上角的按钮和托盘菜单里的「翻译源」都是这个值）
		if (BeginSettingsTable("##tbl_source"))
		{
			RowLabel(Language::Engine.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##translate_source", mTranslate->TranslateName[S.SetTranslateSource]))
			{
				for (int i = 0; i <= Translate::AiTranslate; i++)
				{
					const bool SourceChosen = (S.SetTranslateSource == i);
					if (ImGui::Selectable(mTranslate->TranslateName[i], SourceChosen)) {
						S.SetTranslateSource = i;
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
			InputInfo.LText = S.SetBaiduID;
			ImGui::InputText("##baidu_id", S.SetBaiduID, IM_ARRAYSIZE(S.SetBaiduID), flags, &InputKeyEvent, &InputInfo);
			RowLabel(Language::BaiduKey.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			InputInfo.LText = S.SetBaiduKey;
			ImGui::InputText("##baidu_key", S.SetBaiduKey, IM_ARRAYSIZE(S.SetBaiduKey), flags, &InputKeyEvent, &InputInfo);
			RowLabel(Language::YoudaoID.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			InputInfo.LText = S.SetYoudaoID;
			ImGui::InputText("##youdao_id", S.SetYoudaoID, IM_ARRAYSIZE(S.SetYoudaoID), flags, &InputKeyEvent, &InputInfo);
			RowLabel(Language::YoudaoKey.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			InputInfo.LText = S.SetYoudaoKey;
			ImGui::InputText("##youdao_key", S.SetYoudaoKey, IM_ARRAYSIZE(S.SetYoudaoKey), flags, &InputKeyEvent, &InputInfo);
			ImGui::EndTable();
		}
	}

	//页1 AI 模型（本地 llama.cpp）
	void ImGuiInterFace::DrawSettingsAI()
	{
		if (BeginSettingsTable("##tbl_ai"))
		{
			//模型选择：列出 Modes 文件夹（程序目录含逐级向上）里扫到的 .gguf；
			//选中一项就写进下面的模型路径，点「保存」时按变化自动卸载模型、下次翻译用新模型
			const std::string AiModelName = CurrentAiModelName(S.SetAiModelList, S.SetAiModelIndex, S.SetAiModelPath);
			RowLabel(Language::AIModelSelect.c_str());
			ImGui::SetNextItemWidth(-96.0f);
			if (!S.SetAiModelList.empty()) {
				if (ImGui::BeginCombo("##ai_model_list", AiModelName.c_str(), flags)) {
					for (int n = 0; n < (int)S.SetAiModelList.size(); n++) {
						const bool is_selected = (S.SetAiModelIndex == n);
						if (ImGui::Selectable(TOOL::BaseName(S.SetAiModelList[n]).c_str(), is_selected)) {
							S.SetAiModelIndex = n;
							TOOL::CopyToBuffer(S.SetAiModelPath, sizeof(S.SetAiModelPath), S.SetAiModelList[n]);
						}
						if (is_selected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
			else {
				ImGui::TextUnformatted(Language::NotAiModelText.c_str());
			}
			ImGui::SameLine();
			if (ImGui::Button(Language::AIModelRefresh.c_str(), ImVec2(88.0f, 0.0f))) {
				//重新扫一遍 Modes（模型是程序跑起来之后才放进去的，不用重开程序刷新）
				S.SetAiModelList = Translate::AiModelFiles();
				S.SetAiModelIndex = FindAiModelIndex(S.SetAiModelList, S.SetAiModelPath);
			}
			//运行设备：本地 AI 用哪块设备推理（和第 4 页的渲染设备一个套路）。
			//前三项固定（自动最高 / 自动最低 / CPU），第 3 项往后是这次识别到的设备。
			{
				static std::vector<std::string> AiDeviceLabels;
				static std::vector<const char*> AiDeviceItems;
				AiDeviceLabels.clear();
				AiDeviceItems.clear();
				AiDeviceLabels.push_back(Language::AIDeviceAutoBest);
				AiDeviceLabels.push_back(Language::AIDeviceAutoWorst);
				AiDeviceLabels.push_back(Language::AIDeviceCPU);
				for (size_t i = 0; i < S.SetAiDeviceList.size(); i++)
				{
					const LlamaTranslate::DeviceInfo& Device = S.SetAiDeviceList[i];
					std::string TypeText;
					switch (Device.type)
					{
					case 1: TypeText = Language::AIDeviceTypeGPU; break;	//GGML_BACKEND_DEVICE_TYPE_GPU
					case 2: TypeText = Language::AIDeviceTypeIGPU; break;	//GGML_BACKEND_DEVICE_TYPE_IGPU
					case 3: TypeText = Language::AIDeviceTypeACCEL; break;	//GGML_BACKEND_DEVICE_TYPE_ACCEL
					default: TypeText = Language::AIDeviceTypeOther; break;	//CPU / META / 其它
					}
					//显存大小只有拿得到时才显示（CPU 设备是 0）
					if (Device.memTotal > 0)
					{
						TypeText += " " + std::to_string((unsigned long long)(Device.memTotal / (1024ull * 1024ull))) + " MB";
					}
					AiDeviceLabels.push_back(FillDeviceText(Language::AIDeviceItem,
						{ Device.desc.empty() ? Device.name : Device.desc, TypeText }));
				}
				for (size_t i = 0; i < AiDeviceLabels.size(); i++)
				{
					AiDeviceItems.push_back(AiDeviceLabels[i].c_str());
				}
				//当前设置换算成下拉框下标；指定的设备这次没识别到就先显示第一项
				int AiDeviceIndex = S.SetAiDeviceMode;
				if (S.SetAiDeviceMode == (int)LlamaTranslate::DeviceMode::Specific)
				{
					AiDeviceIndex = 0;
					for (size_t i = 0; i < S.SetAiDeviceList.size(); i++)
					{
						if (S.SetAiDeviceList[i].name == S.SetAiDeviceName)
						{
							AiDeviceIndex = (int)i + 3;
							break;
						}
					}
				}
				if (AiDeviceIndex < 0 || AiDeviceIndex >= (int)AiDeviceItems.size()) { AiDeviceIndex = 0; }

				RowLabel(Language::AIDevice.c_str());
				ImGui::SetNextItemWidth(-96.0f);
				if (ImGui::BeginCombo("##ai_device", AiDeviceItems[AiDeviceIndex], flags))
				{
					for (int n = 0; n < (int)AiDeviceItems.size(); n++)
					{
						const bool is_selected = (AiDeviceIndex == n);
						if (ImGui::Selectable(AiDeviceItems[n], is_selected))
						{
							if (n >= 3 && (size_t)(n - 3) < S.SetAiDeviceList.size())
							{
								//第 3 项往后都是具体设备：记下设备标识，加载时按它找设备
								S.SetAiDeviceMode = (int)LlamaTranslate::DeviceMode::Specific;
								S.SetAiDeviceName = S.SetAiDeviceList[n - 3].name;
							}
							else
							{
								S.SetAiDeviceMode = (n < 0) ? 0 : ((n > 2) ? 2 : n);
								S.SetAiDeviceName.clear();
							}
						}
						if (is_selected) { ImGui::SetItemDefaultFocus(); }
					}
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				if (ImGui::Button(Language::AIDeviceRefresh.c_str(), ImVec2(88.0f, 0.0f)))
				{
					//设备是 llama 后端注册的；这里重扫一遍（换了显卡/插了外接显卡后不用重开程序）
					S.SetAiDeviceList = Translate::AiDeviceList();
				}
			}
			//指定的设备这次没识别到：加载时会自动退回 CPU（见 LlamaTranslate::Load）
			if (S.SetAiDeviceMode == (int)LlamaTranslate::DeviceMode::Specific)
			{
				bool AiDeviceFound = false;
				for (size_t i = 0; i < S.SetAiDeviceList.size(); i++)
				{
					if (S.SetAiDeviceList[i].name == S.SetAiDeviceName) { AiDeviceFound = true; break; }
				}
				if (!AiDeviceFound)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s",
						FillDeviceText(Language::AIDeviceMissing, { S.SetAiDeviceName }).c_str());
				}
			}
			//当前使用设备：显示这次加载真正跑在什么设备上——它可能和上面选的模式不一样
			//（例如指定了显卡但显存不够，加载时会自动退回 CPU）；没加载模型时给一句提示
			RowLabel(Language::AIDeviceCurrent.c_str());
			{
				const std::string ActiveDevice = (mTranslate != nullptr) ? mTranslate->AiActiveDevice() : std::string();
				if (ActiveDevice.empty()) {
					ImGui::Text("%s", Language::AIDeviceCurrentNone.c_str());
				}
				else {
					ImGui::Text("%s", ActiveDevice.c_str());
				}
			}
			RowLabel(Language::AIModelPath.c_str());
			ImGui::SetNextItemWidth(-96.0f);
			InputInfo.LText = S.SetAiModelPath;
			ImGui::InputText("##ai_path", S.SetAiModelPath, IM_ARRAYSIZE(S.SetAiModelPath), flags, &InputKeyEvent, &InputInfo);
			ImGui::SameLine();
			if (ImGui::Button(Language::AIModelDefault.c_str(), ImVec2(88.0f, 0.0f))) {
				TOOL::CopyToBuffer(S.SetAiModelPath, sizeof(S.SetAiModelPath), Translate::DefaultAiModelPath());
			}
			RowLabel(Language::AIThreads.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputInt("##ai_threads", &S.SetAiThreads);
			if (S.SetAiThreads < 0) { S.SetAiThreads = 0; }
			RowLabel(Language::AINCtx.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputInt("##ai_nctx", &S.SetAiNCtx);
			if (S.SetAiNCtx < 256) { S.SetAiNCtx = 256; }
			RowLabel(Language::AIMaxTokens.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputInt("##ai_maxtokens", &S.SetAiMaxTokens);
			if (S.SetAiMaxTokens < 16) { S.SetAiMaxTokens = 16; }
			RowLabel(Language::AITemperature.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputFloat("##ai_temperature", &S.SetAiTemperature, 0.05f, 0.1f);
			if (S.SetAiTemperature < 0.01f || S.SetAiTemperature > 2.0f) { S.SetAiTemperature = 0.7f; }
			//模型闲置多久自动卸载（秒，0 = 一直留着）；只影响主循环的判断，改了不用重新加载模型
			RowLabel(Language::AIIdleUnload.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputInt("##ai_idleunload", &S.SetAiIdleUnload);
			if (S.SetAiIdleUnload < 0) { S.SetAiIdleUnload = 0; }
			if (S.SetAiIdleUnload > 86400) { S.SetAiIdleUnload = 86400; }
			ImGui::EndTable();
		}
		ImGui::Text(Language::AIHint.c_str());
		ImGui::Text(Language::AIDeviceHint.c_str());
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
	}

	//页2 快捷键
	void ImGuiInterFace::DrawSettingsHotkey()
	{
		const char* CharMakeUpS[2] = { "Alt","Ctrl" };
		if (BeginSettingsTable("##tbl_hotkey"))
		{
			RowLabel(Language::KeyCombination.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##makeup", CharMakeUpS[S.SetMakeUp], flags))
			{
				for (int n = 0; n < 2; n++)
				{
					const bool is_selected = (S.SetMakeUp == n);
					if (ImGui::Selectable(CharMakeUpS[n], is_selected))
						S.SetMakeUp = n;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			RowLabel(Language::ScreenshotTranslation.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputText("##screenshot_key", S.SetScreenshotkey, IM_ARRAYSIZE(S.SetScreenshotkey));
			RowLabel(Language::SelectTranslation.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputText("##choice_key", S.SetChoicekey, IM_ARRAYSIZE(S.SetChoicekey));
			RowLabel(Language::ReplaceTranslation.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputText("##replace_key", S.SetReplacekey, IM_ARRAYSIZE(S.SetReplacekey));
			ImGui::EndTable();
		}
	}

	//页3 常规
	void ImGuiInterFace::DrawSettingsGeneral()
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
	}

	//页4 子段：识别模型 / 字体 / 界面语言
	void ImGuiInterFace::DrawSettingsInterfaceFonts()
	{
		if (BeginSettingsTable("##tbl_interface"))
		{
			RowLabel(Language::TesseractModel.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (S.ModelS.size() != 0) {
				if (ImGui::BeginCombo("##tesseract_model", S.ModelS[S.ModelIndex].c_str(), flags))
				{
					for (int n = 0; n < S.ModelS.size(); n++)
					{
						const bool is_selected = (S.ModelIndex == n);
						if (ImGui::Selectable(S.ModelS[n].c_str(), is_selected))
							S.ModelIndex = n;
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
				//启动器路径：用宽字符 API 取，程序装在中文目录下也能正常打开文件夹
				wchar_t buffer[MAX_PATH] = { 0 };
				GetCurrentDirectoryW(MAX_PATH, buffer);//获取启动器路径
				//拼接为绝对路径
				std::wstring Folder(buffer);
				Folder += L"\\TTF";
				ShellExecuteW(NULL, L"open", Folder.c_str(), NULL, NULL, SW_SHOWDEFAULT);//打开文件夹
			}
			ImGui::SameLine();
			if (ImGui::Button(Language::TessDataFolder.c_str())) {
				wchar_t buffer[MAX_PATH] = { 0 };
				GetCurrentDirectoryW(MAX_PATH, buffer);//获取启动器路径
				//拼接为绝对路径
				std::wstring Folder(buffer);
				Folder += L"\\TessData";
				ShellExecuteW(NULL, L"open", Folder.c_str(), NULL, NULL, SW_SHOWDEFAULT);//打开文件夹
			}
			RowLabel(Language::TTF_Typeface.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (Variable::FontBool) {
				if (S.FontS.size() != 0) {
					if (ImGui::BeginCombo("##ttf_typeface", S.FontS[S.FontIndex].c_str(), flags))
					{
						for (int n = 0; n < S.FontS.size(); n++)
						{
							const bool is_selected = (S.FontIndex == n);
							if (ImGui::Selectable(S.FontS[n].c_str(), is_selected))
								S.FontIndex = n;
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
				for (int n = 0; n < Variable::BaiduitemsName.size() - 1; n++)
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
			if (ImGui::BeginCombo("##interface_language", S.LanguageS[S.LanguageIndex].c_str(), flags))
			{
				for (int n = 0; n < S.LanguageS.size(); n++)
				{
					const bool is_selected = (S.LanguageIndex == n);
					if (ImGui::Selectable(S.LanguageS[n].c_str(), is_selected))
						S.LanguageIndex = n;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::EndTable();
		}
	}

	//页4 子段：渲染设备选择
	void ImGuiInterFace::DrawSettingsRenderDevice()
	{
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
				if (S.SetVulkanDeviceMode < 0 || S.SetVulkanDeviceMode >= (int)RenderDeviceItems.size())
				{
					S.SetVulkanDeviceMode = 0;//列表变了（比如换了显卡）就退回第一项，避免越界
				}
				if (ImGui::BeginCombo("##render_device", RenderDeviceItems[S.SetVulkanDeviceMode], flags))
				{
					for (int n = 0; n < (int)RenderDeviceItems.size(); n++)
					{
						const bool is_selected = (S.SetVulkanDeviceMode == n);
						if (ImGui::Selectable(RenderDeviceItems[n], is_selected))
						{
							S.SetVulkanDeviceMode = n;
							if (n >= 3 && (size_t)(n - 3) < Variable::VulkanDetectedDevices.size())
							{
								//第 3 项往后都是具体设备，记下名字，重启后按名字找
								S.PendingVulkanDeviceMode = Variable::VulkanDeviceModeEnum::Specific;
								S.PendingVulkanDeviceName = Variable::VulkanDetectedDevices[n - 3].name;
							}
							else
							{
								const int Mode = (n < 0) ? 0 : ((n > 2) ? 2 : n);
								S.PendingVulkanDeviceMode = (Variable::VulkanDeviceModeEnum)Mode;
							}
						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				S.VulkanDeviceModeChanged = (S.PendingVulkanDeviceMode != Variable::VulkanDeviceMode)
					|| (S.PendingVulkanDeviceMode == Variable::VulkanDeviceModeEnum::Specific && S.PendingVulkanDeviceName != Variable::VulkanDeviceName);
			}
			ImGui::EndTable();
		}
		if (S.VulkanDeviceModeChanged)
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
	}

	//页4 子段：截图颜色 / 截图脚本
	void ImGuiInterFace::DrawSettingsInterfaceMisc()
	{
		if (BeginSettingsTable("##tbl_interface2"))
		{
			RowLabel(Language::ScreenshotColor.c_str());
			ImGui::ColorEdit4("##screenshot_color", (float*)&S.ScreenshotColor, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);
			RowLabel(Language::Script.c_str());
			ImGui::Checkbox("##script", &S.LScriptBool);//同名会出现冲突
			if (S.LScriptBool) {
				if (S.ScriptS.size() != 0) {
					ImGui::SameLine();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::BeginCombo("##script_combo", S.ScriptS[S.ScriptIndex].c_str(), flags))
					{
						for (int n = 0; n < S.ScriptS.size(); n++)
						{
							const bool is_selected = (S.ScriptIndex == n);
							if (ImGui::Selectable(S.ScriptS[n].c_str(), is_selected))
								S.ScriptIndex = n;
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
	}

	//页4 界面：字体 / 渲染设备 / 其它
	void ImGuiInterFace::DrawSettingsInterface()
	{
		DrawSettingsInterfaceFonts();
		DrawSettingsRenderDevice();
		DrawSettingsInterfaceMisc();
	}

	//页5 一言
	void ImGuiInterFace::DrawSettingsHitokoto()
	{
		if (BeginSettingsTable("##tbl_hitokoto"))
		{
			RowLabel(Language::PopUpNotification.c_str());
			ImGui::Checkbox("##popup_notification", &S.SetPopUpNotificationBool);
			if (S.SetPopUpNotificationBool) {
				RowLabel(Language::IndependentTypeface.c_str());
				ImGui::Checkbox("##hitokoto_font", &S.SetHitokotoFontBool);
				if (S.SetHitokotoFontBool) {
					RowLabel(Language::InternalFontPattern.c_str());
					ImGui::Checkbox("##hitokoto_ttf", &S.SetHitokotoTTFBool);
					RowLabel(Language::TTF_Typeface.c_str());
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (!S.SetHitokotoTTFBool) {
						if (S.FontS.size() != 0) {
							if (ImGui::BeginCombo("##hitokoto_typeface", S.FontS[S.SetHitokotoFontIndex].c_str(), flags))
							{
								for (int n = 0; n < S.FontS.size(); n++)
								{
									const bool is_selected = (S.SetHitokotoFontIndex == n);
									if (ImGui::Selectable(S.FontS[n].c_str(), is_selected))
										S.SetHitokotoFontIndex = n;
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
				ImGui::SliderFloat("##hitokoto_posx", &S.SetHitokotoPosX, 0.0f, 1.0f);
				RowLabel(Language::PositionY.c_str());
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::SliderFloat("##hitokoto_posy", &S.SetHitokotoPosY, 0.0f, 1.0f);
				RowLabel(Language::HitokotoTimeInterval.c_str());
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputInt("##hitokoto_interval", &S.SetHitokotoTimeInterval);
				RowLabel(Language::HitokotoDisplayDuration.c_str());
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputInt("##hitokoto_duration", &S.SetHitokotoDisplayDuration);
				RowLabel(Language::HitokotoFontSize.c_str());
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputFloat("##hitokoto_fontsize", &S.SetHitokotoFontSize);
			}
			ImGui::EndTable();
		}
	}

	//页6 子段：备份恢复（从坚果云 WebDav 拉回来）
	void ImGuiInterFace::DrawSettingsRecovery()
	{
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f);
		if (ImGui::Button(S.RecoveryWindow ? Language::Return.c_str() : Language::Recovery.c_str())) {
			if (S.RecoveryWindow) {
				S.RecoveryWindow = false;
			}
			else {
				S.RecoveryWindow = true;
				S.RecoveryList = WebDav_List(Variable::WebDav_WebFile + "/");
				S.RecoveryIndex = 0;
				S.RecoveryChoice = 0;

				Variable::WebDav_url = S.SetWebDav_url;
				Variable::WebDav_username = S.SetWebDav_username;
				Variable::WebDav_password = S.SetWebDav_password;
				Variable::WebDav_WebFile = S.SetWebDav_WebFile;
			}
		}

		if (S.RecoveryWindow && (S.RecoveryList.size() != 0))
		{
			if (ImGui::BeginCombo(Language::RecoveryList.c_str(), S.RecoveryList[S.RecoveryIndex].c_str(), flags))
			{
				for (int n = 0; n < S.RecoveryList.size(); n++)
				{
					const bool is_selected = (S.RecoveryIndex == n);
					if (ImGui::Selectable(S.RecoveryList[n].c_str(), is_selected))
						S.RecoveryIndex = n;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (S.RecoveryChoice == 0) {
				ImGui::SameLine();
				if (ImGui::Button(Language::Restoration.c_str())) {
					S.RecoveryChoice = 1;
				}
				ImGui::SameLine();
				if (ImGui::Button(Language::Delete.c_str())) {
					S.RecoveryChoice = 2;
				}
			}
			else if (S.RecoveryChoice == 1) {
				ImGui::SameLine();
				if (ImGui::Button(Language::Cancel.c_str())) {
					S.RecoveryChoice = 0;
				}
				ImGui::SameLine();
				if (ImGui::Button(Language::Confirm.c_str())) {
					S.RecoveryChoice = 0;
					S.RecoveryWindow = false;
					WebDav_DownloadDirectory(S.RecoveryList[S.RecoveryIndex]);

					SetBool = true;
					Variable::ReadFile(iniData);
				}
			}
			else {
				ImGui::SameLine();
				if (ImGui::Button(Language::Confirm.c_str())) {
					S.RecoveryChoice = 0;
					WebDav_Delete(S.RecoveryList[S.RecoveryIndex].c_str());
					S.RecoveryList[S.RecoveryIndex] = S.RecoveryList.back();
					S.RecoveryList.pop_back();
					S.RecoveryIndex = 0;
					if (S.RecoveryList.size() == 0) {
						S.RecoveryWindow = false;
					}
				}
				ImGui::SameLine();
				if (ImGui::Button(Language::Cancel.c_str())) {
					S.RecoveryChoice = 0;
				}
			}

		}
	}

	//页6 备份（坚果云 WebDav）
	void ImGuiInterFace::DrawSettingsBackup()
	{
		if (ImGui::Button(Language::jianguoyunWebDav.c_str())) {
			ShellExecute(NULL, "open", "https://www.jianguoyun.com/", NULL, NULL, SW_SHOWMAXIMIZED);
		}
		if (BeginSettingsTable("##tbl_backup"))
		{
			RowLabel(Language::ServerAddress.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			InputInfo.LText = S.SetWebDav_url;
			ImGui::InputText("##webdav_url", S.SetWebDav_url, IM_ARRAYSIZE(S.SetWebDav_url), flags, &InputKeyEvent, &InputInfo);
			RowLabel(Language::Account.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			InputInfo.LText = S.SetWebDav_username;
			ImGui::InputText("##webdav_username", S.SetWebDav_username, IM_ARRAYSIZE(S.SetWebDav_username), flags, &InputKeyEvent, &InputInfo);
			RowLabel(Language::SecretKey.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			InputInfo.LText = S.SetWebDav_password;
			ImGui::InputText("##webdav_password", S.SetWebDav_password, IM_ARRAYSIZE(S.SetWebDav_password), flags, &InputKeyEvent, &InputInfo);
			RowLabel(Language::ApplyName.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			InputInfo.LText = S.SetWebDav_WebFile;
			ImGui::InputText("##webdav_webfile", S.SetWebDav_WebFile, IM_ARRAYSIZE(S.SetWebDav_WebFile), flags, &InputKeyEvent, &InputInfo);
			ImGui::EndTable();
		}

		ImGui::Checkbox("Opcode/", &S.LDirectory_Opcode);
		ImGui::SameLine();
		ImGui::Checkbox("Language/", &S.LDirectory_Language);
		ImGui::SameLine();
		ImGui::Checkbox("TessData/", &S.LDirectory_TessData);
		ImGui::SameLine();
		ImGui::Checkbox("TTF/", &S.LDirectory_TTF);
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

			char computerName[MAX_COMPUTERNAME_LENGTH + 1];
			DWORD size = sizeof(computerName);

			GetComputerNameA(computerName, &size);

			std::string BackupsName = std::string(computerName) + "_" + toString(year) + "_" + toString(month) + "_" + toString(day);

			std::cout << BackupsName << std::endl;

			if (!WebDav_Directory(S.SetWebDav_WebFile, BackupsName)) {
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
			if (S.LDirectory_Opcode)WebDav_UploadDirectory(Exename + "Opcode\\", BackupsName, "Opcode");
			if (S.LDirectory_Language)WebDav_UploadDirectory(Exename + "Language\\", BackupsName, "Language");
			if (S.LDirectory_TessData)WebDav_UploadDirectory(Exename + "TessData\\", BackupsName, "TessData");
			if (S.LDirectory_TTF)WebDav_UploadDirectory(Exename + "TTF\\", BackupsName, "TTF");
		}
		DrawSettingsRecovery();
	}

	//页7 关于
	void ImGuiInterFace::DrawSettingsAbout()
	{
		ImGui::TextUnformatted(Language::AboutText.c_str());
		ImGui::Separator();
		ImGui::TextUnformatted("v2.0.0");//工程的版本号没有对应变量，先写死一个
		if (ImGui::Button(u8"GitHub")) {
			ShellExecute(NULL, "open", "https://github.com/wuxingwushu/TranslatorKyi", NULL, NULL, SW_SHOWMAXIMIZED);//打开链接
		}
		ImGui::Spacing();
	}

	//设置界面：窗口骨架 + 左侧导航 + 按当前页分发到各 Draw*Section + 底部栏
	void ImGuiInterFace::SetUpInterface()
	{
		if (SetBool) {
			SetBool = false;
			InitSettingsState();
		}

		const ImVec4 AccentColor(0.282f, 0.780f, 0.486f, 1.0f);
		const ImU32 AccentU32 = ImGui::GetColorU32(AccentColor);
		//每页第一行的小标题：AI/快捷键沿用原来的段标题，其余用导航名
		const char* PageTitles[8] = {
			Language::NavTranslate.c_str(), Language::AIModel.c_str(), Language::ShortcutKeys.c_str(), Language::NavGeneral.c_str(),
			Language::NavInterface.c_str(), Language::NavHitokoto.c_str(), Language::NavBackup.c_str(), Language::NavAbout.c_str()
		};

		ImGui::SetNextWindowSize(ImVec2(760.0f, 640.0f), ImGuiCond_FirstUseEver);//第一次显示时的默认大小
		ImGui::SetNextWindowSizeConstraints(ImVec2(560.0f, 380.0f), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("SetUI", &SetBool, ImGuiWindowFlags_NoTitleBar);//创建窗口

		//标题条：一条绿色竖条 + 标题 + 右侧灰色版本号
		{
			const char* VersionText = "v2.0.0";
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

		//左侧分类导航
		DrawSettingsNav();

		//右侧内容区
		ImGui::BeginChild("##page", ImVec2(0.0f, -FooterH));
		{
			//页面小标题
			ImGui::TextUnformatted(PageTitles[S.SetPage]);
			ImGui::Separator();

			switch (S.SetPage)
			{
			case 0: DrawSettingsTranslate(); break;//翻译服务：百度 / 有道的账号密钥
			case 1: DrawSettingsAI(); break;//AI 模型（本地 llama.cpp）
			case 2: DrawSettingsHotkey(); break;//快捷键
			case 3: DrawSettingsGeneral(); break;//常规
			case 4: DrawSettingsInterface(); break;//界面
			case 5: DrawSettingsHitokoto(); break;//一言
			case 6: DrawSettingsBackup(); break;//备份（坚果云 WebDav）
			case 7: DrawSettingsAbout(); break;//关于
			default: S.SetPage = 0; break;
			}

			//所有输入框都画完了：延迟粘贴（Ctrl+V）统一在这里落到当前输入框里
			InputText();
		}
		ImGui::EndChild();

		//底部固定栏：保存 / GitHub / 关闭 + 「已保存」提示
		ImGui::Separator();
		if (ImGui::Button(Language::Save.c_str(), ImVec2(96.0f, 0.0f))) {
			SaveSettings();
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
		ImGui::Text("%s", HitokotoText().c_str());//取到之后自动显示（后台线程取的，不阻塞界面）
		if ((S.SavedTime != 0) && ((clock() - S.SavedTime) < (2 * CLOCKS_PER_SEC))) {
			//右对齐显示「已保存」
			const float SavedWidth = ImGui::CalcTextSize(Language::Saved.c_str()).x;
			ImGui::SameLine(ImGui::GetWindowWidth() - SavedWidth - ImGui::GetStyle().WindowPadding.x);
			ImGui::TextColored(AccentColor, "%s", Language::Saved.c_str());
		}
		//Ctrl+S 也触发保存（焦点在窗口里，含子窗口）
		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsWindowFocused(ImGuiHoveredFlags_ChildWindows) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
			SaveSettings();
		}

		BeginWindowPosX = (int)ImGui::GetWindowPos().x;
		BeginWindowPosY = (int)ImGui::GetWindowPos().y;
		BeginWindowSizeX = (int)ImGui::GetWindowWidth();
		BeginWindowSizeY = (int)ImGui::GetWindowHeight();
		ImGui::End();
	}
}