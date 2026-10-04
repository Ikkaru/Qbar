#pragma once
#include <QObject>
#include <QTimer>
#include <QColor>
#include <QString>

// Picks bar text colour from the wallpaper behind the bar.
//
// The bar surface is translucent, so text colour is really a question of
// contrast against whatever is currently on screen behind it. A fixed white
// reads crisp on a dark wallpaper and washed out on a light one. We sample the
// strip of wallpaper that the bar actually covers and pick whichever of the two
// configured text colours has the higher WCAG contrast ratio against it.
class ContrastService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString textColor READ textColor NOTIFY changed)
    Q_PROPERTY(QColor textColorValue READ textColorValue NOTIFY changed)
    Q_PROPERTY(bool autoContrast READ autoContrast WRITE setAutoContrast NOTIFY changed)
    Q_PROPERTY(bool darkText READ darkText NOTIFY changed)

public:
    explicit ContrastService(QObject* parent = nullptr);

    QString textColor() const { return m_textColor; }
    QColor textColorValue() const { return m_textColorValue; }
    bool autoContrast() const { return m_autoContrast; }
    bool darkText() const { return m_darkText; }

    void setAutoContrast(bool on);

    // The bar's own surface contributes to what sits behind the text, so it has
    // to be part of the calculation. A solid bar needs no wallpaper sample at
    // all; a clear bar needs the wallpaper unchanged.
    void setBackdrop(const QString& type, const QColor& color, double opacity);
    void setTextColors(const QColor& onLight, const QColor& onDark);

    // Contrast alone is a poor switch on its own: a pastel wallpaper can favour
    // dark text and still be the wrong call aesthetically. This is a plain
    // 0-255 brightness floor the sampled background must clear before light
    // text is allowed to turn dark. Keeps pastel wallpapers on white text even
    // when black would technically score a higher contrast ratio.
    void setLightThreshold(int level);
    void setBarHeight(int px);
    int lightThreshold() const { return m_lightThreshold; }

    void refresh();

signals:
    void changed();

private:
    // Effective colour of the strip behind the bar, after backdrop blending.
    bool sampleBackdropColor(QColor* out) const;
    bool sampleWallpaperStrip(QColor* out) const;
    bool sampleScreenStrip(QColor* out) const;
    static double relativeLuminance(const QColor& c);

    QString m_textColor = "#FFFFFF";
    QColor m_textColorValue = Qt::white;
    QColor m_onLight = QColor("#101010");
    QColor m_onDark = QColor("#FFFFFF");

    QString m_backdrop = "clear";
    QColor m_backdropColor = QColor("#1e1e28");
    double m_backdropOpacity = 0.55;

    bool m_autoContrast = true;
    bool m_darkText = false; // false = light text, used on a dark backdrop
    int m_lightThreshold = 165;
    int m_barHeight = 40;

    QTimer* m_timer = nullptr;
};