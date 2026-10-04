#include "window_status_service.h"
#include "workspaces_service.h"
#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

namespace {

bool isShellWindow(HWND hwnd) {
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    const QString cls = QString::fromWCharArray(className);
    return cls == "Progman" || cls == "WorkerW" || cls == "SHELLDLL_DefView";
}

QString processName(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return QString();
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    QString name;
    if (QueryFullProcessImageNameW(h, 0, path, &size)) {
        const QString full = QString::fromWCharArray(path);
        name = full.mid(full.lastIndexOf('\\') + 1);
        name = name.mid(0, name.lastIndexOf('.'));
    }
    CloseHandle(h);
    return name;
}

} // namespace

WindowStatusService::WindowStatusService(QObject* parent) : QObject(parent) {
    refresh();
}

void WindowStatusService::refresh() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return;

    QString newApp;
    QString newTitle;
    bool desktop = false;

    if (isShellWindow(hwnd)) {
        // Focused on the desktop: show the desktop as the app and the current
        // virtual desktop index as the title.
        desktop = true;
        newApp = QStringLiteral("Desktop");
        newTitle = QStringLiteral("Workspace %1")
                       .arg(m_workspaces ? m_workspaces->activeIndex() + 1 : 1);
    } else {
        wchar_t title[512];
        GetWindowTextW(hwnd, title, 512);
        newTitle = QString::fromWCharArray(title);

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        newApp = processName(pid);
    }

    if (newApp != m_appName || newTitle != m_windowTitle || desktop != m_isDesktop) {
        m_appName = newApp;
        m_windowTitle = newTitle;
        m_isDesktop = desktop;
        emit changed();
    }
}