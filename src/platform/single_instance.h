#pragma once
#include <QObject>

// Named-mutex guard so a second copy of qbar exits instead of running alongside.
//
// This matters more once autostart exists: the bar starts at logon, so any later
// double-click of the exe races with it. Two bars means two AppBar registrations,
// two low-level mouse hooks and two fullscreen pollers all fighting over the same
// screen.
//
// The mutex is named per user session (Local\), so one user logged on twice gets
// two independent bars rather than the second silently exiting.
class SingleInstance : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(QObject* parent = nullptr);
    ~SingleInstance() override;

    // True when this process owns the mutex. False means another copy already
    // holds it and the caller should exit before doing any setup.
    bool acquire();
    void release();

private:
    void* m_handle = nullptr;   // HANDLE
    QString m_name;
};