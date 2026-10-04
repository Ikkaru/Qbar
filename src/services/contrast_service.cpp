#include "contrast_service.h"

#include <QImage>
#include <QScreen>
#include <QGuiApplication>
#include <QImageReader>
#include <QFile>
#include <QRegularExpression>

#include <windows.h>
#include <cmath>
#include <vector>

namespace {

// The wallpaper path lives here for both picture and slideshow wallpapers.
QString wallpaperPath() {
    wchar_t path[MAX_PATH] = {0};
    if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0)) {
        return QString::fromWCharArray(path);
    }
    return QString();
}

// Solid colour wallpapers leave the picture path empty and store the colour in
// the desktop background key as "r g b" in 0-255.
QColor solidBackgroundColor() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Control Panel\\Desktop", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return QColor();
    }
    char value[64] = {0};
    DWORD size = sizeof(value);
    DWORD type = 0;
    QString text;
    if (RegQueryValueExW(key, L"Background", nullptr, &type,
                         reinterpret_cast<BYTE*>(value), &size) == ERROR_SUCCESS) {
        text = QString::fromLatin1(value, int(size)).trimmed();
    }
    RegCloseKey(key);

    const QStringList parts = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 3) return QColor();
    bool ok = false;
    const int r = parts[0].toInt(&ok); if (!ok) return QColor();
    const int g = parts[1].toInt(&ok); if (!ok) return QColor();
    const int b = parts[2].toInt(&ok); if (!ok) return QColor();
    return QColor(r, g, b);
}

// Slack on the brightness floor while dark text is active, so a background
// hovering at the threshold does not flicker between colours.
constexpr int kThresholdSlack = 8;

// sRGB channel -> linear light. Averaging raw 8-bit values would badly
// over-weight the dark end and make mid-tone wallpapers look darker than they
// read on screen.
double channelToLinear(int v) {
    const double s = v / 255.0;
    return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

} // namespace

ContrastService::ContrastService(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &ContrastService::refresh);
    m_timer->start();
}

void ContrastService::setAutoContrast(bool on) {
    if (m_autoContrast == on) return;
    m_autoContrast = on;
    refresh();
    emit changed();
}

void ContrastService::setBackdrop(const QString& type, const QColor& color, double opacity) {
    m_backdrop = type;
    m_backdropColor = color;
    m_backdropOpacity = opacity;
    refresh();
}

void ContrastService::setTextColors(const QColor& onLight, const QColor& onDark) {
    m_onLight = onLight;
    m_onDark = onDark;
    refresh();
}

void ContrastService::setBarHeight(int px) {
    if (m_barHeight == px) return;
    m_barHeight = px;
    refresh();
}

void ContrastService::setLightThreshold(int level) {
    level = qBound(0, level, 255);
    if (m_lightThreshold == level) return;
    m_lightThreshold = level;
    refresh();
}

double ContrastService::relativeLuminance(const QColor& c) {
    return 0.2126 * channelToLinear(c.red())
         + 0.7152 * channelToLinear(c.green())
         + 0.0722 * channelToLinear(c.blue());
}

