#pragma once
#include <QString>
#include <windows.h>

#include <QColor>

void qbarLog(const QString& msg);

namespace Dwm {
    // Only disableBackdrop remains of the DWMWA_SYSTEMBACKDROP_TYPE pair, and
    // nothing sets a type any more: DWMSBT_TRANSIENTWINDOW paints a flat tint
    // with no blur, and DWMSBT_MAINWINDOW is flat with a locked tint. Both were
    // measured uniform across the bar while the wallpaper behind them varied, so
    // neither is usable and the accent in setAcrylic() replaced them both.
    void disableBackdrop(HWND hwnd);
    void enableRoundedCorners(HWND hwnd);
    void disableRoundedCorners(HWND hwnd);
    void enableShadow(HWND hwnd);
    void setBorderColor(HWND hwnd, QColor color);

    // Opts the window into dark mode. Without this DWM renders the LIGHT variant
    // of every system material, which is why an acrylic bar came out white.
    void setDarkMode(HWND hwnd, bool on);

    // Real acrylic via the composition attribute. Unlike DWMWA_SYSTEMBACKDROP_TYPE
    // this blurs what is actually behind the window instead of painting a flat
    // tint, and it works on a layered window. `strength` 0-1 maps to the tint
    // alpha, so 0 is clear glass and 1 is effectively opaque.
    void setAcrylic(HWND hwnd, const QColor& tint, double strength);
    void clearAcrylic(HWND hwnd);
}
