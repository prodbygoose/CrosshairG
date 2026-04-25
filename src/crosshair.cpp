#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>

#define WM_TRAY_ICON          (WM_USER+1)
#define WM_UPDATE_OVERLAY     (WM_USER+2)
#define IDI_TRAY              1001
#define ID_TRAY_SHOW          2001
#define ID_TRAY_SETTINGS      2002
#define ID_TRAY_EXIT          2003
#define TIMER_RECENTER        1
#define OVERLAY_CLASS         L"CHOverlay"
#define MAIN_CLASS            L"CHMain"

#define IDC_SIZE_SLIDER       3001
#define IDC_THICK_SLIDER      3002
#define IDC_GAP_SLIDER        3003
#define IDC_OUTLINE_SLIDER    3004
#define IDC_DOT_SLIDER        3005
#define IDC_STYLE_COMBO       3006
#define IDC_COLOR_BTN         3007
#define IDC_OUTLINE_COLOR_BTN 3008
#define IDC_VISIBLE_CHK       3009
#define IDC_LOCK_CHK          3010
#define IDC_PREVIEW           3011
#define IDC_MONITOR_CHK       3012
#define IDC_CENTER_BTN        3013

#define IDC_LBL_SIZE          3020
#define IDC_LBL_WIDTH         3021
#define IDC_LBL_GAP           3022
#define IDC_LBL_OUTLINE       3023
#define IDC_LBL_DOT           3024

#define CLR_BG         RGB(13,13,15)
#define CLR_SURFACE    RGB(22,22,26)
#define CLR_SURFACE2   RGB(30,30,36)
#define CLR_BORDER     RGB(45,45,55)
#define CLR_ACCENT     RGB(0,210,120)
#define CLR_ACCENT2    RGB(0,160,90)
#define CLR_TEXT       RGB(230,230,235)
#define CLR_TEXT_DIM   RGB(120,120,135)
#define CLR_TEXT_MUTED RGB(60,60,72)
#define CLR_DANGER     RGB(255,70,70)
#define CLR_SLIDER_TRK RGB(40,40,50)
#define CLR_SLIDER_THM RGB(0,210,120)

struct Config {
    int size, thickness, gap, outlineSize, dotSize, circleRadius;
    COLORREF color, outlineColor;
    int  style;
    bool visible, lockToCenter, useSecondMonitor;
};
static Config g_cfg = { 12,2,4,1,3,16, RGB(0,255,0),RGB(0,0,0), 0,true,true,false };

static HWND      g_hMain    = NULL;
static HWND      g_hOverlay = NULL;
static HINSTANCE g_hInst    = NULL;
static NOTIFYICONDATA g_nid = {};
static wchar_t   g_iniPath[MAX_PATH];

static HFONT g_fntLabel  = NULL;
static HFONT g_fntValue  = NULL;
static HFONT g_fntTitle  = NULL;
static HFONT g_fntMono   = NULL;

