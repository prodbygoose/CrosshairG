#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <ole2.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>

#include "WebView2.h"

#define TIMER_RECENTER  1
#define OVERLAY_CLASS   L"CHOverlay"
#define MAIN_CLASS      L"CHMain"

struct Config {
    int size=12, thickness=1, gap=4, outlineSize=1, dotSize=3;
    COLORREF color=RGB(0,255,20), outlineColor=RGB(0,0,0);
    int style=0;
    bool visible=true, lockToCenter=false, useSecondMonitor=false;
    char theme[32]="nvg";
};

static Config    g_cfg;
static HWND      g_hMain    = NULL;
static HWND      g_hOverlay = NULL;
static HINSTANCE g_hInst    = NULL;
static wchar_t   g_iniPath[MAX_PATH];

static ICoreWebView2Controller*  g_wvController = nullptr;
static ICoreWebView2*            g_wvView        = nullptr;
static EventRegistrationToken    g_msgToken      = {};

static bool    g_cursorClipped = false;   // tracks whether ClipCursor is currently active

static void SaveConfig() {
    wchar_t b[32];
    auto W=[&](const wchar_t*k,int v){ _itow_s(v,b,10); WritePrivateProfileStringW(L"X",k,b,g_iniPath); };
    W(L"Size",g_cfg.size); W(L"Thick",g_cfg.thickness); W(L"Gap",g_cfg.gap);
    W(L"Ol",g_cfg.outlineSize); W(L"Dot",g_cfg.dotSize);
    W(L"CR",GetRValue(g_cfg.color)); W(L"CG",GetGValue(g_cfg.color)); W(L"CB",GetBValue(g_cfg.color));
    W(L"OR",GetRValue(g_cfg.outlineColor)); W(L"OG",GetGValue(g_cfg.outlineColor)); W(L"OB",GetBValue(g_cfg.outlineColor));
    W(L"Style",g_cfg.style); W(L"Vis",g_cfg.visible?1:0);
    W(L"Lock",g_cfg.lockToCenter?1:0); W(L"Mon2",g_cfg.useSecondMonitor?1:0);
    wchar_t wtheme[32]; MultiByteToWideChar(CP_UTF8,0,g_cfg.theme,-1,wtheme,32);
    WritePrivateProfileStringW(L"X",L"Theme",wtheme,g_iniPath);
}
static void ClampConfig() {
    auto clamp=[](int v,int lo,int hi){ return v<lo?lo:v>hi?hi:v; };
    g_cfg.size       = clamp(g_cfg.size,      0, 50);
    g_cfg.thickness  = clamp(g_cfg.thickness,  1, 50);
    g_cfg.gap        = clamp(g_cfg.gap,        0, 50);
    g_cfg.outlineSize= clamp(g_cfg.outlineSize,0, 10);
    g_cfg.dotSize    = clamp(g_cfg.dotSize,    1, 20);
    g_cfg.style      = clamp(g_cfg.style,      0,  5);
}
static void LoadConfig() {
    auto R=[&](const wchar_t*k,int d){ return (int)GetPrivateProfileIntW(L"X",k,d,g_iniPath); };
    g_cfg.size=R(L"Size",12); g_cfg.thickness=R(L"Thick",1); g_cfg.gap=R(L"Gap",4);
    g_cfg.outlineSize=R(L"Ol",1); g_cfg.dotSize=R(L"Dot",3);
    g_cfg.color=RGB(R(L"CR",0),R(L"CG",255),R(L"CB",20));
    g_cfg.outlineColor=RGB(R(L"OR",0),R(L"OG",0),R(L"OB",0));
    g_cfg.style=R(L"Style",0); g_cfg.visible=R(L"Vis",1)!=0;
    g_cfg.lockToCenter=R(L"Lock",0)!=0; g_cfg.useSecondMonitor=R(L"Mon2",0)!=0;
    wchar_t wtheme[32]=L"nvg";
    GetPrivateProfileStringW(L"X",L"Theme",L"nvg",wtheme,32,g_iniPath);
    WideCharToMultiByte(CP_UTF8,0,wtheme,-1,g_cfg.theme,32,nullptr,nullptr);
    ClampConfig();
}

