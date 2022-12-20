#include "main.h"
/*
_______________#########_______________________
______________############_____________________
______________#############____________________
_____________##__###########___________________
____________###__######_#####__________________                     江城子  . 程序员之歌
____________###_#######___####_________________
___________###__##########_####________________               十年生死两茫茫，写程序，到天亮。
__________####__###########_####_______________                     千行代码，Bug何处藏。
________#####___###########__#####_____________               纵使上线又怎样，朝令改，夕断肠。
_______######___###_########___#####___________
_______#####___###___########___######_________               领导每天新想法，天天改，日日忙。
______######___###__###########___######_______                     相顾无言，惟有泪千行。
_____######___####_##############__######______               每晚灯火阑珊处，夜难寐，加班狂。
____#######__#####################_#######_____
____#######__##############################____
___#######__######_#################_#######___
___#######__######_######_#########___######___
___#######____##__######___######_____######___
___#######________######____#####_____#####____
____######________#####_____#####_____####_____
_____#####________####______#####_____###______
______#####______;###________###______#________
________##_______####________####______________
*/





int main(int,char**)
{
    IniDataInit();//初始化软件数据


    //防止软件多开
    if (FindWindow(NULL, _T("ImGui Tool")))
    {
        MessageBoxEx(NULL, TEXT("ヽ(*。>Д<)o゜"), TEXT("已经启动啦！"), MB_OK, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
        return FALSE;
    }

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL , NULL, NULL, _T("ImGui Tool"), NULL };
    ::RegisterClassEx(&wc);
    g_hInst = wc.hInstance;
    //创建窗口                 //WS_EX_TOPMOST 窗口置顶    
    hwnd = ::CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, _T("ImGui Tool"), WS_OVERLAPPEDWINDOW, 100, 100, 1, 1, NULL, NULL, wc.hInstance, NULL);
    //创建DX11设备
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
    strncpy_s(nid.szTip, TEXT("人家叫翻译姬！"), sizeof(TEXT("人家叫翻译姬！")));//鼠标停止系统托盘上的提示
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
    nid.uCallbackMessage = WM_IAWENTRAY;
    nid.hIcon = (HICON)LoadImage(NULL, TEXT("product.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);//ico 图片 只支持32x32  ,  16x16
    Shell_NotifyIcon(NIM_ADD, &nid);

    ::ShowWindow(hwnd, SW_HIDE);//开启win显示
    ::UpdateWindow(hwnd);//更新行为窗口


    

    //初始化 IMGUI 
    ImGuiIO& io = IMGUI_init();
    

    

    
    
    
    bool done = false;
    while (!done)
    {
        Sleep(20);//0.02秒的响应间隔,降低CPU的使用率
        /*
        VK_CONTROL = Ctrl
        VK_MENU    = Alt
        */

        //快捷键检测
        if ((GetKeyState(screenshot_key_1) < 0) && (GetKeyState(screenshot_key_2) < 0)) {
            //关闭其他UI
            translate_interface = false;
            SystemTray_interface = false;
            if (!Screenshot_interface) {
                Screenshot_init = true;
            }
            Screenshot_interface = true;
            Screenshot_click = true;
        }


        if ((GetKeyState(choice_key_1) < 0) && (GetKeyState(choice_key_2) < 0)) {
            
            //等待键盘松开（避免模拟按键 ctrl + c 时有其他按键，导致无法获取选择内容）
            while ((GetKeyState(choice_key_1) < 0) || (GetKeyState(choice_key_2) < 0)){
                //关闭其他UI
                SystemTray_interface = false;
                Screenshot_interface = false;
                //打开翻译界面
                translate_interface_time = clock();
                translate_interface = true;
                translate_click = true;
            }

            std::string strS = ClipboardTochar();//读取当前剪切板的内容，保存起来

            Sleep(5);//内容拷贝，避免指针丢失报错

            //获取当前选择的内容 ctrl + c
            keybd_event(17, 0, 0, 0);//按下 ctrl
            keybd_event(67, 0, 0, 0);//按下 c
            keybd_event(17, 0, KEYEVENTF_KEYUP, 0);//松开 ctrl
            keybd_event(67, 0, KEYEVENTF_KEYUP, 0);//松开 c


            Sleep(5);//等待上面的内容复制到剪切板上

            translate_English = ClipboardTochar();//从剪切板上读取出来

            if (strlen(translate_English.c_str()) <= 1) {
                translate_Chinese =  u8"不存在单词";
            }
            else {
                translate_Chinese = Translate_Baidu(Baidu_ID.c_str(), Baidu_Key.c_str(), UnicodeToUtf8(translate_English), English.c_str(), Chinese.c_str());//翻译内容
            }

            CopyToClipboard(strS);//还原原来剪切板的内容
        }

       
        if ((GetKeyState(replace_key_1) < 0) && (GetKeyState(replace_key_2) < 0)) {
            //等待键盘松开（避免模拟按键 ctrl + c 时有其他按键，导致无法获取选择内容）
            while ((GetKeyState(replace_key_1) < 0) || (GetKeyState(replace_key_2) < 0)) {
                //关闭其他UI
                SystemTray_interface = false;
                Screenshot_interface = false;
                translate_interface = false;
            }
            std::string strS = ClipboardTochar();//读取当前剪切板的内容，保存起来

            Sleep(5);//内容拷贝，避免指针丢失报错

            //获取当前选择的内容 ctrl + c
            keybd_event(17, 0, 0, 0);//按下 ctrl
            keybd_event(67, 0, 0, 0);//按下 c
            keybd_event(17, 0, KEYEVENTF_KEYUP, 0);//松开 ctrl
            keybd_event(67, 0, KEYEVENTF_KEYUP, 0);//松开 c

            Sleep(5);//等待上面的内容复制到剪切板上

            translate_English = ClipboardTochar();//从剪切板上读取出来

            if (strlen(translate_English.c_str()) > 0) {
                translate_Chinese = Translate_Baidu(Baidu_ID.c_str(), Baidu_Key.c_str(), UnicodeToUtf8(translate_English), English.c_str(), ChineseReplace.c_str());//翻译内容
            }

            CopyToClipboard(translate_Chinese);//把翻译的内容复制到剪切板上

            //粘贴出去 ctrl + v
            keybd_event(17, 0, 0, 0);//按下 ctrl
            keybd_event(86, 0, 0, 0);//按下 v
            keybd_event(17, 0, KEYEVENTF_KEYUP, 0);//松开 ctrl
            keybd_event(86, 0, KEYEVENTF_KEYUP, 0);//松开 v

            Sleep(5);//等待上面的内容粘贴出去

            CopyToClipboard(strS);//还原原来剪切板的内容
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
         
            
            if (translate_interface) {
                if (translate_click) {
                    static POINT pt = { 0,0 };
                    GetCursorPos(&pt);//获取鼠标位置
                    ImGui::SetNextWindowPos({ float(pt.x + 10), float(pt.y) });//设置窗口生成位置
                    translate_click = false;
                }

                bool k = ImGui::Begin(u8"翻译结果", &translate_interface, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar);//创建窗口

                ImGui::PushTextWrapPos(300);//限制字体的范围（像素）
                ImGui::Text(translate_English.c_str());
                ImGui::SameLine();//让下一个元素并排
                ImGui::SetCursorPosX(305);//设置下一个元素生成的位置
                if (ImGui::Button("<")) {
                    CopyToClipboard(translate_English.c_str());
                }
                //设置上一个元素的鼠标悬停提示
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(u8"复制");//提示内容
                }

                ImGui::Text(translate_Chinese.c_str());
                ImGui::SameLine();//让一个元素并排
                ImGui::SetCursorPosX(305);//设置下一个元素生成的位置
                if (ImGui::Button(">")) {
                    CopyToClipboard(translate_Chinese.c_str());
                }
                //设置上一个元素的鼠标悬停提示
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(u8"复制");//提示内容
                }

                //鼠标在界面上
                static POINT pt = { 0,0 };
                GetCursorPos(&pt);//获取鼠标位置
                if ((pt.x > ImGui::GetWindowPos().x) && (pt.y > ImGui::GetWindowPos().y) && (pt.x < (ImGui::GetWindowPos().x + ImGui::GetWindowWidth())) && (pt.y < (ImGui::GetWindowPos().y + ImGui::GetWindowHeight()))) {
                    translate_interface_time = clock();
                }
                else if ((clock() - translate_interface_time) > Residence_Time) {
                    translate_interface = false;
                }
    
                ImGui::End();
            }
    
            
            
            //截图操作界面
            if (Screenshot_interface) {
                //截一次图运行一次
                if (Screenshot_init) {
                    //设置生成位置
                    ImGui::SetNextWindowPos({ -8, -8 });
                    ImGui::SetNextWindowSize(ImVec2(windows_Width + 16, windows_Heigth + 16));
                    //截取图片
                    screen("TemporaryData");
                    m_pImageTextureView1 = DX11LoadTextureImageFromFile("TemporaryData");
                    image_ID = m_pImageTextureView1;
                    Screenshot_init = false;
                }
 
                if (Screenshot_click) {
                    //左键点击事件
                    if (GetKeyState(VK_LBUTTON) < 0) {
                        //获取左键位置
                        GetCursorPos(&MousePosition_1);
                        GetCursorPos(&MousePosition_2);
                        //获取鼠标左键点击的第一个位置，然后就关掉这个事件
                        Screenshot_click = false;
                    }
                }
                else {
                    //更新鼠标移动结束位置
                    GetCursorPos(&MousePosition_2);

                    l_int32 x, y, w, h;

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
                        //释放图片纹理内存
                        m_pImageTextureView1->Release();
                        m_pImageTextureView1 = NULL;
                        MousePosition_1 = { 0,0 };
                        MousePosition_2 = { 0,0 };

                        //关闭窗口
                        Screenshot_interface = false;

                        //画面过小，取消识别
                        if (w > 10 && h > 10) {
                            translate_English = Tesseract_OCR(x, y, w, h);//要翻译界面关闭了才可以要不然会出BUG

                            if (strlen(translate_English.c_str()) > 0) {
                                translate_Chinese = Translate_Baidu(Baidu_ID.c_str(), Baidu_Key.c_str(), translate_English, English.c_str(), Chinese.c_str());//翻译内容
                            }
                            else {
                                translate_Chinese = u8"内容不存在！";
                            }

                            translate_interface_time = clock();
                            translate_interface = true;
                            translate_click = true;
                        } 
                    }
                }

                //创建窗口
                ImGui::Begin("My shapes", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar);

                //显示图片
                ImGui::Image(image_ID, ImVec2(windows_Width, windows_Heigth));
                
                //创建画布,用于把框选之外的画面变暗。
                ImDrawList* draw_list = ImGui::GetWindowDrawList();

                draw_list->AddQuadFilled(
                    //矩形框的四个点（顺时针）
                    ImVec2(0, 0),
                    ImVec2(0, MousePosition_1.y),
                    ImVec2(MousePosition_2.x, MousePosition_1.y),
                    ImVec2(MousePosition_2.x, 0),
                    //颜色
                    IM_COL32(0, 0, 0, 100));

                draw_list->AddQuadFilled(
                    //矩形框的四个点（顺时针）
                    ImVec2(MousePosition_2.x, 0),
                    ImVec2(windows_Width, 0),
                    ImVec2(windows_Width, MousePosition_2.y),
                    ImVec2(MousePosition_2.x, MousePosition_2.y),
                    //颜色
                    IM_COL32(0, 0, 0, 100));

                draw_list->AddQuadFilled(
                    //矩形框的四个点（顺时针）
                    ImVec2(MousePosition_1.x, MousePosition_2.y),
                    ImVec2(windows_Width, MousePosition_2.y),
                    ImVec2(windows_Width, windows_Heigth),
                    ImVec2(MousePosition_1.x, windows_Heigth),
                    //颜色
                    IM_COL32(0, 0, 0, 100));

                draw_list->AddQuadFilled(
                    //矩形框的四个点（顺时针）
                    ImVec2(0, MousePosition_1.y),
                    ImVec2(MousePosition_1.x, MousePosition_1.y),
                    ImVec2(MousePosition_1.x, windows_Heigth),
                    ImVec2(0, windows_Heigth),
                    //颜色
                    IM_COL32(0, 0, 0, 100));

                ImGui::End();
            }
            
    
    

    
            
            //系统托盘的右键菜单
            if (SystemTray_interface) {
                //设置生成位置在鼠标的右上角
                if (SystemTray_init) {
                    static POINT pt = { 0,0 };
                    GetCursorPos(&pt);//获取鼠标位置
                    ImGui::SetNextWindowPos({ float(pt.x), float(pt.y) - 90 });//设置窗口生成位置
                    SystemTray_init = false;
                }
                
                //创建右键菜单
                auto w = ImGui::Begin(u8"窗口", &SystemTray_interface, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);//创建窗口
                
                

                //设置按键
                if (ImGui::Button(u8"设置")) {
                    translate_interface = false;
                    translate_click = false;
                }
                if (SystemTray_mode) {
                    if (ImGui::Button(u8"百度")) {
                        SystemTray_mode = false;
                    }
                }
                else {
                    if (ImGui::Button(u8"有道")) {
                        SystemTray_mode = true;
                    }
                }
                if (ImGui::Button(u8"退出")){
                    done = true;
                }


                //鼠标在界面上
                static POINT pt = { 0,0 };
                GetCursorPos(&pt);//获取鼠标位置
                if ((pt.x > ImGui::GetWindowPos().x) && (pt.y > ImGui::GetWindowPos().y) && (pt.x < (ImGui::GetWindowPos().x + ImGui::GetWindowWidth())) && (pt.y < (ImGui::GetWindowPos().y + ImGui::GetWindowHeight()))) {
                    SystemTray_interface_time = clock();
                }
                else if ((clock() - SystemTray_interface_time) > Residence_Time) {
                    SystemTray_interface = false;
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
            SystemTray_interface_time = clock();
            if (SystemTray_interface) {
                SystemTray_interface = false;
                SystemTray_init = false;
            }
            else {
                SystemTray_interface = true;
                SystemTray_init = true;
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










//获得剪贴板的内容
std::string ClipboardTochar() {
    OpenClipboard(NULL);//打开剪贴板
    hmem = GetClipboardData(CF_TEXT);//获取剪切板内容块
    std::string CharS = (char*)GlobalLock(hmem);//获取内容块的地址
    CloseClipboard();//关闭剪贴板
    return CharS;
}



//将内容复制到剪贴板
void CopyToClipboard(std::string str) {
    str = Utf8ToUnicode(str.c_str());// utf8 转到 Unicode 要不然粘贴出来是乱吗
    OpenClipboard(NULL);
    if (!EmptyClipboard())       // 清空剪切板，写入之前，必须先清空剪切板
    {
        puts("清空剪切板失败\n");
        CloseClipboard();
    }

    HGLOBAL hMemory;
    if ((hMemory = GlobalAlloc(GMEM_MOVEABLE, strlen(str.c_str()) + 1)) == NULL)    // 对剪切板分配内存
    {
        puts("内存赋值错误!!!\n");
        CloseClipboard();
    }

    LPTSTR lpMemory;
    if ((lpMemory = (LPTSTR)GlobalLock(hMemory)) == NULL)             // 将内存区域锁定
    {
        puts("锁定内存错误!!!\n");
        CloseClipboard();
    }

    memcpy_s(lpMemory, strlen(str.c_str()) + 1, str.c_str(), strlen(str.c_str()) + 1);   // 将数据复制进入内存区域

    GlobalUnlock(hMemory);                   // 解除内存锁定

    if (SetClipboardData(CF_TEXT, hMemory) == NULL)
    {
        puts("设置剪切板数据失败!!!\n");
        CloseClipboard();
    }


    CloseClipboard();//关闭剪贴板
}


//IMGUI 初始化
ImGuiIO& IMGUI_init() {
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

    clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);



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

    return io;
}

//加载图片
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
    D3DX11CreateTextureFromFile(g_pd3dDevice, lpszFilePath, &loadInfo, NULL, (ID3D11Resource**)(&pTexture2D), &result);
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

    pTexture2D->Release();
    pTexture2D = NULL;

    return pFontTextureView;
}




//截图&保存（全屏）
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
    void* buff = buf;//备份保存申请内存的地址，因为后面 buf 这个内存地址会丢失导致内存泄露。

    HBITMAP bm = CreateCompatibleBitmap(_dc, w, h);//建立和屏幕兼容的bitmap
    SelectObject(dc, bm);//将memBitmap选入内存DC    
    StretchBlt(dc, 0, 0, w, h, _dc, 0, 0, w, h, SRCCOPY);//复制屏幕图像到内存DC

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

    void* dcf = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &buf, NULL, NULL);
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



    DeleteObject(dcf);
    DeleteObject(bm);
    delete[] buff;
    CloseHandle(f);
    DeleteDC(_dc);
    DeleteDC(dc);
}
