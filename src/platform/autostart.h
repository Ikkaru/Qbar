#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>

// Windows autostart via the per-user Run key.
//
// This is the key Task Manager's Startup tab reads, which is the point: the user
// can enable, disable and remove qbar from there without knowing that anything
// else exists. A Startup-folder shortcut or a scheduled task would show up too,
// but a Run entry is a single value that is trivial to inspect and remove.
//
// Two things make this less obvious than it looks, and both were found by
// watching what Task Manager actually writes:
//
//  1. Disabling an entry in Task Manager does NOT delete the Run value. It writes
//     StartupApproved\Run\qbar = { 03 00 ... } and leaves the command in place.
//     So "the Run value exists" says nothing about whether the entry will run.
//     Reading only the Run key makes this class report a disabled entry as
//     enabled, and the user is left with a config that says true, a registry
//     that looks registered, and a bar that silently never starts at logon.
//
//  2. Writing the Run value does not re-enable an entry either. Once the approval
//     blob says disabled, the shell skips it no matter what the Run value holds.
//     So registering has to clear the approval too, or the change is invisible
//     until someone opens Task Manager and wonders why nothing happened.
//
// The division of authority this class therefore implements: config decides
// whether an entry EXISTS, Task Manager decides whether it is ALLOWED to run.
// sync() creates or removes, but never resurrects an entry the user disabled in
// Task Manager - otherwise the bar would silently undo the decision every time
// it started.
class Autostart : public QObject {
    Q_OBJECT
    // Effective state: an entry the user disabled in Task Manager reports false,
    // because that is what actually happens at logon.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit Autostart(QObject* parent = nullptr);

    bool enabled() const;
    void setEnabled(bool on);

    // Brings the registry in line with `want`: adds the entry when config asks
    // for one and none exists, removes it when config drops it. An entry that
    // exists but has been disabled in Task Manager is left alone.
    //
    // Called at startup and again on config reload, so editing config.json by hand
    // works without a restart.
    void sync(bool want);

    // Full command line as registered, exe quoted. Empty when not registered.
    QString command() const;

    // True when the Run value is present and non-empty, regardless of whether
    // Task Manager permits it to run. `enabled()` and this differ exactly when
    // the user disabled the entry.
    bool registered() const;

    // Pure part of the consent logic, split out so it can be tested without
    // touching the registry: given the StartupApproved blob, may it start?
    // An absent or empty blob means Windows has no opinion yet, so it may.
    static bool approvalAllowsStartup(const QByteArray& approval);

signals:
    void enabledChanged();

private:
    static QByteArray readApproval();
    static bool clearApproval();

    bool writeRunValue(const QString& command);
    bool clearRunValue();
};