static std::string ColorToHex(COLORREF c) {
    char buf[8];
    snprintf(buf,sizeof(buf),"#%02x%02x%02x",GetRValue(c),GetGValue(c),GetBValue(c));
    return buf;
}
static COLORREF HexToColor(const std::string& hex) {
    if (hex.size()<7) return RGB(0,255,20);
    int r=0,g=0,b=0;
    if(sscanf(hex.c_str()+1,"%02x%02x%02x",&r,&g,&b)!=3) return RGB(0,255,20);
    return RGB(r,g,b);
}
static std::string WtoA(const wchar_t* w) {
    int n=WideCharToMultiByte(CP_UTF8,0,w,-1,nullptr,0,nullptr,nullptr);
    if(n<=0) return "";
    std::string s(n,' ');
    WideCharToMultiByte(CP_UTF8,0,w,-1,&s[0],n,nullptr,nullptr);
    if(!s.empty()&&s.back()=='\0') s.pop_back();
    return s;
}
static std::wstring AtoW(const std::string& s) {
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);
    if(n<=0) return L"";
    std::wstring w(n,L' ');
    MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,&w[0],n);
    if(!w.empty()&&w.back()==L'\0') w.pop_back();
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
        "\"visible\":%s,\"lockMouse\":%s,\"secondMonitor\":%s,"
        "\"theme\":\"%s\"}}",
        g_cfg.style,g_cfg.size,g_cfg.thickness,g_cfg.gap,
        g_cfg.outlineSize,g_cfg.dotSize,
        ColorToHex(g_cfg.color).c_str(),
        ColorToHex(g_cfg.outlineColor).c_str(),
        g_cfg.visible?"true":"false",
        g_cfg.lockToCenter?"true":"false",
        g_cfg.useSecondMonitor?"true":"false",
        g_cfg.theme
    );
    std::wstring wjson = AtoW(json);
    g_wvView->PostWebMessageAsString(wjson.c_str());
}

static std::string jsonStr(const std::string& msg, const std::string& key){
    std::string needle="\""+key+"\":\"";
    size_t p=msg.find(needle);
    if(p==std::string::npos) return "";
    p+=needle.size();
    size_t e=msg.find('"',p);
    if(e==std::string::npos||e-p>256) return "";
    return msg.substr(p,e-p);
}
static int jsonInt(const std::string& msg, const std::string& key){
    std::string needle="\""+key+"\":";
    size_t p=msg.find(needle);
    if(p==std::string::npos) return INT_MIN;
    p+=needle.size();
    if(p>=msg.size()) return INT_MIN;
    char* end=nullptr;
    long v=strtol(msg.c_str()+p,&end,10);
    if(end==msg.c_str()+p) return INT_MIN;
    return (int)v;
}
static int jsonBool(const std::string& msg, const std::string& key){
    std::string needle="\""+key+"\":";
    size_t p=msg.find(needle);
    if(p==std::string::npos) return -1;
    p+=needle.size();
    if(p+4<=msg.size()&&msg.substr(p,4)=="true")  return 1;
    if(p+5<=msg.size()&&msg.substr(p,5)=="false") return 0;
    return -1;
}

