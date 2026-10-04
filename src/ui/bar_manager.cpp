#include "bar_manager.h"
#include "platform/dwm.h"
#include <QCursor>
#include <QFile>
#include <QTextStream>
#include <QGuiApplication>
#include <QScreen>
#include <QQmlContext>
#include <QQmlEngine>
#include <windows.h>
#include <QThread>

BarManager::BarManager(QObject* parent) : QObject(parent) {
    m_mouseTimer = new QTimer(this);
    m_mouseTimer->setInterval(50);
    connect(m_mouseTimer, &QTimer::timeout, this, [this]() {
        if (!m_autoHide || !m_window) return;
        QPoint pos = QCursor::pos();
        bool nearEdge = (pos.y() <= m_revealZone);
        bool mouseOverBar = m_window->geometry().contains(pos);

        if (nearEdge || mouseOverBar) {
            if (!m_barVisible) {
                m_barVisible = true;
                m_window->setBarVisible(true);
                emit barVisibleChanged();
            }
        } else if (m_barVisible) {
            m_barVisible = false;
            m_window->setBarVisible(false);
            emit barVisibleChanged();
        }
    });

    m_fullscreenTimer = new QTimer(this);
    m_fullscreenTimer->setInterval(500);
    connect(m_fullscreenTimer, &QTimer::timeout, this, &BarManager::checkFullscreen);
}

void BarManager::createWindow() {
    if (m_window) return;
    m_window = new BarWindow();

    // Apply config
    if (m_config) {
        const bool overlay = (m_config->bar.integrations == "overlay" ||
                              m_config->bar.integrations == "overlay-hide");
        m_window->setUseAppBar(!overlay);
        m_window->setShape(m_config->bar.shape);
        m_window->setBackdrop(m_config->bar.backdrop);
        m_window->setBackdropStrength(m_config->bar.backdropStrength);
        m_window->setBackdropColor(QColor(m_config->bar.backdropColor));
        m_window->setBackdropOpacity(m_config->bar.backdropOpacity);
        m_window->setBorderWidth(m_config->bar.borderWidth);
        m_window->setBorderColor(QColor(m_config->bar.borderColor));
        m_window->setMarginHorizontal(m_config->bar.marginHorizontal);
        m_window->setBarHeight(m_config->bar.height);
        m_window->setMarginTop(m_config->bar.marginTop);
        m_window->setCornerRadius(m_config->bar.cornerRadius);
        m_window->setSurfaceOpacity(m_config->bar.surfaceOpacity);
        m_window->setShadowEnabled(m_config->bar.shadowEnabled);
        m_window->setShadowHeight(m_config->bar.shadowHeight);
        m_window->setShadowOpacity(m_config->bar.shadowOpacity);
        m_window->setShadowHairline(m_config->bar.shadowHairline);
        m_window->setShadowHairlineOpacity(m_config->bar.shadowHairlineOpacity);
        m_autoHide = m_config->bar.autoHide;
    }
}

void BarManager::setupWindow(QQmlContext* context) {
    if (!m_window) createWindow();
    if (!m_window) return;

    m_window->setAppContext(context);
    m_window->loadQml(QUrl("qrc:/qml/Bar.qml"));

    if (m_autoHide) {
        m_barVisible = false;
        m_window->setBarVisible(false);
    } else {
        m_barVisible = true;
        m_window->setBarVisible(true);
    }
    m_window->show();
    if (m_autoHide) {
        m_mouseTimer->start();
    }
    m_fullscreenTimer->start();
}

void BarManager::reload() {
    if (!m_config || !m_window) return;
    const bool overlay = (m_config->bar.integrations == "overlay" ||
                          m_config->bar.integrations == "overlay-hide");
    m_window->setUseAppBar(!overlay);
    m_window->setShape(m_config->bar.shape);
    m_window->setBackdrop(m_config->bar.backdrop);
    m_window->setBackdropStrength(m_config->bar.backdropStrength);
    m_window->setBackdropColor(QColor(m_config->bar.backdropColor));
    m_window->setBackdropOpacity(m_config->bar.backdropOpacity);
    m_window->setBorderWidth(m_config->bar.borderWidth);
    m_window->setBorderColor(QColor(m_config->bar.borderColor));
    m_window->setMarginHorizontal(m_config->bar.marginHorizontal);
    m_window->setBarHeight(m_config->bar.height);
    m_window->setMarginTop(m_config->bar.marginTop);
    m_window->setCornerRadius(m_config->bar.cornerRadius);
    m_window->setSurfaceOpacity(m_config->bar.surfaceOpacity);
    m_window->setShadowEnabled(m_config->bar.shadowEnabled);
    m_window->setShadowHeight(m_config->bar.shadowHeight);
    m_window->setShadowOpacity(m_config->bar.shadowOpacity);
    m_window->setShadowHairline(m_config->bar.shadowHairline);
    m_window->setShadowHairlineOpacity(m_config->bar.shadowHairlineOpacity);
    const bool autoHide = m_config->bar.autoHide;
    if (autoHide != m_autoHide) {
        m_autoHide = autoHide;
        if (m_autoHide) {
            m_mouseTimer->start();
        } else {
            m_mouseTimer->stop();
            // Same rule as the fullscreen timer: only undo a hide that this logic
            // caused, never one the user asked for.
            if (!m_barVisible && m_hiddenByFullscreen) {
                m_hiddenByFullscreen = false;
                m_barVisible = true;
                m_window->setBarVisible(true);
                emit barVisibleChanged();
            }
        }
    }
}

