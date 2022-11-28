#include <iostream>
#include "FilePath.h"
#include "base.h"
#include "imgui/GUI.h"
#include <shellapi.h>//有关系统托盘的头文件
#define WM_HIDE WM_USER+100 


/*     
ypedef struct _NOTIFYICONDATAA {
        DWORD cbSize;                    //结构体的大小，以字节为单位。
        HWND hWnd;						//窗口的句柄。标示的窗口用来接收与托盘图标相关的消息。
        UINT uID;						//应用程序定义的任务栏图标的标识符。
        UINT uFlags;					//此成员表明具体哪些其他成员为合法数据（即哪些成员起作用）。
                                        //此成员可以为以下值的组合：
                                        //  NIF_ICON 　　hIcon成员起作用。 　
                                        //　NIF_MESSAGE 　uCallbackMessage成员起作用。 　
                                        //　NIF_TIP 　　  szTip成员起作用。 　
                                        //　NIF_STATE 　　dwState和dwStateMask成员起作用。
                                        //　NIF_INFO 　　 使用气球提示代替普通的工具提示框。szInfo, uTimeout, szInfoTitle和dwInfoFlags成员起作用。 　
                                        //　NIF_GUID 　　 保留。

        UINT uCallbackMessage;			//应用程序定义的消息标示。当托盘图标区域发生鼠标事件或者使用键盘选择或激活图标时，
                                        //系统将使用此标示向由hWnd成员标示的窗口发送消息。
                                        //消息响应函数的wParam参数标示了消息事件发生的任务栏图标，lParam参数根据事件的不同，
                                        //包含了鼠标或键盘的具体消息，例如当鼠标指针移过托盘图标时，lParam将为WM_MOUSEMOVE。
        HICON hIcon;					//增加、修改或删除的图标的句柄。
        CHAR   szTip[64];				//指向一个以\0结束的字符串的指针。字符串的内容为标准工具提示的信息。
                                        //包含最后的\0字符，szTip最多含有64个字符。
} NOTIFYICONDATAA                         */

//系统托盘
void Minimized(HWND hwnd, int flag)
{
    NOTIFYICONDATA nid;
    nid.cbSize = (DWORD)sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_HIDE;				//自定义消息
    HINSTANCE hin = (HINSTANCE)GetWindowLong(hwnd, -6);//获得程序句柄！！！    GWL_HINSTANCE = -6
    nid.hIcon = LoadIcon(hin, MAKEINTRESOURCE(1));   //增加一个MAKEINTRESOURCE(1)的图标句柄，MAKEINTRESOURCE(1)即：应用程序图标
    lstrcpy(nid.szTip, TEXT("双击恢复窗口"));             //最小化到托盘，鼠标移到托盘上面显示的信息提示条。
    switch (flag)
    {
    case 0:
    {//添加托盘图标，隐藏窗口
        Shell_NotifyIcon(NIM_ADD, &nid);
        //BOOL Shell_NotifyIcon( DWORD dwMessage,PNOTIFYICONDATA lpdata);
        //dwMessage为输入参数，传递发送的消息，表明要执行的操作。可选的值如下：
        //NIM_ADD 向托盘区域添加一个图标。
        //NIM_DELETE 删除托盘区域的一个图标。
        //NIM_MODIFY  修改托盘区域的一个图标。
        //NIM_SETFOCUS 设置焦点。比如当用户操作托盘图标弹出菜单，而有按下ESC键将菜单消除后，程序应该使用此消息来将焦点设置到托盘图标上。 
        //lpdata 为输入参数，是指向NOTIFYICONDATA结构体的指针，结构体内容用来配合第一个参数wMessage进行图标操作。
        ShowWindow(hwnd, SW_HIDE);//隐藏窗口
    }
    break;
    case 1:
    {//删除托盘图标
        ShowWindow(hwnd, SW_SHOWNORMAL);
        Shell_NotifyIcon(NIM_DELETE, &nid);
        SetForegroundWindow(hwnd);				//Foreground 前台
        //函数原型：BOOL SetForegroundWindow（HWND hWnd）
        //该函数将创建指定窗口的线程设置到前台，并且激活该窗口。
        //键盘输入转向该窗口，并为用户改各种可视的记号。系统给创建前台窗口的线程分配的权限稍高于其他线程。 
    }
    break;
    default:
        break;
    }
}














