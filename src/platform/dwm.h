#pragma once
#include <QString>
#include <windows.h>

#include <QColor>

void qbarLog(const QString& msg);

namespace Dwm {
    void enableBackdrop(HWND hwnd, const QString& type); // "acrylic" | "mica" | "solid"
    void disableBackdrop(HWND hwnd);
    void enableRoundedCorners(HWND hwnd);
    void disableRoundedCorners(HWND hwnd);
    void enableShadow(HWND hwnd);
    void setBorderColor(HWND hwnd, QColor color);
}