static void CreateFonts() {
    g_fntLabel = CreateFontW(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_fntValue = CreateFontW(-13,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_fntTitle = CreateFontW(-12,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_fntMono  = CreateFontW(-11,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
}

static void SaveConfig() {
    wchar_t b[32];
    auto W=[&](const wchar_t*k,int v){ _itow_s(v,b,10); WritePrivateProfileStringW(L"X",k,b,g_iniPath); };
    W(L"Size",g_cfg.size); W(L"Thick",g_cfg.thickness); W(L"Gap",g_cfg.gap);
    W(L"Ol",g_cfg.outlineSize); W(L"Dot",g_cfg.dotSize); W(L"Circle",g_cfg.circleRadius);
    W(L"CR",GetRValue(g_cfg.color)); W(L"CG",GetGValue(g_cfg.color)); W(L"CB",GetBValue(g_cfg.color));
    W(L"OR",GetRValue(g_cfg.outlineColor)); W(L"OG",GetGValue(g_cfg.outlineColor)); W(L"OB",GetBValue(g_cfg.outlineColor));
    W(L"Style",g_cfg.style); W(L"Vis",g_cfg.visible?1:0); W(L"Lock",g_cfg.lockToCenter?1:0); W(L"Mon2",g_cfg.useSecondMonitor?1:0);
}
static void LoadConfig() {
    auto R=[&](const wchar_t*k,int d){ return (int)GetPrivateProfileIntW(L"X",k,d,g_iniPath); };
    g_cfg.size=R(L"Size",12); g_cfg.thickness=R(L"Thick",2); g_cfg.gap=R(L"Gap",4);
    g_cfg.outlineSize=R(L"Ol",1); g_cfg.dotSize=R(L"Dot",3); g_cfg.circleRadius=R(L"Circle",16);
    g_cfg.color=RGB(R(L"CR",0),R(L"CG",255),R(L"CB",0));
    g_cfg.outlineColor=RGB(R(L"OR",0),R(L"OG",0),R(L"OB",0));
    g_cfg.style=R(L"Style",0); g_cfg.visible=R(L"Vis",1)!=0; g_cfg.lockToCenter=R(L"Lock",1)!=0; g_cfg.useSecondMonitor=R(L"Mon2",0)!=0;
}

static void FillRoundRect(HDC hdc, RECT r, int rad, COLORREF c) {
    HBRUSH br=CreateSolidBrush(c); HPEN pen=CreatePen(PS_SOLID,1,c);
    HBRUSH ob=(HBRUSH)SelectObject(hdc,br); HPEN op=(HPEN)SelectObject(hdc,pen);
    RoundRect(hdc,r.left,r.top,r.right,r.bottom,rad,rad);
    SelectObject(hdc,ob); SelectObject(hdc,op);
    DeleteObject(br); DeleteObject(pen);
}
static void DrawRoundBorder(HDC hdc, RECT r, int rad, COLORREF c, int w=1) {
    HPEN pen=CreatePen(PS_SOLID,w,c); HBRUSH ob=(HBRUSH)SelectObject(hdc,GetStockObject(NULL_BRUSH));
    HPEN op=(HPEN)SelectObject(hdc,pen);
    RoundRect(hdc,r.left,r.top,r.right,r.bottom,rad,rad);
    SelectObject(hdc,ob); SelectObject(hdc,op); DeleteObject(pen);
}
static void DrawText_(HDC hdc, const wchar_t* t, RECT r, COLORREF c, HFONT f, UINT fmt=DT_LEFT|DT_VCENTER|DT_SINGLELINE) {
    SetTextColor(hdc,c); SetBkMode(hdc,TRANSPARENT);
    HFONT of=(HFONT)SelectObject(hdc,f);
    DrawTextW(hdc,t,-1,&r,fmt);
    SelectObject(hdc,of);
}
static void DrawColorSwatch(HDC hdc, int x, int y, int w, int h, COLORREF c) {
    RECT r={x,y,x+w,y+h};
    FillRoundRect(hdc,r,4,c);
    DrawRoundBorder(hdc,r,4,CLR_BORDER);
}

static void PaintCrosshair(HDC hdc, int cx, int cy) {
    int s=g_cfg.size, t=g_cfg.thickness, gap=g_cfg.gap, ol=g_cfg.outlineSize;
    SelectObject(hdc, GetStockObject(NULL_BRUSH));

    auto DrawCross=[&](COLORREF c, int w){
        HPEN pen=CreatePen(PS_SOLID,w,c); SelectObject(hdc,pen);
        if(g_cfg.style==0||g_cfg.style==2||g_cfg.style==5){

            MoveToEx(hdc, cx-s,   cy, NULL); LineTo(hdc, cx-gap, cy);
            MoveToEx(hdc, cx+gap+1, cy, NULL); LineTo(hdc, cx+s+1, cy);

            MoveToEx(hdc, cx, cy-s,   NULL); LineTo(hdc, cx, cy-gap);
            MoveToEx(hdc, cx, cy+gap+1, NULL); LineTo(hdc, cx, cy+s+1);
        }
        if(g_cfg.style==3||g_cfg.style==4||g_cfg.style==5){
            HBRUSH ob=(HBRUSH)SelectObject(hdc,GetStockObject(NULL_BRUSH));
            int r=g_cfg.size;

            Ellipse(hdc, cx-r, cy-r, cx+r+1, cy+r+1);
            SelectObject(hdc,ob);
        }
        DeleteObject(pen);
    };

    auto DrawDot=[&](COLORREF c, int extra){
        int d=g_cfg.dotSize+extra;
        HBRUSH br=CreateSolidBrush(c);
        HPEN pen=CreatePen(PS_SOLID,1,c);
        SelectObject(hdc,br); SelectObject(hdc,pen);
        Ellipse(hdc, cx-d, cy-d, cx+d+1, cy+d+1);
        DeleteObject(br); DeleteObject(pen);
    };
    if(ol>0) DrawCross(g_cfg.outlineColor,t+ol*2);
    DrawCross(g_cfg.color,t);
    if(g_cfg.style==1||g_cfg.style==2||g_cfg.style==4||g_cfg.style==5){ if(ol>0) DrawDot(g_cfg.outlineColor,ol); DrawDot(g_cfg.color,0); }
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT lprc, LPARAM data) {
    auto* list=reinterpret_cast<std::vector<RECT>*>(data);
    list->push_back(*lprc);
    return TRUE;
}

static void RepositionOverlay() {
    if(!g_hOverlay) return;
    int sz=200;
    RECT target={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};

    if(g_cfg.useSecondMonitor) {
        std::vector<RECT> monitors;
        EnumDisplayMonitors(NULL,NULL,MonitorEnumProc,(LPARAM)&monitors);
        if(monitors.size()>=2) target=monitors[1];
    }

    int cx=(target.left+target.right)/2;
    int cy=(target.top+target.bottom)/2;
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
        if(g_cfg.lockToCenter){RECT c={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};ClipCursor(&c);}
        else ClipCursor(NULL);
        return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void CreateOverlay(){
    WNDCLASSEXW wc={}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=OverlayProc;
    wc.hInstance=g_hInst; wc.lpszClassName=OVERLAY_CLASS; RegisterClassExW(&wc);
    int sz=200,sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);
    g_hOverlay=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,
        OVERLAY_CLASS,L"",WS_POPUP,(sw-sz)/2,(sh-sz)/2,sz,sz,NULL,NULL,g_hInst,NULL);
    SetLayeredWindowAttributes(g_hOverlay,RGB(255,0,255),0,LWA_COLORKEY);
    ShowWindow(g_hOverlay,SW_SHOWNOACTIVATE);
    SetTimer(g_hOverlay,TIMER_RECENTER,250,NULL);
}

static LRESULT CALLBACK SliderProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(msg==WM_PAINT){
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        int mn=(int)SendMessageW(hwnd,TBM_GETRANGEMIN,0,0);
        int mx=(int)SendMessageW(hwnd,TBM_GETRANGEMAX,0,0);
        int val=(int)SendMessageW(hwnd,TBM_GETPOS,0,0);
        int tw=rc.right, th=rc.bottom;
        int trackY=th/2, trackH=4, thumbR=8;

        int usable=tw-thumbR*2;
        int thumbX=thumbR+(mx>mn ? (int)((double)(val-mn)/(mx-mn)*usable) : 0);

        HBRUSH bg=CreateSolidBrush(CLR_BG); FillRect(hdc,&rc,bg); DeleteObject(bg);

        RECT trk={thumbR,trackY-trackH/2,tw-thumbR,trackY+trackH/2+1};
        FillRoundRect(hdc,trk,3,CLR_SLIDER_TRK);

        if(thumbX>thumbR){ RECT fill={thumbR,trackY-trackH/2,thumbX,trackY+trackH/2+1}; FillRoundRect(hdc,fill,3,CLR_ACCENT); }

        RECT thumb={thumbX-thumbR,trackY-thumbR,thumbX+thumbR,trackY+thumbR};
        FillRoundRect(hdc,thumb,thumbR,CLR_SURFACE2);
        DrawRoundBorder(hdc,thumb,thumbR,CLR_ACCENT,2);
        EndPaint(hwnd,&ps); return 0;
    }
    if(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||msg==WM_MOUSEMOVE||msg==WM_KEYDOWN||msg==WM_KEYUP){
        LRESULT r=DefSubclassProc(hwnd,msg,wp,lp);
        InvalidateRect(hwnd,NULL,FALSE); UpdateWindow(hwnd);
        return r;
    }
    if(msg==WM_ERASEBKGND){
        HDC hdc=(HDC)wp; RECT rc; GetClientRect(hwnd,&rc);
        HBRUSH b=CreateSolidBrush(CLR_BG); FillRect(hdc,&rc,b); DeleteObject(b); return 1;
    }
    return DefSubclassProc(hwnd,msg,wp,lp);
}

struct ColorBtnData { COLORREF* pColor; const wchar_t* label; };
static ColorBtnData g_colorBtns[2];

static LRESULT CALLBACK ColorBtnProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR uid,DWORD_PTR data){
    ColorBtnData* d=(ColorBtnData*)data;
    if(msg==WM_PAINT){
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        POINT cur; GetCursorPos(&cur); ScreenToClient(hwnd,&cur);
        bool hot=PtInRect(&rc,cur);
        bool pressed=(GetKeyState(VK_LBUTTON)&0x8000)&&hot;
        COLORREF bg=pressed?CLR_BORDER:hot?CLR_SURFACE2:CLR_SURFACE;
        FillRoundRect(hdc,rc,6,bg);
        DrawRoundBorder(hdc,rc,6,hot?CLR_ACCENT:CLR_BORDER);
        int cy=(rc.bottom-rc.top)/2;
        RECT sw={rc.left+10,rc.top+cy-7,rc.left+24,rc.top+cy+7};
        FillRoundRect(hdc,sw,3,*d->pColor);
        DrawRoundBorder(hdc,sw,3,CLR_BORDER);
        RECT lr={rc.left+30,rc.top,rc.right-8,rc.bottom};
        DrawText_(hdc,d->label,lr,CLR_TEXT,g_fntValue,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        EndPaint(hwnd,&ps); return 0;
    }
    if(msg==WM_MOUSEMOVE){

        TRACKMOUSEEVENT tme={sizeof(tme),TME_LEAVE,hwnd,0};
        TrackMouseEvent(&tme);
        InvalidateRect(hwnd,NULL,FALSE); return 0;
    }
    if(msg==WM_MOUSELEAVE){ InvalidateRect(hwnd,NULL,FALSE); return 0; }
    if(msg==WM_LBUTTONDOWN){ SetCapture(hwnd); InvalidateRect(hwnd,NULL,FALSE); return 0; }
    if(msg==WM_LBUTTONUP){
        if(GetCapture()==hwnd){
            ReleaseCapture();
            RECT rc; GetClientRect(hwnd,&rc);
            POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            if(PtInRect(&rc,pt))
                SendMessageW(GetParent(hwnd),WM_COMMAND,
                    MAKEWPARAM(GetDlgCtrlID(hwnd),BN_CLICKED),(LPARAM)hwnd);
            InvalidateRect(hwnd,NULL,FALSE);
        }
        return 0;
    }
    if(msg==WM_ERASEBKGND) return 1;
    return DefSubclassProc(hwnd,msg,wp,lp);
}

static LRESULT CALLBACK ToggleProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR data){
    const wchar_t* label=(const wchar_t*)data;
    if(msg==WM_PAINT){
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        HBRUSH bg=CreateSolidBrush(CLR_BG); FillRect(hdc,&rc,bg); DeleteObject(bg);
        bool checked=SendMessageW(hwnd,BM_GETCHECK,0,0)==BST_CHECKED;
        int pw=36,ph=18,px=rc.right-pw-4,py=(rc.bottom-rc.top)/2-ph/2;
        RECT pill={px,py,px+pw,py+ph};
        FillRoundRect(hdc,pill,ph/2,checked?CLR_ACCENT:CLR_SLIDER_TRK);
        int kx=checked?px+pw-ph+3:px+3, ky=py+3, kr=ph-6;
        RECT knob={kx,ky,kx+kr,ky+kr};
        FillRoundRect(hdc,knob,kr/2,CLR_TEXT);
        RECT lr={4,0,px-8,rc.bottom};
        DrawText_(hdc,label,lr,checked?CLR_TEXT:CLR_TEXT_DIM,g_fntValue,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        EndPaint(hwnd,&ps); return 0;
    }
    if(msg==WM_LBUTTONDOWN){ SetCapture(hwnd); return 0; }
    if(msg==WM_LBUTTONUP){
        if(GetCapture()==hwnd){
            ReleaseCapture();
            RECT rc; GetClientRect(hwnd,&rc);
            POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            if(PtInRect(&rc,pt)){
                bool c=SendMessageW(hwnd,BM_GETCHECK,0,0)==BST_CHECKED;
                SendMessageW(hwnd,BM_SETCHECK,c?BST_UNCHECKED:BST_CHECKED,0);
                InvalidateRect(hwnd,NULL,FALSE);
                SendMessageW(GetParent(hwnd),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(hwnd),BN_CLICKED),(LPARAM)hwnd);
            }
        }
        return 0;
    }
    if(msg==WM_ERASEBKGND){ HDC hdc=(HDC)wp; RECT rc; GetClientRect(hwnd,&rc); HBRUSH b=CreateSolidBrush(CLR_BG); FillRect(hdc,&rc,b); DeleteObject(b); return 1; }
    return DefSubclassProc(hwnd,msg,wp,lp);
}

static LRESULT CALLBACK CenterBtnProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(msg==WM_PAINT){
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        POINT cur; GetCursorPos(&cur); ScreenToClient(hwnd,&cur);
        bool hot=PtInRect(&rc,cur);
        bool pressed=(GetKeyState(VK_LBUTTON)&0x8000)&&hot;
        COLORREF bg=pressed?CLR_ACCENT2:hot?RGB(0,180,105):CLR_SURFACE2;
        COLORREF fg=pressed||hot?CLR_BG:CLR_ACCENT;
        FillRoundRect(hdc,rc,6,bg);
        DrawRoundBorder(hdc,rc,6,hot?CLR_ACCENT:CLR_BORDER);

        int cx=rc.left+(rc.right-rc.left)/2;
        int cy=rc.top+(rc.bottom-rc.top)/2;

        HPEN pen=CreatePen(PS_SOLID,1,fg);
        SelectObject(hdc,pen);
        MoveToEx(hdc,cx-36,cy,NULL); LineTo(hdc,cx-26,cy);
        MoveToEx(hdc,cx-31,cy-5,NULL); LineTo(hdc,cx-31,cy+5);
        DeleteObject(pen);

        RECT lr={rc.left,rc.top,rc.right,rc.bottom};
        DrawText_(hdc,L"Center / Calibrate Crosshair",lr,fg,g_fntValue,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        EndPaint(hwnd,&ps); return 0;
    }
    if(msg==WM_MOUSEMOVE){
        TRACKMOUSEEVENT tme={sizeof(tme),TME_LEAVE,hwnd,0};
        TrackMouseEvent(&tme);
        InvalidateRect(hwnd,NULL,FALSE); return 0;
    }
    if(msg==WM_MOUSELEAVE){ InvalidateRect(hwnd,NULL,FALSE); return 0; }
    if(msg==WM_LBUTTONDOWN){ SetCapture(hwnd); InvalidateRect(hwnd,NULL,FALSE); return 0; }
    if(msg==WM_LBUTTONUP){
        if(GetCapture()==hwnd){
            ReleaseCapture();
            RECT rc; GetClientRect(hwnd,&rc);
            POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            if(PtInRect(&rc,pt))
                SendMessageW(GetParent(hwnd),WM_COMMAND,MAKEWPARAM(IDC_CENTER_BTN,BN_CLICKED),(LPARAM)hwnd);
            InvalidateRect(hwnd,NULL,FALSE);
        }
        return 0;
    }
    if(msg==WM_ERASEBKGND) return 1;
    return DefSubclassProc(hwnd,msg,wp,lp);
}
static LRESULT CALLBACK PreviewProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(msg==WM_PAINT){
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        FillRoundRect(hdc,rc,8,RGB(8,8,10));
        DrawRoundBorder(hdc,rc,8,CLR_BORDER);

        SetPixel(hdc,rc.right/2,rc.bottom/4,CLR_BORDER);
        SetPixel(hdc,rc.right/2,3*rc.bottom/4,CLR_BORDER);
        SetPixel(hdc,rc.right/4,rc.bottom/2,CLR_BORDER);
        SetPixel(hdc,3*rc.right/4,rc.bottom/2,CLR_BORDER);
        SetBkMode(hdc,TRANSPARENT);
        PaintCrosshair(hdc,(rc.right-rc.left)/2,(rc.bottom-rc.top)/2);
        EndPaint(hwnd,&ps); return 0;
    }
    if(msg==WM_ERASEBKGND) return 1;
    return DefSubclassProc(hwnd,msg,wp,lp);
}

