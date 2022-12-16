#include <iostream>
#include "FilePath.h"
#include "base.h"
#include "imgui/GUI.h"
#include <atlimage.h>
#include<windows.h>

static bool tex_open = false;
static bool tex_openpos = false;
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


clock_t caidan_time;


/*
clock_t shijian;
shijian = clock();
printf("%d", shijian - clock());
*/



ID3D11ShaderResourceView* DX11LoadTextureImageFromFile(LPCSTR lpszFilePath)
{

    ID3D11Texture2D* pTexture2D = NULL;
    D3D11_TEXTURE2D_DESC dec;


    HRESULT result;
    D3DX11_IMAGE_LOAD_INFO loadInfo;
    ZeroMemory(&loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO));
    loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    loadInfo.MipLevels = D3DX11_DEFAULT; //这时会产生最大的mipmaps层。 
    loadInfo.MipFilter = D3DX11_FILTER_LINEAR;
    result = D3DX11CreateTextureFromFile(g_pd3dDevice, lpszFilePath, &loadInfo, NULL, (ID3D11Resource**)(&pTexture2D), NULL);
    pTexture2D->GetDesc(&dec);

    if (result != S_OK)
    {
        return NULL;
    }

    ID3D11ShaderResourceView* pFontTextureView = NULL;

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = dec.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_pd3dDevice->CreateShaderResourceView(pTexture2D, &srvDesc, &pFontTextureView);


    return pFontTextureView;
}





void screen(LPCSTR fileName)
{
    HWND window = GetDesktopWindow();
    HDC _dc = GetWindowDC(window);//屏幕DC
    HDC dc = CreateCompatibleDC(0);//内存DC

    RECT re;
    GetWindowRect(window, &re);
    int w = re.right,
        h = re.bottom;
    void* buf = new char[w * h * 4];

    HBITMAP bm = CreateCompatibleBitmap(_dc, w, h);//建立和屏幕兼容的bitmap
    SelectObject(dc, bm);//将memBitmap选入内存DC    
    StretchBlt(dc, 0, 0, w, h, _dc, 0, 0, w, h, SRCCOPY);//复制屏幕图像到内存DC

    //g_pd3dDevice->CreateTexture2D();


    void* f = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);

    GetObject(bm, 84, buf);

    tagBITMAPINFO bi;
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = 0;
    bi.bmiHeader.biSizeImage = 0;

    CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &buf, 0, 0);
    GetDIBits(dc, bm, 0, h, buf, &bi, DIB_RGB_COLORS);

    BITMAPFILEHEADER bif;
    bif.bfType = MAKEWORD('B', 'M');
    bif.bfSize = w * h * 4 + 54;
    bif.bfOffBits = 54;

    BITMAPINFOHEADER bii;
    bii.biSize = 40;
    bii.biWidth = w;
    bii.biHeight = h;
    bii.biPlanes = 1;
    bii.biBitCount = 32;
    bii.biCompression = 0;
    bii.biSizeImage = w * h * 4;

    DWORD r;
    WriteFile(f, &bif, sizeof(bif), &r, NULL);
    WriteFile(f, &bii, sizeof(bii), &r, NULL);
    WriteFile(f, buf, w * h * 4, &r, NULL);

    CloseHandle(f);
    DeleteDC(_dc);
    DeleteDC(dc);
}









void Helpmarker(const char* Text, ImVec4 Color)
{
    ImGui::TextColored(Color, u8"(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(Text);
    }
}


