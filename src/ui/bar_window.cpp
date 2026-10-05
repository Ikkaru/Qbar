#include "bar_window.h"
#include "platform/dwm.h"
#include <QScreen>
#include <QGuiApplication>
#include <QColor>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <windows.h>
#include <shellapi.h>

BarWindow::BarWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->setClearColor(Qt::transparent);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_quickWidget);

    updateGeometry();

    HWND hwnd = reinterpret_cast<HWND>(winId());

    // System backdrop paints the entire window rectangle and cannot be shaped
    // to a rounded pill, so the corner voids would show raw acrylic. Painting
    // the backdrop in QML keeps the shape exact.
    Dwm::disableBackdrop(hwnd);
    Dwm::setBorderColor(hwnd, QColor(0, 0, 0, 0));
    applyCornerPreference();
    applyBackdrop();

    updateAppBar();
}

BarWindow::~BarWindow() {
    if (m_appBarRegistered) {
        APPBARDATA abd = { sizeof(abd) };
        abd.hWnd = reinterpret_cast<HWND>(winId());
        SHAppBarMessage(ABM_REMOVE, &abd);
        m_appBarRegistered = false;
    }
}

void BarWindow::closeEvent(QCloseEvent* event) {
    if (m_appBarRegistered) {
        APPBARDATA abd = { sizeof(abd) };
        abd.hWnd = reinterpret_cast<HWND>(winId());
        SHAppBarMessage(ABM_REMOVE, &abd);
        m_appBarRegistered = false;
    }
    QWidget::closeEvent(event);
}

void BarWindow::setAppContext(QQmlContext* context) {
    m_appContext = context;
}

void BarWindow::loadQml(const QUrl& url) {
    QQmlComponent component(m_appContext->engine(), url);
    if (component.isError()) {
        for (const auto& e : component.errors()) {
            qbarLog("QML error: " + e.toString());
        }
        return;
    }
    QObject* root = component.create();
    if (!root) {
        for (const auto& e : component.errors()) {
            qbarLog("QML create error: " + e.toString());
        }
        return;
    }
    m_quickWidget->setContent(url, &component, root);
}

void BarWindow::setBackdropStrength(double s) {
    if (qFuzzyCompare(m_backdropStrength, s)) return;
    m_backdropStrength = s;
    // The acrylic tint alpha is derived from this, so it has to be re-applied or
    // changing the strength in config would do nothing at all.
    applyBackdrop();
    emit backdropChanged();
}

void BarWindow::setBackdropColor(const QColor& c) {
    if (m_backdropColor == c) return;
    m_backdropColor = c;
    applyBackdrop();
    emit backdropChanged();
}

void BarWindow::setBackdropOpacity(double o) {
    if (qFuzzyCompare(m_backdropOpacity, o)) return;
    m_backdropOpacity = o;
    emit backdropChanged();
}

void BarWindow::setBorderWidth(int w) {
    if (m_borderWidth == w) return;
    m_borderWidth = w;
    emit borderChanged();
}

void BarWindow::setBorderColor(const QColor& c) {
    if (m_borderColor == c) return;
    m_borderColor = c;
    emit borderChanged();
}

void BarWindow::applyBackdrop() {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;

    // Dark mode first, and before any material is selected. DWM picks the light
    // or dark variant of a material from this flag alone, and setting it after
    // the backdrop leaves the window on the light one until it is recreated.
    Dwm::setDarkMode(hwnd, true);

    if (m_backdrop == "acrylic") {
        // The composition attribute, not DWMWA_SYSTEMBACKDROP_TYPE. The system
        // backdrop paints a flat tint with no blur at all - measured on this
        // machine as a uniform 69,69,69 across the whole bar - which reads as a
        // solid block. The accent path blurs what is actually behind the window
        // and honours the tint alpha, so `backdropStrength` can dial it from
        // clear glass to near-opaque.
        Dwm::disableBackdrop(hwnd);
        Dwm::setAcrylic(hwnd, m_backdropColor, m_backdropStrength);
        return;
    }

    // No acrylic tint to apply, so clear any leftover accent policy.
    Dwm::clearAcrylic(hwnd);

    if (m_backdrop == "mica") {
        // Deliberately NOT DWMSBT_MAINWINDOW. Real Mica samples the window's own
        // background rather than the desktop, so it measures as a flat 29,29,29
        // with no blur at all, and its tint cannot be tuned: setting the window's
        // class background brush to five different colours left it at exactly
        // 29,29,29 every time. A material that cannot be made less strong or
        // less weak is not usable for a bar.
        //
        // The accent blur gives real Mica-like behaviour - a neutral wash that
        // stays legible - and the QML layer above it sets the density. That is
        // what finally makes the "medium strength Mica" this mode is for
        // reachable, rather than the two available extremes.
        Dwm::disableBackdrop(hwnd);
        Dwm::setAcrylic(hwnd, m_backdropColor, m_backdropStrength);
        return;
    }

    Dwm::disableBackdrop(hwnd);
}

