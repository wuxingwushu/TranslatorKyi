#include "base.h"
#include <windows.h>//WIN API
#include "imgui/GUI.h"//显示界面
#include "Function/tesseract.h"//图片识别文字
#include "Function/Translate.h"//翻译文字
#include "ini.h"//软件数据
using namespace inih;//启用 ini 读取
//#define	PI					3.14






//按键
static int  screenshot_key_1;//截图按键 screenshot_key_1 && screenshot_key_2
static char screenshot_key_2;

static int  choice_key_1;//选择按键 choice_key_1 && choice_key_2
static char choice_key_2;

static int  replace_key_1;//截图按键 replace_key_1 && replace_key_2
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
	screenshot_key_1 = iniData.Get<int>("Key", "screenshot_key_1");
	screenshot_key_2 = iniData.Get<char>("Key", "screenshot_key_2");
	choice_key_1 = iniData.Get<int>("Key", "choice_key_1");
	choice_key_2 = iniData.Get<char>("Key", "choice_key_2");
	replace_key_1 = iniData.Get<int>("Key", "replace_key_1");
	replace_key_2 = iniData.Get<char>("Key", "screenshot_key_2");

	Residence_Time = int(iniData.Get<float>("Set", "Residence_Time") * 1000);
}















//翻译界面
static bool translate_interface = false;//界面开关
static bool translate_click = false;//判断鼠标第一次点击
static clock_t translate_interface_time;//鼠标离开界面的时间戳
char* translate_English = "adwafiawbfiuawbfiuawbfiubawifbiauwbifabwifbaiwfbawifbia";
std::string translate_Chinese = u8"问问大安分局那附近安徽佛啊武大吉奥法海哦发哦妇女求佛i我念佛i啊佛南";



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
//IMGUI 初始化
ImGuiIO& IMGUI_init();


static int windows_Width{ GetSystemMetrics(SM_CXSCREEN) };//获取显示器的宽
static int windows_Heigth{ GetSystemMetrics(SM_CYSCREEN) };//获取显示器的高



//加载图片
ID3D11ShaderResourceView* DX11LoadTextureImageFromFile(LPCSTR lpszFilePath);
//截图&保存（全屏）
void screen(LPCSTR fileName);



static HWND hwnd = NULL;

HINSTANCE g_hInst = NULL;

#define WM_IAWENTRAY    WM_USER+2  //系统托盘的自定义消息 

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