bool ContrastService::sampleWallpaperStrip(QColor* out) const {
    // The registry value is not always populated: slideshow wallpapers, some
    // third-party wallpaper tools and freshly changed wallpapers all leave it
    // empty or stale. `TranscodedImageCache` carries the real path as UTF-16, so
    // it is tried as well before giving up and reading the screen.
    QStringList candidates;
    const QString spi = wallpaperPath();
    if (!spi.isEmpty()) candidates << spi;

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD size = 0, type = 0;
        if (RegQueryValueExW(key, L"Wallpaper", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 2) {
            std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, L'\0');
            if (RegQueryValueExW(key, L"Wallpaper", nullptr, &type,
                                 reinterpret_cast<BYTE*>(buf.data()), &size) == ERROR_SUCCESS) {
                const QString regPath = QString::fromWCharArray(buf.data());
                if (!regPath.isEmpty()) candidates << regPath;
            }
        }
        if (RegQueryValueExW(key, L"TranscodedImageCache", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 2) {
            std::vector<BYTE> buf(size, 0);
            if (RegQueryValueExW(key, L"TranscodedImageCache", nullptr, &type, buf.data(), &size) == ERROR_SUCCESS) {
                // Binary header first, then the path as a NUL-terminated UTF-16
                // string. Scan for the first run of printable characters.
                QString decoded;
                for (int i = 0; i + 1 < size; i += 2) {
                    const wchar_t c = reinterpret_cast<wchar_t*>(buf.data())[i / 2];
                    if (c == L'\0') { if (!decoded.isEmpty()) break; else continue; }
                    if (c < 32 || c > 126) { decoded.clear(); continue; }
                    // The path is plain ASCII; anything outside that range is
                    // binary header noise rather than part of it.
                    if (c >= 128) { decoded.clear(); continue; }
                    decoded.append(QChar(u' ', c));
                }
                if (!decoded.isEmpty() && decoded.contains(QLatin1Char(':')))
                    candidates << decoded;
            }
        }
        RegCloseKey(key);
    }

    for (const QString& path : candidates) {
        if (!QFile::exists(path)) continue;
        QImageReader reader(path);
        // Reading at a small size keeps a multi-megapixel JPEG off the critical
        // path; we only need the average colour of a thin strip.
        QSize size = reader.size();
        if (size.isValid() && !size.isEmpty()) {
            const int targetH = 64;
            const double scale = double(targetH) / size.height();
            reader.setScaledSize(QSize(qMax(1, int(size.width() * scale)), targetH));
        }
        QImage img = reader.read();
        if (!img.isNull()) {
            // Only the strip the bar actually covers. The bar is a fraction of
            // the screen height, so the sample is the same fraction off the
            // top of the wallpaper.
            QScreen* screen = QGuiApplication::primaryScreen();
            const double barFraction = 40.0 / (screen ? screen->geometry().height() : 1080.0);
            const int stripH = qMax(1, int(img.height() * barFraction));
            const QImage strip = img.copy(0, 0, img.width(), stripH);

            qint64 r = 0, g = 0, b = 0;
            for (int y = 0; y < strip.height(); ++y) {
                const QRgb* line = reinterpret_cast<const QRgb*>(strip.constScanLine(y));
                for (int x = 0; x < strip.width(); ++x) {
                    r += qRed(line[x]);
                    g += qGreen(line[x]);
                    b += qBlue(line[x]);
                }
            }
            const qint64 n = qint64(strip.width()) * strip.height();
            if (n > 0) {
                *out = QColor(int(r / n), int(g / n), int(b / n));
                return true;
            }
        }
    }

    const QColor solid = solidBackgroundColor();
    if (solid.isValid()) {
        *out = solid;
        return true;
    }
    return sampleScreenStrip(out);
}

