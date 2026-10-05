#include "popup_host.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QCursor>
#include <QDebug>

HHOOK PopupHost::s_hook = nullptr;
PopupHost* PopupHost::s_instance = nullptr;

PopupHost::PopupHost(QObject* parent) : QObject(parent) {
    qApp->installEventFilter(this);
    m_closeTimer.setSingleShot(true);
    connect(&m_closeTimer, &QTimer::timeout, this, &PopupHost::finishClose);
}

void PopupHost::setAnchor(QQuickItem* item) {
    if (m_anchor == item) return;
    m_anchor = item;
    emit anchorChanged();
}

void PopupHost::open(const QString& url) {
    const bool wasOpen = (m_popup != nullptr);
    // Clicking the same anchor again dismisses the popup. Without this the
    // popup had no way to close, because WindowDeactivate fires immediately
    // after show and killed it the moment it opened.
    if (m_popup && m_popupUrl == url) {
        close();
        return;
    }
    // Replacing a popup must not animate: the old one is going away outright.
    finishClose();
    m_popupUrl = url;
    if (wasOpen) emit isOpenChanged();

    // PopupHost itself is a context property, so qmlEngine(this) can be null.
    // The anchor lives inside the QML tree, so its engine is authoritative.
    QQmlEngine* engine = m_anchor ? qmlEngine(m_anchor) : nullptr;
    if (!engine) engine = qmlEngine(this);
    if (!engine) {
        qWarning() << "PopupHost: no QML engine for" << url;
        return;
    }

    QQmlComponent component(engine, QUrl(url));
    if (component.isError()) {
        for (const auto& e : component.errors())
            qWarning() << "Popup error:" << e.toString();
        return;
    }

    QObject* obj = component.create();
    m_popup = qobject_cast<QQuickWindow*>(obj);
    if (!m_popup) {
        qWarning() << "PopupHost:" << url << "is not a Window";
        delete obj;
        return;
    }

    m_popup->setFlag(Qt::WindowDoesNotAcceptFocus, true);
    m_popup->setFlag(Qt::WindowStaysOnTopHint, true);

    // Dismiss on focus loss or outside click — a popup, not a dialog.
    m_popup->installEventFilter(this);

    if (m_anchor) {
        const QPointF global = m_anchor->mapToGlobal(QPointF(0, 0));
        // Centred under the anchor rather than flush with its left edge, so a
        // narrow tooltip still sits under the icon it describes.
        int px = static_cast<int>(global.x() + m_anchor->width() / 2 - m_popup->width() / 2);
        int py = static_cast<int>(global.y() + m_anchor->height() + 4);

        // Clamp into the work area so a popup near a screen edge stays on screen.
        QScreen* screen = QGuiApplication::screenAt(QPoint(px, py));
        if (!screen) screen = QGuiApplication::primaryScreen();
        if (screen) {
            const QRect wa = screen->availableGeometry();
            px = qBound(wa.left(), px, wa.right() - m_popup->width());
            py = qBound(wa.top(), py, wa.bottom() - m_popup->height());
        }

        m_popup->setX(px);
        m_popup->setY(py);
    }

    m_popup->show();
    installHook();

    // No-activate: strip WS_EX_ACTIVATE from the popup after realisation.
    const HWND hwnd = reinterpret_cast<HWND>(m_popup->winId());
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, (ex | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW) & ~WS_EX_APPWINDOW);
    emit isOpenChanged();
}

bool PopupHost::eventFilter(QObject* watched, QEvent* event) {
    // Dismiss on any press outside the popup and outside its anchor. The bar is
    // WS_EX_NOACTIVATE so it never takes focus, which rules out the usual
    // focus-out approach. Presses on other applications' windows are caught by
    // the low-level hook instead, which never reaches this filter.
    if (m_popup && event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPointF global = mouse->globalPosition();

        if (!rect().contains(global.toPoint()) && !anchorRect().contains(global.toPoint()))
            close();
    }
    return QObject::eventFilter(watched, event);
}

void PopupHost::close() {
    if (!m_popup || m_closing) return;
    m_closing = true;
    emit closeRequested();
    // The popup plays its own close animation on closeRequested; this timer is
    // what actually destroys it once that has had time to finish.
    m_closeTimer.start(120);
    // currentUrl is cleared here rather than in finishClose(). During those 120ms
    // the old popup is still visible and fading, so a QML hover landing in that
    // window would still compare equal to its own URL and skip opening its
    // replacement - leaving the pointer inside the hover area with nothing
    // showing and no further onEntered to correct it.
    m_popupUrl.clear();
    emit isOpenChanged();
}

void PopupHost::finishClose() {
    m_closeTimer.stop();
    m_closing = false;
    removeHook();
    if (m_popup) {
        m_popup->close();
        m_popup->deleteLater();
        m_popup = nullptr;
        m_popupUrl.clear();   // already cleared by close(); harmless if open() called this
        emit isOpenChanged();
    }
}

QRect PopupHost::rect() const {
    if (!m_popup) return QRect();
    return QRect(m_popup->x(), m_popup->y(), m_popup->width(), m_popup->height());
}

QRect PopupHost::anchorRect() const {
    if (!m_anchor) return QRect();
    const QPointF topLeft = m_anchor->mapToGlobal(QPointF(0, 0));
    return QRect(topLeft.toPoint(), QSize(m_anchor->width(), m_anchor->height()));
}

bool PopupHost::containsGlobal(const QPoint& p) const {
    if (m_popup && rect().contains(p)) return true;
    if (m_anchor && anchorRect().contains(p)) return true;
    return false;
}

void PopupHost::installHook() {
    if (m_hook) return;
    s_instance = this;
    s_hook = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, GetModuleHandleW(nullptr), 0);
    m_hook = (s_hook != nullptr);
}

void PopupHost::removeHook() {
    if (s_hook) {
        UnhookWindowsHookEx(s_hook);
        s_hook = nullptr;
    }
    s_instance = nullptr;
    m_hook = false;
}

LRESULT CALLBACK PopupHost::mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance) {
        switch (wParam) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_NCLBUTTONDOWN:
        case WM_NCRBUTTONDOWN:
        case WM_NCMBUTTONDOWN: {
            auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            if (!s_instance->containsGlobal(QPoint(info->pt.x, info->pt.y)))
                QMetaObject::invokeMethod(s_instance, "close", Qt::QueuedConnection);
            break;
        }
        default:
            break;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}