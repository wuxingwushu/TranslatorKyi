#include "../Interface.h"

// =====================================================================================
// 一言（Hitokoto）句子显示窗口
// =====================================================================================
namespace GAME {

	void ImGuiInterFace::HitokotoSentence() {
		static std::string Hitokoto;
		static std::string LaidOutHitokoto;//已经算过窗口宽度的那一句
		static float lenH = 0;
		static clock_t WaitStart = 0;//这一次弹窗是从什么时候开始等句子的
		if (HitokotoBool) {
			HitokotoBool = false;
			RequestHitokoto();//后台线程取：界面先出来，句子到了再显示，别在这里同步等网络
			WaitStart = clock();
		}

		//每帧读一次最近取到的句子：取不到时给的是上一次取到的那句（所以取词失败会显示上一句），
		//取到新的一句（含第一次取到）才重新算窗口宽度
		Hitokoto = HitokotoText();
		if (Hitokoto.empty()) {
			//这一次还没取到、手上也没有上一句：先不画窗口（总比卡住界面好），
			//但不能一直占着「一言」这个界面状态——它会挡住右键菜单等其它界面。
			//等够 10 秒还取不到就收摊，主循环回到 No_Enum，下个间隔到点再试。
			if ((clock() - WaitStart) > 10000) {
				EndDisplayBool = true;
				InterFaceBool = false;
				ChildWindowBool = false;
			}
			else {
				//把滞留计时续上：显示时长从「句子到手」那一刻开始算，
				//否则网络慢的时候弹窗刚到手就被 DoYouWantToUpdateTheScreen() 关掉了。
				TranslateTime = clock();
			}
			return;
		}
		if (Hitokoto != LaidOutHitokoto) {
			LaidOutHitokoto = Hitokoto;

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
		//置顶用的窗口句柄必须在 End() 之前取（以前是 End() 之后 FindWindow(NULL, "HitokotoUI")，
		//多视口下这个系统窗口是 ImGui 自己建的，按标题找不仅可能找错实例，找不到时也就不置顶了）
		HWND hwnd = WindowTopMostHandle();
		ImGui::End();

		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

}