bool ContrastService::sampleScreenStrip(QColor* out) const {
    // Last resort: read the pixels actually on screen. This is the only method
    // that stays correct when the wallpaper is driven by something we cannot
    // read (slideshow, third-party tool, registry left empty) — and it also
    // accounts for whatever window happens to sit behind the bar.
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return false;

    const QRect geo = screen->geometry();
    const int barH = qBound(1, m_barHeight, geo.height() / 2);
    // Sample just below the bar so the bar's own pixels are not what we read.
    const int y = geo.top() + barH + 2;
    if (y + 6 > geo.bottom()) return false;

    const HWND desktop = GetDesktopWindow();
    HDC src = GetDC(desktop);
    if (!src) return false;

    const int w = geo.width();
    HDC mem = CreateCompatibleDC(src);
    HBITMAP bmp = mem ? CreateCompatibleBitmap(src, w, 8) : nullptr;
    if (!mem || !bmp) {
        if (mem) DeleteDC(mem);
        if (bmp) DeleteObject(bmp);
        ReleaseDC(desktop, src);
        return false;
    }
    HGDIOBJ old = SelectObject(mem, bmp);
    // GetDIBits cannot offset vertically, so the region has to be blitted into
    // a memory bitmap first and then read back.
    BitBlt(mem, 0, 0, w, 8, src, geo.left(), y, SRCCOPY);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -8; // negative: top-down rows
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    QImage img(w, 8, QImage::Format_RGB32);
    const int got = GetDIBits(mem, 0, 0, w * 8, img.bits(), &bi, DIB_RGB_COLORS);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(desktop, src);
    if (got == 0) return false;

    qint64 r = 0, g = 0, b = 0;
    for (int yy = 0; yy < img.height(); ++yy) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(yy));
        for (int x = 0; x < img.width(); ++x) {
            r += qRed(line[x]); g += qGreen(line[x]); b += qBlue(line[x]);
        }
    }
    const qint64 n = qint64(img.width()) * img.height();
    if (n == 0) return false;
    *out = QColor(int(r / n), int(g / n), int(b / n));
    return true;
}

bool ContrastService::sampleBackdropColor(QColor* out) const {
    if (m_backdrop == "solid") {
        // The bar paints its own colour, so the wallpaper is irrelevant.
        *out = m_backdropColor;
        return true;
    }
    if (!sampleWallpaperStrip(out)) return false;

    if (m_backdrop == "acrylic") {
        // Acrylic tints toward the backdrop colour and darkens the result. We
        // cannot reproduce the blur, but blending toward the tint still puts
        // the sample in the right ballpark.
        *out = QColor::fromRgbF(
            out->redF()   * (1.0 - m_backdropOpacity) + m_backdropColor.redF()   * m_backdropOpacity,
            out->greenF() * (1.0 - m_backdropOpacity) + m_backdropColor.greenF() * m_backdropOpacity,
            out->blueF()  * (1.0 - m_backdropOpacity) + m_backdropColor.blueF()  * m_backdropOpacity);
    }
    return true;
}

void ContrastService::refresh() {
    QColor chosen = m_onDark;

    if (m_autoContrast) {
        QColor bg;
        if (sampleBackdropColor(&bg)) {
            const double l = relativeLuminance(bg);
            // WCAG 2.x contrast ratio: (lighter + 0.05) / (darker + 0.05).
            // The bar of the ratio must be the background luminance, otherwise
            // the comparison inverts and white always wins.
            const double ratioWhite = (relativeLuminance(m_onDark) + 0.05) / (l + 0.05);
            const double ratioBlack = (l + 0.05) / (relativeLuminance(m_onLight) + 0.05);

            // Perceived brightness on a plain 0-255 scale. Contrast says which
            // colour reads better; this says whether the wallpaper is bright
            // enough to be worth abandoning white text for at all. A pastel
            // wallpaper can win on ratio and still be the wrong call, so the
            // two tests have to agree before the colour flips.
            const int brightness = qRound(0.299 * bg.red()
                                        + 0.587 * bg.green()
                                        + 0.114 * bg.blue());
            const bool brightEnough = m_darkText
                ? brightness > m_lightThreshold - kThresholdSlack
                : brightness >= m_lightThreshold;
            const bool wantDarkText = brightEnough && ratioBlack > ratioWhite;

            // Hysteresis: only flip when the winner is clearly ahead. Wallpapers
            // that sit near the midpoint would otherwise oscillate on every
            // refresh.
            if (wantDarkText != m_darkText) {
                const double margin = qAbs(ratioBlack - ratioWhite);
                if (margin > 0.4) m_darkText = wantDarkText;
            }
            chosen = m_darkText ? m_onLight : m_onDark;
        }
    } else {
        chosen = m_onDark;
        m_darkText = false;
    }

    if (chosen.name() == m_textColor) return;
    m_textColor = chosen.name();
    m_textColorValue = chosen;
    emit changed();
}