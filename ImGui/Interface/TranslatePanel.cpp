#include "../Interface.h"
#include "InterfaceInternal.h"
#include "../../AngelScript/AngelScriptCode.h"
#include "../../Function/tesseract.h"
#include <cstdio>//翻译记录落盘用的 fgetc/fwrite/fclose

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

		//鼠标所在那块屏幕的可用区域（rcWork，已经扣掉任务栏）。
		//多显示器下不能用 GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)：那只是主屏尺寸，
		//鼠标在副屏（可能是负坐标）时会把窗口算到屏幕外，或者被夹回主屏。
		RECT CursorMonitorWorkRect() {
			RECT Work = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
			POINT Cursor = { 0, 0 };
			GetCursorPos(&Cursor);
			HMONITOR Monitor = MonitorFromPoint(Cursor, MONITOR_DEFAULTTONEAREST);
			MONITORINFO Info = {};
			Info.cbSize = sizeof(MONITORINFO);
			if ((Monitor != nullptr) && GetMonitorInfo(Monitor, &Info)) {
				Work = Info.rcWork;
			}
			return Work;
		}
	}

	// =====================================================================================
	// 翻译记录（只留最近 20 条）
	// -------------------------------------------------------------------------------------
	//  存放位置：与 Data.ini 同目录的 TranslateHistory.dat。
	//  格式：长度前缀的纯文本 —— 每条记录先写「字节数 + 换行」，再写原始字节 + 换行，
	//        原文/译文各一段。译文里本来就有换行和空格（自动换行/多段），
	//        按行读会被拆散，所以不能一行一条。
	// =====================================================================================
	namespace {
		constexpr int TranslateHistoryMax = 20;//只保留最近 20 次翻译
		const char* TranslateHistoryPath = "./TranslateHistory.dat";
	}

	//把多行文本压成一行：列表里每条记录只占一行，换行/制表符都换成空格
	//（声明在 InterfaceInternal.h，设置页的「翻译记录」也用同一个）
	std::string HistoryOneLine(const std::string& Text) {
		std::string Out = Text;
		for (char& Ch : Out) {
			if ((Ch == '\r') || (Ch == '\n') || (Ch == '\t')) { Ch = ' '; }
		}
		return Out;
	}

	void ImGuiInterFace::TranslateHistoryLoad()
	{
		TranslateHistoryLoaded = true;
		TranslateHistoryList.clear();

		FILE* File = TOOL::OpenUtf8File(TranslateHistoryPath, L"rb");
		if (File == nullptr) { return; }//第一次运行：还没有这个文件，正常

		//长度前缀读取：先读字节数，再按字节数读原文（内容里的换行不影响解析）
		auto ReadField = [&](std::string& Out) -> bool {
			int Length = -1;
			if (fscanf(File, "%d", &Length) != 1) { return false; }
			if (fgetc(File) == EOF) { return false; }//吃掉长度行末尾的换行
			if (Length < 0) { return false; }
			Out.assign((size_t)Length, '\0');
			if ((Length > 0) && (fread(&Out[0], 1, (size_t)Length, File) != (size_t)Length)) { return false; }
			fgetc(File);//吃掉内容末尾的换行
			return true;
		};

		while ((int)TranslateHistoryList.size() < TranslateHistoryMax) {
			TranslateHistoryItem Item;
			if (!ReadField(Item.Source)) { break; }//文件读完/写坏：保留已经读到的部分
			if (!ReadField(Item.Target)) { break; }
			TranslateHistoryList.push_back(Item);
		}
		fclose(File);
	}

	void ImGuiInterFace::TranslateHistorySave()
	{
		FILE* File = TOOL::OpenUtf8File(TranslateHistoryPath, L"wb");
		if (File == nullptr) {
			TOOL::logger->warn("translate history: cannot write {}", TranslateHistoryPath);
			return;
		}
		for (const TranslateHistoryItem& Item : TranslateHistoryList) {
			fprintf(File, "%d\n", (int)Item.Source.size());
			if (!Item.Source.empty()) { fwrite(Item.Source.data(), 1, Item.Source.size(), File); }
			fputc('\n', File);
			fprintf(File, "%d\n", (int)Item.Target.size());
			if (!Item.Target.empty()) { fwrite(Item.Target.data(), 1, Item.Target.size(), File); }
			fputc('\n', File);
		}
		fclose(File);
	}

	void ImGuiInterFace::TranslateHistoryAdd(const std::string& Source, const std::string& Target)
	{
		if (Source.empty()) { return; }//没有原文（比如翻译失败）就不记
		if (!TranslateHistoryLoaded) { TranslateHistoryLoad(); }

		//同一句原文重复翻译时只留最新那条：否则连点几次「翻译」列表就被同一句刷满了
		for (size_t i = 0; i < TranslateHistoryList.size(); ++i) {
			if (TranslateHistoryList[i].Source == Source) {
				TranslateHistoryList.erase(TranslateHistoryList.begin() + i);
				break;
			}
		}
		TranslateHistoryItem Item;
		Item.Source = Source;
		Item.Target = Target;
		TranslateHistoryList.insert(TranslateHistoryList.begin(), Item);//新的排最前面
		if ((int)TranslateHistoryList.size() > TranslateHistoryMax) {
			TranslateHistoryList.resize(TranslateHistoryMax);//超出的旧记录直接丢掉
		}
		TranslateHistorySave();
	}

	//「记录」按钮展开的列表：点一条把原文/译文填回窗口（不重新翻译）
	void ImGuiInterFace::TranslateHistoryPopup()
	{
		if (!TranslateHistoryOpen) { return; }

		const int Count = (int)TranslateHistoryList.size();
		const float LineHeight = ImGui::GetTextLineHeight();
		const float PadY = ImGui::GetStyle().FramePadding.y;
		//列表高度：按条数长，但最少 4 行、最多 10 行（再多就滚动）
		const float ListHeight = std::min(
			std::max(LineHeight * 4.0f, LineHeight * (float)std::min(Count, 8) + PadY * 2.0f),
			LineHeight * 10.0f);

		ImGui::Separator();
		if (ImGui::Button(Language::HistoryClear.c_str())) {
			TranslateHistoryList.clear();
			TranslateHistorySave();
		}
		ImGui::SameLine();
		if (ImGui::Button(Language::Close.c_str())) { TranslateHistoryOpen = false; }

		if (Count == 0) {
			ImGui::TextDisabled("%s", Language::HistoryEmpty.c_str());
			return;
		}

		ImGui::BeginChild("##history", ImVec2(-FLT_MIN, ListHeight), true);
		for (int i = 0; i < Count; ++i) {
			const TranslateHistoryItem& Item = TranslateHistoryList[i];
			//每条只画一行：多行文本压成一行，完整内容挂在 tooltip 上。
			//用 PushID 而不是在标签后面拼 "##n"：原文里本来就可能出现 "##"，
			//拼进去会被 ImGui 当成「显示文本/ID」的分隔符，把显示内容截断。
			ImGui::PushID(i);
			const std::string Label = HistoryOneLine(Item.Source);
			if (ImGui::Selectable(Label.c_str())) {
				//填回窗口：只改显示内容，不再发一次翻译请求
				eng = Item.Source;
				zhong = Item.Target;
				Variable::eng = eng;
				Variable::zhong = zhong;
				TranslateHistoryOpen = false;
				TranslateTime = clock();//窗口刚变矮，别让鼠标落到矩形外被立刻关掉
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s\n\n%s", Item.Source.c_str(), Item.Target.c_str());
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
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
				//注意这里是 ShowTranslateWindow() 而不是 SetInterFace()：截图翻译的 OCR 跑完会
				//从这里接着发起翻译，用户要是识别期间已经点了「X」，不能再把窗口弹回来。
				//用户自己发起的新翻译不靠这里显示（调用方随后会 SetInterFace(TranslateEnum)），
				//所以那条路不受影响。
				ShowTranslateWindow();
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
			//同上面普通翻译源那条路：截图 OCR 跑完会从这里接着发起翻译，
			//用户识别期间点过「X」的话不能再把窗口弹回来（用户自己发起的新翻译由调用方
			//SetInterFace(TranslateEnum) 显示，不受影响）。
			ShowTranslateWindow();
		}

		mTranslate->AiBeginTranslation(English, AiTargetLangCode());
		return false;
	}

	//把一次「选择翻译」真正发出去（原文已经在界面上摆好了）。
	//调用方保证此刻没有脚本在后台跑（脚本共用一个 asIScriptContext，不能并发）。
	void ImGuiInterFace::StartSelectionTask(const std::string& English)
	{
		AngelScriptOpcode::AngelScriptCode* Script = AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode();

		if (Script->GetOpenBool() && Variable::ScriptBool)
		{
			//脚本模式：整个脚本丢到后台线程里跑（脚本里的 TranslateAPI() 是同步的，不能让主循环等它），
			//原文通过信箱交给脚本（见 AngelScriptCode::SetScriptInput），跑完由 UpdateTranslateTask() 取回。
			Script->SetScriptInput(English);
			ScriptRunning = Script->BeginRun(Script->ChoiceFunction);
			if (ScriptRunning) { return; }

			//脚本没起来（函数句柄为空等）：退回去走普通翻译流程，别让界面一直停在「翻译中…」
			TOOL::logger->warn("selection translate: script did not start, falling back to translate");
		}

		//非脚本模式：交给统一入口（普通源起 HTTP 线程、本地 AI 起模型线程），
		//它同样会摆好「翻译中…」并保证窗口显示出来
		RequestTranslate(English);
	}

	//「选择翻译」（Alt+Q / Ctrl+Q）：和截图翻译一样，窗口必须**一按就出来**。
	//以前脚本模式是在主循环里同步 RunFunction(ChoiceFunction) 的：脚本里的 TranslateAPI() 是同步接口，
	//要等翻译回来才返回，于是调用方那句 SetInterFace(TranslateEnum) 得到译文算完才执行 ——
	//用户看到的就是「按下快捷键后卡一下，窗口才出来」。现在顺序倒过来：
	//先把原文 +「翻译中…」摆上并显示窗口，脚本 / HTTP / AI 全在它背后跑，
	//结果由 UpdateTranslateTask() 每帧填回界面（和截图翻译完全同一条路）。
	void ImGuiInterFace::BeginSelectionTranslate(const std::string& English)
	{
		AngelScriptOpcode::AngelScriptCode* Script = AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode();

		//先把界面摆上：原文 +「翻译中…」。这是用户自己发起的新一轮翻译，用 SetInterFace()
		//（它会顺带清掉 TranslateWindowClosed，所以之前用「X」关过窗口现在也能重新弹出来）。
		Variable::eng = English;
		Variable::zhong = Language::Translating;
		eng = Variable::eng;
		zhong = Variable::zhong;
		SetInterFace(TranslateEnum);

		//脚本模式、且上一次脚本还在后台跑（AI 源可能几十秒）：
		//**排队**等它跑完，绝不能在这里等 —— 那是主线程上的 join，界面会整个卡死（用户报的卡死就是这个）。
		//连按多次时只保留最后一次要翻的文本。
		if ((Script->GetOpenBool() && Variable::ScriptBool) && Script->IsRunning())
		{
			SelectionPending = true;
			PendingSelectionText = English;
			return;
		}

		//上一次脚本刚好跑完、而主循环还没来得及收走（ScriptRunning 还挂着）：
		//先在这里结算掉，免得 UpdateTranslateTask() 把这次残留当成「本次脚本刚结束」而提前 return。
		if (ScriptRunning && !Script->IsRunning()) { ScriptRunning = false; }

		StartSelectionTask(English);
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
			AngelScriptOpcode::AngelScriptCode* Script = AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode();
			if (Script->IsRunning()) { return; }//还没跑完

			ScriptRunning = false;

			//排队中的新一轮「选择翻译」：用户已经按过了，界面上摆的是新原文 +「翻译中…」。
			//这一轮的旧结果直接丢掉（不然会把用户眼前的新一轮覆盖成上一句的译文），接着发新的。
			if (SelectionPending)
			{
				StartPendingSelection();
				return;
			}

			//跑完了：脚本通过信箱写回的原文/译文同步到界面并开始计时
			eng = Script->ScriptInput();
			zhong = Script->ScriptOutput();
			Variable::eng = eng;
			Variable::zhong = zhong;
			TranslateHistoryAdd(eng, zhong);//记一笔（选择翻译/截图脚本走的都是这条路）
			if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum)) { TranslateTime = clock(); }
			return;
		}

		//没有脚本在跑：把排队中的「选择翻译」发出去（上一次脚本已经结束、排队的那次终于轮到）
		if (SelectionPending)
		{
			StartPendingSelection();
			return;
		}

		//脚本模式：识别出的原文已经在上一帧画到界面上了，现在才把脚本交给后台线程跑。
		//（脚本里的 TranslateAPI() 是同步接口，跑在后台线程里，不会再卡住主循环。）
		if (ScriptPending)
		{
			ScriptPending = false;
			ScriptRunning = true;
			AngelScriptOpcode::AngelScriptCode::GetAngelScriptCode()->SetScriptInput(Variable::eng);
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
			TranslateHistoryAdd(Source, Result);//原文用 Source：eng 可能已经被用户改过
			//译文已经写进界面了：显示时长（Variable::DisplayTime）从这一刻才开始算。
			if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum))
			{
				TranslateTime = clock();
			}
			else
			{
				ShowTranslateWindow();//用户点过「X」就不再弹（见 TranslateWindowClosed）
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
		TranslateHistoryAdd(AiTaskSourceText, zhong);//原文用 AiTaskSourceText：eng 可能已经被用户改过
		if (GetInterFaceBool() && (GetInterFaceEnum() == TranslateEnum))
		{
			//窗口已经开着：只续时间，别再 SetInterFace()（那会把窗口重新挪到鼠标位置）
			TranslateTime = clock();
		}
		else
		{
			ShowTranslateWindow();//用户点过「X」就不再弹（见 TranslateWindowClosed）
		}
	}

	void ImGuiInterFace::TranslateInterface()
	{
		//自适应尺寸：宽度可以拖（记在 TranslateWinWidth，默认 380），高度每帧按内容算出来
		static float TranslateWinWidth = 380.0f;//窗口宽度（拖动后被记住）
		static float TranslateWinHeight = 320.0f;//窗口高度（每帧按内容重算，不用手拖）
		static float TrChromeLast = 0.0f;//上一帧实测的「非文本框部分」高度（顶上那几行 + 内外边距），用来算窗口还放不放得下
		//鼠标所在屏幕的可用区域：窗口弹出的位置和尺寸都要夹在它里面，
		//否则鼠标贴着屏幕右边/下边时，窗口会有一半露在屏幕外（用户报的「显示不全」）。
		const RECT TrWork = CursorMonitorWorkRect();
		const float TrWorkLeft = (float)TrWork.left;
		const float TrWorkTop = (float)TrWork.top;
		const float TrWorkRight = (float)TrWork.right;
		const float TrWorkBottom = (float)TrWork.bottom;
		const float TrWorkWidth = std::max(1.0f, TrWorkRight - TrWorkLeft);
		const float TrWorkHeight = std::max(1.0f, TrWorkBottom - TrWorkTop);
		//窗口比屏幕还宽时按屏幕宽收（窄屏/缩放 200% 的笔记本上真会发生）
		TranslateWinWidth = std::min(TranslateWinWidth, TrWorkWidth);

		if (TranslateBool) {
			TranslateBool = false;
			POINT MousePos = { 0,0 };
			GetCursorPos(&MousePos);//获取鼠标位置
			//以鼠标为左上角弹出；右边/下边放不下就往左/上挪，保证整个窗口都在屏幕里。
			//（ImGui 自己的 ClampWindowPos 只在「位置不是 API 设的」时才跑，这里每帧都用
			// SetWindowPos 定位，所以必须自己夹。）
			float PosX = float(MousePos.x);
			float PosY = float(MousePos.y);
			if (PosX + TranslateWinWidth > TrWorkRight) { PosX = TrWorkRight - TranslateWinWidth; }
			if (PosY + TranslateWinHeight > TrWorkBottom) { PosY = TrWorkBottom - TranslateWinHeight; }
			PosX = std::max(TrWorkLeft, PosX);
			PosY = std::max(TrWorkTop, PosY);
			ImGui::SetNextWindowPos(ImVec2(PosX, PosY));//设置窗口生成位置
			Variable::WrapSize = kuangshu / int(std::max(1.0f, Variable::FontSize));
		}
		ImGui::SetNextWindowSize(ImVec2(TranslateWinWidth, TranslateWinHeight));
		//最小尺寸也要跟着屏幕收：屏幕比 300x160 还小时，写死的下限会让窗口反而超出屏幕
		ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(300.0f, TrWorkWidth), std::min(160.0f, TrWorkHeight)), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin(u8"TranslateUI", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);//创建窗口

		//下拉框展开时鼠标落在弹出窗口上，主窗口的命中测试会失败——用 KeepAliveBool 把滞留计时续上。
		//ChildWindows 标志不能少：翻译记录列表是子窗口，鼠标停在子窗口上时父窗口的
		//IsWindowHovered() 默认返回 false，不加这个标志的话用户正在翻记录、窗口却被自动关掉。
		KeepAliveBool = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) || ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

		//Data.ini 里的语言列表可能被改短，下标兜个底
		const int LangCount = (int)Variable::Baiduitems.size();
		if (LangCount > 0) {
			if (mTranslate->mFrom < 0 || mTranslate->mFrom >= LangCount) { mTranslate->mFrom = 0; }
			if (mTranslate->mTo < 1 || mTranslate->mTo >= LangCount) { mTranslate->mTo = (LangCount > 1) ? 1 : 0; }
		}
		const char* FromName = (LangCount > 0) ? Variable::BaiduitemsName[mTranslate->mFrom].c_str() : "";
		const char* ToName = (LangCount > 0) ? Variable::BaiduitemsName[mTranslate->mTo].c_str() : "";

		//第一行：左边是翻译源（点一下循环切换），右边是「X」关闭键。
		//翻译源放最上面：它的宽度随引擎名变化，放在语言下拉那一行会挤到换行（原来就是这样），
		//单独占一行就不用再算「放不放得下」。
		const std::string EngineLabel = Language::Engine + ": " + mTranslate->TranslateName[mTranslate->mTranslate];
		if (ImGui::Button(EngineLabel.c_str())) {
			mTranslate->mTranslate++;
			if (mTranslate->mTranslate > Translate::AiTranslate) { mTranslate->mTranslate = 0; }
			//翻译源是模式开关：点一下立刻写回 Data.ini，不然重启后又变回百度
			Variable::Translate = mTranslate->mTranslate;
			Variable::SaveFile();
		}

		//「X」固定在右上角：按内容区右边缘反推出按钮左边缘应该在的 X，再让光标跳过去。
		//引擎名很长时（比如本地 AI 模型的名字）算出来的位置会落到按钮左边，
		//那种情况下保持紧跟其后，避免两个按钮叠在一起。
		const float CloseBtnWidth = ImGui::CalcTextSize("X").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float CloseOffsetX = ImGui::GetWindowContentRegionMax().x - CloseBtnWidth;
		ImGui::SameLine(0);
		if (CloseOffsetX > ImGui::GetCursorPosX()) { ImGui::SetCursorPosX(CloseOffsetX); }
		if (ImGui::Button("X")) {
			//关掉窗口，并且**记住是用户关的**：后台翻译/识别的结果回来时不能再把窗口弹出来，
			//否则刚点完「X」结果一到窗口又冒出来，等于关不掉。
			EndDisplayBool = true;
			InterFaceBool = false;
			ChildWindowBool = false;
			TranslateWindowClosed = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", Language::Close.c_str());
		}

		//第二行：源语言 / 互换 / 目标语言
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

		//文本框宽度（顺便算自动换行宽度，AngelScript 的 Autowrap 用）
		kuangshu = (int)ImGui::GetContentRegionAvail().x;
		Variable::WrapSize = kuangshu / int(std::max(1.0f, Variable::FontSize));

		//文本框高度自适应：按文本在「文本框自身宽度」下换行后的真实高度算；长文到上限后交给文本框自己滚动
		const ImGuiStyle& UiStyle = ImGui::GetStyle();
		const float LineHeight = ImGui::GetTextLineHeight();
		const float TextWrapWidth = std::max(40.0f, ImGui::GetContentRegionAvail().x - UiStyle.FramePadding.x * 2.0f);
		const float MinBoxHeight = LineHeight * 3.0f + UiStyle.FramePadding.y * 2.0f;
		//文本框高度上限：正常按屏幕可用高度的 40%；窗口贴着屏幕底部弹出时再按「顶到屏幕底还剩多少」收一次，
		//保证按钮和文本框都在屏幕里。这里用鼠标所在显示器的可用高度（rcWork），
		//不再用 GetSystemMetrics(SM_CYSCREEN)/windows_Heigth：那只是主屏，副屏或任务栏都会算错。
		const float TrChrome = (TrChromeLast > 1.0f) ? TrChromeLast : (LineHeight * 6.0f + UiStyle.ItemSpacing.y * 6.0f + UiStyle.WindowPadding.y * 2.0f);
		const float TrRoomForBoxes = TrWorkBottom - ImGui::GetWindowPos().y - TrChrome;
		float MaxBoxHeight = TrWorkHeight * 0.4f;
		if (TrRoomForBoxes > MinBoxHeight * 2.0f) { MaxBoxHeight = std::min(MaxBoxHeight, TrRoomForBoxes * 0.5f); }
		MaxBoxHeight = std::max(MinBoxHeight, MaxBoxHeight);
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
		if (TranslateFocusInput) {
			//快捷键唤出的空白窗口：把光标放进原文框，直接就能打字/粘贴
			TranslateFocusInput = false;
			ImGui::SetKeyboardFocusHere();
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
		ImGui::SameLine();
		//「记录」：展开/收起翻译记录列表（点一条把原文/译文填回窗口，不重新翻译）
		if (ImGui::Button(Language::History.c_str(), ImVec2(80.0f, 0.0f))) {
			TranslateHistoryOpen = !TranslateHistoryOpen;
			if (TranslateHistoryOpen && !TranslateHistoryLoaded) { TranslateHistoryLoad(); }
		}

		//译文
		InputTextMultilineString("##zhong", zhong, ImVec2(-FLT_MIN, ZhongBoxHeight), flags, MyText);

		//记录列表画在译文框下面：它在主窗口内部展开，窗口高度按内容自适应，
		//所以不需要动 BeginWindowSizeX_2 / ChildWindowBool 那套右侧子窗口机制。
		TranslateHistoryPopup();

		//自适应尺寸：宽度只在拖动时记（默认 380，最小 300——但屏幕比 300 还窄时以屏幕为准），
		//高度永远等于内容实际需要的高度
		if (GetKeyState(VK_LBUTTON) < 0) {
			const float WinW = ImGui::GetWindowWidth();
			if (WinW > 1.0f) { TranslateWinWidth = WinW; }
		}
		const float TrMinWidth = std::min(300.0f, TrWorkWidth);
		if (TranslateWinWidth < TrMinWidth) { TranslateWinWidth = TrMinWidth; }
		TranslateWinHeight = ImGui::GetCursorPosY() + UiStyle.WindowPadding.y;
		TrChromeLast = TranslateWinHeight - (EngBoxHeight + ZhongBoxHeight);//量一下这一帧「非文本框部分」到底有多高，下一帧用它算上限（第一帧先用估算值）
		ImGui::SetWindowSize(ImVec2(TranslateWinWidth, TranslateWinHeight));
		BeginWindowPosX = (int)ImGui::GetWindowPos().x;
		BeginWindowPosY = (int)ImGui::GetWindowPos().y;
		//窗口的实际宽高也必须记下来：DoYouWantToUpdateTheScreen() 用这四个值拼出「鼠标是不是停在窗口上」
		//的矩形。以前这里只记了位置，尺寸一直是最初的默认值 280x148（甚至会被一言面板改成 -1），
		//于是鼠标停在窗口的下半部分/右侧时命中测试失败 → 滞留计时不刷新 → 鼠标还在界面上窗口就被关掉了。
		BeginWindowSizeX = (int)ImGui::GetWindowWidth();
		BeginWindowSizeY = (int)ImGui::GetWindowHeight();
		//置顶用的窗口句柄必须在 End() 之前取（以前是 End() 之后 FindWindow(NULL, "TranslateUI")：
		//多视口下这个系统窗口是 ImGui 自己建的，按标题找可能找错实例，找不到时也就不置顶了）
		HWND hwnd = WindowTopMostHandle();
		ImGui::End();

		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

}