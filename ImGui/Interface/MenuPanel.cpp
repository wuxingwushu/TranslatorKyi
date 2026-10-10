#include "../Interface.h"

// =====================================================================================
// 菜单界面（托盘右键弹出的菜单）
// =====================================================================================
namespace GAME {

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
			//不再 exit(0)：置位后由主循环跳出循环、走 cleanUp() 的正常收尾通道（会等后台线程结束）。
			//exit(0) 不会析构堆上的对象，后台线程可能在进程退出过程中用到半销毁的成员。
			ExitRequestBool = true;
		}

		BeginWindowPosX = (int)ImGui::GetWindowPos().x;
		BeginWindowPosY = (int)ImGui::GetWindowPos().y;
		BeginWindowSizeX = (int)ImGui::GetWindowWidth();
		BeginWindowSizeY = (int)ImGui::GetWindowHeight();
		//置顶用的窗口句柄必须在 End() 之前取（以前是 End() 之后 FindWindow(NULL, "MenuUI")：
		//多视口下这个系统窗口是 ImGui 自己建的，按标题找可能找错实例，找不到时也就不置顶了）
		HWND hwnd = WindowTopMostHandle();
		ImGui::End();

		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

}