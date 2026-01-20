#include "stdafx.hpp"
#include "snow.hpp"
#include "renderer.hpp"


#define MUTEX_NAME          "MutexSnowWallpaperSingleton"
#define TBCREATED_TEXT      "TaskbarCreated"
#define SNOWWNDCLS_NAME     "SNOW"
#define MAINWNDCLS_NAME     "SNOWMAN"
#define WND_EXSIZE(n)       (sizeof(LONG_PTR) * (n))
#define WND_EXOFFSET(n)     WND_EXSIZE(n)

#define WNDEX_SNOWEXSIZE    WND_EXSIZE(1)
#define WNDEX_SNOWWINDOW    WND_EXOFFSET(0)

#define WNDEX_MAINEXSIZE    WND_EXSIZE(1)
#define WNDEX_MAINTMAP      WND_EXOFFSET(0)

#define WM_SNOWSTART    (WM_USER + 0)
#define WM_SNOWSTOP     (WM_USER + 1)
#define WM_SNOWCLOSE    (WM_USER + 2)

#define WM_MAINNIMSG    (WM_APP + 0)
#define WM_MAINNIINSERT (WM_APP + 1)    /* wparam: TRUE for insertion, FALSE for deletion */
#define WM_MAINSTINIT   (WM_APP + 2)    /* wparam: monitor handle, lparam: snow window handle */

#define SNOW_BMPSIZE    128
#define SNOW_TIMERID    1
#define SNOW_NIID       1
#define SNOW_NITIPSTR   "Animated Snow Wallpaper"


struct SnowWindow {
    HRESULT hr;
    HMONITOR hmon;
    SnowList sl;
    SnowRenderer sr;
};

using MonitorSet = std::unordered_set<HMONITOR>;
using ThreadMap = std::unordered_map<HMONITOR, std::pair<HWND, std::jthread>>;


inline bool isInMonitor(HWND hwnd, HMONITOR hmon) {
    HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    return hm && hm == hmon;
}

HRESULT tryRender(SnowRenderer& sr, SnowList& sl, HRESULT hr, float alpha = 1.0f) {
    hr = (hr == DXGI_STATUS_OCCLUDED) ? sr.presentTest() : sr.render(sl, alpha);
    if (hr != S_OK && hr != DXGI_STATUS_OCCLUDED) { //error occur, refresh device and discard this frame
        hr = SUCCEEDED(sr.refreash()) ? S_FALSE : E_UNEXPECTED;
    }
    return hr;
}

LRESULT CALLBACK SnowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    SnowWindow* psw = (SnowWindow*)GetWindowLongPtr(hwnd, WNDEX_SNOWWINDOW);

    switch (msg) {
    case WM_CREATE:
        BEGINCASECODE;
        LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lparam;
        psw = (SnowWindow*)lpcs->lpCreateParams;

        MONITORINFO mi = { sizeof(MONITORINFO) };
        GetMonitorInfo(psw->hmon, &mi);
        int cx = mi.rcMonitor.right - mi.rcMonitor.left, cy = mi.rcMonitor.bottom - mi.rcMonitor.top;
        SetWindowPos(hwnd, HWND_BOTTOM, mi.rcMonitor.left, mi.rcMonitor.top, cx, cy, SWP_NOACTIVATE | SWP_NOREDRAW | SWP_SHOWWINDOW);

        HICON hico = (HICON)LoadImage(lpcs->hInstance, MAKEINTRESOURCE(IDI_SNOW), IMAGE_ICON, SNOW_BMPSIZE, SNOW_BMPSIZE, LR_DEFAULTCOLOR);
        psw->sl.initialize(cx, cy, mi.rcWork.bottom - mi.rcMonitor.top, GetDpiForWindow(hwnd));
        psw->hr = psw->sr.initialize(hico, cx, cy, hwnd);
        DestroyIcon(hico);

        SetWindowLongPtr(hwnd, WNDEX_SNOWWINDOW, (LONG_PTR)psw);
        SetTimer(hwnd, SNOW_TIMERID, (1000 / SnowList::FPS), nullptr);
        ENDCASECODE;
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, SNOW_TIMERID);
        PostQuitMessage(0);
        return 0;
    case WM_SNOWSTART:  //start the animation
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetTimer(hwnd, SNOW_TIMERID, (1000 / SnowList::FPS), nullptr);
        return 0;
    case WM_SNOWSTOP:   //stop the animation
        ShowWindow(hwnd, SW_HIDE);
        KillTimer(hwnd, SNOW_TIMERID);
        return 0;
    case WM_SNOWCLOSE:  //close window and exit thread
        DestroyWindow(hwnd);
        return 0;
    case WM_DPICHANGED: //refresh animation if DPI changed
        BEGINCASECODE;
        UINT dpi = LOWORD(wparam);
        if (isInMonitor(hwnd, psw->hmon) && dpi != psw->sl.dpi) {
            psw->sl.dpi = dpi;
            psw->sl.refreshList();
        }
        ENDCASECODE;
        return 0;
    case WM_DISPLAYCHANGE:  //refresh animation and device if resolution changed
        if (isInMonitor(hwnd, psw->hmon)) {
            MONITORINFO mi = { sizeof(MONITORINFO) };
            GetMonitorInfo(psw->hmon, &mi);
            UINT xres = mi.rcMonitor.right - mi.rcMonitor.left;
            UINT yres = mi.rcMonitor.bottom - mi.rcMonitor.top;
            UINT ground = mi.rcWork.bottom - mi.rcMonitor.top;
            if (xres != psw->sl.xres || yres != psw->sl.yres || ground != psw->sl.ground) {
                psw->sl.xres = xres;
                psw->sl.yres = yres;
                psw->sl.ground = ground;
                psw->sl.refreshList();
                psw->hr = psw->sr.resize(xres, yres);
                SetWindowPos(hwnd, 0, 0, 0, xres, yres, SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOMOVE);
            }
        }
        return 0;
    case WM_SETTINGCHANGE:  //update ground if working area is changed
        if (wparam == SPI_SETWORKAREA && isInMonitor(hwnd, psw->hmon)) {    //update current ground
            MONITORINFO mi = { sizeof(MONITORINFO) };
            GetMonitorInfo(psw->hmon, &mi);
            UINT ground = mi.rcWork.bottom - mi.rcMonitor.top;
            if (ground != psw->sl.ground) {
                psw->sl.ground = ground;
                psw->sl.applyGround();
            }
        }
        return 0;
    case WM_PAINT:
        BEGINCASECODE;
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        psw->hr = tryRender(psw->sr, psw->sl, psw->hr);
        EndPaint(hwnd, &ps);
        ENDCASECODE;
        return 0;
    case WM_TIMER:
        psw->sl.nextFrame();
        psw->hr = tryRender(psw->sr, psw->sl, psw->hr);
        if (psw->hr == E_UNEXPECTED) PostMessage(hwnd, WM_SNOWSTOP, 0, 0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void wallpaperThread(HMONITOR hmon, HWND hmain) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); //for WIC
    SnowWindow sw{ S_OK, hmon, { std::random_device{}() }, {} };

    HWND hwnd = CreateWindowEx(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        TEXT(SNOWWNDCLS_NAME), nullptr,
        WS_POPUP,
        0, 0, 0, 0,
        nullptr, nullptr, GetModuleHandle(nullptr), &sw
    );
    PostMessage(hmain, WM_MAINSTINIT, (WPARAM)hmon, (LPARAM)hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    CoUninitialize();
}


