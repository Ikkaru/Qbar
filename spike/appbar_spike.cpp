// Phase 0 spike: manual autohide AppBar.
// Register AppBar, track mouse, slide bar in/out, update work area.
// Build: g++ -o appbar_spike.exe appbar_spike.cpp -luser32 -lshell32
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdarg>

static FILE* g_log = nullptr;
static HWND g_hwnd = nullptr;
static BOOL g_visible = TRUE;
static const int BAR_H = 40;
static const int REVEAL_ZONE = 8;

static void log(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fprintf(g_log, "\n");
    fflush(g_log);
}

static void printWorkArea(const char* tag) {
    RECT rc;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
    log("[%s] workarea: top=%ld bottom=%ld (h=%ld)", tag, rc.top, rc.bottom, rc.bottom - rc.top);
}

static void setBarAppBar(BOOL show) {
    APPBARDATA abd = { sizeof(abd) };
    abd.hWnd = g_hwnd;
    abd.uEdge = ABE_TOP;
    if (show) {
        abd.rc = { 0, 0, GetSystemMetrics(SM_CXSCREEN), BAR_H };
    } else {
        abd.rc = { 0, -BAR_H, GetSystemMetrics(SM_CXSCREEN), 0 };
    }
    SHAppBarMessage(ABM_SETPOS, &abd);
}

static void updateBar(BOOL show) {
    if (show == g_visible) return;
    g_visible = show;
    setBarAppBar(show);
    printWorkArea(show ? "bar-shown" : "bar-hidden");
    if (show) {
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), BAR_H,
                     SWP_SHOWWINDOW | SWP_NOACTIVATE);
    } else {
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, -BAR_H, GetSystemMetrics(SM_CXSCREEN), BAR_H,
                     SWP_HIDEWINDOW | SWP_NOACTIVATE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        printWorkArea("before-register");

        APPBARDATA abd = { sizeof(abd) };
        abd.hWnd = hwnd;
        abd.uCallbackMessage = WM_APP + 1;
        abd.uEdge = ABE_TOP;
        abd.rc = { 0, 0, GetSystemMetrics(SM_CXSCREEN), BAR_H };
        BOOL okNew = (BOOL)SHAppBarMessage(ABM_NEW, &abd);
        log("ABM_NEW: %d", okNew);
        SHAppBarMessage(ABM_SETPOS, &abd);
        printWorkArea("after-register");

        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), BAR_H,
                     SWP_SHOWWINDOW | SWP_NOACTIVATE);

        SetTimer(hwnd, 1, 50, nullptr);
        return 0;
    }
    case WM_TIMER: {
        POINT pt;
        GetCursorPos(&pt);
        BOOL nearEdge = (pt.y <= REVEAL_ZONE);
        updateBar(nearEdge);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY: {
        APPBARDATA abd = { sizeof(abd) };
        abd.hWnd = hwnd;
        SHAppBarMessage(ABM_REMOVE, &abd);
        printWorkArea("after-remove");
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main() {
    g_log = fopen("spike.log", "w");
    if (!g_log) return 1;

    HINSTANCE hInst = GetModuleHandle(nullptr);
    WNDCLASSEX wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "AppBarSpike";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        "AppBarSpike", "AppBar Spike",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), BAR_H,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd) { log("CreateWindowEx failed: %lu", GetLastError()); return 1; }

    log("Spike running. Move mouse to top edge to reveal bar. ESC to exit.");

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    fclose(g_log);
    return 0;
}
