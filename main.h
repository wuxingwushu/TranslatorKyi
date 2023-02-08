#include "base.h"
#include "Tool.h"//数据格式转换
#include "imgui/GUI.h"//显示界面
#include "Function/tesseract.h"//图片识别文字
#include "Function/Translate.h"//翻译文字
#include "ini.h"//软件数据
using namespace inih;//启用 ini 读取
//#define	PI					3.14


//模板 转 字符串
template<typename T>std::string toString(const T& t);

//ini文件处理工具函数
std::string ColorArraytoString(int* color);
void GetColor(std::vector<int> colordata, int* color, float* set_color);
void PreservationSetColor(float* setcolor, int* color);

//读取INI数据
void IniDataInit();
//保存INI数据
void IniDataPreservation();


//IMGUI 初始化
ImGuiIO& IMGUI_init();
//加提示
void TipsUI(const char* label, const char* Tips);

//获得剪贴板的内容
std::string ClipboardTochar();
//将内容复制到剪贴板
void CopyToClipboard(std::string str);

//加载图片
ID3D11ShaderResourceView* DX11LoadTextureImageFromFile(LPCSTR lpszFilePath);
//截图&保存（全屏）
void screen(LPCSTR fileName);

//DX11
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);



/*
 ┌───┐   ┌───┬───┬───┬───┐ ┌───┬───┬───┬───┐ ┌───┬───┬───┬───┐ ┌───┬───┬───┐
 │Esc   │   │  F1  │  F2  │  F3  │  F4  │ │  F5  │  F6  │  F7  │  F8  │ │  F9  │  F10 │  F11 │  F12 │ │  P/S │  S L │  P/B │           ┌┐    ┌┐    ┌┐
 └───┘   └───┴───┴───┴───┘ └───┴───┴───┴───┘ └───┴───┴───┴───┘ └───┴───┴───┘           └┘    └┘    └┘
 ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───────┐ ┌───┬───┬───┐ ┌───┬───┬───┬───┐
 │  ~ ` │ ! 1  │ @ 2  │  # 3 │ $ 4  │ % 5  │ ^ 6  │ & 7  │ * 8  │ ( 9  │  ) 0 │ _ -  │ + =  │     BacSp    │ │ Ins  │ Hom  │  PUp │ │ N L  │   /  │   *  │   -  │
 ├───┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─────┤ ├───┼───┼───┤ ├───┼───┼───┼───┤
 │   Tab    │   Q  │   W  │   E  │   R  │   T  │   Y  │   U  │   I  │   O  │   P  │  { [ │  } ] │   | \    │ │  Del │  End │  PDn │ │   7  │   8  │   9  │      │
 ├─────┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴─────┤ └───┴───┴───┘ ├───┼───┼───┤   +  │
 │    Caps    │   A  │   S  │   D  │   F  │   G  │   H  │   J  │   K  │   L  │  : ; │  " ' │      Enter     │                            │   4  │   5  │   6  │      │
 ├──────┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴────────┤         ┌───┐         ├───┼───┼───┼───┤
 │     Shift      │   Z  │   X  │   C  │   V  │   B  │   N  │   M  │  < , │  > . │  ? / │        Shift       │         │  ↑  │         │   1  │   2  │   3  │      │
 ├─────┬──┴─┬─┴──┬┴───┴───┴───┴───┴───┴──┬┴───┼───┴┬────┬────┤ ┌───┼───┼───┐ ├───┴───┼───┤   E││
 │   Ctrl   │        │   Alt  │                    Space                     │  Alt   │        │        │   Ctrl │ │  ←  │  ↓  │  →  │ │   0          │  .   │←─┘│
 └─────┴────┴────┴───────────────────────┴────┴────┴────┴────┘ └───┴───┴───┘ └───────┴───┴───┘
*/



static LPTSTR lpPath = new char[MAX_PATH];

//设置UI用的变量
//****************************************************************************************************
static char* set_Baidu_English = new char[5] {};
static char* set_Baidu_Chinese = new char[5] {};
static char* set_Baidu_ChineseReplace = new char[5] {};
static char* set_Baidu_ID = new char[30] {};
static char* set_Baidu_Key = new char[30] {};

static char* set_Youdao_English = new char[10] {};
static char* set_Youdao_Chinese = new char[10] {};
static char* set_Youdao_ChineseReplace = new char[10] {};
static char* set_Youdao_ID = new char[50] {};
static char* set_Youdao_Key = new char[50] {};

static char* set_TesseractModel = new char[10] {};
static char* set_Font_path = new char[500] {};
static int set_Residence_Time;

static float set_ButtonColor[4];
static float set_ButtonHoveredColor[4];
static float set_ButtonActiveColor[4];