void BarManager::toggleBar() {
    // The user took manual control, so the fullscreen timer must not fight the
    // toggle by restoring the bar on its next tick.
    m_hiddenByFullscreen = false;
    m_barVisible = !m_barVisible;
    m_window->setBarVisible(m_barVisible);
    emit barVisibleChanged();
}

void BarManager::debugNote(const QString& msg) {
    QFile f("qbar.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << msg << "\n";
        s.flush();
        f.close();
    }
}

void BarManager::sendChord(WORD vk) {
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_LWIN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = vk;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_LWIN;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
}

void BarManager::openQuickSettings() {
    sendChord('A');
}

void BarManager::openNotificationCenter() {
    sendChord('N');
}

void BarManager::checkFullscreen() {
    if (!m_config) return;
    const bool hideWindow = m_config->bar.hideOnFullWindow;
    const bool hideScreen = m_config->bar.hideOnFullscreen;
    if (!hideWindow && !hideScreen) return;
    HWND hwnd = GetForegroundWindow();
    if (!m_window) return;

    // A shell window (desktop, taskbar) or no foreground window at all is by
    // definition not fullscreen. Leaving fullscreen often hands focus to the
    // shell for a moment, so this must fall through to the restore branch
    // below rather than returning early — otherwise the bar stays hidden until
    // some ordinary window takes focus.
    bool shouldHide = false;
    if (hwnd && hwnd != reinterpret_cast<HWND>(m_window->winId())) {
        wchar_t className[256];
        GetClassNameW(hwnd, className, 256);
        QString cls = QString::fromWCharArray(className);
        const bool isShell = (cls == "Progman" || cls == "WorkerW" ||
                              cls == "Shell_TrayWnd" || cls == "Shell_SecondaryTrayWnd");
        if (!isShell) {
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);

            // Compare against the monitor rect, not the work area. Our own
            // AppBar reservation shrinks the work area, and that reservation is
            // released while the bar is hidden — so a window restored to
            // maximized after leaving fullscreen would cover the full monitor
            // and read as fullscreen again. That kept the bar hidden until the
            // window was dragged or resized.
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfoW(mon, &mi)) {
                const RECT& monRect = mi.rcMonitor;

                // A real fullscreen window covers the whole monitor *and* has no
                // caption. A maximized window also covers the monitor but keeps
                // WS_CAPTION, so this test cannot mistake one for the other.
                const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
                const bool borderless = (style & WS_CAPTION) == 0;
                const bool coversMonitor =
                    windowRect.left   <= monRect.left &&
                    windowRect.top    <= monRect.top &&
                    windowRect.right  >= monRect.right &&
                    windowRect.bottom >= monRect.bottom;

                const bool fullscreen = borderless && coversMonitor;

                // Covering the work area while still not fullscreen means the
                // window is maximized over the bar's reserved strip.
                const RECT& work = mi.rcWork;
                const bool coversWorkArea =
                    windowRect.left   <= work.left &&
                    windowRect.top    <= work.top &&
                    windowRect.right  >= work.right &&
                    windowRect.bottom >= work.bottom;

                shouldHide = (fullscreen && hideScreen) ||
                             (coversWorkArea && !fullscreen && hideWindow);
            }
        }
    }

    // Only the fullscreen logic may undo a fullscreen hide. A bar the user toggled
    // off stays off, otherwise the timer brings it back on the next tick.
    if (shouldHide && m_barVisible) {
        m_barVisible = false;
        m_hiddenByFullscreen = true;
        m_window->setBarVisible(false);
        emit barVisibleChanged();
    } else if (!shouldHide && !m_barVisible && m_hiddenByFullscreen) {
        m_hiddenByFullscreen = false;
        m_barVisible = true;
        m_window->setBarVisible(true);
        emit barVisibleChanged();
    }
}
