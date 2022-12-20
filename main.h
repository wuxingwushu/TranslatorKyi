#include "base.h"
#include "Tool.h"//数据格式转换
#include "imgui/GUI.h"//显示界面
#include "Function/tesseract.h"//图片识别文字
#include "Function/Translate.h"//翻译文字
#include "ini.h"//软件数据
using namespace inih;//启用 ini 读取
//#define	PI					3.14






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


//翻译目标语言设置
std::string English;//检测目标语言
std::string Chinese;//翻译目标语言
std::string ChineseReplace;//替换的目标语言

//翻译API 的 ID 和 Key
std::string Baidu_ID;
std::string Baidu_Key;

//按键
static int  screenshot_key_1;//截图按键 screenshot_key_1 && screenshot_key_2
static char screenshot_key_2;

static int  choice_key_1;//选择按键 choice_key_1 && choice_key_2
static char choice_key_2;

static int  replace_key_1;//替换按键 replace_key_1 && replace_key_2
static char replace_key_2;

//软件设置
static int Residence_Time;//鼠标离开界面时的显示时间



void IniDataInit() {
/*
INIReader r{ "./Data.ini" };
r.Get<int>("data", "int1");//获取单个变量
r.GetVector<int>("data", "intixx")//获取变量数组
std::cout <<   << std::endl;
*/

	INIReader iniData{ "./Data.ini" };

	English = iniData.Get<std::string>("language", "English");
	Chinese = iniData.Get<std::string>("language", "Chinese");
	ChineseReplace = iniData.Get<std::string>("language", "ChineseReplace");

	Baidu_ID = iniData.Get<std::string>("API", "Baidu_ID");
	Baidu_Key = iniData.Get<std::string>("API", "Baidu_Key");

	screenshot_key_1 = iniData.Get<int>("Key", "screenshot_key_1");
	screenshot_key_2 = iniData.Get<char>("Key", "screenshot_key_2");
	choice_key_1 = iniData.Get<int>("Key", "choice_key_1");
	choice_key_2 = iniData.Get<char>("Key", "choice_key_2");
	replace_key_1 = iniData.Get<int>("Key", "replace_key_1");
	replace_key_2 = iniData.Get<char>("Key", "replace_key_2");

	Residence_Time = int(iniData.Get<float>("Set", "Residence_Time") * 1000);
}
static HGLOBAL hmem;














//翻译界面
static bool translate_interface = false;//界面开关
static bool translate_click = false;//判断鼠标第一次点击
static clock_t translate_interface_time;//鼠标离开界面的时间戳
std::string translate_English;
std::string translate_Chinese;



//截图界面
static bool Screenshot_interface = false;//界面开关
static bool Screenshot_click = false;//判断鼠标第一次点击
static bool Screenshot_init = false;//是否是第一次打开界面
static POINT MousePosition_1 = { 0,0 };//第一次左键点击位置
static POINT MousePosition_2 = { 0,0 };//左键松开位置
static ImTextureID image_ID;//IMGUI图片ID
static ID3D11ShaderResourceView* m_pImageTextureView1;//DX11的图片ID

//系统托盘菜单
static bool SystemTray_interface = false;//界面开关
static bool SystemTray_init = false;//是否是第一次打开界面
static bool SystemTray_mode = false;//翻译方式 0：有道   1：百度
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



//IMGUI 初始化
ImGuiIO& IMGUI_init();
//获得剪贴板的内容
std::string ClipboardTochar();
//将内容复制到剪贴板
void CopyToClipboard(std::string str);
//加载图片
ID3D11ShaderResourceView* DX11LoadTextureImageFromFile(LPCSTR lpszFilePath);
//截图&保存（全屏）
void screen(LPCSTR fileName);

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