const char* items[] = { "Shift", "Ctrl", "Alt" };
//****************************************************************************************************

//翻译目标语言设置
std::string Baidu_English;//检测目标语言
std::string Baidu_Chinese;//翻译目标语言
std::string Baidu_ChineseReplace;//替换的目标语言
//翻译API 的 ID 和 Key
std::string Baidu_ID;
std::string Baidu_Key;


//翻译目标语言设置
std::string Youdao_English;//检测目标语言
std::string Youdao_Chinese;//翻译目标语言
std::string Youdao_ChineseReplace;//替换的目标语言
//翻译API 的 ID 和 Key
std::string Youdao_ID;
std::string Youdao_Key;


//按键
static int  screenshot_key_1;//截图按键 screenshot_key_1 && screenshot_key_2
static char* screenshot_key_2 = new char[2] {};

//static int  choice_key_1;//选择按键 choice_key_1 && choice_key_2
static char* choice_key_2 = new char[2] {};

//static int  replace_key_1;//替换按键 replace_key_1 && replace_key_2
static char* replace_key_2 = new char[2] {};

//软件设置
static int Residence_Time;//鼠标离开界面时的显示时间
static int WindowWidth;//翻译界面 宽度 初始大小
static bool Fontbool;//是否选用字体样式
std::string Font_path;//字体样式
static float FontSize;//字体大小
static float ButtonRounding;//按键圆润度
std::string TesseractModel;//Tesseract-ORC 模型
static int ButtonColor[4];//按键颜色
static int ButtonHoveredColor[4];//鼠标悬停颜色
static int ButtonActiveColor[4];//鼠标点击颜色

//读取INI数据
void IniDataInit() {
	strcpy(lpPath, "./Data.ini");
	INIReader iniData{ lpPath };

	Youdao_English = iniData.Get<std::string>("language", "Youdao_English");
	strcpy(set_Youdao_English, Youdao_English.c_str());
	Youdao_Chinese = iniData.Get<std::string>("language", "Youdao_Chinese");
	strcpy(set_Youdao_Chinese, Youdao_Chinese.c_str());
	Youdao_ChineseReplace = iniData.Get<std::string>("language", "Youdao_ChineseReplace");
	strcpy(set_Youdao_ChineseReplace, Youdao_ChineseReplace.c_str());
	Youdao_ID = iniData.Get<std::string>("API", "Youdao_ID");
	strcpy(set_Youdao_ID, Youdao_ID.c_str());
	Youdao_Key = iniData.Get<std::string>("API", "Youdao_Key");
	strcpy(set_Youdao_Key, Youdao_Key.c_str());

	Baidu_English = iniData.Get<std::string>("language", "Baidu_English");
	strcpy(set_Baidu_English, Baidu_English.c_str());
	Baidu_Chinese = iniData.Get<std::string>("language", "Baidu_Chinese");
	strcpy(set_Baidu_Chinese, Baidu_Chinese.c_str());
	Baidu_ChineseReplace = iniData.Get<std::string>("language", "Baidu_ChineseReplace");
	strcpy(set_Baidu_ChineseReplace, Baidu_ChineseReplace.c_str());
	Baidu_ID = iniData.Get<std::string>("API", "Baidu_ID");
	strcpy(set_Baidu_ID, Baidu_ID.c_str());
	Baidu_Key = iniData.Get<std::string>("API", "Baidu_Key");
	strcpy(set_Baidu_Key, Baidu_Key.c_str());

	screenshot_key_1 = iniData.Get<int>("Key", "screenshot_key_1");
	screenshot_key_2[0] = iniData.Get<char>("Key", "screenshot_key_2");
	//choice_key_1 = iniData.Get<int>("Key", "choice_key_1");
	choice_key_2[0] = iniData.Get<char>("Key", "choice_key_2");
	//replace_key_1 = iniData.Get<int>("Key", "replace_key_1");
	replace_key_2[0] = iniData.Get<char>("Key", "replace_key_2");

	Residence_Time = int(iniData.Get<float>("Set", "Residence_Time") * 1000);
	WindowWidth = iniData.Get<int>("Set", "WindowWidth");
	Fontbool = iniData.Get<bool>("Set", "Fontbool");
	Font_path = iniData.Get<std::string>("Set", "Font_path");
	strcpy(set_Font_path, Font_path.c_str());
	FontSize = iniData.Get<float>("Set", "FontSize");
	ButtonRounding = iniData.Get<float>("Set", "ButtonRounding");
	TesseractModel = iniData.Get<std::string>("Set", "TesseractModel");
	strcpy(set_TesseractModel, TesseractModel.c_str());
	GetColor(iniData.GetVector<int>("Set", "ButtonColor"), ButtonColor, set_ButtonColor);
	GetColor(iniData.GetVector<int>("Set", "ButtonHoveredColor"), ButtonHoveredColor, set_ButtonHoveredColor);
	GetColor(iniData.GetVector<int>("Set", "ButtonActiveColor"), ButtonActiveColor, set_ButtonActiveColor);
}

