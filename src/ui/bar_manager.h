#pragma once
#include <QObject>
#include <QTimer>
#include <QQmlContext>
#include <windows.h>
#include "bar_window.h"
#include "core/config.h"
#include "core/theme.h"

class BarManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool barVisible READ barVisible NOTIFY barVisibleChanged)

public:
    explicit BarManager(QObject* parent = nullptr);

    void setConfig(Config* config) { m_config = config; }
    void setTheme(Theme* theme) { m_theme = theme; }
    // Split from setupWindow so main.cpp can register the `barWindow` context
    // property before any QML is parsed.
    void createWindow();
    void setupWindow(QQmlContext* context);
    BarWindow* window() const { return m_window; }

    bool barVisible() const { return m_barVisible; }

    void reload();
    void toggleBar();

    Q_INVOKABLE void debugNote(const QString& msg);

    // Shell panel triggers. Windows has no public API for these flyouts, so the
    // key chord is synthesised with SendInput — the same mechanism the shell uses.
    // QML cannot call a plain public method on a context property; only slots
    // and Q_INVOKABLEs are reachable, and a rejected call fails silently.
    Q_INVOKABLE void openQuickSettings();   // Win+A  — Wi-Fi, volume, brightness, battery
    Q_INVOKABLE void openNotificationCenter(); // Win+N — notifications + calendar

signals:
    void barVisibleChanged();

private:
    void checkFullscreen();
    void sendChord(WORD vk);

    Config* m_config = nullptr;
    Theme* m_theme = nullptr;
    BarWindow* m_window = nullptr;
    QTimer* m_mouseTimer = nullptr;
    QTimer* m_fullscreenTimer = nullptr;
    bool m_barVisible = true;
    bool m_autoHide = true;
    // True while the bar is hidden by the fullscreen logic rather than by a
    // manual toggle, so only that logic may bring it back.
    bool m_hiddenByFullscreen = false;
    int m_revealZone = 8;
};