static void SectionLabel(HDC hdc, const wchar_t* t, int x, int y, int w) {

    RECT line={x,y+6,x+8,y+7};
    FillRoundRect(hdc,line,1,CLR_ACCENT);
    RECT lr={x+12,y,x+w,y+16};
    DrawText_(hdc,t,lr,CLR_ACCENT,g_fntTitle,DT_LEFT|DT_TOP|DT_SINGLELINE);
}

struct SliderRow { HWND hSlider; HWND hValLabel; };
static SliderRow g_rows[5];

static HWND MakeSlider(HWND p, int id, int x, int y, int w, int mn, int mx, int val){

    HWND h=CreateWindowW(TRACKBAR_CLASSW,L"",
        WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,
        x,y,w,32,p,(HMENU)(intptr_t)id,g_hInst,NULL);
    SendMessageW(h,TBM_SETRANGE,TRUE,MAKELPARAM(mn,mx));
    SendMessageW(h,TBM_SETPOS,TRUE,val);
    SendMessageW(h,TBM_SETPAGESIZE,0,1);
    SetWindowSubclass(h,SliderProc,id,0);
    return h;
}

static void ShowAppMenu(){

    POINT pt; GetCursorPos(&pt);
    HMENU m=CreatePopupMenu();
    AppendMenuW(m,g_cfg.visible?MF_STRING|MF_CHECKED:MF_STRING,ID_TRAY_SHOW,L"Show Crosshair");
    AppendMenuW(m,MF_SEPARATOR,0,NULL);
    AppendMenuW(m,MF_STRING,ID_TRAY_EXIT,L"Exit CrosshairG");
    SetForegroundWindow(g_hMain);
    TrackPopupMenu(m,TPM_BOTTOMALIGN|TPM_LEFTALIGN,pt.x,pt.y,0,g_hMain,NULL);
    DestroyMenu(m);
}
static void ToggleMenu(){
    if(IsIconic(g_hMain)){ ShowWindow(g_hMain,SW_RESTORE); SetForegroundWindow(g_hMain); }
    else if(IsWindowVisible(g_hMain)){ ShowWindow(g_hMain,SW_MINIMIZE); }
    else { ShowWindow(g_hMain,SW_RESTORE); SetForegroundWindow(g_hMain); }
}
static void ToggleCrosshair(){
    g_cfg.visible=!g_cfg.visible;
    ShowWindow(g_hOverlay,g_cfg.visible?SW_SHOWNOACTIVATE:SW_HIDE);
    InvalidateRect(g_hOverlay,NULL,TRUE);
    HWND hChk=GetDlgItem(g_hMain,IDC_VISIBLE_CHK);
    SendMessageW(hChk,BM_SETCHECK,g_cfg.visible?BST_CHECKED:BST_UNCHECKED,0);
    InvalidateRect(hChk,NULL,TRUE);
    SaveConfig();
}