BOOL CALLBACK MonitorEnumProc(HMONITOR hmon, HDC hdc, LPRECT lprect, LPARAM lparam) {
    ((MonitorSet*)lparam)->emplace(hmon);
    return TRUE;
}

LRESULT onCreate(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lparam;
    ThreadMap* ptm = (ThreadMap*)lpcs->lpCreateParams;

    MonitorSet mset;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&mset);
    for (auto hmon : mset) {
        MSG msg;
        auto& item = *ptm->try_emplace(hmon, nullptr, std::jthread(wallpaperThread, hmon, hwnd)).first;
        GetMessage(&msg, hwnd, WM_MAINSTINIT, WM_MAINSTINIT);
        item.second.first = (HWND)msg.lParam;
    }

    SendMessage(hwnd, WM_MAINNIINSERT, TRUE, 0);    //insert notify icon
    SetWindowLongPtr(hwnd, WNDEX_MAINTMAP, (LONG_PTR)ptm);
    return 0;
}

LRESULT onDestroy(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    ThreadMap* ptm = (ThreadMap*)GetWindowLongPtr(hwnd, WNDEX_MAINTMAP);
    for (auto& item : *ptm) {   //kill threads
        PostMessage(item.second.first, WM_SNOWCLOSE, 0, 0);
    }
    ptm->clear();

    SendMessage(hwnd, WM_MAINNIINSERT, FALSE, 0);   //remove notify icon
    PostQuitMessage(0);
    return 0;
}

LRESULT onCommand(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    ThreadMap* ptm = (ThreadMap*)GetWindowLongPtr(hwnd, WNDEX_MAINTMAP);
    HMENU hnimenu = GetSubMenu(GetMenu(hwnd), 0);
    if (HIWORD(wparam) == 0) {  //menu
        switch (LOWORD(wparam)) {
        case ID_NIM_REFRESH:    //restart threads
            BEGINCASECODE;
            for (auto& item : *ptm) {
                PostMessage(item.second.first, WM_SNOWCLOSE, 0, 0);
            }
            ptm->clear();

            MonitorSet mset;
            EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&mset);
            for (auto hmon : mset) {
                MSG msg;
                auto& item = *ptm->try_emplace(hmon, nullptr, std::jthread(wallpaperThread, hmon, hwnd)).first;
                GetMessage(&msg, hwnd, WM_MAINSTINIT, WM_MAINSTINIT);
                item.second.first = (HWND)msg.lParam;
            }
            ENDCASECODE;
            break;
        case ID_NIM_EXIT:   //exit program
            DestroyWindow(hwnd);
            break;
        }
    }
    return 0;
}

