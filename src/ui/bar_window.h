#pragma once
#include <QWidget>
#include <QQuickWidget>
#include <QQmlContext>

class BarWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int barHeight READ barHeight WRITE setBarHeight NOTIFY barHeightChanged)
    Q_PROPERTY(int marginTop READ marginTop WRITE setMarginTop NOTIFY marginTopChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(bool barVisible READ barVisible WRITE setBarVisible NOTIFY barVisibleChanged)
    Q_PROPERTY(QString shape READ shape WRITE setShape NOTIFY shapeChanged)
    Q_PROPERTY(QString backdropMode READ backdropMode NOTIFY backdropChanged)
    Q_PROPERTY(int effectiveRadius READ effectiveRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(double surfaceOpacity READ surfaceOpacity WRITE setSurfaceOpacity NOTIFY surfaceOpacityChanged)
    Q_PROPERTY(double backdropStrength READ backdropStrength NOTIFY backdropChanged)
    Q_PROPERTY(QColor backdropColor READ backdropColor NOTIFY backdropChanged)
    Q_PROPERTY(double backdropOpacity READ backdropOpacity NOTIFY backdropChanged)
    Q_PROPERTY(int borderWidth READ borderWidth NOTIFY borderChanged)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY borderChanged)
    Q_PROPERTY(bool shadowEnabled READ shadowEnabled WRITE setShadowEnabled NOTIFY shadowChanged)
    Q_PROPERTY(int shadowHeight READ shadowHeight WRITE setShadowHeight NOTIFY shadowChanged)
    Q_PROPERTY(double shadowOpacity READ shadowOpacity WRITE setShadowOpacity NOTIFY shadowChanged)
    Q_PROPERTY(bool shadowHairline READ shadowHairline WRITE setShadowHairline NOTIFY shadowChanged)
    Q_PROPERTY(double shadowHairlineOpacity READ shadowHairlineOpacity WRITE setShadowHairlineOpacity NOTIFY shadowChanged)

public:
    explicit BarWindow(QWidget* parent = nullptr);
    ~BarWindow() override;

    int barHeight() const { return m_barHeight; }
    int marginTop() const { return m_marginTop; }
    int cornerRadius() const { return m_cornerRadius; }
    bool barVisible() const { return m_barVisible; }

    void setBarHeight(int h);
    void setMarginTop(int m);
    void setCornerRadius(int r);
    void setShape(const QString& shape);
    QString shape() const { return m_shape; }
    QString backdropMode() const { return m_backdrop; }
    void setBackdrop(const QString& backdrop);
    void setBackdropStrength(double s);
    void setBackdropColor(const QColor& c);
    void setBackdropOpacity(double o);
    void setBorderWidth(int w);
    void setBorderColor(const QColor& c);
    void setMarginHorizontal(int m);

    // pill = fully rounded capsule; square = 0; rounded = config cornerRadius
    int effectiveRadius() const {
        if (m_shape == "pill") return m_barHeight / 2;
        if (m_shape == "square") return 0;
        return m_cornerRadius;
    }
    void setBarVisible(bool visible);
    void setUseAppBar(bool use);
    void loadQml(const QUrl& url);
    void setAppContext(QQmlContext* context);

double surfaceOpacity() const { return m_surfaceOpacity; }
    void setSurfaceOpacity(double o) {
        if (qFuzzyCompare(m_surfaceOpacity, o)) return;
        m_surfaceOpacity = o;
        emit surfaceOpacityChanged();
    }
    double backdropStrength() const { return m_backdropStrength; }
    QColor backdropColor() const { return m_backdropColor; }
    double backdropOpacity() const { return m_backdropOpacity; }
    int borderWidth() const { return m_borderWidth; }
    QColor borderColor() const { return m_borderColor; }
    bool shadowEnabled() const { return m_shadowEnabled; }
    int shadowHeight() const { return m_shadowHeight; }
    double shadowOpacity() const { return m_shadowOpacity; }

    void setShadowEnabled(bool on) {
        if (m_shadowEnabled == on) return;
        m_shadowEnabled = on;
        emit shadowChanged();
    }
    void setShadowHeight(int h) {
        if (m_shadowHeight == h) return;
        m_shadowHeight = qMax(0, h);
        emit shadowChanged();
    }
    void setShadowOpacity(double o) {
        if (qFuzzyCompare(m_shadowOpacity, o)) return;
        m_shadowOpacity = qBound(0.0, o, 1.0);
        emit shadowChanged();
    }
    bool shadowHairline() const { return m_shadowHairline; }
    double shadowHairlineOpacity() const { return m_shadowHairlineOpacity; }
    void setShadowHairline(bool on) {
        if (m_shadowHairline == on) return;
        m_shadowHairline = on;
        emit shadowChanged();
    }
    void setShadowHairlineOpacity(double o) {
        if (qFuzzyCompare(m_shadowHairlineOpacity, o)) return;
        m_shadowHairlineOpacity = qBound(0.0, o, 1.0);
        emit shadowChanged();
    }

signals:
    void barHeightChanged();
    void marginTopChanged();
    void cornerRadiusChanged();
    void barVisibleChanged();
    void shapeChanged();
    void surfaceOpacityChanged();
    void backdropChanged();
    void borderChanged();
    void shadowChanged();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void updateAppBar();
    void updateGeometry();
    void applyCornerPreference();
    void applyBackdrop();

    QQuickWidget* m_quickWidget = nullptr;
    QQmlContext* m_appContext = nullptr;
    int m_barHeight = 40;
    int m_marginTop = 2;
    int m_cornerRadius = 12;
    bool m_barVisible = true;
    bool m_appBarRegistered = false;
    bool m_useAppBar = true;
    QString m_shape = "pill";
    QString m_backdrop = "none";
    double m_surfaceOpacity = 0.55;
    double m_backdropStrength = 0.5;
    QColor m_backdropColor = QColor("#1e1e28");
    double m_backdropOpacity = 0.55;
    int m_borderWidth = 0;
    QColor m_borderColor = QColor("#404050");
    int m_marginHorizontal = 12;
    bool m_shadowEnabled = true;
    int m_shadowHeight = 14;
    double m_shadowOpacity = 0.22;
    bool m_shadowHairline = true;
    double m_shadowHairlineOpacity = 0.28;
};
