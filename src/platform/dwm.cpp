#include "dwm.h"
#include <QColor>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWCP_NONE
#define DWMWCP_NONE 1
#endif

void Dwm::enableBackdrop(HWND hwnd, const QString& type) {
    if (!hwnd) return;
    int backdrop = DWMSBT_NONE;
    if (type == "mica") {
        backdrop = DWMSBT_MAINWINDOW;
    } else if (type == "acrylic") {
        backdrop = DWMSBT_TRANSIENTWINDOW;
    }
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}

void Dwm::disableBackdrop(HWND hwnd) {
    if (!hwnd) return;
    int backdrop = DWMSBT_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}

void Dwm::enableRoundedCorners(HWND hwnd) {
    if (!hwnd) return;
    int preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
}

void Dwm::disableRoundedCorners(HWND hwnd) {
    if (!hwnd) return;
    int preference = DWMWCP_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
}

void Dwm::enableShadow(HWND hwnd) {
    if (!hwnd) return;
    MARGINS margins = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void Dwm::setBorderColor(HWND hwnd, QColor color) {
    if (!hwnd) return;
    COLORREF rgb = RGB(color.red(), color.green(), color.blue());
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgb, sizeof(rgb));
}