void BarWindow::setBackdrop(const QString& backdrop) {
    if (m_backdrop == backdrop) return;
    m_backdrop = backdrop;
    applyBackdrop();
}

void BarWindow::applyCornerPreference() {
    // Rounding is done in QML (barWindow.effectiveRadius). Letting DWM round
    // the window as well leaves white corner pixels on a translucent widget.
    Dwm::disableRoundedCorners(reinterpret_cast<HWND>(winId()));
}

void BarWindow::setShape(const QString& shape) {
    if (m_shape == shape) return;
    m_shape = shape;
    updateGeometry();
    updateAppBar();
    applyCornerPreference();
    applyBackdrop();
    emit shapeChanged();
    emit cornerRadiusChanged();
}

void BarWindow::setMarginHorizontal(int m) {
    if (m_marginHorizontal == m) return;
    m_marginHorizontal = m;
    updateGeometry();
    updateAppBar();
}

void BarWindow::setBarHeight(int h) {
    if (m_barHeight == h) return;
    m_barHeight = h;
    updateGeometry();
    updateAppBar();
    emit barHeightChanged();
}

void BarWindow::setMarginTop(int m) {
    if (m_marginTop == m) return;
    m_marginTop = m;
    updateGeometry();
    emit marginTopChanged();
}

void BarWindow::setCornerRadius(int r) {
    if (m_cornerRadius == r) return;
    m_cornerRadius = r;
    emit cornerRadiusChanged();
}

void BarWindow::setUseAppBar(bool use) {
    if (m_useAppBar == use) return;
    if (m_useAppBar && !use && m_appBarRegistered) {
        APPBARDATA abd = { sizeof(abd) };
        abd.hWnd = reinterpret_cast<HWND>(winId());
        SHAppBarMessage(ABM_REMOVE, &abd);
        m_appBarRegistered = false;
    }
    m_useAppBar = use;
    updateGeometry();
    updateAppBar();
}

void BarWindow::setBarVisible(bool visible) {
    if (m_barVisible == visible) return;
    m_barVisible = visible;
    updateAppBar();
    if (visible) {
        show();
    } else {
        hide();
    }
    emit barVisibleChanged();
}

void BarWindow::updateGeometry() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect scr = screen->geometry();

    // Inset horizontally only when floating (overlay). Under AppBar the shell
    // reserves the full width, so insetting would leave an unpainted gap.
    const bool floating = (m_shape == "pill") && !m_useAppBar;
    const int inset = floating ? m_marginHorizontal : 0;

    // square sits flush against the top edge with no margin
    const int topInset = (m_shape == "square") ? 0 : m_marginTop;

    setGeometry(scr.x() + inset, scr.y() + topInset,
                scr.width() - 2 * inset, m_barHeight);
}

void BarWindow::updateAppBar() {
    if (!m_useAppBar) return;
    APPBARDATA abd = { sizeof(abd) };
    abd.hWnd = reinterpret_cast<HWND>(winId());
    abd.uCallbackMessage = WM_APP + 1;
    abd.uEdge = ABE_TOP;

    if (!m_appBarRegistered) {
        abd.rc = { 0, 0, GetSystemMetrics(SM_CXSCREEN), m_barHeight };
        if (SHAppBarMessage(ABM_NEW, &abd)) {
            m_appBarRegistered = true;
        }
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    // Use full screen geometry, not availableGeometry: the work area is already
    // shrunken by our own reservation, which would feed back into the rect.
    // Reserve the real window rect so the work area matches what is painted.
    const QRect g = geometry();
    const int height = m_barHeight;

    if (m_barVisible) {
        abd.rc = { g.left(), g.top(), g.right(), g.top() + height };
    } else {
        abd.rc = { g.left(), g.top() - height - 8, g.right(), g.top() };
    }

    SHAppBarMessage(ABM_SETPOS, &abd);
}

bool BarWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_DISPLAYCHANGE || msg->message == WM_DPICHANGED) {
        updateGeometry();
        updateAppBar();
    }
    return QWidget::nativeEvent(eventType, message, result);
}