static void UpdateValLabel(int rowIdx, int val){
    wchar_t buf[16]; swprintf_s(buf,L"%d",val);
    SetWindowTextW(g_rows[rowIdx].hValLabel,buf);
    InvalidateRect(g_rows[rowIdx].hValLabel,NULL,TRUE);
}
static void RefreshAllVals(){
    UpdateValLabel(0,g_cfg.size);
    UpdateValLabel(1,g_cfg.thickness);
    UpdateValLabel(2,g_cfg.gap);
    UpdateValLabel(3,g_cfg.outlineSize);
    UpdateValLabel(4,g_cfg.dotSize);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){

    case WM_CREATE: {
        InitCommonControls();
        CreateFonts();

        int PAD=16, y=70;
        int W=280;

        HWND hCombo=CreateWindowW(L"COMBOBOX",L"",
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,
            PAD,y,W,220,hwnd,(HMENU)IDC_STYLE_COMBO,g_hInst,NULL);
        SendMessageW(hCombo,CB_ADDSTRING,0,(LPARAM)L"  Cross");
        SendMessageW(hCombo,CB_ADDSTRING,0,(LPARAM)L"  Dot");
        SendMessageW(hCombo,CB_ADDSTRING,0,(LPARAM)L"  Cross + Dot");
        SendMessageW(hCombo,CB_ADDSTRING,0,(LPARAM)L"  Circle");
        SendMessageW(hCombo,CB_ADDSTRING,0,(LPARAM)L"  Circle + Dot");
        SendMessageW(hCombo,CB_ADDSTRING,0,(LPARAM)L"  Circle + Cross + Dot");
        SendMessageW(hCombo,CB_SETCURSEL,g_cfg.style,0);
        y+=34;

        y+=8;

        struct SliderDef { int id; int lblId; const wchar_t* label; int mn,mx,val; int rowIdx; };
        SliderDef defs[]={
            {IDC_SIZE_SLIDER,   IDC_LBL_SIZE,   L"SIZE",   0,50,g_cfg.size,       0},
            {IDC_THICK_SLIDER,  IDC_LBL_WIDTH,  L"WIDTH",  0,50,g_cfg.thickness,  1},
            {IDC_GAP_SLIDER,    IDC_LBL_GAP,    L"GAP",    0,50,g_cfg.gap,        2},
            {IDC_OUTLINE_SLIDER,IDC_LBL_OUTLINE,L"OUTLINE",0,50,g_cfg.outlineSize,3},
            {IDC_DOT_SLIDER,    IDC_LBL_DOT,    L"DOT",    0,50,g_cfg.dotSize,    4},
        };
        int ROW=32, LBL_W=58, VAL_W=36;
        for(auto& d : defs){
            int midY=y+ROW/2;

            HWND hLbl=CreateWindowW(L"STATIC",d.label,WS_CHILD|WS_VISIBLE,
                PAD, midY-8, LBL_W, 16, hwnd, (HMENU)(intptr_t)d.lblId, g_hInst, NULL);
            SendMessageW(hLbl,WM_SETFONT,(WPARAM)g_fntLabel,TRUE);

            wchar_t vbuf[8]; swprintf_s(vbuf,L"%d",d.val);
            HWND hVal=CreateWindowW(L"STATIC",vbuf,WS_CHILD|WS_VISIBLE|SS_RIGHT,
                PAD+W-VAL_W, midY-8, VAL_W, 16, hwnd, NULL, g_hInst, NULL);
            SendMessageW(hVal,WM_SETFONT,(WPARAM)g_fntValue,TRUE);
            g_rows[d.rowIdx].hValLabel=hVal;

            HWND hS=MakeSlider(hwnd,d.id, PAD+LBL_W+6, y, W-LBL_W-VAL_W-12, d.mn,d.mx,d.val);
            g_rows[d.rowIdx].hSlider=hS;
            y+=ROW;
        }
        y+=8;

        g_colorBtns[0]={&g_cfg.color,       L"Crosshair Color"};
        g_colorBtns[1]={&g_cfg.outlineColor, L"Outline Color"};
        HWND hC1=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,PAD,y,134,32,hwnd,(HMENU)IDC_COLOR_BTN,g_hInst,NULL);
        HWND hC2=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,PAD+W-134,y,134,32,hwnd,(HMENU)IDC_OUTLINE_COLOR_BTN,g_hInst,NULL);
        SetWindowSubclass(hC1,ColorBtnProc,0,(DWORD_PTR)&g_colorBtns[0]);
        SetWindowSubclass(hC2,ColorBtnProc,1,(DWORD_PTR)&g_colorBtns[1]);
        y+=40;

        static wchar_t lbl1[]=L"Show Crosshair";
        static wchar_t lbl2[]=L"Lock mouse to primary monitor";
        static wchar_t lbl3[]=L"Use second monitor";
        HWND hT1=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,PAD,y,W,24,hwnd,(HMENU)IDC_VISIBLE_CHK,g_hInst,NULL);
        SendMessageW(hT1,BM_SETCHECK,g_cfg.visible?BST_CHECKED:BST_UNCHECKED,0);
        SetWindowSubclass(hT1,ToggleProc,0,(DWORD_PTR)lbl1);
        y+=30;
        HWND hT2=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,PAD,y,W,24,hwnd,(HMENU)IDC_LOCK_CHK,g_hInst,NULL);
        SendMessageW(hT2,BM_SETCHECK,g_cfg.lockToCenter?BST_CHECKED:BST_UNCHECKED,0);
        SetWindowSubclass(hT2,ToggleProc,1,(DWORD_PTR)lbl2);
        y+=30;
        HWND hT3=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,PAD,y,W,24,hwnd,(HMENU)IDC_MONITOR_CHK,g_hInst,NULL);
        SendMessageW(hT3,BM_SETCHECK,g_cfg.useSecondMonitor?BST_CHECKED:BST_UNCHECKED,0);
        SetWindowSubclass(hT3,ToggleProc,2,(DWORD_PTR)lbl3);
        y+=34;

        HWND hCtr=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,PAD,y,W,34,hwnd,(HMENU)IDC_CENTER_BTN,g_hInst,NULL);
        SetWindowSubclass(hCtr,CenterBtnProc,0,0);
        y+=42;

        CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,
            PAD,y,W,80,hwnd,(HMENU)IDC_PREVIEW,g_hInst,NULL);
        HWND hPrev=GetDlgItem(hwnd,IDC_PREVIEW);
        SetWindowSubclass(hPrev,PreviewProc,0,0);

        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        int CW=rc.right, CH=rc.bottom;
        bool wide=(CW>=560);
        int PAD=16, HDR=52, GAP=8;

        HBRUSH bgBr=CreateSolidBrush(CLR_BG); FillRect(hdc,&rc,bgBr); DeleteObject(bgBr);

        RECT hdr={0,0,CW,HDR};
        FillRoundRect(hdc,hdr,0,CLR_SURFACE);
        RECT acc={0,0,3,HDR}; FillRoundRect(hdc,acc,0,CLR_ACCENT);
        RECT hkR={14,10,CW,28}; DrawText_(hdc,L"Menu: Ctrl+F5     Toggle: Ctrl+F6",hkR,CLR_TEXT,g_fntValue,DT_LEFT|DT_TOP|DT_SINGLELINE);
        HPEN div=CreatePen(PS_SOLID,1,CLR_BORDER); SelectObject(hdc,div);
        MoveToEx(hdc,PAD,HDR,NULL); LineTo(hdc,CW-PAD,HDR); DeleteObject(div);

        if(!wide){
            int W=CW-PAD*2;

            RECT rCombo,rS0,rC1,rV1,rPrev;
            GetWindowRect(GetDlgItem(hwnd,IDC_STYLE_COMBO), &rCombo); ScreenToClient(hwnd,(POINT*)&rCombo);
            GetWindowRect(GetDlgItem(hwnd,IDC_SIZE_SLIDER),  &rS0);   ScreenToClient(hwnd,(POINT*)&rS0);
            GetWindowRect(GetDlgItem(hwnd,IDC_COLOR_BTN),    &rC1);   ScreenToClient(hwnd,(POINT*)&rC1);
            GetWindowRect(GetDlgItem(hwnd,IDC_VISIBLE_CHK),  &rV1);   ScreenToClient(hwnd,(POINT*)&rV1);
            GetWindowRect(GetDlgItem(hwnd,IDC_PREVIEW),      &rPrev); ScreenToClient(hwnd,(POINT*)&rPrev);
            SectionLabel(hdc,L"SHAPE",      PAD, rCombo.top-20, W);
            SectionLabel(hdc,L"DIMENSIONS", PAD, rS0.top-20,    W);
            SectionLabel(hdc,L"COLOR",      PAD, rC1.top-20,    W);
            SectionLabel(hdc,L"OPTIONS",    PAD, rV1.top-20,    W);
            SectionLabel(hdc,L"PREVIEW",    PAD, rPrev.top-20,  W);
        } else {
            int half=(CW-PAD*3)/2;
            int LX=PAD, RX=PAD*2+half;
            RECT rCombo,rS0,rC1,rV1,rPrev;
            GetWindowRect(GetDlgItem(hwnd,IDC_STYLE_COMBO), &rCombo); ScreenToClient(hwnd,(POINT*)&rCombo);
            GetWindowRect(GetDlgItem(hwnd,IDC_SIZE_SLIDER),  &rS0);   ScreenToClient(hwnd,(POINT*)&rS0);
            GetWindowRect(GetDlgItem(hwnd,IDC_COLOR_BTN),    &rC1);   ScreenToClient(hwnd,(POINT*)&rC1);
            GetWindowRect(GetDlgItem(hwnd,IDC_VISIBLE_CHK),  &rV1);   ScreenToClient(hwnd,(POINT*)&rV1);
            GetWindowRect(GetDlgItem(hwnd,IDC_PREVIEW),      &rPrev); ScreenToClient(hwnd,(POINT*)&rPrev);
            SectionLabel(hdc,L"SHAPE",      LX,  rCombo.top-20, half);
            SectionLabel(hdc,L"DIMENSIONS", LX,  rS0.top-20,    half);
            SectionLabel(hdc,L"COLOR",      LX,  rC1.top-20,    half);
            SectionLabel(hdc,L"OPTIONS",    LX,  rV1.top-20,    half);
            SectionLabel(hdc,L"PREVIEW",    RX,  rPrev.top-20,  half);
            HPEN vdiv=CreatePen(PS_SOLID,1,CLR_BORDER); SelectObject(hdc,vdiv);
            int midX=PAD+(CW-PAD*2)/2;
            MoveToEx(hdc,midX,HDR+GAP*2,NULL); LineTo(hdc,midX,CH-PAD); DeleteObject(vdiv);
        }

        EndPaint(hwnd,&ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdcCtrl=(HDC)wp;
        SetTextColor(hdcCtrl,CLR_TEXT_DIM);
        SetBkColor(hdcCtrl,CLR_BG);
        static HBRUSH staticBg=NULL;
        if(!staticBg) staticBg=CreateSolidBrush(CLR_BG);
        return (LRESULT)staticBg;
    }

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        HDC hdcCtrl=(HDC)wp;
        SetBkColor(hdcCtrl,CLR_SURFACE);
        static HBRUSH ctrlBg=NULL;
        if(!ctrlBg) ctrlBg=CreateSolidBrush(CLR_SURFACE);
        return (LRESULT)ctrlBg;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm=(MINMAXINFO*)lp;
        mm->ptMinTrackSize.x=280;
        mm->ptMinTrackSize.y=480;
        return 0;
    }

    case WM_SIZE: {
        if(wp==SIZE_MINIMIZED) return 0;
        RECT rc; GetClientRect(hwnd,&rc);
        int CW=rc.right, CH=rc.bottom;
        int PAD=16, HDR=52, GAP=8;
        bool wide=(CW>=560);

        if(!wide){

            int W=CW-PAD*2;
            int y=HDR+GAP;
            int ROW=32, LBL_W=58, VAL_W=36;

            y+=20;
            SetWindowPos(GetDlgItem(hwnd,IDC_STYLE_COMBO),NULL,PAD,y,W,24,SWP_NOZORDER);
            y+=32+GAP;

            y+=20;
            int ids[]   ={IDC_SIZE_SLIDER,IDC_THICK_SLIDER,IDC_GAP_SLIDER,IDC_OUTLINE_SLIDER,IDC_DOT_SLIDER};
            int lblIds[]={IDC_LBL_SIZE,IDC_LBL_WIDTH,IDC_LBL_GAP,IDC_LBL_OUTLINE,IDC_LBL_DOT};
            for(int i=0;i<5;i++){
                int midY=y+ROW/2;
                SetWindowPos(GetDlgItem(hwnd,lblIds[i]),  NULL,PAD,          midY-8,LBL_W,16,  SWP_NOZORDER);
                SetWindowPos(g_rows[i].hValLabel,         NULL,PAD+W-VAL_W,  midY-8,VAL_W,16,  SWP_NOZORDER);
                SetWindowPos(GetDlgItem(hwnd,ids[i]),     NULL,PAD+LBL_W+6,  y,     W-LBL_W-VAL_W-12,ROW,SWP_NOZORDER);
                InvalidateRect(GetDlgItem(hwnd,ids[i]),NULL,TRUE);
                y+=ROW;
            }
            y+=GAP;

            y+=20;
            int btnW=(W-GAP)/2;
            SetWindowPos(GetDlgItem(hwnd,IDC_COLOR_BTN),        NULL,PAD,       y,btnW,32,SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd,IDC_OUTLINE_COLOR_BTN),NULL,PAD+btnW+GAP,y,btnW,32,SWP_NOZORDER);
            y+=40+GAP;

            y+=20;
            SetWindowPos(GetDlgItem(hwnd,IDC_VISIBLE_CHK), NULL,PAD,y,W,26,SWP_NOZORDER); y+=30;
            SetWindowPos(GetDlgItem(hwnd,IDC_LOCK_CHK),    NULL,PAD,y,W,26,SWP_NOZORDER); y+=30;
            SetWindowPos(GetDlgItem(hwnd,IDC_MONITOR_CHK), NULL,PAD,y,W,26,SWP_NOZORDER); y+=34;
            SetWindowPos(GetDlgItem(hwnd,IDC_CENTER_BTN),  NULL,PAD,y,W,34,SWP_NOZORDER); y+=42;

            y+=20;
            int prevH=CH-y-PAD;
            if(prevH<80) prevH=80;
            SetWindowPos(GetDlgItem(hwnd,IDC_PREVIEW),NULL,PAD,y,W,prevH,SWP_NOZORDER);

        } else {

            int half=(CW-PAD*3)/2;
            int LX=PAD, RX=PAD*2+half;
            int y=HDR+GAP;

            int ly=y+20;
            SetWindowPos(GetDlgItem(hwnd,IDC_STYLE_COMBO),NULL,LX,ly,half,24,SWP_NOZORDER);
            ly+=32+GAP+20;

            int ids[]   ={IDC_SIZE_SLIDER,IDC_THICK_SLIDER,IDC_GAP_SLIDER,IDC_OUTLINE_SLIDER,IDC_DOT_SLIDER};
            int lblIds[]={IDC_LBL_SIZE,IDC_LBL_WIDTH,IDC_LBL_GAP,IDC_LBL_OUTLINE,IDC_LBL_DOT};
            int ROW=32, LBL_W=58, VAL_W=36;
            for(int i=0;i<5;i++){
                int midY=ly+ROW/2;
                SetWindowPos(GetDlgItem(hwnd,lblIds[i]),  NULL,LX,             midY-8,LBL_W,16,  SWP_NOZORDER);
                SetWindowPos(g_rows[i].hValLabel,         NULL,LX+half-VAL_W,  midY-8,VAL_W,16,  SWP_NOZORDER);
                SetWindowPos(GetDlgItem(hwnd,ids[i]),     NULL,LX+LBL_W+6,     ly,    half-LBL_W-VAL_W-12,ROW,SWP_NOZORDER);
                InvalidateRect(GetDlgItem(hwnd,ids[i]),NULL,TRUE);
                ly+=ROW;
            }
            ly+=GAP+20;

            int btnW=(half-GAP)/2;
            SetWindowPos(GetDlgItem(hwnd,IDC_COLOR_BTN),        NULL,LX,          ly,btnW,32,SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd,IDC_OUTLINE_COLOR_BTN),NULL,LX+btnW+GAP, ly,btnW,32,SWP_NOZORDER);
            ly+=40+GAP+20;

            SetWindowPos(GetDlgItem(hwnd,IDC_VISIBLE_CHK), NULL,LX,ly,half,26,SWP_NOZORDER); ly+=30;
            SetWindowPos(GetDlgItem(hwnd,IDC_LOCK_CHK),    NULL,LX,ly,half,26,SWP_NOZORDER); ly+=30;
            SetWindowPos(GetDlgItem(hwnd,IDC_MONITOR_CHK), NULL,LX,ly,half,26,SWP_NOZORDER); ly+=34;
            SetWindowPos(GetDlgItem(hwnd,IDC_CENTER_BTN),  NULL,LX,ly,half,34,SWP_NOZORDER);

            int ry=y+20;
            int prevH=CH-ry-PAD;
            if(prevH<80) prevH=80;
            SetWindowPos(GetDlgItem(hwnd,IDC_PREVIEW),NULL,RX,ry,half,prevH,SWP_NOZORDER);
        }

        InvalidateRect(hwnd,NULL,TRUE);
        return 0;
    }

    case WM_HSCROLL: {
        HWND hCtrl=(HWND)lp; int val=(int)SendMessageW(hCtrl,TBM_GETPOS,0,0);
        switch(GetDlgCtrlID(hCtrl)){
            case IDC_SIZE_SLIDER:    g_cfg.size=val;        UpdateValLabel(0,val); break;
            case IDC_THICK_SLIDER:   g_cfg.thickness=val;   UpdateValLabel(1,val); break;
            case IDC_GAP_SLIDER:     g_cfg.gap=val;         UpdateValLabel(2,val); break;
            case IDC_OUTLINE_SLIDER: g_cfg.outlineSize=val; UpdateValLabel(3,val); break;
            case IDC_DOT_SLIDER:     g_cfg.dotSize=val;     UpdateValLabel(4,val); break;
        }
        InvalidateRect(hCtrl,NULL,FALSE);
        UpdateWindow(hCtrl);
        InvalidateRect(GetDlgItem(hwnd,IDC_PREVIEW),NULL,FALSE);
        InvalidateRect(g_hOverlay,NULL,TRUE);
        SaveConfig(); return 0;
    }

    case WM_COMMAND: {
        int id=LOWORD(wp);
        if(id==IDC_STYLE_COMBO&&HIWORD(wp)==CBN_SELCHANGE){
            g_cfg.style=(int)SendMessageW((HWND)lp,CB_GETCURSEL,0,0);
            InvalidateRect(GetDlgItem(hwnd,IDC_PREVIEW),NULL,FALSE);
            InvalidateRect(g_hOverlay,NULL,TRUE); SaveConfig();
        }
        if(id==IDC_VISIBLE_CHK){
            g_cfg.visible=SendMessageW((HWND)lp,BM_GETCHECK,0,0)==BST_CHECKED;
            ShowWindow(g_hOverlay,g_cfg.visible?SW_SHOWNOACTIVATE:SW_HIDE);
            InvalidateRect(g_hOverlay,NULL,TRUE); SaveConfig();
        }
        if(id==IDC_LOCK_CHK){
            g_cfg.lockToCenter=SendMessageW((HWND)lp,BM_GETCHECK,0,0)==BST_CHECKED;
            if(!g_cfg.lockToCenter) ClipCursor(NULL); SaveConfig();
        }
        if(id==IDC_MONITOR_CHK){
            g_cfg.useSecondMonitor=SendMessageW((HWND)lp,BM_GETCHECK,0,0)==BST_CHECKED;
            RepositionOverlay(); SaveConfig();
        }
        if(id==IDC_CENTER_BTN){

            RepositionOverlay();

            SetWindowTextW(hwnd,L"CrosshairG v1.0  ✓ Centered!");
            SetTimer(hwnd,99,1200,NULL);
        }
        if(id==IDC_COLOR_BTN){
            CHOOSECOLORW cc={}; static COLORREF cust[16]={};
            cc.lStructSize=sizeof(cc); cc.hwndOwner=hwnd; cc.rgbResult=g_cfg.color;
            cc.lpCustColors=cust; cc.Flags=CC_FULLOPEN|CC_RGBINIT;
            if(ChooseColorW(&cc)){ g_cfg.color=cc.rgbResult; InvalidateRect(GetDlgItem(hwnd,IDC_COLOR_BTN),NULL,FALSE); InvalidateRect(GetDlgItem(hwnd,IDC_PREVIEW),NULL,FALSE); InvalidateRect(g_hOverlay,NULL,TRUE); SaveConfig(); }
        }
        if(id==IDC_OUTLINE_COLOR_BTN){
            CHOOSECOLORW cc={}; static COLORREF cust[16]={};
            cc.lStructSize=sizeof(cc); cc.hwndOwner=hwnd; cc.rgbResult=g_cfg.outlineColor;
            cc.lpCustColors=cust; cc.Flags=CC_FULLOPEN|CC_RGBINIT;
            if(ChooseColorW(&cc)){ g_cfg.outlineColor=cc.rgbResult; InvalidateRect(GetDlgItem(hwnd,IDC_OUTLINE_COLOR_BTN),NULL,FALSE); InvalidateRect(GetDlgItem(hwnd,IDC_PREVIEW),NULL,FALSE); InvalidateRect(g_hOverlay,NULL,TRUE); SaveConfig(); }
        }
        if(id==ID_TRAY_SETTINGS){ ShowWindow(hwnd,SW_RESTORE); SetForegroundWindow(hwnd); }
        if(id==ID_TRAY_SHOW) ToggleCrosshair();
        if(id==ID_TRAY_EXIT){ ClipCursor(NULL); PostQuitMessage(0); DestroyWindow(hwnd); }
        return 0;
    }

    case WM_TIMER:
        if(wp==99){ SetWindowTextW(hwnd,L"CrosshairG v1.0"); KillTimer(hwnd,99); }
        return 0;

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* m=(MEASUREITEMSTRUCT*)lp;
        if(m->CtlID==IDC_STYLE_COMBO) m->itemHeight=26;
        return TRUE;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* d=(DRAWITEMSTRUCT*)lp;
        if(d->CtlID!=IDC_STYLE_COMBO) break;
        HDC hdc=d->hDC;
        RECT r=d->rcItem;

        bool isEdit  =(d->itemState&ODS_COMBOBOXEDIT);
        bool hovered =(d->itemState&ODS_SELECTED)&&!isEdit;
        COLORREF bg = hovered ? CLR_ACCENT2 : CLR_SURFACE;
        COLORREF fg = hovered ? CLR_TEXT    : CLR_TEXT_DIM;
        if(isEdit){ bg=CLR_SURFACE; fg=CLR_TEXT; }

        HBRUSH br=CreateSolidBrush(bg); FillRect(hdc,&r,br); DeleteObject(br);

        if(d->itemID!=(UINT)-1){
            wchar_t buf[64]={};
            SendMessageW(d->hwndItem,CB_GETLBTEXT,d->itemID,LPARAM(buf));

            const wchar_t* t=buf; while(*t==L' ') t++;
            SetTextColor(hdc, fg);
            SetBkMode(hdc,TRANSPARENT);
            RECT tr={r.left+10,r.top,r.right-4,r.bottom};
            HFONT of=(HFONT)SelectObject(hdc,g_fntValue);
            DrawTextW(hdc,t,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
            SelectObject(hdc,of);
        }

        if(d->itemState&ODS_FOCUS) DrawFocusRect(hdc,&r);
        return TRUE;
    }

    case WM_INITMENUPOPUP: {

        HMENU hSys=GetSystemMenu(hwnd,FALSE);
        if((HMENU)wp==hSys){
            DeleteMenu(hSys,ID_TRAY_EXIT,MF_BYCOMMAND);
            AppendMenuW(hSys,MF_SEPARATOR,0,NULL);
            AppendMenuW(hSys,MF_STRING,ID_TRAY_EXIT,L"Exit CrosshairG");
        }
        return 0;
    }
    case WM_SYSCOMMAND:
        if(LOWORD(wp)==ID_TRAY_EXIT){ ClipCursor(NULL); PostQuitMessage(0); DestroyWindow(hwnd); return 0; }
        if(LOWORD(wp)==SC_CLOSE){ ClipCursor(NULL); PostQuitMessage(0); DestroyWindow(hwnd); return 0; }
        return DefWindowProcW(hwnd,msg,wp,lp);
    case WM_HOTKEY:
        if(wp==1) ToggleMenu();
        if(wp==2) ToggleCrosshair();
        return 0;
    case WM_CLOSE:
        ClipCursor(NULL); PostQuitMessage(0); DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        ClipCursor(NULL); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE,LPWSTR,int){
    g_hInst=hInst;
    GetModuleFileNameW(NULL,g_iniPath,MAX_PATH);
    wchar_t* sl=wcsrchr(g_iniPath,L'\\');
    if(sl) wcscpy_s(sl+1,MAX_PATH-(sl-g_iniPath)-1,L"crosshair.ini");
    LoadConfig();

    WNDCLASSEXW wc={}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=MainProc;
    wc.hInstance=hInst; wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MAIN_CLASS;

    wc.hIcon   = (HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
    if(!wc.hIcon)   wc.hIcon  =LoadIcon(NULL,IDI_APPLICATION);
    if(!wc.hIconSm) wc.hIconSm=LoadIcon(NULL,IDI_APPLICATION);
    RegisterClassExW(&wc);

    g_hMain=CreateWindowExW(WS_EX_APPWINDOW,MAIN_CLASS,L"CrosshairG v1.0",
        WS_OVERLAPPEDWINDOW,
        100,100,312,560,NULL,NULL,hInst,NULL);

    BOOL dark=TRUE;
    DwmSetWindowAttribute(g_hMain,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));

    CreateOverlay();
    ShowWindow(g_hMain,SW_SHOW);
    SetForegroundWindow(g_hMain);

    RECT initRc; GetClientRect(g_hMain,&initRc);
    SendMessageW(g_hMain,WM_SIZE,SIZE_RESTORED,MAKELPARAM(initRc.right,initRc.bottom));

    if(!RegisterHotKey(g_hMain,1,MOD_CONTROL,VK_F5))
        MessageBoxW(g_hMain,
            L"Ctrl+F5 is already in use by another application.\nThe menu toggle hotkey will not work.",
            L"CrosshairG - Hotkey Conflict", MB_OK|MB_ICONWARNING);
    if(!RegisterHotKey(g_hMain,2,MOD_CONTROL,VK_F6))
        MessageBoxW(g_hMain,
            L"Ctrl+F6 is already in use by another application.\nThe crosshair toggle hotkey will not work.",
            L"CrosshairG - Hotkey Conflict", MB_OK|MB_ICONWARNING);

    MSG m;
    while(GetMessageW(&m,NULL,0,0)){ TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}