static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);



void Helpmarker(const char* Text, ImVec4 Color)
{
    ImGui::TextColored(Color, u8"(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(Text);
    }
}


int main(int, char**)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Tool"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("ImGui Tool"), WS_OVERLAPPEDWINDOW, 100, 100, 1, 1, NULL, NULL, wc.hInstance, NULL);

    //系统托盘
    Minimized(hwnd,0);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_HIDE);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;


    ImGui::StyleColorsDark();


    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    io.IniFilename = nullptr;
    ImFontConfig Font_cfg;
    Font_cfg.FontDataOwnedByAtlas = false;

    //ImFont* Font = io.Fonts->AddFontFromFileTTF("..\\ImGui Tool\\Font.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
    ImFont* Font = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, 18.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
    ImFont* Font_Big = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, 24.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;


        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        {
            ImGuiStyle& Style = ImGui::GetStyle();
            auto Color = Style.Colors;

            Style.ChildRounding = 8.0f;
            Style.FrameRounding = 5.0f;

            Color[ImGuiCol_Button] = ImColor(10, 105, 56, 255);
            Color[ImGuiCol_ButtonHovered] = ImColor(30, 125, 76, 255);
            Color[ImGuiCol_ButtonActive] = ImColor(0, 95, 46, 255);

            Color[ImGuiCol_FrameBg] = ImColor(54, 54, 54, 150);
            Color[ImGuiCol_FrameBgActive] = ImColor(42, 42, 42, 150);
            Color[ImGuiCol_FrameBgHovered] = ImColor(100, 100, 100, 150);

            Color[ImGuiCol_CheckMark] = ImColor(10, 105, 56, 255);

            Color[ImGuiCol_SliderGrab] = ImColor(10, 105, 56, 255);
            Color[ImGuiCol_SliderGrabActive] = ImColor(0, 95, 46, 255);

            Color[ImGuiCol_Header] = ImColor(10, 105, 56, 255);
            Color[ImGuiCol_HeaderHovered] = ImColor(30, 125, 76, 255);
            Color[ImGuiCol_HeaderActive] = ImColor(0, 95, 46, 255);



            static bool WinPos = true;//用于初始化窗口位置
            int Screen_Width{ GetSystemMetrics(SM_CXSCREEN) };//获取显示器的宽
            int Screen_Heigth{ GetSystemMetrics(SM_CYSCREEN) };//获取显示器的高

            if (WinPos)//初始化窗口
            {
                ImGui::SetNextWindowPos({ float(Screen_Width - 600) / 2,float(Screen_Heigth - 400) / 2 });
                WinPos = false;//初始化完毕
            }

            static bool open = true;
            if (open) {
                ImGui::Begin(u8"窗口 abd 1233", &open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);//创建窗口
                //创建按键
                if (ImGui::Button(u8"Close想 Me")) {
                    open = false;
                }ImGui::SameLine();//让一个元素并排

                if (ImGui::Button("Close")) {
                    open = false;
                }

                static int a = 0;
                if (ImGui::CollapsingHeader("Gong_danxuan")) {
                    ImGui::RadioButton("anjian0", &a, 0);
                    ImGui::RadioButton("anjian1", &a, 1);
                    ImGui::RadioButton("anjian2", &a, 2);
                }


                static bool b = false;
                ImGui::Checkbox("kai", &b);

                ImGui::Text("Text");
                ImGui::BulletText("bullet Text");

                static float f = 0.0f;
                ImGui::SliderFloat("hua_kuai", &f, 0.0f, 1.0f);
                //ImGui::SliderInt();

                ImGui::TextWrapped("w");

                ImGui::End();
            }

            
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}


bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
