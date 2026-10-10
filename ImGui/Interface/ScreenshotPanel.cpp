#include "../Interface.h"
#include "../../Function/tesseract.h"
#include "../../AngelScript/AngelScriptCode.h"

// =====================================================================================
// 截图操作界面：框选、放大镜、把选区交给 OCR 识别
// =====================================================================================
namespace GAME {

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

		//置顶用的窗口句柄必须在 End() 之前取（以前是 End() 之后 FindWindow(NULL, "ScreenshotUI")：
		//多视口下这个系统窗口是 ImGui 自己建的，按标题找可能找错实例，找不到时也就不置顶了）
		HWND hwnd = WindowTopMostHandle();
		ImGui::End();

		if (hwnd) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

}