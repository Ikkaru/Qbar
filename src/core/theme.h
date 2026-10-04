#pragma once
#include <QObject>
#include <QColor>
#include <QMap>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor primary READ primary NOTIFY changed)
    Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY changed)
    Q_PROPERTY(QColor primaryContainer READ primaryContainer NOTIFY changed)
    Q_PROPERTY(QColor onPrimaryContainer READ onPrimaryContainer NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor onSurface READ onSurface NOTIFY changed)
    Q_PROPERTY(QColor surfaceContainer READ surfaceContainer NOTIFY changed)
    Q_PROPERTY(QColor onSurfaceContainer READ onSurfaceContainer NOTIFY changed)
    Q_PROPERTY(QColor outline READ outline NOTIFY changed)
    Q_PROPERTY(QColor surfaceVariant READ surfaceVariant NOTIFY changed)
    // Accent hue for state that warns without being an error - currently the
    // battery icon while power saving is on. M3 tertiary role.
    Q_PROPERTY(QColor tertiary READ tertiary NOTIFY changed)
    Q_PROPERTY(QColor onTertiary READ onTertiary NOTIFY changed)
    // Destructive / low-battery state. Fixed hue, not seed-derived.
    Q_PROPERTY(QColor error READ error NOTIFY changed)
    Q_PROPERTY(QColor onError READ onError NOTIFY changed)
    // Battery fill states. Fixed hues, not seed-derived, because they mean the
    // same thing on every machine.
    Q_PROPERTY(QColor charging READ charging NOTIFY changed)
    Q_PROPERTY(QColor full READ full NOTIFY changed)
    // Colour for content that sits directly on the bar surface. Kept separate
    // from onSurfaceContainer because popups have their own opaque background
    // and must stay fixed while the bar itself follows the wallpaper.
    Q_PROPERTY(QColor barTextColor READ barTextColor WRITE setBarTextColor NOTIFY barTextColorChanged)
    Q_PROPERTY(QString font READ font WRITE setFont NOTIFY fontChanged)

public:
    explicit Theme(QObject* parent = nullptr) : QObject(parent) {}

    void generate(const QString& seedHex);

    QColor primary() const { return m_primary; }
    QColor onPrimary() const { return m_onPrimary; }
    QColor primaryContainer() const { return m_primaryContainer; }
    QColor onPrimaryContainer() const { return m_onPrimaryContainer; }
    QColor surface() const { return m_surface; }
    QColor onSurface() const { return m_onSurface; }
    QColor surfaceContainer() const { return m_surfaceContainer; }
    QColor onSurfaceContainer() const { return m_onSurfaceContainer; }
    QColor outline() const { return m_outline; }
    QColor surfaceVariant() const { return m_surfaceVariant; }
    QColor tertiary() const { return m_tertiary; }
    QColor onTertiary() const { return m_onTertiary; }
    QColor error() const { return m_error; }
    QColor onError() const { return m_onError; }
    QColor charging() const { return m_charging; }
    QColor full() const { return m_full; }
    QColor barTextColor() const { return m_barTextColor; }
    void setBarTextColor(const QColor& c) {
        if (m_barTextColor == c) return;
        m_barTextColor = c;
        emit barTextColorChanged();
    }
    QString font() const { return m_font; }
    void setFont(const QString& f) {
        if (m_font == f) return;
        m_font = f;
        emit fontChanged();
    }

signals:
    void changed();
    void fontChanged();
    void barTextColorChanged();

private:
    QString m_font = "Segoe UI Variable";
    QColor m_barTextColor = QColor("#FFFFFF");
    QColor m_primary;
    QColor m_onPrimary;
    QColor m_primaryContainer;
    QColor m_onPrimaryContainer;
    QColor m_surface;
    QColor m_onSurface;
    QColor m_surfaceContainer;
    QColor m_onSurfaceContainer;
    QColor m_outline;
    QColor m_surfaceVariant;
    QColor m_tertiary;
    QColor m_onTertiary;
    QColor m_error;
    QColor m_onError;
    QColor m_charging;
    QColor m_full;

    // OKLCH helpers
    static QColor oklchToRgb(double L, double C, double H);
    static QColor hexToRgb(const QString& hex);
};