static void HandleUIMessage(const std::string& msg) {
    if(msg.empty()||msg.front()!='{') return;

    std::string type=jsonStr(msg,"type");
    if(type.empty()) return;

    if(type=="ready"){
        ShowWindow(g_hMain, SW_SHOWMAXIMIZED);
        SetForegroundWindow(g_hMain);
        SendStateToUI(); return;
    }

    if(type=="config"){
        std::string key=jsonStr(msg,"key");
        if(key.empty()) return;

        if(key=="shape"){
            int v=jsonInt(msg,"value"); if(v!=INT_MIN) g_cfg.style=v;
        } else if(key=="size"){
            int v=jsonInt(msg,"value"); if(v!=INT_MIN) g_cfg.size=v;
        } else if(key=="thickness"){
            int v=jsonInt(msg,"value"); if(v!=INT_MIN) g_cfg.thickness=v;
        } else if(key=="gap"){
            int v=jsonInt(msg,"value"); if(v!=INT_MIN) g_cfg.gap=v;
        } else if(key=="outlineSize"){
            int v=jsonInt(msg,"value"); if(v!=INT_MIN) g_cfg.outlineSize=v;
        } else if(key=="dotSize"){
            int v=jsonInt(msg,"value"); if(v!=INT_MIN) g_cfg.dotSize=v;
        } else if(key=="color"){
            std::string v=jsonStr(msg,"value"); if(!v.empty()) g_cfg.color=HexToColor(v);
        } else if(key=="outlineColor"){
            std::string v=jsonStr(msg,"value"); if(!v.empty()) g_cfg.outlineColor=HexToColor(v);
        } else if(key=="visible"){
            int v=jsonBool(msg,"value");
            if(v>=0){ g_cfg.visible=v!=0;
                SetWindowPos(g_hOverlay,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|(g_cfg.visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
                InvalidateRect(g_hOverlay,NULL,TRUE); }
        } else if(key=="lockMouse"){
            int v=jsonBool(msg,"value");
            if(v>=0){ g_cfg.lockToCenter=v!=0; if(!g_cfg.lockToCenter && g_cursorClipped){ ClipCursor(NULL); g_cursorClipped=false; } }
        } else if(key=="secondMonitor"){
            int v=jsonBool(msg,"value"); if(v>=0) g_cfg.useSecondMonitor=v!=0;
        } else if(key=="theme"){
            std::string v=jsonStr(msg,"value");
            if(!v.empty()) strncpy_s(g_cfg.theme,32,v.c_str(),_TRUNCATE);
        }
        ClampConfig();
        InvalidateRect(g_hOverlay,NULL,TRUE);
        SaveConfig(); return;
    }
    if(type=="bootDone"){
        if(g_cfg.visible) ShowWindow(g_hOverlay,SW_SHOWNOACTIVATE);
        return;
    }
    if(type=="center"){
        SendMessageW(g_hOverlay,WM_TIMER,TIMER_RECENTER,0); return;
    }
    if(type=="minimize"){ ShowWindow(g_hMain,SW_MINIMIZE); return; }
    if(type=="maximize"){
        ShowWindow(g_hMain, IsZoomed(g_hMain) ? SW_RESTORE : SW_MAXIMIZE);
        return;
    }
    if(type=="startDrag"){
        // Start OS window-move loop (reliable cross-style drag from WebView2)
        if(!IsZoomed(g_hMain)){
            ReleaseCapture();
            PostMessage(g_hMain, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        return;
    }
    if(type=="startResizeTop"){
        // Top-edge resize: no NC area there, so we post HTTOP manually
        if(!IsZoomed(g_hMain)){
            POINT pt; GetCursorPos(&pt);
            ReleaseCapture();
            PostMessage(g_hMain, WM_NCLBUTTONDOWN, HTTOP, MAKELPARAM(pt.x, pt.y));
        }
        return;
    }
    if(type=="close"){
        DestroyWindow(g_hMain); return;
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
    int maxReach = g_cfg.size + g_cfg.thickness + g_cfg.outlineSize + g_cfg.dotSize + 4;
    int sz = maxReach * 2 + 20;
    if(sz < 200) sz = 200;
    RECT target={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
    if(g_cfg.useSecondMonitor){
        std::vector<RECT> m; EnumDisplayMonitors(NULL,NULL,MonitorEnumProc,(LPARAM)&m);
        if(m.size()>=2) target=m[1];
    }
    int cx=(target.left+target.right)/2, cy=(target.top+target.bottom)/2;
    SetWindowPos(g_hOverlay,HWND_TOPMOST,cx-sz/2,cy-sz/2,sz,sz,SWP_NOACTIVATE);
}

static LRESULT CALLBACK OverlayProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_PAINT){
        RECT rc; GetClientRect(hwnd,&rc);
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
        HBRUSH bg=CreateSolidBrush(RGB(255,0,255));
        FillRect(hdc,&rc,bg); DeleteObject(bg);
        SetBkMode(hdc,TRANSPARENT);
        if(g_cfg.visible) PaintCrosshair(hdc,rc.right/2,rc.bottom/2);
        EndPaint(hwnd,&ps); return 0;
    }
    if(msg==WM_TIMER&&wp==TIMER_RECENTER){
        if(g_cfg.visible){
            RepositionOverlay();
            SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        }
        if(g_cfg.lockToCenter){
            RECT c={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
            if(g_cfg.useSecondMonitor){
                std::vector<RECT> m; EnumDisplayMonitors(NULL,NULL,MonitorEnumProc,(LPARAM)&m);
                if(m.size()>=2) c=m[1];
            }
            if(!g_cursorClipped){ ClipCursor(&c); g_cursorClipped=true; }
        } else {
            if(g_cursorClipped){ ClipCursor(NULL); g_cursorClipped=false; }
        }
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
    SetWindowPos(g_hOverlay,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_HIDEWINDOW);
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

    RECT rc; GetClientRect(hwnd, &rc);
    ctrl->put_Bounds(rc);
    ctrl->put_IsVisible(TRUE);

    ICoreWebView2Controller2* ctrl2=nullptr;
    if(SUCCEEDED(ctrl->QueryInterface(IID_ICoreWebView2Controller2,(void**)&ctrl2)) && ctrl2){
        COREWEBVIEW2_COLOR bg={255,8,10,8};
        ctrl2->put_DefaultBackgroundColor(bg);
        ctrl2->Release();
    }

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

    g_wvView->add_WebMessageReceived(new MsgHandler(), &g_msgToken);

    g_wvView->Navigate(L"https://crosshairg.local/index.html");
    return S_OK;
}

static void InitWebView(HWND hwnd){
    wchar_t udPath[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, udPath);
    wcscat_s(udPath, MAX_PATH, L"\\CrosshairG");
    CreateDirectoryW(udPath, NULL);
    wcscat_s(udPath, MAX_PATH, L"\\webview_data");
    CreateDirectoryW(udPath, NULL);

    CreateCoreWebView2EnvironmentWithOptions(nullptr, udPath, nullptr, new EnvHandler(hwnd));
}

static LRESULT CALLBACK MainProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_NCHITTEST:
        // Maximized: no resize, whole window is client area
        if(IsZoomed(hwnd)) return HTCLIENT;
        // Windowed: DefWindowProc maps mouse pos to NC resize codes (HTLEFT etc.)
        return DefWindowProcW(hwnd,msg,wp,lp);

    case WM_NCCALCSIZE:
        if(wp){
            if(!IsZoomed(hwnd)){
                // Keep left/right/bottom NC resize borders; suppress top border
                // so the header stays flush with the top (no white bar).
                NCCALCSIZE_PARAMS* p=(NCCALCSIZE_PARAMS*)lp;
                int pad=GetSystemMetrics(SM_CXPADDEDBORDER);
                int cx =GetSystemMetrics(SM_CXSIZEFRAME)+pad;
                int cy =GetSystemMetrics(SM_CYSIZEFRAME)+pad;
                p->rgrc[0].left  +=cx;
                p->rgrc[0].right -=cx;
                p->rgrc[0].bottom-=cy;
                // top not adjusted → no top NC strip → no DWM white bar
            }
            return 0; // maximized: client = window rect
        }
        return DefWindowProcW(hwnd,msg,wp,lp);

    case WM_NCPAINT: {
        // Paint the NC border strips dark so they match the theme background
        if(IsZoomed(hwnd)) return 0;
        HDC hdc=GetWindowDC(hwnd);
        RECT wr; GetWindowRect(hwnd,&wr);
        int ww=wr.right-wr.left, wh=wr.bottom-wr.top;
        int pad=GetSystemMetrics(SM_CXPADDEDBORDER);
        int cx=GetSystemMetrics(SM_CXSIZEFRAME)+pad;
        int cy=GetSystemMetrics(SM_CYSIZEFRAME)+pad;
        HBRUSH br=CreateSolidBrush(RGB(8,10,8));
        RECT r;
        r={0,    0,    cx,    wh}; FillRect(hdc,&r,br); // left strip
        r={ww-cx,0,    ww,    wh}; FillRect(hdc,&r,br); // right strip
        r={0,    wh-cy,ww,    wh}; FillRect(hdc,&r,br); // bottom strip
        DeleteObject(br);
        ReleaseDC(hwnd,hdc);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        // Maximize to work area so taskbar stays visible
        MINMAXINFO* mmi=(MINMAXINFO*)lp;
        HMONITOR mon=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi={sizeof(mi)};
        GetMonitorInfoW(mon,&mi);
        mmi->ptMaxPosition.x=mi.rcWork.left;
        mmi->ptMaxPosition.y=mi.rcWork.top;
        mmi->ptMaxSize.x=mi.rcWork.right-mi.rcWork.left;
        mmi->ptMaxSize.y=mi.rcWork.bottom-mi.rcWork.top;
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc=(HDC)wp;
        RECT rc; GetClientRect(hwnd,&rc);
        HBRUSH br=CreateSolidBrush(RGB(8,10,8));
        FillRect(hdc,&rc,br);
        DeleteObject(br);
        return 1;
    }
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
            SetWindowPos(g_hOverlay,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|(g_cfg.visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
            InvalidateRect(g_hOverlay,NULL,TRUE);
            if(!g_cfg.visible){ g_cfg.lockToCenter=false; if(g_cursorClipped){ ClipCursor(NULL); g_cursorClipped=false; } }
            SendStateToUI(); SaveConfig();
        }
        return 0;
    case WM_SYSCOMMAND:
        if(LOWORD(wp)==SC_CLOSE){ DestroyWindow(hwnd); return 0; }
        return DefWindowProcW(hwnd,msg,wp,lp);
    case WM_CLOSE:
        DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        if(g_wvController){ g_wvController->Release(); g_wvController=nullptr; }
        if(g_wvView){
            g_wvView->remove_WebMessageReceived(g_msgToken);
            g_wvView->Release(); g_wvView=nullptr;
        }
        if(g_cursorClipped){ ClipCursor(NULL); g_cursorClipped=false; }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE,LPWSTR,int){
    g_hInst=hInst;

    // ── Single-instance guard ──────────────────────────────────────────────────
    // If our window class is already registered and visible, another instance
    // is running — bring it forward and exit. No mutex needed for a GUI app.
    {
        HWND hExisting = FindWindowW(MAIN_CLASS, NULL);
        if(hExisting){
            if(IsIconic(hExisting)) ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
            return 0;
        }
    }

    GetModuleFileNameW(NULL,g_iniPath,MAX_PATH);
    wchar_t* sl=wcsrchr(g_iniPath,L'\\');
    if(sl) wcscpy_s(sl+1,MAX_PATH-(sl-g_iniPath)-1,L"crosshair.ini");

    LoadConfig();
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc={}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=MainProc;
    wc.hInstance=hInst; wc.hbrBackground=NULL;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MAIN_CLASS;
    wc.hIcon  =(HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_DEFAULTCOLOR);
    wc.hIconSm=(HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
    if(!wc.hIcon)   wc.hIcon  =LoadIcon(NULL,IDI_APPLICATION);
    if(!wc.hIconSm) wc.hIconSm=LoadIcon(NULL,IDI_APPLICATION);
    RegisterClassExW(&wc);

    int ww=820, wh=540;
    int wx=(GetSystemMetrics(SM_CXSCREEN)-ww)/2;
    int wy=(GetSystemMetrics(SM_CYSCREEN)-wh)/2;
    g_hMain=CreateWindowExW(WS_EX_APPWINDOW,MAIN_CLASS,L"CrosshairG v1.4.5",
        WS_POPUP|WS_THICKFRAME,wx,wy,ww,wh,NULL,NULL,hInst,NULL);

    CreateOverlay();
    InitWebView(g_hMain);

    if(!RegisterHotKey(g_hMain,1,MOD_CONTROL,VK_F5))
        MessageBoxW(g_hMain,L"Ctrl+F5 is in use by another app.",L"CrosshairG",MB_OK|MB_ICONWARNING);
    if(!RegisterHotKey(g_hMain,2,MOD_CONTROL,VK_F6))
        MessageBoxW(g_hMain,L"Ctrl+F6 is in use by another app.",L"CrosshairG",MB_OK|MB_ICONWARNING);

    MSG m;
    while(GetMessageW(&m,NULL,0,0)){ TranslateMessage(&m); DispatchMessageW(&m); }

    CoUninitialize();
    return 0;
}
