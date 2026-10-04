#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QVector>
#include <QString>
#include <windows.h>

// Global hotkeys via RegisterHotKey. The bar is WS_EX_NOACTIVATE and never
// takes focus, so a registered hotkey is the only way to trigger an action
// from outside the bar.
class Hotkeys : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit Hotkeys(QObject* parent = nullptr);
    ~Hotkeys() override;

    // chord is "Ctrl+Alt+R" style. Returns false when the chord cannot be
    // parsed or Windows refuses the registration (already taken by another app).
    Q_INVOKABLE bool registerHotkey(const QString& action, const QString& chord);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void triggered(const QString& action);

private:
    struct Entry { int id; QString action; };
    QVector<Entry> m_entries;
    int m_nextId = 1;
};