#include "single_instance.h"
#include <windows.h>
#include <QCoreApplication>
#include <QString>

SingleInstance::SingleInstance(QObject* parent) : QObject(parent) {
    // Local\ rather than Global\: the latter is session-wide and would make a
    // second user on the same machine unable to start their own bar.
    m_name = QStringLiteral("Local\\qbar-single-instance");
}

SingleInstance::~SingleInstance() {
    release();
}

bool SingleInstance::acquire() {
    if (m_handle)
        return true;

    // CreateMutex, not CreateEvent: a mutex is owned by a thread and released
    // automatically when that thread dies, so a bar that crashes cannot leave a
    // stale lock behind and block the next login.
    HANDLE h = CreateMutexW(nullptr, TRUE, m_name.toStdWString().c_str());
    if (!h)
        return false;   // last error tells us why, but there is nothing useful to do

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Somebody else got here first. Close our handle without releasing, since
        // we never owned it.
        CloseHandle(h);
        return false;
    }

    m_handle = h;
    return true;
}

void SingleInstance::release() {
    if (!m_handle)
        return;

    ReleaseMutex(static_cast<HANDLE>(m_handle));
    CloseHandle(static_cast<HANDLE>(m_handle));
    m_handle = nullptr;
}