//保存INI数据
void IniDataPreservation() {
	WritePrivateProfileString("language", "Youdao_English", set_Youdao_English, lpPath);
	WritePrivateProfileString("language", "Youdao_Chinese", set_Youdao_Chinese, lpPath);
	WritePrivateProfileString("language", "Youdao_ChineseReplace", set_Youdao_ChineseReplace, lpPath);
	WritePrivateProfileString("API", "Youdao_ID", set_Youdao_ID, lpPath);
	WritePrivateProfileString("API", "Youdao_Key", set_Youdao_Key, lpPath);

	WritePrivateProfileString("language", "Baidu_English", set_Baidu_English, lpPath);
	WritePrivateProfileString("language", "Baidu_Chinese", set_Baidu_Chinese, lpPath);
	WritePrivateProfileString("language", "Baidu_ChineseReplace", set_Baidu_ChineseReplace, lpPath);
	WritePrivateProfileString("API", "Baidu_ID", set_Baidu_ID, lpPath);
	WritePrivateProfileString("API", "Baidu_Key", set_Baidu_Key, lpPath);

	WritePrivateProfileString("Key", "screenshot_key_1", toString(screenshot_key_1).c_str(), lpPath);
	WritePrivateProfileString("Key", "screenshot_key_2", toString(screenshot_key_2[0]).c_str(), lpPath);
	WritePrivateProfileString("Key", "choice_key_2", toString(choice_key_2[0]).c_str(), lpPath);
	WritePrivateProfileString("Key", "replace_key_2", toString(replace_key_2[0]).c_str(), lpPath);

	WritePrivateProfileString("Set", "Residence_Time", toString(set_Residence_Time).c_str(), lpPath);
	WritePrivateProfileString("Set", "WindowWidth", toString(WindowWidth).c_str(), lpPath);
	WritePrivateProfileString("Set", "Fontbool", toString(Fontbool).c_str(), lpPath);
	WritePrivateProfileString("Set", "Font_path", toString(set_Font_path).c_str(), lpPath);
	WritePrivateProfileString("Set", "FontSize", toString(FontSize).c_str(), lpPath);
	WritePrivateProfileString("Set", "ButtonRounding", toString(ButtonRounding).c_str(), lpPath);
	WritePrivateProfileString("Set", "TesseractModel", set_TesseractModel, lpPath);
	WritePrivateProfileString("Set", "ButtonColor", ColorArraytoString(ButtonColor).c_str(), lpPath);
	WritePrivateProfileString("Set", "ButtonHoveredColor", ColorArraytoString(ButtonHoveredColor).c_str(), lpPath);
	WritePrivateProfileString("Set", "ButtonActiveColor", ColorArraytoString(ButtonActiveColor).c_str(), lpPath);
}














//设置
static bool Set_interface = false;
static bool Set_init = false;//是否是第一次打开界面

//翻译界面
static bool translate_interface = false;//界面开关
static bool translate_click = false;//判断鼠标第一次点击
static clock_t translate_interface_time;//鼠标离开界面的时间戳
std::string translate_English = "Error";
std::string translate_Chinese = u8"空";



//截图界面
static bool Screenshot_interface = false;//界面开关
static bool Screenshot_click = false;//判断鼠标第一次点击
static bool Screenshot_init = false;//是否是第一次打开界面
static bool Screenshot_translate = false;//是否翻译了内容，防止截图BUG
static int Screenshot_translate_shu = 2;
static POINT MousePosition_1 = { 0,0 };//第一次左键点击位置
static POINT MousePosition_2 = { 0,0 };//左键松开位置
static ImTextureID image_ID;//IMGUI图片ID
static ID3D11ShaderResourceView* m_pImageTextureView1;//DX11的图片ID
static l_int32 x, y, w, h;


//系统托盘菜单
static bool SystemTray_interface = false;//界面开关
static bool SystemTray_init = false;//是否是第一次打开界面
static bool SystemTray_mode = false;//翻译方式 1：有道   0：百度
static clock_t SystemTray_interface_time;//鼠标离开界面的时间戳


//IMGUI
static ImVec4 clear_color;



static int windows_Width{ GetSystemMetrics(SM_CXSCREEN) };//获取显示器的宽
static int windows_Heigth{ GetSystemMetrics(SM_CYSCREEN) };//获取显示器的高







static HWND hwnd = NULL;

HINSTANCE g_hInst = NULL;

#define WM_IAWENTRAY    WM_USER+2  //系统托盘的自定义消息 

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;





