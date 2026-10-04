#include "workspaces_service.h"
#include <windows.h>
#include <shlobj.h>
#include <comdef.h>
#include <wrl/client.h>
#include <QDebug>

using namespace Microsoft::WRL;

WorkspacesService::WorkspacesService(QObject* parent) : QObject(parent) {
    // IVirtualDesktopManager is undocumented but stable for years.
    // For now, provide a single "Desktop" — full COM wiring comes in Phase 2 deep dive.
    m_names << "Desktop 1";
    m_activeIndex = 0;
}

QString WorkspacesService::nameAt(int index) const {
    if (index < 0 || index >= m_names.size()) return QString();
    return m_names[index];
}

void WorkspacesService::switchTo(int index) {
    if (index < 0 || index >= m_names.size() || index == m_activeIndex) return;
    m_activeIndex = index;
    emit activeIndexChanged();
}
