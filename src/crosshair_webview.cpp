#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <ole2.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <string>
#include <functional>

#include "WebView2.h"

#define TIMER_RECENTER 1
#define OVERLAY_CLASS  L"CHOverlay"
#define MAIN_CLASS     L"CHMain"

struct Config {
    int size=12, thickness=2, gap=4, outlineSize=1, dotSize=3;
    COLORREF color=RGB(0,255,20), outlineColor=RGB(0,0,0);
    int style=0;
    bool visible=true, lockToCenter=false, useSecondMonitor=false;
};

static Config    g_cfg;
static HWND      g_hMain    = NULL;
static HWND      g_hOverlay = NULL;
static HINSTANCE g_hInst    = NULL;
static wchar_t   g_iniPath[MAX_PATH];
static wchar_t   g_uiPath[MAX_PATH];

static ICoreWebView2Controller* g_wvController = nullptr;
static ICoreWebView2*           g_wvView        = nullptr;
static void SaveConfig() {
    wchar_t b[32];
    auto W=[&](const wchar_t*k,int v){ _itow_s(v,b,10); WritePrivateProfileStringW(L"X",k,b,g_iniPath); };
    W(L"Size",g_cfg.size); W(L"Thick",g_cfg.thickness); W(L"Gap",g_cfg.gap);
    W(L"Ol",g_cfg.outlineSize); W(L"Dot",g_cfg.dotSize);
    W(L"CR",GetRValue(g_cfg.color)); W(L"CG",GetGValue(g_cfg.color)); W(L"CB",GetBValue(g_cfg.color));
    W(L"OR",GetRValue(g_cfg.outlineColor)); W(L"OG",GetGValue(g_cfg.outlineColor)); W(L"OB",GetBValue(g_cfg.outlineColor));
    W(L"Style",g_cfg.style); W(L"Vis",g_cfg.visible?1:0);
    W(L"Lock",g_cfg.lockToCenter?1:0); W(L"Mon2",g_cfg.useSecondMonitor?1:0);
}
static void LoadConfig() {
    auto R=[&](const wchar_t*k,int d){ return (int)GetPrivateProfileIntW(L"X",k,d,g_iniPath); };
    g_cfg.size=R(L"Size",12); g_cfg.thickness=R(L"Thick",2); g_cfg.gap=R(L"Gap",4);
    g_cfg.outlineSize=R(L"Ol",1); g_cfg.dotSize=R(L"Dot",3);
    g_cfg.color=RGB(R(L"CR",0),R(L"CG",255),R(L"CB",20));
    g_cfg.outlineColor=RGB(R(L"OR",0),R(L"OG",0),R(L"OB",0));
    g_cfg.style=R(L"Style",0); g_cfg.visible=R(L"Vis",1)!=0;
    g_cfg.lockToCenter=R(L"Lock",0)!=0; g_cfg.useSecondMonitor=R(L"Mon2",0)!=0;
}
static std::string ColorToHex(COLORREF c) {
    char buf[8];
    snprintf(buf,sizeof(buf),"#%02x%02x%02x",GetRValue(c),GetGValue(c),GetBValue(c));
    return buf;
}
static COLORREF HexToColor(const std::string& hex) {
    if (hex.size()<7) return RGB(0,255,20);
    int r=0,g=0,b=0;
    sscanf(hex.c_str()+1,"%02x%02x%02x",&r,&g,&b);
    return RGB(r,g,b);
}
static std::string WtoA(const wchar_t* w) {
    int n=WideCharToMultiByte(CP_UTF8,0,w,-1,nullptr,0,nullptr,nullptr);
    if(n<=0) return "";
    std::string s(n-1,' ');
    WideCharToMultiByte(CP_UTF8,0,w,-1,&s[0],n,nullptr,nullptr);
    return s;
}
static std::wstring AtoW(const std::string& s) {
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);
    if(n<=0) return L"";
    std::wstring w(n-1,L' ');
    MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,&w[0],n);
    return w;
}
static void SendStateToUI() {
    if (!g_wvView) return;
    char json[512];
    snprintf(json,sizeof(json),
        "{\"type\":\"state\",\"config\":{"
        "\"shape\":%d,\"size\":%d,\"thickness\":%d,\"gap\":%d,"
        "\"outlineSize\":%d,\"dotSize\":%d,"
        "\"color\":\"%s\",\"outlineColor\":\"%s\","
        "\"visible\":%s,\"lockMouse\":%s,\"secondMonitor\":%s}}",
        g_cfg.style,g_cfg.size,g_cfg.thickness,g_cfg.gap,
        g_cfg.outlineSize,g_cfg.dotSize,
        ColorToHex(g_cfg.color).c_str(),
        ColorToHex(g_cfg.outlineColor).c_str(),
        g_cfg.visible?"true":"false",
        g_cfg.lockToCenter?"true":"false",
        g_cfg.useSecondMonitor?"true":"false"
    );
    std::wstring wjson = AtoW(json);
    g_wvView->PostWebMessageAsString(wjson.c_str());
}
static void HandleUIMessage(const std::string& msg) {
    auto getStr=[&](const std::string& key)->std::string{
        std::string s="\""+key+"\":\"";
        size_t p=msg.find(s); if(p==std::string::npos) return "";
        p+=s.size(); size_t e=msg.find("\"",p);
        return e!=std::string::npos?msg.substr(p,e-p):"";
    };
    auto getInt=[&](const std::string& key)->int{
        std::string s="\""+key+"\":";
        size_t p=msg.find(s); if(p==std::string::npos) return -1;
        return atoi(msg.c_str()+p+s.size());
    };
    auto getBool=[&](const std::string& key)->int{
        std::string s="\""+key+"\":";
        size_t p=msg.find(s); if(p==std::string::npos) return -1;
        return msg.substr(p+s.size(),4)=="true"?1:0;
    };

    std::string type=getStr("type");

    if(type=="ready"){ SendStateToUI(); return; }

    if(type=="config"){
        std::string key=getStr("key");
        if(key=="shape")         g_cfg.style=getInt("value");
        else if(key=="size")     g_cfg.size=getInt("value");
        else if(key=="thickness")g_cfg.thickness=getInt("value");
        else if(key=="gap")      g_cfg.gap=getInt("value");
        else if(key=="outlineSize")g_cfg.outlineSize=getInt("value");
        else if(key=="dotSize")  g_cfg.dotSize=getInt("value");
        else if(key=="color")    g_cfg.color=HexToColor(getStr("value"));
        else if(key=="outlineColor")g_cfg.outlineColor=HexToColor(getStr("value"));
        else if(key=="visible"){
            int v=getBool("value"); if(v>=0){ g_cfg.visible=v!=0;
                ShowWindow(g_hOverlay,g_cfg.visible?SW_SHOWNOACTIVATE:SW_HIDE);
                InvalidateRect(g_hOverlay,NULL,TRUE); }
        }
        else if(key=="lockMouse"){
            int v=getBool("value"); if(v>=0){ g_cfg.lockToCenter=v!=0;
                if(!g_cfg.lockToCenter) ClipCursor(NULL); }
        }
        else if(key=="secondMonitor"){
            int v=getBool("value"); if(v>=0) g_cfg.useSecondMonitor=v!=0;
        }
        InvalidateRect(g_hOverlay,NULL,TRUE);
        SaveConfig(); return;
    }
    if(type=="center"){
        SendMessageW(g_hOverlay,WM_TIMER,TIMER_RECENTER,0); return;
    }
    if(type=="minimize"){ ShowWindow(g_hMain,SW_MINIMIZE); return; }
    if(type=="close"){
        ClipCursor(NULL); PostQuitMessage(0); DestroyWindow(g_hMain); return;
    }
}
static void PaintCrosshair(HDC hdc, int cx, int cy) {
    int s=g_cfg.size,t=g_cfg.thickness,gap=g_cfg.gap,ol=g_cfg.outlineSize;
    SelectObject(hdc,GetStockObject(NULL_BRUSH));
    auto DrawCross=[&](COLORREF c,int w){
        HPEN pen=CreatePen(PS_SOLID,w,c); SelectObject(hdc,pen);
        if(g_cfg.style==0||g_cfg.style==2||g_cfg.style==5){
            MoveToEx(hdc,cx-s,cy,NULL);   LineTo(hdc,cx-gap,cy);
            MoveToEx(hdc,cx+gap+1,cy,NULL); LineTo(hdc,cx+s+1,cy);
            MoveToEx(hdc,cx,cy-s,NULL);   LineTo(hdc,cx,cy-gap);
            MoveToEx(hdc,cx,cy+gap+1,NULL); LineTo(hdc,cx,cy+s+1);
        }
        if(g_cfg.style==3||g_cfg.style==4||g_cfg.style==5){
            HBRUSH ob=(HBRUSH)SelectObject(hdc,GetStockObject(NULL_BRUSH));
            Ellipse(hdc,cx-s,cy-s,cx+s+1,cy+s+1); SelectObject(hdc,ob);
        }
        DeleteObject(pen);
    };
    auto DrawDot=[&](COLORREF c,int extra){
        int d=g_cfg.dotSize+extra;
        HBRUSH br=CreateSolidBrush(c); HPEN pen=CreatePen(PS_SOLID,1,c);
        SelectObject(hdc,br); SelectObject(hdc,pen);
        Ellipse(hdc,cx-d,cy-d,cx+d+1,cy+d+1);
        DeleteObject(br); DeleteObject(pen);
    };
    if(ol>0) DrawCross(g_cfg.outlineColor,t+ol*2);
    DrawCross(g_cfg.color,t);
    if(g_cfg.style==1||g_cfg.style==2||g_cfg.style==4||g_cfg.style==5){
        if(ol>0) DrawDot(g_cfg.outlineColor,ol);
        DrawDot(g_cfg.color,0);
    }
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR,HDC,LPRECT lprc,LPARAM data){
    reinterpret_cast<std::vector<RECT>*>(data)->push_back(*lprc); return TRUE;
}
static void RepositionOverlay(){
    if(!g_hOverlay) return;
    int sz=200;
    RECT target={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
    if(g_cfg.useSecondMonitor){
        std::vector<RECT> m; EnumDisplayMonitors(NULL,NULL,MonitorEnumProc,(LPARAM)&m);
        if(m.size()>=2) target=m[1];
    }
    int cx=(target.left+target.right)/2, cy=(target.top+target.bottom)/2;
    SetWindowPos(g_hOverlay,HWND_TOPMOST,cx-sz/2,cy-sz/2,sz,sz,SWP_NOACTIVATE|SWP_SHOWWINDOW);
}

static LRESULT CALLBACK OverlayProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_PAINT){
        RECT rc; GetClientRect(hwnd,&rc);
        PAINTSTRUCT ps; HDC hdcWin=BeginPaint(hwnd,&ps);
        HDC hdcS=GetDC(NULL); HDC hdcM=CreateCompatibleDC(hdcS);
        HBITMAP bmp=CreateCompatibleBitmap(hdcS,rc.right,rc.bottom);
        SelectObject(hdcM,bmp);
        HBRUSH bg=CreateSolidBrush(RGB(255,0,255)); FillRect(hdcM,&rc,bg); DeleteObject(bg);
        SetBkMode(hdcM,TRANSPARENT);
        if(g_cfg.visible) PaintCrosshair(hdcM,rc.right/2,rc.bottom/2);
        BitBlt(hdcWin,0,0,rc.right,rc.bottom,hdcM,0,0,SRCCOPY);
        DeleteObject(bmp); DeleteDC(hdcM); ReleaseDC(NULL,hdcS);
        EndPaint(hwnd,&ps); return 0;
    }
    if(msg==WM_TIMER&&wp==TIMER_RECENTER){
        RepositionOverlay();
        SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        if(g_cfg.lockToCenter){
            RECT c={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
            ClipCursor(&c);
        } else ClipCursor(NULL);
        return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

static void CreateOverlay(){
    WNDCLASSEXW wc={}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=OverlayProc;
    wc.hInstance=g_hInst; wc.lpszClassName=OVERLAY_CLASS; RegisterClassExW(&wc);
    int sz=200,sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);
    g_hOverlay=CreateWindowExW(
        WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,
        OVERLAY_CLASS,L"",WS_POPUP,(sw-sz)/2,(sh-sz)/2,sz,sz,NULL,NULL,g_hInst,NULL);
    SetLayeredWindowAttributes(g_hOverlay,RGB(255,0,255),0,LWA_COLORKEY);
    BOOL excl=FALSE;
    DwmSetWindowAttribute(g_hOverlay,DWMWA_EXCLUDED_FROM_PEEK,&excl,sizeof(excl));
    SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
    ShowWindow(g_hOverlay,SW_SHOWNOACTIVATE);
    SetWindowPos(g_hOverlay,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetTimer(g_hOverlay,TIMER_RECENTER,250,NULL);
}

static void InitWebView(HWND hwnd);

struct EnvHandler : ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    HWND hwnd; LONG ref=1;
    EnvHandler(HWND h):hwnd(h){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{ return E_NOINTERFACE; }
    ULONG   STDMETHODCALLTYPE AddRef()  override{ return ++ref; }
    ULONG   STDMETHODCALLTYPE Release() override{ if(--ref==0){delete this;return 0;}return ref; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Environment* env) override;
};

struct CtrlHandler : ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    HWND hwnd; LONG ref=1;
    CtrlHandler(HWND h):hwnd(h){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{ return E_NOINTERFACE; }
    ULONG   STDMETHODCALLTYPE AddRef()  override{ return ++ref; }
    ULONG   STDMETHODCALLTYPE Release() override{ if(--ref==0){delete this;return 0;}return ref; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Controller* ctrl) override;
};

struct MsgHandler : ICoreWebView2WebMessageReceivedEventHandler {
    LONG ref=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{ return E_NOINTERFACE; }
    ULONG   STDMETHODCALLTYPE AddRef()  override{ return ++ref; }
    ULONG   STDMETHODCALLTYPE Release() override{ if(--ref==0){delete this;return 0;}return ref; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        wchar_t* msg=nullptr;
        args->TryGetWebMessageAsString(&msg);
        if(msg){ HandleUIMessage(WtoA(msg)); CoTaskMemFree(msg); }
        return S_OK;
    }
};

HRESULT EnvHandler::Invoke(HRESULT hr, ICoreWebView2Environment* env){
    if(FAILED(hr)){
        MessageBoxW(hwnd,
            L"WebView2 runtime not found.\n\nDownload from:\nhttps://developer.microsoft.com/microsoft-edge/webview2/",
            L"CrosshairG",MB_OK|MB_ICONERROR);
        return hr;
    }
    env->CreateCoreWebView2Controller(hwnd, new CtrlHandler(hwnd));
    return S_OK;
}

HRESULT CtrlHandler::Invoke(HRESULT hr, ICoreWebView2Controller* ctrl){
    if(FAILED(hr)) return hr;
    g_wvController=ctrl; ctrl->AddRef();
    ctrl->get_CoreWebView2(&g_wvView);

    RECT rc; GetClientRect(hwnd,&rc);
    ctrl->put_Bounds(rc);

    ICoreWebView2Settings* settings=nullptr;
    g_wvView->get_Settings(&settings);
    if(settings){
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->Release();
    }
    wchar_t uiFolderPath[MAX_PATH];
    GetModuleFileNameW(NULL,uiFolderPath,MAX_PATH);
    wchar_t* sl3=wcsrchr(uiFolderPath,L'\\');
    if(sl3) wcscpy_s(sl3+1,MAX_PATH-(sl3-uiFolderPath)-1,L"ui");

    ICoreWebView2_3* wv3=nullptr;
    if(SUCCEEDED(g_wvView->QueryInterface(IID_ICoreWebView2_3,(void**)&wv3)) && wv3){
        wv3->SetVirtualHostNameToFolderMapping(
            L"crosshairg.local", uiFolderPath,
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
        wv3->Release();
    }

    EventRegistrationToken token;
    g_wvView->add_WebMessageReceived(new MsgHandler(), &token);

    g_wvView->Navigate(L"https://crosshairg.local/index.html");
    return S_OK;
}

static void InitWebView(HWND hwnd){
    wchar_t udPath[MAX_PATH];
    GetModuleFileNameW(NULL,udPath,MAX_PATH);
    wchar_t* sl=wcsrchr(udPath,L'\\');
    if(sl) wcscpy_s(sl+1,MAX_PATH-(sl-udPath)-1,L"webview_data");

    CreateCoreWebView2EnvironmentWithOptions(nullptr,udPath,nullptr,new EnvHandler(hwnd));
}

static LRESULT CALLBACK MainProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_SIZE:
        if(g_wvController){
            RECT rc; GetClientRect(hwnd,&rc);
            g_wvController->put_Bounds(rc);
        }
        return 0;
    case WM_HOTKEY:
        if(wp==1){
            if(IsIconic(hwnd)){ ShowWindow(hwnd,SW_RESTORE); SetForegroundWindow(hwnd); }
            else if(IsWindowVisible(hwnd)) ShowWindow(hwnd,SW_MINIMIZE);
            else { ShowWindow(hwnd,SW_RESTORE); SetForegroundWindow(hwnd); }
        }
        if(wp==2){
            g_cfg.visible=!g_cfg.visible;
            ShowWindow(g_hOverlay,g_cfg.visible?SW_SHOWNOACTIVATE:SW_HIDE);
            InvalidateRect(g_hOverlay,NULL,TRUE);
            SendStateToUI(); SaveConfig();
        }
        return 0;
    case WM_SYSCOMMAND:
        if(LOWORD(wp)==SC_CLOSE){ ClipCursor(NULL); PostQuitMessage(0); DestroyWindow(hwnd); return 0; }
        return DefWindowProcW(hwnd,msg,wp,lp);
    case WM_CLOSE:
        ClipCursor(NULL); PostQuitMessage(0); DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        if(g_wvController){ g_wvController->Release(); g_wvController=nullptr; }
        if(g_wvView){ g_wvView->Release(); g_wvView=nullptr; }
        ClipCursor(NULL); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE,LPWSTR,int){
    g_hInst=hInst;

    GetModuleFileNameW(NULL,g_iniPath,MAX_PATH);
    wchar_t* sl=wcsrchr(g_iniPath,L'\\');
    if(sl) wcscpy_s(sl+1,MAX_PATH-(sl-g_iniPath)-1,L"crosshair.ini");

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* sl2 = wcsrchr(exePath, L'\\');
    if (sl2) {
        wcscpy_s(sl2+1, MAX_PATH-(sl2-exePath)-1, L"ui\\index.html");
    }
    for (wchar_t* p = exePath; *p; p++) if (*p == L'\\') *p = L'/';
    swprintf_s(g_uiPath, MAX_PATH, L"file:///%s", exePath);

    LoadConfig();
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc={}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=MainProc;
    wc.hInstance=hInst; wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MAIN_CLASS;
    wc.hIcon  =(HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_DEFAULTCOLOR);
    wc.hIconSm=(HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
    if(!wc.hIcon)   wc.hIcon  =LoadIcon(NULL,IDI_APPLICATION);
    if(!wc.hIconSm) wc.hIconSm=LoadIcon(NULL,IDI_APPLICATION);
    RegisterClassExW(&wc);

    g_hMain=CreateWindowExW(WS_EX_APPWINDOW,MAIN_CLASS,L"CrosshairG v1.3",
        WS_OVERLAPPEDWINDOW,100,100,820,540,NULL,NULL,hInst,NULL);

    BOOL dark=TRUE;
    DwmSetWindowAttribute(g_hMain,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));

    CreateOverlay();
    InitWebView(g_hMain);

    ShowWindow(g_hMain,SW_SHOW);
    SetForegroundWindow(g_hMain);

    if(!RegisterHotKey(g_hMain,1,MOD_CONTROL,VK_F5))
        MessageBoxW(g_hMain,L"Ctrl+F5 is in use by another app.",L"CrosshairG",MB_OK|MB_ICONWARNING);
    if(!RegisterHotKey(g_hMain,2,MOD_CONTROL,VK_F6))
        MessageBoxW(g_hMain,L"Ctrl+F6 is in use by another app.",L"CrosshairG",MB_OK|MB_ICONWARNING);

    MSG m;
    while(GetMessageW(&m,NULL,0,0)){ TranslateMessage(&m); DispatchMessageW(&m); }

    CoUninitialize();
    return 0;
}
