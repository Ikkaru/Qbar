#include "dwm.h"
#include <QColor>
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_LEGACY
#define DWMWA_USE_IMMERSIVE_DARK_MODE_LEGACY 19
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

// Kept for completeness, but nothing calls it: the only backdrop Windows can
// actually tune is the composition accent, and both system-backdrop types are
// unusable for a bar. DWMSBT_MAINWINDOW (Mica) samples the window's own
// background, so it is flat and its tint is locked; DWMSBT_TRANSIENTWINDOW
// (acrylic) paints a flat tint with no blur. Both measured uniform across the
// whole bar while the wallpaper behind them varied.
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

void Dwm::setDarkMode(HWND hwnd, bool on) {
    if (!hwnd) return;
    // Attribute 20 is the one honoured on Windows 10 2004 and later; 19 is the
    // original that older builds understand. Try the modern one first and fall
    // back rather than leaving the window on the light material.
    BOOL dark = on ? TRUE : FALSE;
    if (DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark)) != S_OK)
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_LEGACY, &dark, sizeof(dark));
}

// The composition-attribute structs. SetWindowCompositionAttribute is
// undocumented and absent from the SDK headers, so it is resolved at runtime. It
// has been stable since Windows 8.1 and is the only way to get a real blur on a
// layered window.
namespace {

constexpr int WcaAccentPolicy = 19;
constexpr int AccentDisable = 0;
constexpr int AccentEnableAcrylicBlurBehind = 4;
constexpr int AccentEnableHostBackdrop = 5;

struct AccentPolicy {
    int accentState;
    int accentFlags;
    int gradientColor;   // 0xAABBGGRR
    int animationId;
};

struct WindowCompositionAttributeData {
    int attribute;
    void* data;
    int dataSize;
};

// The struct is passed by pointer, so the signature only needs void*.
typedef BOOL (WINAPI *SetWindowCompositionAttributeFn)(HWND, void*);

SetWindowCompositionAttributeFn compositionAttributeFn() {
    static const SetWindowCompositionAttributeFn fn = [] {
        const HMODULE user32 = GetModuleHandleW(L"user32.dll");
        return user32 ? reinterpret_cast<SetWindowCompositionAttributeFn>(
                            GetProcAddress(user32, "SetWindowCompositionAttribute"))
                      : nullptr;
    }();
    return fn;
}

bool setCompositionAttribute(HWND hwnd, const WindowCompositionAttributeData& data) {
    const auto fn = compositionAttributeFn();
    return fn && fn(hwnd, const_cast<WindowCompositionAttributeData*>(&data));
}

} // namespace

void Dwm::setAcrylic(HWND hwnd, const QColor& tint, double strength) {
    if (!hwnd) return;

    // Windows 11 ignores gradientColor for both acrylic states - the tint comes
    // from the system's own light/dark material. So the accent here is used only
    // to get a genuine blur, and density is controlled by the QML layer on top
    // (see Bar.qml). HOSTBACKDROP is the lighter of the two and is what makes it
    // read as glass rather than a solid block; ACRYLICBLURBEHIND is noticeably
    // denser and flattens out the wallpaper behind it.
    AccentPolicy policy;
    policy.accentState = AccentEnableHostBackdrop;
    policy.accentFlags = 0;
    policy.gradientColor = 0xFF000000;
    policy.animationId = 0;

    WindowCompositionAttributeData data;
    data.attribute = WcaAccentPolicy;
    data.data = &policy;
    data.dataSize = sizeof(policy);

    setCompositionAttribute(hwnd, data);
}

void Dwm::clearAcrylic(HWND hwnd) {
    if (!hwnd) return;
    AccentPolicy policy;
    policy.accentState = AccentDisable;
    policy.accentFlags = 0;
    policy.gradientColor = 0;
    policy.animationId = 0;

    WindowCompositionAttributeData data;
    data.attribute = WcaAccentPolicy;
    data.data = &policy;
    data.dataSize = sizeof(policy);

    setCompositionAttribute(hwnd, data);
}