int main(int,char**)
{
    
    screen("1.png");
    
    //防止多开
    if (FindWindow(NULL, _T("ImGui Tool")))
    {
        MessageBoxEx(NULL, TEXT("ヽ(*。>Д<)o゜"), TEXT("已经启动啦！"), MB_OK, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
        return FALSE;
    }

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL , NULL, NULL, _T("ImGui Tool"), NULL };
    ::RegisterClassEx(&wc);
    g_hInst = wc.hInstance;   //WS_EX_TOPMOST 窗口置顶    
    HWND hwnd = ::CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, _T("ImGui Tool"), WS_OVERLAPPEDWINDOW, 100, 100, 1, 1, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }


    //系统托盘创建
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hwnd;
    nid.uID = 1;
    strncpy_s(nid.szTip, TEXT("自制锁屏软件卖萌中"), sizeof(TEXT("自制锁屏软件卖萌中")));
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
    nid.uCallbackMessage = WM_IAWENTRAY;
    nid.hIcon = (HICON)LoadImage(NULL, TEXT("favicon.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);//ico 图片 只支持32x32  ,  16x16
    Shell_NotifyIcon(NIM_ADD, &nid);

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



    //ImGui 风格设置
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

    
    



    ID3D11ShaderResourceView* m_pImageTextureView1 = DX11LoadTextureImageFromFile("1.png");
    ImTextureID my_tex_id = m_pImageTextureView1;


    
    
    bool done = false;
    while (!done)
    {
        /*
        VK_CONTROL = Ctrl
        VK_MENU    = Alt
        */
        if ((GetKeyState(VK_MENU) < 0) && (GetKeyState('Q') < 0)) {
            printf("bei");
        }
        

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
    
            static bool WinPos = true;//用于初始化窗口位置
            int Screen_Width{ GetSystemMetrics(SM_CXSCREEN) };//获取显示器的宽
            int Screen_Heigth{ GetSystemMetrics(SM_CYSCREEN) };//获取显示器的高
    
            if (WinPos)//初始化窗口
            {
                ImGui::SetNextWindowPos({ float(Screen_Width - 600) / 2,float(Screen_Heigth - 400) / 2 });
                WinPos = false;//初始化完毕
            }
    
    
            //翻译界面
            static bool open = true;
            
            if (open) {
                ImGui::Begin(u8"翻译结果", &open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);//创建窗口
    
                //鼠标在界面上
                if (ImGui::IsWindowHovered()) {
                    
                }

                
                
                
    
                ImGui::PushTextWrapPos(300);//限制字体的范围（像素）
                ImGui::Text("this is a second, it is was used for check the rightable of PushTextWrapPos");
                ImGui::SameLine();//让下一个元素并排
                ImGui::SetCursorPosX(305);//设置下一个元素生成的位置
                if (ImGui::Button(u8" ")) {
                    
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(u8"复制");
                }
    
                ImGui::Text(u8"问问大安分局那附近安徽佛啊武大吉奥法海哦发哦妇女求佛i我念佛i啊佛南");
                ImGui::SameLine();//让一个元素并排
                ImGui::SetCursorPosX(305);//设置下一个元素生成的位置
                if (ImGui::Button(u8" ")) {
    
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(u8"复制");
                }
    
                ImGui::End();
            }
    
            static bool dianji = true;
            static bool dianji_open = true;
            static POINT ptdianji_1 = { 0,0 };
            static POINT ptdianji_2 = { 0,0 };
            //截图操作界面
            if (dianji) {
                ImGui::SetNextWindowPos({ 0, 0 });
                ImGui::Begin("My shapes", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
                
                if (dianji_open) {
                    if (GetKeyState(VK_LBUTTON) < 0) {
                        GetCursorPos(&ptdianji_1);
                        //获取鼠标左键点击的第一个位置
                        dianji_open = false;
                    }
                }
                else {
                    GetCursorPos(&ptdianji_2);
                    //更新鼠标移动结束位置
                    if (GetKeyState(VK_LBUTTON) >= 0) {
                        //关闭窗口获取
                        dianji = false;
                    }
                }

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImGui::Image(my_tex_id, ImVec2(1920, 1080));
                
                draw_list->AddQuadFilled(
                    ImVec2(0, 0),
                    ImVec2(0, ptdianji_1.y),
                    ImVec2(ptdianji_2.x, ptdianji_1.y),
                    ImVec2(ptdianji_2.x, 0),
                    IM_COL32(0, 255, 0, 100));

                draw_list->AddQuadFilled(
                    ImVec2(ptdianji_2.x, 0),
                    ImVec2(1920, 0),
                    ImVec2(1920, ptdianji_2.y),
                    ImVec2(ptdianji_2.x, ptdianji_2.y),
                    IM_COL32(0, 255, 0, 100));

                draw_list->AddQuadFilled(
                    ImVec2(ptdianji_1.x, ptdianji_2.y),
                    ImVec2(1920, ptdianji_2.y),
                    ImVec2(1920, 1080),
                    ImVec2(ptdianji_1.x, 1080),
                    IM_COL32(0, 255, 0, 100));

                draw_list->AddQuadFilled(
                    ImVec2(0, ptdianji_1.y),
                    ImVec2(ptdianji_1.x, ptdianji_1.y),
                    ImVec2(ptdianji_1.x, 1080),
                    ImVec2(0, 1080),
                    IM_COL32(0, 255, 0, 100));

                /*
                // Get the current ImGui cursor position
                ImVec2 p = ImGui::GetCursorScreenPos();

                // Draw a red circle
                

                // Draw a 3 pixel thick yellow line
                draw_list->AddLine(ImVec2(p.x, p.y), ImVec2(p.x +1, p.y +1), IM_COL32(255, 255, 0, 255), 1.0f);


                // Advance the ImGui cursor to claim space in the window (otherwise the window will appears small and needs to be resized)
                ImGui::Dummy(ImVec2(300, 300));*/


                

                ImGui::End();
            }
            
    
    

    
            
            //系统托盘的右键菜单
            if (tex_open) {
                //设置生成位置在鼠标的右上角
                if (tex_openpos) {
                    static POINT pt = { 0,0 };
                    GetCursorPos(&pt);
                    ImGui::SetNextWindowPos({ float(pt.x), float(pt.y) - 60 });
                    tex_openpos = false;
                }
                //创建右键菜单
                auto w = ImGui::Begin(u8"窗口", &tex_open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);//创建窗口
                //鼠标在界面上
                if (ImGui::IsWindowHovered()) {
                    caidan_time = clock();
                }
                if ((clock() - caidan_time) > 3000) {
                    tex_open = false;
                }





                if (ImGui::Button(u8"设置")) {
    
                }
                if (ImGui::Button(u8"退出")){
                    ImGui_ImplDX11_Shutdown();
                    ImGui_ImplWin32_Shutdown();
                    ImGui::DestroyContext();
    
                    CleanupDeviceD3D();
                    ::DestroyWindow(hwnd);
                    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    
                    return 0;
                }
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
    case WM_IAWENTRAY:
        /*
        MK_LBUTTON　鼠标左键被按下。
        MK_MBUTTON　鼠标中键被按下。
        MK_RBUTTON　鼠标右键被按下。
        */
        switch (lParam)
        {
        case WM_RBUTTONDOWN://右键图标
        {
            caidan_time = clock();
            if (tex_open) {
                tex_open = false;
                tex_openpos = false;
            }
            else {
                tex_open = true;
                tex_openpos = true;
            }
        }
            break;
        case WM_LBUTTONUP://左键图标
            break;
        }
        break;





     



    case WM_KEYDOWN://对大小写不敏感
        if (wParam == 'F') {//检测F是否被按下
            //SetWindowText(hWnd, "off FFFF");//当检测到F被按下是改变标题
        }
        break;


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