LRESULT onMainNIMsg(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    UINT niid = HIWORD(lparam), msg = LOWORD(lparam);   //NOTIFYICON_VERSION_4
    HMENU hnimenu = GetSubMenu(GetMenu(hwnd), 0);   //context menu for notify icon
    switch (msg) {
    case WM_CONTEXTMENU:
    case NIN_SELECT:
    case NIN_KEYSELECT: //show context menu, see Remarks section for TrackPopupMenu() in MSDN
        SetForegroundWindow(hwnd);
        TrackPopupMenuEx(hnimenu, TPM_NOANIMATION, MAKEPOINTS(wparam).x, MAKEPOINTS(wparam).y, hwnd, nullptr);
        PostMessage(hwnd, WM_NULL, 0, 0);
        break;
    }
    return 0;
}

LRESULT onMainNIInsert(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
    nid.hWnd = hwnd;
    nid.uID = SNOW_NIID;
    if (wparam) {
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        nid.uCallbackMessage = WM_MAINNIMSG;
        nid.hIcon = LoadIcon((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), MAKEINTRESOURCE(IDI_ICON));
        nid.uVersion = NOTIFYICON_VERSION_4;
        _tcscpy_s(nid.szTip, TEXT(SNOW_NITIPSTR));
        Shell_NotifyIcon(NIM_ADD, &nid);
        Shell_NotifyIcon(NIM_SETVERSION, &nid);
    } else {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
    return 0;
}

LRESULT onMainSTInit(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    ThreadMap* ptm = (ThreadMap*)GetWindowLongPtr(hwnd, WNDEX_MAINTMAP);
    if (ptm->contains((HMONITOR)wparam)) {
        ptm->at((HMONITOR)wparam).first = (HWND)lparam;
    }
    return 0;
}

LRESULT onDisplayChange(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    ThreadMap* ptm = (ThreadMap*)GetWindowLongPtr(hwnd, WNDEX_MAINTMAP);
    MonitorSet mset;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&mset);
    for (auto it = ptm->begin(); it != ptm->end();) {   //remove invalid threads
        auto& item = *it;
        if (!mset.contains(item.first)) {
            PostMessage(item.second.first, WM_SNOWCLOSE, 0, 0);
            it = ptm->erase(it);
        } else {
            ++it;
        }
    }
    for (auto& hmon : mset) {   //create new threads
        MSG msg;
        auto& item = *ptm->try_emplace(hmon, nullptr, std::jthread(wallpaperThread, hmon, hwnd)).first;
        GetMessage(&msg, hwnd, WM_MAINSTINIT, WM_MAINSTINIT);
        item.second.first = (HWND)msg.lParam;
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        return onCreate(hwnd, wparam, lparam);
    case WM_DESTROY:
        return onDestroy(hwnd, wparam, lparam);
    case WM_COMMAND:
        return onCommand(hwnd, wparam, lparam);
    case WM_MAINSTINIT: //fallback, shuld not reach in normal case
        return onMainSTInit(hwnd, wparam, lparam);
    case WM_MAINNIMSG:
        return onMainNIMsg(hwnd, wparam, lparam);
    case WM_MAINNIINSERT:
        return onMainNIInsert(hwnd, wparam, lparam);
    case WM_DISPLAYCHANGE:
        return onDisplayChange(hwnd, wparam, lparam);
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}


ATOM WINAPI RegisterClassExWrapper(LPCTSTR lpClsName, WNDPROC lpfnWndProc, int cbWndExtra) {
    WNDCLASSEX wndc = { sizeof(WNDCLASSEX) };
    wndc.lpszClassName = lpClsName;
    wndc.lpfnWndProc = lpfnWndProc;
    wndc.hInstance = GetModuleHandle(nullptr);
    wndc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndc.cbWndExtra = cbWndExtra;
    return RegisterClassEx(&wndc);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    HANDLE hmutex = CreateMutex(nullptr, false, TEXT(MUTEX_NAME));
    DWORD dwerr = GetLastError();
    if (!hmutex || dwerr == ERROR_ALREADY_EXISTS) { //run only one wallpaper process
        return (int)dwerr;
    }

    RegisterClassExWrapper(TEXT(SNOWWNDCLS_NAME), SnowProc, WNDEX_SNOWEXSIZE);
    RegisterClassExWrapper(TEXT(MAINWNDCLS_NAME), WndProc, WNDEX_MAINEXSIZE);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ThreadMap tmap;
    UINT tbmsg = RegisterWindowMessage(TEXT(TBCREATED_TEXT));
    HWND hwnd = CreateWindowEx(
        0,
        TEXT(MAINWNDCLS_NAME),
        nullptr,
        WS_POPUP,
        0, 0, 0, 0,
        nullptr,
        LoadMenu(hInstance, MAKEINTRESOURCE(IDR_NIMENU)),
        hInstance,
        &tmap
    );

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == tbmsg) { //handle taskbar created message
            PostMessage(hwnd, WM_MAINNIINSERT, TRUE, 0);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
