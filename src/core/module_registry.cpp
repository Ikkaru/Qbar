#include "module_registry.h"
#include <QSet>

// Every entry here must point at a file that exists AND is listed in
// resources.qrc. A Loader pointed at a missing qrc URL fails at runtime, not
// at build time, so a stale entry here is a crash waiting for someone to add
// the id to layout in config.json.
//
// media, tray and notifications are deliberately absent: they are planned but
// unwritten. volume is absent too, because the volume control lives in
// Indicators.qml as an icon plus popups/VolumePopup.qml - there is no bar-row
// module for it.
static const QMap<QString, QString> s_moduleQml = {
    {"clock", "qrc:/qml/modules/Clock.qml"},
    {"workspaces", "qrc:/qml/modules/Workspaces.qml"},
    {"windowstatus", "qrc:/qml/modules/WindowStatus.qml"},
    {"indicators", "qrc:/qml/modules/Indicators.qml"},
    {"graph", "qrc:/qml/modules/Graph.qml"},
    {"network", "qrc:/qml/modules/Network.qml"},
};

void ModuleRegistry::rebuild() {
    m_left = QJsonArray();
    m_center = QJsonArray();
    m_right = QJsonArray();
    if (!m_config) { emit layoutChanged(); return; }

    auto filter = [this](const QStringList& ids) {
        QJsonArray arr;
        QSet<QString> seen;
        for (const auto& id : ids) {
            if (seen.contains(id)) continue;
            if (m_config->moduleEnabled(id) && s_moduleQml.contains(id)) {
                seen.insert(id);
                arr.append(id);
            }
        }
        return arr;
    };

    m_left = filter(m_config->layout.left);
    m_center = filter(m_config->layout.center);
    m_right = filter(m_config->layout.right);
    emit layoutChanged();
}

QString ModuleRegistry::qmlUrlFor(const QString& id) const {
    return s_moduleQml.value(id);
}
