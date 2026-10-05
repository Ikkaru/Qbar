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

    // Real acrylic: ACCENT_ENABLE_ACRYLICBLURBEHIND. The lighter HOSTBACKDROP
    // variant is deliberately NOT used - acrylic is meant to be the denser of the
    // two, and swapping in the lighter material is what made an earlier attempt at
    // this mode read as washed-out glass rather than acrylic.
    //
    // Windows 11 does not honour gradientColor for the tint - density comes from
    // the system's own light/dark material - so the accent is used for the blur
    // and the QML layer above it controls density (see Bar.qml).
    // Alpha 0x66, not 0xFF. A fully opaque gradient makes the accent cancel the
    // blur entirely - measured identical to having no material at all - because
    // the tint is composited on top of the blur at full strength. Partly
    // transparent is what leaves the blur visible underneath.
    AccentPolicy policy;
    policy.accentState = AccentEnableAcrylicBlurBehind;
    policy.accentFlags = 0;
    policy.gradientColor = 0x66000000;
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
