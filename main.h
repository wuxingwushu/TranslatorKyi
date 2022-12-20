#include "base.h"
#include <windows.h>//WIN API
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

char* English = "auto";
char* Chinese = "zh";
char* ChineseReplace = "en";

void IniDataInit() {
/*
INIReader r{ "./Data.ini" };
r.Get<int>("data", "int1");//获取单个变量
r.GetVector<int>("data", "intixx")//获取变量数组
std::cout <<   << std::endl;
*/

	INIReader iniData{ "./Data.ini" };

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















//string 转 wstring
std::string ws2s(const std::wstring& ws)
{
    size_t i;
    std::string curLocale = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, "chs");
    const wchar_t* _source = ws.c_str();
    size_t _dsize = 2 * ws.size() + 1;
    char* _dest = new char[_dsize];
    memset(_dest, 0x0, _dsize);
    wcstombs_s(&i, _dest, _dsize, _source, _dsize);
    std::string result = _dest;
    delete[] _dest;
    setlocale(LC_ALL, curLocale.c_str());
    return result;
}

//wstring 转 string
std::wstring s2ws(const std::string& s)
{
    size_t i;
    std::string curLocale = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, "chs");
    const char* _source = s.c_str();
    size_t _dsize = s.size() + 1;
    wchar_t* _dest = new wchar_t[_dsize];
    wmemset(_dest, 0x0, _dsize);
    mbstowcs_s(&i, _dest, _dsize, _source, _dsize);
    std::wstring result = _dest;
    delete[] _dest;
    setlocale(LC_ALL, curLocale.c_str());
    return result;
}

//Unicode 转到 utf8
std::string  UnicodeToUtf8(const std::string& Unicode)
{
    std::wstring wstr = s2ws(Unicode);

    int ansiiLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    char* pAssii = (char*)malloc(sizeof(char) * ansiiLen);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, pAssii, ansiiLen, nullptr, nullptr);
    std::string ret_str = pAssii;
    free(pAssii);
    return ret_str;
}



//utf8 转到 Unicode
char* Utf8ToUnicode(const char* szU8)
{
    int wcsLen = MultiByteToWideChar(CP_UTF8, NULL, szU8, (int)strlen(szU8), NULL, 0);
    wchar_t* wszString = new wchar_t[wcsLen + 1];
    MultiByteToWideChar(CP_UTF8, NULL, szU8, (int)strlen(szU8), wszString, wcsLen);
    wszString[wcsLen] = '\0';
    int len = WideCharToMultiByte(CP_ACP, 0, wszString, (int)wcslen(wszString), NULL, 0, NULL, NULL);
    char* c = new char[len + 1];
    WideCharToMultiByte(CP_ACP, 0, wszString, (int)wcslen(wszString), c, len, NULL, NULL);
    c[len] = '\0';
    delete[] wszString;
    return c;
}