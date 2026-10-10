#include "../Interface.h"

// =====================================================================================
// 一言（Hitokoto）句子显示窗口
// =====================================================================================
namespace GAME {

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

}