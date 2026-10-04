#include "theme.h"
#include <cmath>

QColor Theme::hexToRgb(const QString& hex) {
    QString h = hex;
    if (h.startsWith('#')) h = h.mid(1);
    if (h.size() == 3) h = QString(h[0]) + h[0] + h[1] + h[1] + h[2] + h[2];
    if (h.size() != 6) return QColor("#89b4fa");
    bool ok;
    int r = h.mid(0, 2).toInt(&ok, 16); if (!ok) return QColor("#89b4fa");
    int g = h.mid(2, 2).toInt(&ok, 16); if (!ok) return QColor("#89b4fa");
    int b = h.mid(4, 2).toInt(&ok, 16); if (!ok) return QColor("#89b4fa");
    return QColor(r, g, b);
}

QColor Theme::oklchToRgb(double L, double C, double H) {
    double hRad = H * M_PI / 180.0;
    double a = C * cos(hRad);
    double b = C * sin(hRad);

    double l_ = L + 0.3963377774 * a + 0.2158037573 * b;
    double m_ = L - 0.1055613458 * a - 0.0638541728 * b;
    double s_ = L - 0.0894841775 * a - 1.2914855480 * b;

    double l = l_ * l_ * l_;
    double m = m_ * m_ * m_;
    double s = s_ * s_ * s_;

    double r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    double bl = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

    auto clamp = [](double v) { return qBound(0.0, v, 1.0); };
    auto gamma = [](double v) {
        return v <= 0.0031308 ? 12.92 * v : 1.055 * pow(v, 1.0/2.4) - 0.055;
    };

    return QColor(
        static_cast<int>(gamma(clamp(r)) * 255),
        static_cast<int>(gamma(clamp(g)) * 255),
        static_cast<int>(gamma(clamp(bl)) * 255)
    );
}

void Theme::generate(const QString& seedHex) {
    QColor seed = hexToRgb(seedHex);

    // Convert seed to OKLCH (simplified)
    double r = seed.redF();
    double g = seed.greenF();
    double b = seed.blueF();

    double l = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    double a = 0.5 * (r - g);
    double b2 = 0.25 * (r + g) - 0.5 * b;

    double L = pow(l, 1.0/3.0);
    double C = sqrt(a*a + b2*b2);
    double H = atan2(b2, a) * 180.0 / M_PI;
    if (H < 0) H += 360.0;

    // Generate M3 roles
    m_primary = oklchToRgb(0.65, 0.22, H);
    m_onPrimary = oklchToRgb(0.98, 0.01, H);
    m_primaryContainer = oklchToRgb(0.35, 0.15, H);
    m_onPrimaryContainer = oklchToRgb(0.95, 0.05, H);
    m_surface = oklchToRgb(0.15, 0.02, H);
    m_onSurface = oklchToRgb(0.95, 0.02, H);
    m_surfaceContainer = oklchToRgb(0.20, 0.03, H);
    m_onSurfaceContainer = oklchToRgb(0.90, 0.03, H);
    m_outline = oklchToRgb(0.50, 0.02, H);
    m_surfaceVariant = oklchToRgb(0.25, 0.04, H);

    // Amber, for the battery-saver fill in the popup. Fixed hue rather than the
    // M3 tertiary role, which would sit on the seed's secondary palette: that
    // rendered gold on a blue seed and pink on a red one, so "power saving" was
    // never reliably the amber it is supposed to be. Its only consumer is that
    // one fill, so there is nothing else for a seed-derived accent to serve.
    m_tertiary = oklchToRgb(0.78, 0.16, 70);
    m_onTertiary = oklchToRgb(0.20, 0.04, 70);

    // M3 error role: fixed red rather than a seed-derived hue. Low battery is
    // red in every theme because it is a warning about the hardware, not a
    // statement about the accent colour.
    m_error = oklchToRgb(0.62, 0.19, 27);
    m_onError = oklchToRgb(0.98, 0.01, 27);

    // Charging green, and the deep blue for a full battery. Both fixed hues for
    // the same reason as error: "charging" and "full" mean the same thing on
    // every machine, so tinting them from the seed would only make them look
    // arbitrary. Dark blue is deliberately deep enough to read against the
    // popup surface, which primary is not.
    m_charging = oklchToRgb(0.72, 0.17, 150);
    m_full = oklchToRgb(0.58, 0.16, 255);

    emit changed();
}
