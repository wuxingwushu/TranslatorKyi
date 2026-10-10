#include "../Interface.h"
#include "InterfaceInternal.h"
#include "../../AngelScript/AngelScriptCode.h"
#include "../../Function/tesseract.h"

// =====================================================================================
// 翻译内容显示界面（TranslateInterface）+ 翻译任务编排
// -------------------------------------------------------------------------------------
//  · InputTextMultilineText / MyText       —— 文本框的 Ctrl+V / Ctrl+C 处理
//  · AiTargetLangCode                       —— 当前目标语言代码
//  · RequestTranslate / UpdateTranslateTask —— 统一翻译入口 + 每帧取结果
//  · TranslateInterface                     —— 窗口绘制
// =====================================================================================
namespace GAME {

	// InputTextMultiline 的 回调函数
	namespace {
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
				//选中的文本可能非常长，必须按目标栈数组的长度截断（旧代码把终止符写在
				//[长度+1] 越界一字节，长度本身也没有上限）。
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
			else if (!ImGui::IsItemDeactivated() && (GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)) {//判断是否有选中的文本
				if (TranslateInputBool) {
					mCursorPos = data->CursorPos;
					mTextLen = data->BufTextLen;
					InputCursorBool = true;
				}

			}
			return 0;
		}

		//ImGui 的输入框只认 char* 缓冲：用「自动扩容」回调把内容直接写进 std::string
		//（与官方 imgui_stdlib.h 同一套路），这样不用再维护 1MB 的定长数组。
		//MyText（光标 / 剪贴板处理）通过 user_data 链在后面继续调用。
		struct StringInputUserData {
			std::string* Str;
			ImGuiInputTextCallback Chain;
			void* ChainUserData;
		};

		int StringInputCallback(ImGuiInputTextCallbackData* data) {
			StringInputUserData* UserData = (StringInputUserData*)data->UserData;
			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
				std::string* Str = UserData->Str;
				IM_ASSERT(data->Buf == Str->data());
				Str->resize((size_t)data->BufTextLen);
				data->Buf = Str->data();
			}
			else if (UserData->Chain != nullptr) {
				data->UserData = UserData->ChainUserData;
				return UserData->Chain(data);
			}
			return 0;
		}

		bool InputTextMultilineString(const char* Label, std::string& Str, const ImVec2& Size,
			ImGuiInputTextFlags Flags, ImGuiInputTextCallback Chain = nullptr, void* ChainUserData = nullptr) {
			StringInputUserData UserData{ &Str, Chain, ChainUserData };
			Flags |= ImGuiInputTextFlags_CallbackResize;
			return ImGui::InputTextMultiline(Label, Str.data(), Str.capacity() + 1, Size, Flags, StringInputCallback, &UserData);
		}
	}

	void ImGuiInterFace::InputTextMultilineText() {
		if ((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0)) {
			while ((GetKeyState(VK_CONTROL) < 0) && (GetKeyState('V') < 0))
			{
				mWindown->pollEvents();
			}
			const std::string ClipboardText = TOOL::UnicodeToUtf8(TOOL::ClipboardTochar());
			//旧写法是在栈上的定长数组里手工拼三段再按容量截断拷回去；现在直接在 std::string 上插入。
			int PastePos = (mCursorPos < (int)eng.size()) ? mCursorPos : (int)eng.size();
			if (PastePos < 0) { PastePos = 0; }
			ImGui::ClearActiveID();//失去焦点，粘贴的内容才会被保存
			eng.insert((size_t)PastePos, ClipboardText);
			mCursorPos = PastePos + (int)ClipboardText.size();
		}
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
			eng = Variable::eng;
			zhong = Variable::zhong;
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
		Variable::zhong = TOOL::ReplaceToken(Language::AILoading, std::to_string(0));
		eng = Variable::eng;
		zhong = Variable::zhong;
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
			eng = Variable::eng;
			zhong = Variable::zhong;
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
			eng = Variable::eng;

			if (ScriptAfterOcr)
			{
				//脚本模式：原文先显示出来（译文框留「翻译中…」），脚本等**下一帧**再跑。
				//脚本里的 TranslateAPI() 是同步接口，跑起来主循环会卡住，不能让原文跟着一起等结果。
				ScriptAfterOcr = false;
				ScriptPending = true;
				Variable::zhong = Language::Translating;
				zhong = Variable::zhong;
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
			eng = Variable::eng;
			zhong = Variable::zhong;
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
				Variable::zhong = TOOL::ReplaceToken(Language::AILoading, std::to_string(ElapsedSeconds));
			}
			else
			{
				Variable::zhong = TOOL::ReplaceToken(Language::AITranslating, std::to_string(ElapsedSeconds));
			}
			zhong = Variable::zhong;

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
			Variable::zhong = Error.empty() ? Language::AIFailedEmpty : TOOL::ReplaceToken(Language::AIFailed, Error);
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
		eng = Variable::eng;
		zhong = Variable::zhong;

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
		const float EngBoxHeight = AdaptiveBoxHeight(eng.c_str());
		const float ZhongBoxHeight = AdaptiveBoxHeight(zhong.c_str());

		//原文
		if (InputCursorBool) {
			ImGui::SetKeyboardFocusHere();//窗口打开时把焦点放到原文框上
		}
		TranslateInputBool = true;
		InputTextMultilineString("##eng", eng, ImVec2(-FLT_MIN, EngBoxHeight), flags, MyText);
		if (InputCursorBool) {
			InputTextMultilineText();//将剪贴板内容粘贴到输入光标位置
		}
		TranslateInputBool = false;

		//按钮行
		if (ImGui::Button(Language::TranslationKey.c_str(), ImVec2(96.0f, 0.0f))) {
			//普通翻译源（百度/爬虫/有道）在这里同步出结果；
			//本地 AI 模型是后台算的，结果由 UpdateTranslateTask() 填进来
			RequestTranslate(eng);
			zhong = Variable::zhong;
		}
		ImGui::SameLine();
		if (ImGui::Button(Language::Clear.c_str(), ImVec2(80.0f, 0.0f))) {
			eng.clear();
			zhong.clear();
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(zhong.empty());
		if (ImGui::Button(Language::CopyResult.c_str(), ImVec2(110.0f, 0.0f))) {
			TOOL::CopyToClipboard(TOOL::Utf8ToUnicode(zhong.c_str()));
		}
		ImGui::EndDisabled();

		//译文
		InputTextMultilineString("##zhong", zhong, ImVec2(-FLT_MIN, ZhongBoxHeight), flags, MyText);

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
		//置顶用的窗口句柄必须在 End() 之前取（以前是 End() 之后 FindWindow(NULL, "TranslateUI")：
		//多视口下这个系统窗口是 ImGui 自己建的，按标题找可能找错实例，找不到时也就不置顶了）
		HWND hwnd = WindowTopMostHandle();
		ImGui::End();

		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

}